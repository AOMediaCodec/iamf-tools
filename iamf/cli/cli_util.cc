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
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/sample_processing_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status GetPerIdMetadata(
    const DecodedUleb128 parameter_id,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const ParamDefinition* param_definition,
    PerIdParameterMetadata& per_id_metadata) {
  RETURN_IF_NOT_OK(ValidateHasValue(param_definition->GetType(),
                                    "`param_definition_type`."));
  // Initialize common fields.
  per_id_metadata.param_definition_type = param_definition->GetType().value();
  per_id_metadata.param_definition = *param_definition;

  // Return early if this is not a recon gain parameter and the rest of the
  // fields are not present.
  if (per_id_metadata.param_definition_type !=
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

absl::Status CollectAndValidateParamDefinitions(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  auto insert_and_check_equivalence =
      [&](const ParamDefinition* param_definition) -> absl::Status {
    const auto parameter_id = param_definition->parameter_id_;
    const auto [iter, inserted] =
        param_definitions.insert({parameter_id, param_definition});
    if (!inserted && *iter->second != *param_definition) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Inequivalent `param_definition_mode` for id = ", parameter_id));
    }

    return absl::OkStatus();
  };

  // Collect all `param_definition`s in Audio Element and Mix Presentation
  // OBUs.
  for (const auto& [audio_element_id_for_debugging, audio_element] :
       audio_elements) {
    for (const auto& audio_element_param :
         audio_element.obu.audio_element_params_) {
      const auto param_definition_type =
          audio_element_param.param_definition_type;
      switch (param_definition_type) {
        case ParamDefinition::kParameterDefinitionDemixing:
        case ParamDefinition::kParameterDefinitionReconGain:
          RETURN_IF_NOT_OK(insert_and_check_equivalence(
              audio_element_param.param_definition.get()));
          break;
        case ParamDefinition::kParameterDefinitionMixGain:
          return absl::InvalidArgumentError(absl::StrCat(
              "Mix gain parameters are not allowed in an audio element= ",
              audio_element_id_for_debugging));
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
        RETURN_IF_NOT_OK(
            insert_and_check_equivalence(&audio_element.element_mix_gain));
      }
      RETURN_IF_NOT_OK(insert_and_check_equivalence(&sub_mix.output_mix_gain));
    }
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

  // The input frame is arranged in (time, channel) axes. Interlace these in the
  // output PCM and skip over any trimmed samples.
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

absl::Status ValidateAndGetCommonTrim(
    uint32_t common_samples_per_frame,
    const std::list<AudioFrameWithData>& audio_frames,
    uint32_t& common_samples_to_trim_at_end,
    uint32_t& common_samples_to_trim_at_start) {
  struct TrimState {
    bool done_trimming_from_start = false;
    uint32_t cumulative_num_samples_to_trim_at_start = 0;
    uint32_t cumulative_num_samples_to_trim_at_end = 0;
  };
  absl::flat_hash_map<DecodedUleb128, TrimState> substream_id_to_trim_state;
  for (const auto& audio_frame : audio_frames) {
    auto& trim_state =
        substream_id_to_trim_state[audio_frame.obu.GetSubstreamId()];

    if (trim_state.cumulative_num_samples_to_trim_at_end > 0) {
      return absl::InvalidArgumentError(
          "Only one frame may have trim at the end.");
    }
    const auto& obu_trim_at_end =
        audio_frame.obu.header_.num_samples_to_trim_at_end;
    const auto& obu_trim_at_start =
        audio_frame.obu.header_.num_samples_to_trim_at_start;

    if (trim_state.done_trimming_from_start && obu_trim_at_start > 0) {
      return absl::InvalidArgumentError(
          "Samples trimmed from start must be consecutive.");
    }

    const uint64_t total_samples_to_trim_in_this_frame =
        obu_trim_at_end + obu_trim_at_start;
    if (total_samples_to_trim_in_this_frame > common_samples_per_frame) {
      return absl::InvalidArgumentError(
          "More samples trimmed than possible in a frame.");
    }
    const auto remaining_samples =
        common_samples_per_frame - total_samples_to_trim_in_this_frame;
    if (remaining_samples == 0 && obu_trim_at_end > 0) {
      return absl::InvalidArgumentError(
          "It is forbidden to fully trim samples from the end.");
    }

    if (obu_trim_at_start < common_samples_per_frame) {
      trim_state.done_trimming_from_start = true;
    }
    trim_state.cumulative_num_samples_to_trim_at_start += obu_trim_at_start;
    trim_state.cumulative_num_samples_to_trim_at_end += obu_trim_at_end;
  }

  if (substream_id_to_trim_state.empty()) {
    // Consider this OK. Maybe the end-user wants to prepare descriptor OBUs
    // separately from audio frames.
    common_samples_to_trim_at_end = 0;
    common_samples_to_trim_at_start = 0;
    return absl::OkStatus();
  }
  common_samples_to_trim_at_end =
      substream_id_to_trim_state.begin()
          ->second.cumulative_num_samples_to_trim_at_end;
  common_samples_to_trim_at_start =
      substream_id_to_trim_state.begin()
          ->second.cumulative_num_samples_to_trim_at_start;
  for (const auto& [substream_id, trim_state] : substream_id_to_trim_state) {
    RETURN_IF_NOT_OK(ValidateEqual(
        common_samples_to_trim_at_end,
        trim_state.cumulative_num_samples_to_trim_at_end,
        absl::StrCat("common_samples_to_trim_at_end vs. substream_id= ",
                     substream_id, "`cumulative_num_samples_to_trim_at_end`")));
    RETURN_IF_NOT_OK(ValidateEqual(
        common_samples_to_trim_at_start,
        trim_state.cumulative_num_samples_to_trim_at_start,
        absl::StrCat(
            "common_samples_to_trim_at_start vs. substream_id= ", substream_id,
            "`cumulative_num_samples_to_trim_at_start`")));
  }

  return absl::OkStatus();
}

void LogChannelNumbers(const std::string& name,
                       const ChannelNumbers& channel_numbers) {
  LOG(INFO) << name << ": [" << channel_numbers.surround << "."
            << channel_numbers.lfe << "." << channel_numbers.height << "]";
}

}  // namespace iamf_tools
