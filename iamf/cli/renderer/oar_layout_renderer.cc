/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/renderer/oar_layout_renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/cli/renderer/oar_wrapper.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/cli/sample_processing_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status ConstructIdToDemixingParamDefinitions(
    const std::vector<const AudioElementWithData*>& audio_elements,
    const Layout& layout,
    absl::flat_hash_map<DecodedUleb128, DemixingParamDefinition>&
        id_to_demixing_param_definitions) {
  for (const auto* audio_element : audio_elements) {
    // Demixing param definitions is only useful if the target layout is
    // a loudspeaker layout.
    auto sound_system_convention_layout =
        std::get_if<LoudspeakersSsConventionLayout>(&layout.specific_layout);
    if (sound_system_convention_layout == nullptr) {
      continue;
    }

    const auto& obu = audio_element->obu;
    const auto audio_element_id = obu.GetAudioElementId();
    const DemixingParamDefinition* demixing_param_definition = nullptr;
    for (const auto& param : obu.audio_element_params_) {
      const auto param_definition_type = param.GetType();
      if (param_definition_type !=
          ParamDefinition::kParameterDefinitionDemixing) {
        continue;
      }
      if (demixing_param_definition != nullptr) {
        return absl::InvalidArgumentError(
            "Not allowed to have multiple demixing parameters in a "
            "single Audio Element.");
      }
      demixing_param_definition =
          std::get_if<DemixingParamDefinition>(&param.param_definition);
      if (demixing_param_definition == nullptr) {
        return absl::InvalidArgumentError(
            "Demixing param definition is inconsistent with audio element "
            "param definition type.");
      }
      // Continue searching. Only to validate that there is at most one
      // `DemixingParamDefinition`.
    }
    if (demixing_param_definition == nullptr) {
      // No demixing info for this audio element; skip.
      continue;
    }

    // Now we have the demixing param definition. Add it to the mapping.
    const auto [iter, inserted] = id_to_demixing_param_definitions.insert(
        {audio_element_id, *demixing_param_definition});
    if (!inserted) {
      return absl::AlreadyExistsError(absl::StrCat(
          "Demixing param definition for Audio element ID= ", audio_element_id,
          " already exists."));
    }
  }

  return absl::OkStatus();
}

InternalSampleType Q15ToSignedDouble(const int16_t input) {
  return static_cast<double>(input) / 32768.0;
}

absl::Status FillIdToOrderedLabelsAndDemixingMatricesForSceneBased(
    const AudioElementWithData& audio_element,
    std::vector<ChannelLabel::Label>& ordered_labels,
    std::optional<std::vector<InternalSampleType>>& demixing_matrix) {
  const auto& obu = audio_element.obu;
  const auto* ambisonics_config = std::get_if<AmbisonicsConfig>(&obu.config_);
  if (ambisonics_config == nullptr) {
    return absl::InvalidArgumentError(
        "Ambisonics config is inconsistent with audio element type.");
  }
  RETURN_IF_NOT_OK(GetChannelLabelsForAmbisonics(
      *ambisonics_config, obu.audio_substream_ids_,
      audio_element.substream_id_to_labels, ordered_labels));

  const auto& demixing_matrix_q15 = ambisonics_config->GetDemixingMatrix();
  if (demixing_matrix_q15.has_value()) {
    demixing_matrix =
        std::vector<InternalSampleType>(demixing_matrix_q15->size());
    std::transform(demixing_matrix_q15->begin(), demixing_matrix_q15->end(),
                   demixing_matrix->begin(), Q15ToSignedDouble);
  }
  return absl::OkStatus();
}

