/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/cli_util.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/sample_processing_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

typedef std::variant<
    const MixGainParamDefinition*, const DemixingParamDefinition*,
    const ReconGainParamDefinition*, const ExtendedParamDefinition*>
    ConcreteParamDefinition;
absl::Status InsertParamDefinitionAndCheckEquivalence(
    const ConcreteParamDefinition param_definition_to_insert,
    absl::flat_hash_map<DecodedUleb128, ConcreteParamDefinition>&
        concrete_param_definitions) {
  const auto parameter_id = std::visit(
      [](const auto* concrete_param_definition) {
        return concrete_param_definition->parameter_id_;
      },
      param_definition_to_insert);
  const auto [existing_param_definition_iter, inserted] =
      concrete_param_definitions.insert(
          {parameter_id, param_definition_to_insert});

  // Use double dispatch to check equivalence. Note this automatically returns
  // false when the two variants do not hold the same type of objects.
  const auto equivalent_to_param_definition_to_insert =
      [&param_definition_to_insert](const auto* rhs) {
        return std::visit([&rhs](const auto& lhs) { return (*lhs == *rhs); },
                          param_definition_to_insert);
      };

  if (!inserted && !std::visit(equivalent_to_param_definition_to_insert,
                               existing_param_definition_iter->second)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Unequivalent `param_definition_mode` for id = ", parameter_id));
  }

  return absl::OkStatus();
};

absl::Status GetPerIdMetadata(
    const DecodedUleb128 parameter_id,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const ParamDefinition* param_definition,
    PerIdParameterMetadata& per_id_metadata) {
  RETURN_IF_NOT_OK(ValidateHasValue(param_definition->GetType(),
                                    "`param_definition_type`."));
  // Initialize common fields.
  per_id_metadata.param_definition = *param_definition;

  // Return early if this is not a recon gain parameter and the rest of the
  // fields are not present.
  if (per_id_metadata.param_definition.GetType() !=
      ParamDefinition::kParameterDefinitionReconGain) {
    return absl::OkStatus();
  }

  const ReconGainParamDefinition* recon_gain_param_definition =
      static_cast<const ReconGainParamDefinition*>(param_definition);
  auto audio_element_iter =
      audio_elements.find(recon_gain_param_definition->audio_element_id_);
  if (audio_element_iter == audio_elements.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Audio Element ID: ", recon_gain_param_definition->audio_element_id_,
        " associated with the recon gain parameter of ID: ", parameter_id,
        " not found"));
  }
  per_id_metadata.audio_element_id = audio_element_iter->first;
  const auto& channel_config = std::get<ScalableChannelLayoutConfig>(
      audio_element_iter->second.obu.config_);
  per_id_metadata.num_layers = channel_config.num_layers;
  per_id_metadata.recon_gain_is_present_flags.resize(
      per_id_metadata.num_layers);
  for (int l = 0; l < per_id_metadata.num_layers; l++) {
    per_id_metadata.recon_gain_is_present_flags[l] =
        (channel_config.channel_audio_layer_configs[l]
             .recon_gain_is_present_flag == 1);
  }
  per_id_metadata.channel_numbers_for_layers =
      audio_element_iter->second.channel_numbers_for_layers;

  return absl::OkStatus();
}

}  // namespace

bool IsStereoLayout(const Layout& layout) {
  const Layout kStereoLayout = {
      .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersSsConventionLayout{
          .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};
  return layout == kStereoLayout;
}

absl::Status GetIndicesForLayout(
    const std::vector<MixPresentationSubMix>& mix_presentation_sub_mixes,
    const Layout& layout, int& output_submix_index, int& output_layout_index) {
  for (int s = 0; s < mix_presentation_sub_mixes.size(); s++) {
    const auto& sub_mix = mix_presentation_sub_mixes[s];
    for (int l = 0; l < sub_mix.num_layouts; l++) {
      const auto& mix_presentation_layout = sub_mix.layouts[l];
      if (layout == mix_presentation_layout.loudness_layout) {
        output_submix_index = s;
        output_layout_index = l;
        return absl::OkStatus();
      }
    }
  }
  return absl::InvalidArgumentError(
      "No match found in the mix presentation submixes for the desired "
      "layout.");
}

absl::Status CollectAndValidateParamDefinitions(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  // A temporary map that stores param definitions in their original concrete
  // types, which will later be transferred to the output `param_definitions`
  // that stores only the base pointers.
  absl::flat_hash_map<DecodedUleb128, ConcreteParamDefinition>
      concrete_param_definitions;

  // Collect all `param_definition`s in Audio Element and Mix Presentation
  // OBUs.
  for (const auto& [audio_element_id_for_debugging, audio_element] :
       audio_elements) {
    for (const auto& audio_element_param :
         audio_element.obu.audio_element_params_) {
      const auto param_definition_type = audio_element_param.GetType();
      switch (param_definition_type) {
        case ParamDefinition::kParameterDefinitionDemixing:
          RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
              &std::get<DemixingParamDefinition>(
                  audio_element_param.param_definition),
              concrete_param_definitions));
          break;
        case ParamDefinition::kParameterDefinitionReconGain:
          RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
              &std::get<ReconGainParamDefinition>(
                  audio_element_param.param_definition),
              concrete_param_definitions));
          break;
        default:
          LOG(WARNING) << "Ignoring parameter definition of type= "
                       << param_definition_type << " in audio element= "
                       << audio_element_id_for_debugging;
          continue;
      }
    }
  }

  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
      for (const auto& audio_element : sub_mix.audio_elements) {
        RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
            &audio_element.element_mix_gain, concrete_param_definitions));
      }
      RETURN_IF_NOT_OK(InsertParamDefinitionAndCheckEquivalence(
          &sub_mix.output_mix_gain, concrete_param_definitions));
    }
  }

  // Now cast to base pointers and store in the output `param_definitions`.
  const auto cast_to_base_pointer = [](const auto* concrete_param_definition) {
    return static_cast<const ParamDefinition*>(concrete_param_definition);
  };
  for (const auto& [parameter_id, concrete_param_definition] :
       concrete_param_definitions) {
    param_definitions[parameter_id] =
        std::visit(cast_to_base_pointer, concrete_param_definition);
  }

  return absl::OkStatus();
}