absl::Status FillIdToOrderedLabelsForChannelBased(
    const AudioElementObu::AudioElementConfig& audio_element_config,
    std::vector<ChannelLabel::Label>& ordered_labels) {
  const auto* channel_config =
      std::get_if<ScalableChannelLayoutConfig>(&audio_element_config);
  if (channel_config == nullptr) {
    return absl::InvalidArgumentError(
        "Channel config is inconsistent with audio element type.");
  }

  if (channel_config->channel_audio_layer_configs.empty()) {
    return absl::InvalidArgumentError(
        "No channel audio layer configs provided.");
  }
  const auto& highest_channel_audio_layer_config =
      channel_config->channel_audio_layer_configs.back();
  const auto& ordered_labels_result =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          highest_channel_audio_layer_config.loudspeaker_layout,
          highest_channel_audio_layer_config.expanded_loudspeaker_layout);
  if (!ordered_labels_result.ok()) {
    return ordered_labels_result.status();
  }

  // Filter out omitted labels.
  ordered_labels.reserve(ordered_labels_result->size());
  for (const auto label : *ordered_labels_result) {
    if (label == ChannelLabel::kOmitted) {
      continue;
    }
    ordered_labels.push_back(label);
  }

  return absl::OkStatus();
}

absl::Status FillIdToOrderedLabelsForObjectBased(
    const AudioElementWithData& audio_element,
    std::vector<ChannelLabel::Label>& ordered_labels) {
  const auto& obu = audio_element.obu;
  const auto* objects_config = std::get_if<ObjectsConfig>(&obu.config_);
  if (objects_config == nullptr) {
    return absl::InvalidArgumentError(
        "Objects config is inconsistent with audio element type.");
  }
  const auto num_objects = objects_config->GetNumObjects();
  ordered_labels.reserve(num_objects);
  for (const auto substream_id : obu.audio_substream_ids_) {
    const auto& labels = LookupInMap(audio_element.substream_id_to_labels,
                                     substream_id, "Labels for substream ID");
    if (!labels.ok()) {
      return labels.status();
    }

    ordered_labels.insert(ordered_labels.end(), labels->begin(), labels->end());
  }

  return absl::OkStatus();
}

absl::Status ConstructIdToOrderedLabelsAndDemixingMatrices(
    const std::vector<const AudioElementWithData*>& audio_elements,
    absl::flat_hash_map<DecodedUleb128, std::vector<ChannelLabel::Label>>&
        id_to_ordered_labels,
    absl::flat_hash_map<DecodedUleb128,
                        std::optional<std::vector<InternalSampleType>>>&
        id_to_demixing_matrices) {
  for (const auto* audio_element : audio_elements) {
    const auto& obu = audio_element->obu;
    const auto audio_element_id = obu.GetAudioElementId();
    id_to_demixing_matrices[audio_element_id] = std::nullopt;
    switch (obu.GetAudioElementType()) {
      case AudioElementObu::kAudioElementSceneBased:
        RETURN_IF_NOT_OK(FillIdToOrderedLabelsAndDemixingMatricesForSceneBased(
            *audio_element, id_to_ordered_labels[audio_element_id],
            id_to_demixing_matrices[audio_element_id]));
        break;
      case AudioElementObu::kAudioElementChannelBased:
        RETURN_IF_NOT_OK(FillIdToOrderedLabelsForChannelBased(
            obu.config_, id_to_ordered_labels[audio_element_id]));
        break;
      case AudioElementObu::kAudioElementObjectBased:
        RETURN_IF_NOT_OK(FillIdToOrderedLabelsForObjectBased(
            *audio_element, id_to_ordered_labels[audio_element_id]));
        break;
      case AudioElementObu::kAudioElementBeginReserved:
      case AudioElementObu::kAudioElementEndReserved:
        return absl::UnimplementedError(absl::StrCat(
            "Unsupported audio element type= ", obu.GetAudioElementType()));
    }
  }

  return absl::OkStatus();
}

// Finds the samples to trim at one end of the frame (0 if trimming is not
// enabled) and enforces that all audio elements are trimmed in the same way.
absl::Status UpdateTrimming(
    const bool trimming_enabled,
    const uint32_t num_samples_to_trim_for_audio_element,
    std::optional<uint32_t>& common_num_samples_to_trim) {
  if (!common_num_samples_to_trim.has_value()) {
    if (trimming_enabled) {
      common_num_samples_to_trim = num_samples_to_trim_for_audio_element;
    } else {
      common_num_samples_to_trim = 0;
    }
  } else if (trimming_enabled) {
    RETURN_IF_NOT_OK(ValidateEqual(*common_num_samples_to_trim,
                                   num_samples_to_trim_for_audio_element,
                                   "samples to trim at start"));
  }
  return absl::OkStatus();
}

absl::Status UpdateAndValidateTrimming(
    const TrimmingSettings& trimming_settings,
    const LabeledFrame& labeled_frame,
    const std::vector<ChannelLabel::Label>& ordered_labels,
    const uint32_t common_num_samples_per_frame,
    std::optional<uint32_t>& common_num_samples_to_trim_at_start,
    std::optional<uint32_t>& common_num_samples_to_trim_at_end,
    std::optional<size_t>& common_num_valid_ticks) {
  // First validate trimming for all frames associated with one audio element.
  size_t num_valid_ticks = 0;
  RETURN_IF_NOT_OK(ValidateTrimming(labeled_frame, ordered_labels,
                                    common_num_samples_per_frame,
                                    trimming_settings, num_valid_ticks));

  // Next update the trimming information and validate against other processed
  // audio elements to make sure all are trimmed consistently.
  RETURN_IF_NOT_OK(UpdateTrimming(trimming_settings.trim_beginning,
                                  labeled_frame.samples_to_trim_at_start,
                                  common_num_samples_to_trim_at_start));
  RETURN_IF_NOT_OK(UpdateTrimming(trimming_settings.trim_end,
                                  labeled_frame.samples_to_trim_at_end,
                                  common_num_samples_to_trim_at_end));
  if (!common_num_valid_ticks.has_value()) {
    common_num_valid_ticks = num_valid_ticks;
  }
  RETURN_IF_NOT_OK(
      ValidateEqual(*common_num_valid_ticks, num_valid_ticks, "valid ticks"));

  return absl::OkStatus();
}

absl::Status UpdateMixGains(
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain_param_definition,
    const int num_samples_per_frame,
    absl::AnyInvocable<absl::Status(uint32_t, float,
                                    const std::optional<MixGainParameterData>&)>
        mix_gain_updater) {
  const float default_gain_value =
      mix_gain_param_definition.default_mix_gain_.GetFloatingPoint();
  const auto parameter_id = mix_gain_param_definition.GetParameterId();
  auto parameter_block_iter = id_to_parameter_block.find(parameter_id);
  if (parameter_block_iter == id_to_parameter_block.end()) {
    // Default mix gain will be used for this frame.
    return mix_gain_updater(static_cast<uint32_t>(num_samples_per_frame),
                            default_gain_value, std::nullopt);
  }

  // Now we have animated values; iterate through all subblocks.
  const auto& parameter_block_obu = *parameter_block_iter->second->obu;
  const auto num_subblocks = parameter_block_obu.GetNumSubblocks();
  for (int i = 0; i < num_subblocks; i++) {
    const auto subblock_duration = parameter_block_obu.GetSubblockDuration(i);
    if (!subblock_duration.ok()) {
      return subblock_duration.status();
    }
    const auto& mix_gain_parameter_data =
        *static_cast<const MixGainParameterData*>(
            parameter_block_obu.subblocks_[i].get());
    RETURN_IF_NOT_OK(mix_gain_updater(static_cast<uint32_t>(*subblock_duration),
                                      default_gain_value,
                                      mix_gain_parameter_data));
  }

  return absl::OkStatus();
}

absl::Status UpdateElementMixGainForOar(
    const uint32_t audio_element_id,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain_param_definition,
    const int num_samples_per_frame, OarWrapper& oar) {
  return UpdateMixGains(
      id_to_parameter_block, mix_gain_param_definition, num_samples_per_frame,
      [&oar, audio_element_id](
          uint32_t duration, float default_gain_value,
          const std::optional<MixGainParameterData>& mix_gain_parameter_data)
          -> absl::Status {
        return oar.UpdateElementMixGain(audio_element_id, duration,
                                        default_gain_value,
                                        mix_gain_parameter_data);
      });
}