absl::StatusOr<absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>>
GenerateParamIdToMetadataMap(
    const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements) {
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  for (const auto& [parameter_id, param_definition] : param_definitions) {
    auto [iter, inserted] = parameter_id_to_metadata.insert(
        {parameter_id, PerIdParameterMetadata()});
    if (!inserted) {
      // An entry corresponding to the same ID is already in the map.
      continue;
    }

    // Create a new entry.
    auto& per_id_metadata = iter->second;
    RETURN_IF_NOT_OK(GetPerIdMetadata(parameter_id, audio_elements,
                                      param_definition, per_id_metadata));
  }
  return parameter_id_to_metadata;
}

absl::Status CompareTimestamps(InternalTimestamp expected_timestamp,
                               InternalTimestamp actual_timestamp,
                               absl::string_view prompt) {
  if (expected_timestamp != actual_timestamp) {
    return absl::InvalidArgumentError(
        absl::StrCat(prompt, "Expected timestamp != actual timestamp: (",
                     expected_timestamp, " vs ", actual_timestamp, ")"));
  }
  return absl::OkStatus();
}

absl::Status WritePcmFrameToBuffer(
    const std::vector<std::vector<int32_t>>& frame,
    uint32_t samples_to_trim_at_start, uint32_t samples_to_trim_at_end,
    uint8_t bit_depth, bool big_endian, std::vector<uint8_t>& buffer) {
  if (bit_depth % 8 != 0) {
    return absl::InvalidArgumentError(
        "This function only supports an integer number of bytes.");
  }
  const size_t num_samples =
      (frame.size() - samples_to_trim_at_start - samples_to_trim_at_end) *
      frame[0].size();

  buffer.resize(num_samples * (bit_depth / 8));

  // The input frame is arranged in (time, channel) axes. Interlace these in
  // the output PCM and skip over any trimmed samples.
  int write_position = 0;
  for (int t = samples_to_trim_at_start;
       t < frame.size() - samples_to_trim_at_end; t++) {
    for (int c = 0; c < frame[0].size(); ++c) {
      const uint32_t sample = static_cast<uint32_t>(frame[t][c]);
      RETURN_IF_NOT_OK(WritePcmSample(sample, bit_depth, big_endian,
                                      buffer.data(), write_position));
    }
  }

  return absl::OkStatus();
}

absl::Status GetCommonSampleRateAndBitDepth(
    const absl::flat_hash_set<uint32_t>& sample_rates,
    const absl::flat_hash_set<uint8_t>& bit_depths,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling) {
  requires_resampling = false;
  if (sample_rates.empty() || bit_depths.empty()) {
    return absl::InvalidArgumentError(
        "Expected at least one sample rate and bit depth.");
  }

  if (sample_rates.size() == 1) {
    common_sample_rate = *sample_rates.begin();
  } else {
    // No common sample rate. The spec recommends the rendering output to be
    // resampled to 48000 Hz.
    common_sample_rate = 48000;
    requires_resampling = true;
  }

  if (bit_depths.size() == 1) {
    common_bit_depth = *bit_depths.begin();
  } else {
    // No common bit-depth. The spec recommends the rendering output to be
    // resampled to 16-bits.
    common_bit_depth = 16;
    requires_resampling = true;
  }

  return absl::OkStatus();
}

absl::Status GetCommonSamplesPerFrame(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    uint32_t& common_samples_per_frame) {
  bool first = true;
  for (const auto& [unused_id, codec_config_obu] : codec_config_obus) {
    if (first) {
      common_samples_per_frame = codec_config_obu.GetNumSamplesPerFrame();
      first = false;
      continue;
    }

    if (common_samples_per_frame != codec_config_obu.GetNumSamplesPerFrame()) {
      return absl::UnknownError(
          "The encoder does not support Codec Config OBUs with a different "
          "number of samples per frame yet.");
    }
  }

  return absl::OkStatus();
}

void LogChannelNumbers(const std::string& name,
                       const ChannelNumbers& channel_numbers) {
  LOG(INFO) << name << ": [" << channel_numbers.surround << "."
            << channel_numbers.lfe << "." << channel_numbers.height << "]";
}

}  // namespace iamf_tools