absl::Status UpdateOutputMixGainForOar(
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const MixGainParamDefinition& mix_gain_param_definition,
    const int num_samples_per_frame, OarWrapper& oar) {
  return UpdateMixGains(
      id_to_parameter_block, mix_gain_param_definition, num_samples_per_frame,
      [&oar](uint32_t duration, float default_gain_value,
             const std::optional<MixGainParameterData>& mix_gain_parameter_data)
          -> absl::Status {
        return oar.UpdateOutputMixGain(duration, default_gain_value,
                                       mix_gain_parameter_data);
      });
}

absl::Status MaybeUpdateDownMixModeForOar(
    const uint32_t audio_element_id,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    const absl::flat_hash_map<DecodedUleb128, DemixingParamDefinition>&
        audio_element_id_to_demixing_param_definitions,
    const int num_samples_per_frame, OarWrapper& oar) {
  const auto demixing_iter =
      audio_element_id_to_demixing_param_definitions.find(audio_element_id);
  if (demixing_iter == audio_element_id_to_demixing_param_definitions.end()) {
    // No demixing info found for this audio element; skip.
    return absl::OkStatus();
  }

  const auto demixing_info_id = demixing_iter->second.GetParameterId();
  const auto parameter_block_iter =
      id_to_parameter_block.find(demixing_info_id);
  if (parameter_block_iter == id_to_parameter_block.end()) {
    // No parameter stream defined for the demixing info of this audio element;
    // the default will be used, and it is OK to skip.
    return absl::OkStatus();
  }

  // Now we have the parameter block containing demixing info; iterate through
  // all subblocks.
  const auto& parameter_block_obu = *parameter_block_iter->second->obu;
  const auto num_subblocks = parameter_block_obu.GetNumSubblocks();
  if (num_subblocks != 1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "There should be exactly 1 subblock for demixing info parameter "
        "blocks ",
        "but got ", num_subblocks, " for parameter ID= ", demixing_info_id));
  }
  const auto& demixing_info_parameter_data =
      *static_cast<const DemixingInfoParameterData*>(
          parameter_block_obu.subblocks_[0].get());
  RETURN_IF_NOT_OK(
      oar.UpdateDownMixMode(audio_element_id, num_samples_per_frame,
                            demixing_info_parameter_data.dmixp_mode));

  return absl::OkStatus();
}

}  // namespace

std::unique_ptr<OarLayoutRenderer> OarLayoutRenderer::Create(
    const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
    const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
    const MixGainParamDefinition& output_mix_gain, const Layout& layout,
    const int32_t num_channels, const uint32_t common_sample_rate,
    const uint32_t common_num_samples_per_frame,
    const TrimmingSettings trimming_settings) {
  const size_t num_audio_elements = audio_elements_in_sub_mix.size();
  if (num_audio_elements == 0) {
    ABSL_LOG(ERROR) << "Rendering without an audio element is not supported.";
    return nullptr;
  }

  // Collect audio element IDs and mix gains.
  std::vector<DecodedUleb128> audio_element_ids;
  std::vector<MixGainParamDefinition> element_mix_gains;
  CollectAudioElementIdsAndMixGains(sub_mix_audio_elements, audio_element_ids,
                                    element_mix_gains);

  std::unique_ptr<OarWrapper> oar = OarWrapper::Create(
      layout, common_sample_rate, num_channels, common_num_samples_per_frame,
      output_mix_gain.GetParameterId());
  if (oar == nullptr) {
    ABSL_LOG(ERROR) << "Failed to create `OarLayoutRenderer`: creating "
                    << "`OarWrapper` failed.";
    return nullptr;
  }

  // Collect demixing info (if available) corresponding to each audio element.
  absl::flat_hash_map<DecodedUleb128, DemixingParamDefinition>
      audio_element_id_to_demixing_param_definitions;
  if (auto construct_status = ConstructIdToDemixingParamDefinitions(
          audio_elements_in_sub_mix, layout,
          audio_element_id_to_demixing_param_definitions);
      !construct_status.ok()) {
    ABSL_LOG(ERROR) << "Failed to create `OarLayoutRenderer`: constructing "
                    << "mappings from audio element ID to demixing param "
                    << "definitions failed. Error message= "
                    << construct_status.message();
    return nullptr;
  }

  // Add audio elements to OAR.
  for (size_t i = 0; i < num_audio_elements; i++) {
    const auto& sub_mix_audio_element = *audio_elements_in_sub_mix[i];
    std::optional<DemixingParamDefinition> demixing_param_definition =
        std::nullopt;
    if (auto iter = audio_element_id_to_demixing_param_definitions.find(
            sub_mix_audio_element.obu.GetAudioElementId());
        iter != audio_element_id_to_demixing_param_definitions.end()) {
      demixing_param_definition = iter->second;
    }

    if (const auto add_audio_element_status = oar->AddAudioElement(
            sub_mix_audio_element.obu.GetAudioElementId(),
            sub_mix_audio_element.obu.GetAudioElementType(),
            sub_mix_audio_element.obu.config_, demixing_param_definition,
            sub_mix_audio_elements[i].rendering_config,
            element_mix_gains[i].GetParameterId());
        !add_audio_element_status.ok()) {
      ABSL_LOG(ERROR) << "Failed to create `OarLayoutRenderer`: adding "
                      << "audio elements failed. Error message= "
                      << add_audio_element_status.message();
      return nullptr;
    }
  }

  // Collect ordered labels and demixing matrices corresponding to each
  // audio element.
  absl::flat_hash_map<DecodedUleb128, std::vector<ChannelLabel::Label>>
      audio_element_id_to_ordered_labels;
  absl::flat_hash_map<DecodedUleb128, OptionalDemixingMatrix>
      audio_element_id_to_demixing_matrices;
  if (auto construct_status = ConstructIdToOrderedLabelsAndDemixingMatrices(
          audio_elements_in_sub_mix, audio_element_id_to_ordered_labels,
          audio_element_id_to_demixing_matrices);
      !construct_status.ok()) {
    ABSL_LOG(ERROR) << "Failed to create `OarLayoutRenderer`: constructing "
                    << "mappings from audio element ID to ordered labels and "
                    << "demixing matrices failed. Error message= "
                    << construct_status.message();
    return nullptr;
  }

  return absl::WrapUnique(new OarLayoutRenderer(
      audio_element_ids, element_mix_gains, output_mix_gain, num_channels,
      common_sample_rate, common_num_samples_per_frame,
      audio_element_id_to_ordered_labels, audio_element_id_to_demixing_matrices,
      audio_element_id_to_demixing_param_definitions, trimming_settings,
      std::move(oar)));
}

OarLayoutRenderer::OarLayoutRenderer(
    const std::vector<DecodedUleb128>& audio_element_ids,
    const std::vector<MixGainParamDefinition>& element_mix_gains,
    const MixGainParamDefinition& output_mix_gain, int32_t num_channels,
    uint32_t common_sample_rate, uint32_t common_num_samples_per_frame,
    const absl::flat_hash_map<DecodedUleb128, std::vector<ChannelLabel::Label>>&
        audio_element_id_to_ordered_labels,
    const absl::flat_hash_map<DecodedUleb128, OptionalDemixingMatrix>&
        audio_element_id_to_demixing_matrices,
    const absl::flat_hash_map<DecodedUleb128, DemixingParamDefinition>&
        audio_element_id_to_demixing_param_definitions,
    const TrimmingSettings& trimming_settings, std::unique_ptr<OarWrapper> oar)
    : LayoutRendererBase(audio_element_ids, element_mix_gains, output_mix_gain,
                         num_channels, common_sample_rate,
                         common_num_samples_per_frame),
      audio_element_id_to_ordered_labels_(audio_element_id_to_ordered_labels),
      audio_element_id_to_demixing_matrices_(
          audio_element_id_to_demixing_matrices),
      audio_element_id_to_demixing_param_definitions_(
          audio_element_id_to_demixing_param_definitions),
      trimming_settings_(trimming_settings),
      empty_channel_(common_num_samples_per_frame, 0.0),
      oar_(std::move(oar)),
      oar_input_buffer_(),
      oar_output_buffer_(common_num_samples_per_frame * num_channels),
      common_num_samples_to_trim_at_start_(std::nullopt),
      common_num_samples_to_trim_at_end_(std::nullopt),
      common_num_valid_ticks_(std::nullopt) {}

absl::Status OarLayoutRenderer::PrepareOperation(
    const IdLabeledFrameMap& id_to_labeled_frame,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block) {
  common_num_samples_to_trim_at_start_ = std::nullopt;
  common_num_samples_to_trim_at_end_ = std::nullopt;
  common_num_valid_ticks_ = std::nullopt;
  return absl::OkStatus();
}

absl::Status OarLayoutRenderer::PerElementOperation(
    size_t index, DecodedUleb128 audio_element_id,
    const LabeledFrame& labeled_frame,
    const MixGainParamDefinition& element_mix_gain,
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block) {
  const auto& ordered_labels =
      LookupInMap(audio_element_id_to_ordered_labels_, audio_element_id,
                  "Ordered labels for audio element ID");
  if (!ordered_labels.ok()) {
    return ordered_labels.status();
  }
  const auto& demixing_matrix =
      LookupInMap(audio_element_id_to_demixing_matrices_, audio_element_id,
                  "Demixing matrix for audio element ID");
  if (!demixing_matrix.ok()) {
    return demixing_matrix.status();
  }

  RETURN_IF_NOT_OK(UpdateAndValidateTrimming(
      trimming_settings_, labeled_frame, *ordered_labels,
      common_num_samples_per_frame_, common_num_samples_to_trim_at_start_,
      common_num_samples_to_trim_at_end_, common_num_valid_ticks_));

  // Add input to OAR. Arrange (and project if necessary) the input samples.
  uint32_t num_input_channels_to_oar = 0;
  RETURN_IF_NOT_OK(PrepareOarInputBuffer(labeled_frame, *ordered_labels,
                                         *demixing_matrix, oar_input_buffer_,
                                         num_input_channels_to_oar));

  const auto audio_element_id_uint32 = static_cast<uint32_t>(audio_element_id);
  RETURN_IF_NOT_OK(oar_->AddInput(audio_element_id_uint32,
                                  absl::MakeSpan(oar_input_buffer_),
                                  num_input_channels_to_oar));

  // Update element mix gain.
  RETURN_IF_NOT_OK(UpdateElementMixGainForOar(
      audio_element_id_uint32, id_to_parameter_block, element_mix_gain,
      common_num_samples_per_frame_, *oar_));

  // Update the down-mix mode if necessary.
  RETURN_IF_NOT_OK(MaybeUpdateDownMixModeForOar(
      audio_element_id_uint32, id_to_parameter_block,
      audio_element_id_to_demixing_param_definitions_,
      common_num_samples_per_frame_, *oar_));

  return absl::OkStatus();
}

absl::Status OarLayoutRenderer::FinalizeOperation(
    const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        id_to_parameter_block,
    std::vector<std::vector<InternalSampleType>>& rendered_samples,
    std::vector<absl::Span<const InternalSampleType>>& valid_rendered_samples) {
  // Update output mix gain.
  RETURN_IF_NOT_OK(
      UpdateOutputMixGainForOar(id_to_parameter_block, output_mix_gain_,
                                common_num_samples_per_frame_, *oar_));

  // Render.
  std::fill(oar_output_buffer_.begin(), oar_output_buffer_.end(), 0.0);
  RETURN_IF_NOT_OK(oar_->Render(absl::MakeSpan(oar_output_buffer_)));

  // Copy the samples back to `rendered_samples`.
  size_t output_buffer_index = 0;
  absl::Span<const float> output_buffer_span =
      absl::MakeConstSpan(oar_output_buffer_);
  for (auto& rendered_samples_for_channel : rendered_samples) {
    rendered_samples_for_channel.resize(common_num_samples_per_frame_);

    RETURN_IF_NOT_OK(StaticCastSpanIfInRange(
        "rendered_samples_for_channel",
        output_buffer_span.subspan(output_buffer_index,
                                   common_num_samples_per_frame_),
        absl::MakeSpan(rendered_samples_for_channel)));
    output_buffer_index += common_num_samples_per_frame_;
  }

  // Handle trimming by pointing `valid_rendered_samples` to only the
  // post-trimming portion of `rendered_samples`.
  if (audio_element_ids_.empty()) {
    // No input, so these common variables are not set. It is fine because
    // we just output silence.
    common_num_samples_to_trim_at_start_ = 0;
    common_num_valid_ticks_ = common_num_samples_per_frame_;
  } else if (!common_num_samples_to_trim_at_start_.has_value() ||
             !common_num_valid_ticks_.has_value()) {
    return absl::InvalidArgumentError("Trimming information is missing.");
  }
  valid_rendered_samples.resize(rendered_samples.size());
  for (size_t c = 0; c < rendered_samples.size(); ++c) {
    valid_rendered_samples[c] =
        absl::MakeConstSpan(rendered_samples[c])
            .subspan(*common_num_samples_to_trim_at_start_,
                     *common_num_valid_ticks_);
  }

  return absl::OkStatus();
}

absl::Status OarLayoutRenderer::PrepareOarInputBuffer(
    const LabeledFrame& labeled_frame,
    const absl::Span<const ChannelLabel::Label> ordered_labels,
    const std::optional<absl::Span<const InternalSampleType>>& demixing_matrix,
    std::vector<OarWrapper::OarSampleType>& oar_input_buffer,
    uint32_t& num_input_channels_to_oar) {
  const int num_channels = ordered_labels.size();
  arranged_samples_.resize(num_channels);

  // Do not trim the input. In OAR's workflow, trimming is handled after
  // rendering.
  constexpr TrimmingSettings kNoTrimming = {
      .trim_beginning = false,
      .trim_end = false,
  };

  // Number of valid samples might be fewer than the frame size, which is OK.
  size_t num_valid_samples = 0;
  RETURN_IF_NOT_OK(ArrangeSamples(labeled_frame, ordered_labels, empty_channel_,
                                  kNoTrimming, arranged_samples_,
                                  num_valid_samples));

  if (demixing_matrix.has_value()) {
    num_input_channels_to_oar = demixing_matrix->size() / num_channels;
    projected_samples_.resize(num_input_channels_to_oar);
    projected_samples_spans_.reserve(num_input_channels_to_oar);
    projected_samples_spans_.clear();
    for (auto& projected_samples_for_channel : projected_samples_) {
      projected_samples_for_channel = empty_channel_;
      projected_samples_spans_.emplace_back(
          absl::MakeSpan(projected_samples_for_channel));
    }
    RETURN_IF_NOT_OK(ProjectSamplesToRender(
        absl::MakeConstSpan(arranged_samples_), *demixing_matrix,
        absl::MakeSpan(projected_samples_spans_)));
    input_spans_.assign(projected_samples_spans_.begin(),
                        projected_samples_spans_.end());
  } else {
    num_input_channels_to_oar = num_channels;
    input_spans_ = arranged_samples_;
  }

  size_t vector_index = 0;
  oar_input_buffer.assign(
      common_num_samples_per_frame_ * num_input_channels_to_oar, 0.0);
  auto oar_input_buffer_span = absl::MakeSpan(oar_input_buffer);

  // Copy and cast samples to the one-dimensional input buffer to OAR in
  // strides of `common_num_samples_per_frame_`. When `num_valid_samples` is
  // smaller than  `common_num_samples_per_frame_`, this leaves unfilled
  // samples as zeros, equivalent to zero-padding to full frames.
  for (size_t c = 0; c < num_input_channels_to_oar;
       c++, vector_index += common_num_samples_per_frame_) {
    RETURN_IF_NOT_OK(StaticCastSpanIfInRange(
        "input_spans", input_spans_[c],
        oar_input_buffer_span.subspan(vector_index, num_valid_samples)));
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
