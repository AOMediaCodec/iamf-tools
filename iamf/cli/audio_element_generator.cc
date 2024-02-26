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
#include "iamf/cli/audio_element_generator.h"

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/audio_element.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"
#include "iamf/param_definitions.h"
#include "iamf/parameter_block.h"

namespace iamf_tools {

namespace {

// Copies the `ParameterDefinitionType` based on the input data. Uses the
// deprecated field as a backup.
absl::Status CopyAudioElementParamDefinitionType(
    iamf_tools_cli_proto::AudioElementParam user_data_parameter,
    ParamDefinition::ParameterDefinitionType& output_param_definition_type) {
  if (user_data_parameter.has_param_definition_type()) {
    switch (user_data_parameter.param_definition_type()) {
      using enum iamf_tools_cli_proto::ParamDefinitionType;
      using enum ParamDefinition::ParameterDefinitionType;
      case PARAM_DEFINITION_TYPE_DEMIXING:
        output_param_definition_type = kParameterDefinitionDemixing;
        return absl::OkStatus();
      case PARAM_DEFINITION_TYPE_RECON_GAIN:
        output_param_definition_type = kParameterDefinitionReconGain;
        return absl::OkStatus();
      default:
        // TODO(b/289541186): Allow the extension types through here.
        LOG(ERROR) << "Unknown or invalid param_definition_type="
                   << static_cast<int>(
                          user_data_parameter.param_definition_type());
        return absl::InvalidArgumentError("");
    }
  } else if (user_data_parameter.has_deprecated_param_definition_type()) {
    LOG(WARNING) << "Please upgrade the `deprecated_param_definition_type` "
                    "field to the new "
                    "`param_definition_type` field.";
    output_param_definition_type =
        static_cast<ParamDefinition::ParameterDefinitionType>(
            user_data_parameter.deprecated_param_definition_type());
    return absl::OkStatus();
  } else {
    LOG(ERROR) << "Missing `param_definition_type` field.";
    return absl::InvalidArgumentError("");
  }
}

absl::Status GenerateAudioSubstreams(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    AudioElementObu& audio_element_obu) {
  if (audio_element_metadata.num_substreams() !=
      audio_element_metadata.audio_substream_ids_size()) {
    LOG(ERROR) << "User data has inconsistent `audio_element_ids`. Found: "
               << audio_element_metadata.audio_substream_ids_size()
               << " substreams, expected: "
               << audio_element_metadata.num_substreams();
    return absl::InvalidArgumentError("");
  }

  audio_element_obu.InitializeAudioSubstreams(
      audio_element_metadata.num_substreams());
  for (int i = 0; i < audio_element_metadata.num_substreams(); ++i) {
    audio_element_obu.audio_substream_ids_[i] =
        audio_element_metadata.audio_substream_ids(i);
  }
  return absl::OkStatus();
}

absl::Status GenerateParameterDefinitions(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu,
    AudioElementObu& audio_element_obu) {
  if (audio_element_metadata.num_parameters() !=
      audio_element_metadata.audio_element_params_size()) {
    LOG(ERROR) << "User data has inconsistent `num_parameters`. Found: "
               << audio_element_metadata.audio_element_params_size()
               << " parameters, expected: "
               << audio_element_metadata.num_parameters();
    return absl::InvalidArgumentError("");
  }

  audio_element_obu.InitializeParams(audio_element_metadata.num_parameters());
  for (int i = 0; i < audio_element_metadata.num_parameters(); ++i) {
    AudioElementParam& audio_element_param =
        audio_element_obu.audio_element_params_[i];
    const auto& user_data_parameter =
        audio_element_metadata.audio_element_params(i);

    RETURN_IF_NOT_OK(CopyAudioElementParamDefinitionType(
        user_data_parameter, audio_element_param.param_definition_type));
    switch (audio_element_param.param_definition_type) {
      using enum ParamDefinition::ParameterDefinitionType;
      case kParameterDefinitionDemixing: {
        auto demixing_param_definition =
            std::make_unique<DemixingParamDefinition>();
        RETURN_IF_NOT_OK(CopyParamDefinition(
            user_data_parameter.demixing_param().param_definition(),
            demixing_param_definition.get()));
        // Copy the `DemixingInfoParameterData` in the IAMF spec.
        RETURN_IF_NOT_OK(CopyDemixingInfoParameterData(
            user_data_parameter.demixing_param()
                .default_demixing_info_parameter_data(),
            demixing_param_definition->default_demixing_info_parameter_data_));
        // Copy the extension portion of `DefaultDemixingInfoParameterData` in
        // the IAMF spec.
        RETURN_IF_NOT_OK(Uint32ToUint8(
            user_data_parameter.demixing_param().default_w(),
            demixing_param_definition->default_demixing_info_parameter_data_
                .default_w));
        RETURN_IF_NOT_OK(Uint32ToUint8(
            user_data_parameter.demixing_param().reserved(),
            demixing_param_definition->default_demixing_info_parameter_data_
                .reserved_default));
        if (demixing_param_definition->duration_ !=
            codec_config_obu.codec_config_.num_samples_per_frame) {
          LOG(ERROR) << "Demixing parameter duration= "
                     << demixing_param_definition->duration_
                     << " is inconsistent with num_samples_per_frame="
                     << codec_config_obu.codec_config_.num_samples_per_frame;
          return absl::InvalidArgumentError("");
        }

        audio_element_param.param_definition =
            std::move(demixing_param_definition);
        break;
      }
      case kParameterDefinitionReconGain: {
        auto recon_gain_param_definition =
            std::make_unique<ReconGainParamDefinition>(
                audio_element_obu.audio_element_id_);
        RETURN_IF_NOT_OK(CopyParamDefinition(
            user_data_parameter.recon_gain_param().param_definition(),
            recon_gain_param_definition.get()));
        if (recon_gain_param_definition->duration_ !=
            codec_config_obu.codec_config_.num_samples_per_frame) {
          LOG(ERROR) << "Recon gain parameter duration= "
                     << recon_gain_param_definition->duration_
                     << " is inconsistent with num_samples_per_frame="
                     << codec_config_obu.codec_config_.num_samples_per_frame;
          return absl::InvalidArgumentError("");
        }
        audio_element_param.param_definition =
            std::move(recon_gain_param_definition);
        break;
      }
      default:
        // TODO(b/289541186): Support the extension fields here.
        return absl::InvalidArgumentError("");
    }
  }

  return absl::OkStatus();
}

absl::Status LoudspeakerLayoutToChannels(
    const ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    ChannelNumbers* channels) {
  switch (loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kLayoutMono:
      *channels = {1, 0, 0};
      break;
    case kLayoutStereo:
      *channels = {2, 0, 0};
      break;
    case kLayout5_1_ch:
      *channels = {5, 1, 0};
      break;
    case kLayout5_1_2_ch:
      *channels = {5, 1, 2};
      break;
    case kLayout5_1_4_ch:
      *channels = {5, 1, 4};
      break;
    case kLayout7_1_ch:
      *channels = {7, 1, 0};
      break;
    case kLayout7_1_2_ch:
      *channels = {7, 1, 2};
      break;
    case kLayout7_1_4_ch:
      *channels = {7, 1, 4};
      break;
    case kLayout3_1_2_ch:
      *channels = {3, 1, 2};
      break;
    case kLayoutBinaural:
      *channels = {2, 0, 0};
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown loudspeaker_layout= ", loudspeaker_layout));
  }
  return absl::OkStatus();
}

absl::Status CollectBcgLabels(
    const ChannelNumbers& layer_channels,
    std::list<std::string>* coupled_substream_labels,
    std::list<std::string>* non_coupled_substream_labels) {
  switch (layer_channels.surround) {
    case 1:
      non_coupled_substream_labels->push_back("M");
      break;
    case 2:
      coupled_substream_labels->push_back("L2");
      coupled_substream_labels->push_back("R2");
      break;
    case 3:
      coupled_substream_labels->push_back("L3");
      coupled_substream_labels->push_back("R3");
      non_coupled_substream_labels->push_back("C");
      break;
    case 5:
      coupled_substream_labels->push_back("L5");
      coupled_substream_labels->push_back("R5");
      coupled_substream_labels->push_back("Ls5");
      coupled_substream_labels->push_back("Rs5");
      non_coupled_substream_labels->push_back("C");
      break;
    case 7:
      coupled_substream_labels->push_back("L7");
      coupled_substream_labels->push_back("R7");
      coupled_substream_labels->push_back("Lss7");
      coupled_substream_labels->push_back("Rss7");
      coupled_substream_labels->push_back("Lrs7");
      coupled_substream_labels->push_back("Rrs7");
      non_coupled_substream_labels->push_back("C");
      break;
    default:
      LOG(ERROR) << "Unsupported number of surround channels: "
                 << layer_channels.surround;
      return absl::InvalidArgumentError("");
  }
  switch (layer_channels.height) {
    case 0:
      // Not adding anything.
      break;
    case 2:
      if (layer_channels.surround == 3) {
        coupled_substream_labels->push_back("Ltf3");
        coupled_substream_labels->push_back("Rtf3");
      } else {
        coupled_substream_labels->push_back("Ltf2");
        coupled_substream_labels->push_back("Rtf2");
      }
      break;
    case 4:
      coupled_substream_labels->push_back("Ltf4");
      coupled_substream_labels->push_back("Rtf4");
      coupled_substream_labels->push_back("Ltb4");
      coupled_substream_labels->push_back("Rtb4");
      break;
    default:
      LOG(ERROR) << "Unsupported number of height channels: "
                 << layer_channels.height;
      return absl::InvalidArgumentError("");
  }
  switch (layer_channels.lfe) {
    case 0:
      // Not adding anything.
      break;
    case 1:
      non_coupled_substream_labels->push_back("LFE");
      break;
    default:
      LOG(ERROR) << "Unsupported number of LFE channels: "
                 << layer_channels.lfe;
      return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

absl::Status CollectDcgLabels(
    const ChannelNumbers& accumulated_channels,
    const ChannelNumbers& layer_channels,
    std::list<std::string>* coupled_substream_labels,
    std::list<std::string>* non_coupled_substream_labels) {
  bool push_l2_in_the_end = false;
  for (int surround = accumulated_channels.surround + 1;
       surround <= layer_channels.surround; surround++) {
    switch (surround) {
      case 2:
        // This is the special case where layer 1 is Mono and layer 2 is
        // Stereo. According to the Spec 3.7.2
        // (https://aomediacodec.github.io/iamf/#syntax-scalable-channel-layout-config):
        // "Center channel comes first and followed by LFE and followed by the
        // other one."
        // "L2" is categorized as "the other one", so we save pushing "L2" till
        // the end.
        push_l2_in_the_end = true;
        break;
      case 3:
        non_coupled_substream_labels->push_back("C");
        break;
      case 5:
        coupled_substream_labels->push_back("L5");
        coupled_substream_labels->push_back("R5");
        break;
      case 7:
        coupled_substream_labels->push_back("Lss7");
        coupled_substream_labels->push_back("Rss7");
        break;
      default:
        if (surround > 7) {
          LOG(ERROR) << "Unsupported number of surround channels: " << surround;
          return absl::InvalidArgumentError("");
        }
        break;
    }
  }

  if (layer_channels.height > accumulated_channels.height) {
    if (accumulated_channels.height == 0) {
      if (layer_channels.height == 4) {
        coupled_substream_labels->push_back("Ltf4");
        coupled_substream_labels->push_back("Rtf4");
        coupled_substream_labels->push_back("Ltb4");
        coupled_substream_labels->push_back("Rtb4");
      } else if (layer_channels.height == 2) {
        if (layer_channels.surround == 3) {
          coupled_substream_labels->push_back("Ltf3");
          coupled_substream_labels->push_back("Rtf3");
        } else {
          coupled_substream_labels->push_back("Ltf2");
          coupled_substream_labels->push_back("Rtf2");
        }
      } else {
        LOG(ERROR) << "Unsupported number of height channels: "
                   << layer_channels.height;
        return absl::InvalidArgumentError("");
      }
    } else if (accumulated_channels.height == 2) {
      coupled_substream_labels->push_back("Ltf4");
      coupled_substream_labels->push_back("Rtf4");
    } else {
      LOG(ERROR) << "Unsupported number of height channels: "
                 << accumulated_channels.height;
      return absl::InvalidArgumentError("");
    }
  }

  if (layer_channels.lfe > accumulated_channels.lfe) {
    if (layer_channels.lfe == 1) {
      non_coupled_substream_labels->push_back("LFE");
    } else {
      LOG(ERROR) << "Unsupported number of LFE channels: "
                 << layer_channels.lfe;
      return absl::InvalidArgumentError("");
    }
  }

  if (push_l2_in_the_end) {
    non_coupled_substream_labels->push_back("L2");
  }

  return absl::OkStatus();
}

void AddSubstreamLabels(
    const std::list<std::string>& coupled_substream_labels,
    const std::list<std::string>& non_coupled_substream_labels,
    const std::vector<DecodedUleb128>& substream_ids,
    SubstreamIdLabelsMap* substream_id_to_labels, int* substream_index) {
  // First add coupled substream labels, two at a time.
  for (auto iter = coupled_substream_labels.begin();
       iter != coupled_substream_labels.end();) {
    const auto substream_id = substream_ids[(*substream_index)++];
    auto& labels_for_substream_id = (*substream_id_to_labels)[substream_id];
    labels_for_substream_id.push_back(*iter++);
    std::string concatenated_labels = labels_for_substream_id.back();
    labels_for_substream_id.push_back(*iter++);
    absl::StrAppend(&concatenated_labels, "/", labels_for_substream_id.back());
    LOG(INFO) << "  substream_id_to_labels[" << substream_id
              << "]: " << concatenated_labels;
  }

  // Then add non-coupled substream labels.
  for (auto iter = non_coupled_substream_labels.begin();
       iter != non_coupled_substream_labels.end();) {
    const auto substream_id = substream_ids[(*substream_index)++];
    (*substream_id_to_labels)[substream_id].push_back(*iter++);
    LOG(INFO) << "  substream_id_to_labels[" << substream_id
              << "]: " << (*substream_id_to_labels)[substream_id].back();
  }
}

absl::Status ValidateSubstreamCounts(
    const std::list<std::string>& coupled_substream_labels,
    const std::list<std::string>& non_coupled_substream_labels,
    const ChannelAudioLayerConfig& layer_config) {
  const auto num_required_coupled_channels =
      static_cast<uint32_t>(coupled_substream_labels.size()) / 2;
  const auto num_required_non_coupled_channels =
      static_cast<uint32_t>(non_coupled_substream_labels.size());
  LOG(INFO) << "num_required_coupled_channels = "
            << num_required_coupled_channels;
  LOG(INFO) << "num_required_non_coupled_channels= "
            << num_required_non_coupled_channels;

  const auto coupled_substream_count_in_obu =
      static_cast<uint32_t>(layer_config.coupled_substream_count);
  const auto substream_count_in_obu =
      static_cast<uint32_t>(layer_config.substream_count);
  if (coupled_substream_count_in_obu != num_required_coupled_channels) {
    LOG(ERROR) << "Coupled substream count different from the required number: "
               << coupled_substream_count_in_obu << " vs "
               << num_required_coupled_channels;
    return absl::InvalidArgumentError("");
  }

  // The sum of coupled and non-coupled channels must be the same as
  // the `substream_count` recorded in the OBU.
  if (substream_count_in_obu !=
      (num_required_non_coupled_channels + num_required_coupled_channels)) {
    LOG(INFO) << "Substream count different from the #non-coupled substreams "
              << " + #coupled substreams: " << substream_count_in_obu << " vs "
              << num_required_non_coupled_channels +
                     num_required_coupled_channels;
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

bool OutputGainApplies(const uint8_t output_gain_flag,
                       const std::string& label) {
  if (label == "M" || label == "L2" || label == "L3") {
    return output_gain_flag & (1 << 5);
  } else if (label == "R2" || label == "R3") {
    return output_gain_flag & (1 << 4);
  } else if (label == "Ls5") {
    return output_gain_flag & (1 << 3);
  } else if (label == "Rs5") {
    return output_gain_flag & (1 << 2);
  } else if (label == "Ltf2" || label == "Ltf3") {
    return output_gain_flag & (1 << 1);
  } else if (label == "Rtf2" || label == "Rtf3") {
    return output_gain_flag & 1;
  }
  return false;
}

absl::Status FinalizeScalableChannelLayoutConfig(
    const AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap* substream_id_to_labels,
    LabelGainMap* label_to_output_gain,
    std::vector<ChannelNumbers>* channel_numbers_for_layers) {
  const auto& config =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_);

  // Starting from no channel at all.
  ChannelNumbers accumulated_channels = {0, 0, 0};
  int substream_index = 0;
  channel_numbers_for_layers->reserve(config.num_layers);
  for (int i = 0; i < config.num_layers; ++i) {
    const int previous_layer_substream_index = substream_index;

    // Figure out the `ChannelNumber` representation of ChannelGroup #i, i.e.
    // the additional channels presented in this layer.
    const auto& layer_config = config.channel_audio_layer_configs[i];
    ChannelNumbers layer_channels;
    RETURN_IF_NOT_OK(LoudspeakerLayoutToChannels(
        layer_config.loudspeaker_layout, &layer_channels));

    // Channel number in each group can only grow or stay the same.
    if (layer_channels.surround < accumulated_channels.surround ||
        layer_channels.lfe < accumulated_channels.lfe ||
        layer_channels.height < accumulated_channels.height) {
      LOG(ERROR) << "Decreasing channel number:";
      LogChannelNumbers("From", accumulated_channels);
      LogChannelNumbers("To", layer_channels);
      return absl::InvalidArgumentError("");
    }

    channel_numbers_for_layers->push_back(layer_channels);
    LOG(INFO) << "Layer[" << i << "]:";
    LogChannelNumbers("  layer_channels", layer_channels);
    LogChannelNumbers("  accumulated_channels", accumulated_channels);

    std::list<std::string> coupled_substream_labels;
    std::list<std::string> non_coupled_substream_labels;
    if (i == 0) {
      RETURN_IF_NOT_OK(CollectBcgLabels(layer_channels,
                                        &coupled_substream_labels,
                                        &non_coupled_substream_labels));
    } else {
      RETURN_IF_NOT_OK(CollectDcgLabels(accumulated_channels, layer_channels,
                                        &coupled_substream_labels,
                                        &non_coupled_substream_labels));
    }
    AddSubstreamLabels(coupled_substream_labels, non_coupled_substream_labels,
                       audio_element_obu.audio_substream_ids_,
                       substream_id_to_labels, &substream_index);
    RETURN_IF_NOT_OK(ValidateSubstreamCounts(
        coupled_substream_labels, non_coupled_substream_labels, layer_config));

    accumulated_channels = layer_channels;

    // Handle output gains.
    if (layer_config.output_gain_is_present_flag == 1) {
      // Loop through all substream IDs added in this layer.
      for (int i = previous_layer_substream_index; i < substream_index; i++) {
        const auto substream_id = audio_element_obu.audio_substream_ids_[i];

        LOG(INFO) << "Output gain for substream ID: " << substream_id << ":";
        for (const auto& label : substream_id_to_labels->at(substream_id)) {
          if (OutputGainApplies(layer_config.output_gain_flag, label)) {
            (*label_to_output_gain)[label] =
                Q7_8ToFloat(layer_config.output_gain);
            LOG(INFO) << "  " << label << ": Q7.8= " << layer_config.output_gain
                      << "; dB= " << (*label_to_output_gain)[label];
          } else {
            LOG(INFO) << "  " << label << ": (not found)";
          }
        }
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ValidateReconGainDefined(
    const CodecConfigObu& codec_config_obu,
    const AudioElementObu& audio_element_obu) {
  bool recon_gain_required = false;
  const auto channel_config =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_);
  const auto& channel_audio_layer_configs =
      channel_config.channel_audio_layer_configs;
  for (int i = 0; i < channel_config.num_layers; i++) {
    uint8_t expected_recon_gain_is_present_flag;
    if (i == 0) {
      // First layer: there is no demixed channel, so recon gain is not
      // required.
      expected_recon_gain_is_present_flag = 0;
    } else if (codec_config_obu.is_lossless_) {
      // Lossless codec does not require recon gain.
      expected_recon_gain_is_present_flag = 0;
    } else {
      expected_recon_gain_is_present_flag = 1;
      recon_gain_required = true;
    }
    if (channel_audio_layer_configs[i].recon_gain_is_present_flag !=
        expected_recon_gain_is_present_flag) {
      LOG(ERROR)
          << "`recon_gain_is_present_flag` for layer " << i << " should be "
          << absl::StrCat(expected_recon_gain_is_present_flag) << " but is "
          << absl::StrCat(
                 channel_audio_layer_configs[i].recon_gain_is_present_flag);
      return absl::InvalidArgumentError("");
    }
  }

  // Look for recon gain definitions.
  bool recon_gain_defined = false;
  for (const auto& audio_element_param :
       audio_element_obu.audio_element_params_) {
    if (audio_element_param.param_definition_type ==
        ParamDefinition::kParameterDefinitionReconGain) {
      recon_gain_defined = true;
      break;
    }
  }

  if (recon_gain_defined != recon_gain_required) {
    LOG(ERROR) << "Recon gain is " << (recon_gain_required ? "" : "not ")
               << "required but is " << (recon_gain_defined ? "" : "not ")
               << "defined in Audio Element OBU ID= "
               << audio_element_obu.audio_element_id_;
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

// Copies the `LoudspeakerLayout` based on the input data. Uses the deprecated
// field as a backup.
absl::Status CopyLoudspeakerLayout(
    const iamf_tools_cli_proto::ChannelAudioLayerConfig& input_layer_config,
    ChannelAudioLayerConfig::LoudspeakerLayout& output_loudspeaker_layout) {
  if (input_layer_config.has_loudspeaker_layout()) {
    using enum iamf_tools_cli_proto::LoudspeakerLayout;
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    static const auto* kInputLoudspeakerLayoutToOutputLoudspeakerLayout =
        new absl::flat_hash_map<iamf_tools_cli_proto::LoudspeakerLayout,
                                ChannelAudioLayerConfig::LoudspeakerLayout>({
            {LOUDSPEAKER_LAYOUT_MONO, kLayoutMono},
            {LOUDSPEAKER_LAYOUT_STEREO, kLayoutStereo},
            {LOUDSPEAKER_LAYOUT_5_1_CH, kLayout5_1_ch},
            {LOUDSPEAKER_LAYOUT_5_1_2_CH, kLayout5_1_2_ch},
            {LOUDSPEAKER_LAYOUT_5_1_4_CH, kLayout5_1_4_ch},
            {LOUDSPEAKER_LAYOUT_7_1_CH, kLayout7_1_ch},
            {LOUDSPEAKER_LAYOUT_7_1_2_CH, kLayout7_1_2_ch},
            {LOUDSPEAKER_LAYOUT_7_1_4_CH, kLayout7_1_4_ch},
            {LOUDSPEAKER_LAYOUT_3_1_2_CH, kLayout3_1_2_ch},
            {LOUDSPEAKER_LAYOUT_BINAURAL, kLayoutBinaural},
            {LOUDSPEAKER_LAYOUT_RESERVED_BEGIN, kLayoutReservedBegin},
            {LOUDSPEAKER_LAYOUT_RESERVED_END, kLayoutReservedEnd},
        });

    if (!LookupInMap(*kInputLoudspeakerLayoutToOutputLoudspeakerLayout,
                     input_layer_config.loudspeaker_layout(),
                     output_loudspeaker_layout)
             .ok()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown loudspeaker_layout= ",
                       input_layer_config.loudspeaker_layout()));
    }
    return absl::OkStatus();
  } else if (input_layer_config.has_deprecated_loudspeaker_layout()) {
    LOG(WARNING) << "Please upgrade the `deprecated_loudspeaker_layout` "
                    "field to the new "
                    "`loudspeaker_layout` field.";
    if (input_layer_config.deprecated_loudspeaker_layout() < 0 ||
        input_layer_config.deprecated_loudspeaker_layout() >=
            ChannelAudioLayerConfig::kLayoutReservedBegin) {
      return absl::InvalidArgumentError("");
    }
    output_loudspeaker_layout =
        static_cast<ChannelAudioLayerConfig::LoudspeakerLayout>(
            input_layer_config.deprecated_loudspeaker_layout());
    return absl::OkStatus();
  } else {
    LOG(ERROR) << "Missing `loudspeaker_layout` field.";
    return absl::InvalidArgumentError("");
  }
}

absl::Status FillScalableChannelLayoutConfig(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu,
    AudioElementWithData& audio_element) {
  if (!audio_element_metadata.has_scalable_channel_layout_config()) {
    LOG(ERROR) << "Audio Element Metadata ["
               << audio_element_metadata.audio_element_id()
               << " is of type AUDIO_ELEMENT_CHANNEL_BASED but does not have"
               << " the `scalable_channel_layout_config` field.";
    return absl::InvalidArgumentError("");
  }

  const auto& input_config =
      audio_element_metadata.scalable_channel_layout_config();
  RETURN_IF_NOT_OK(audio_element.obu.InitializeScalableChannelLayout(
      input_config.num_layers(), input_config.reserved()));
  auto& config =
      std::get<ScalableChannelLayoutConfig>(audio_element.obu.config_);
  if (config.num_layers != input_config.channel_audio_layer_configs_size()) {
    LOG(ERROR) << "Expected " << static_cast<int>(config.num_layers)
               << " layers in the metadata. Found "
               << input_config.channel_audio_layer_configs_size() << " layers.";
    return absl::InvalidArgumentError("");
  }
  for (int i = 0; i < config.num_layers; ++i) {
    ChannelAudioLayerConfig* const layer_config =
        &config.channel_audio_layer_configs[i];

    const auto& input_layer_config =
        input_config.channel_audio_layer_configs(i);

    RETURN_IF_NOT_OK(CopyLoudspeakerLayout(input_layer_config,
                                           layer_config->loudspeaker_layout));
    RETURN_IF_NOT_OK(
        Uint32ToUint8(input_layer_config.output_gain_is_present_flag(),
                      layer_config->output_gain_is_present_flag));
    RETURN_IF_NOT_OK(
        Uint32ToUint8(input_layer_config.recon_gain_is_present_flag(),
                      layer_config->recon_gain_is_present_flag));
    RETURN_IF_NOT_OK(Uint32ToUint8(input_layer_config.reserved_a(),
                                   layer_config->reserved_a));
    RETURN_IF_NOT_OK(Uint32ToUint8(input_layer_config.substream_count(),
                                   layer_config->substream_count));
    RETURN_IF_NOT_OK(Uint32ToUint8(input_layer_config.coupled_substream_count(),
                                   layer_config->coupled_substream_count));

    if (layer_config->output_gain_is_present_flag == 1) {
      RETURN_IF_NOT_OK(Uint32ToUint8(input_layer_config.output_gain_flag(),
                                     layer_config->output_gain_flag));
      RETURN_IF_NOT_OK(Uint32ToUint8(input_layer_config.reserved_b(),
                                     layer_config->reserved_b));
      RETURN_IF_NOT_OK(Int32ToInt16(input_layer_config.output_gain(),
                                    layer_config->output_gain));
    }
  }

  RETURN_IF_NOT_OK(
      ValidateReconGainDefined(codec_config_obu, audio_element.obu));

  return FinalizeScalableChannelLayoutConfig(
      audio_element.obu, &audio_element.substream_id_to_labels,
      &audio_element.label_to_output_gain,
      &audio_element.channel_numbers_for_layers);
}

absl::Status FillAmbisonicsMonoConfig(
    const iamf_tools_cli_proto::AmbisonicsConfig& input_config,
    const DecodedUleb128 audio_element_id, AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (!input_config.has_ambisonics_mono_config()) {
    LOG(ERROR) << "Audio Element Metadata [" << audio_element_id
               << " is of mode AMBISONICS_MODE_MONO but does not have"
               << " the `ambisonics_mono_config` field.";
    return absl::InvalidArgumentError("");
  }
  const auto& input_mono_config = input_config.ambisonics_mono_config();
  RETURN_IF_NOT_OK(audio_element_obu.InitializeAmbisonicsMono(
      input_mono_config.output_channel_count(),
      input_mono_config.substream_count()));
  auto& mono_config = std::get<AmbisonicsMonoConfig>(
      std::get<AmbisonicsConfig>(audio_element_obu.config_).ambisonics_config);
  if (input_mono_config.channel_mapping_size() !=
      input_mono_config.output_channel_count()) {
    LOG(ERROR) << "Audio Element Metadata [" << audio_element_id
               << " has output_channel_count= "
               << input_mono_config.output_channel_count()
               << ", but `channel_mapping` has "
               << input_mono_config.channel_mapping_size() << " elements.";
    return absl::InvalidArgumentError("");
  }

  for (int i = 0; i < input_mono_config.channel_mapping_size(); ++i) {
    RETURN_IF_NOT_OK(Uint32ToUint8(input_mono_config.channel_mapping(i),
                                   mono_config.channel_mapping[i]));
  }

  // Validate the mono config. This ensures no substream indices should be out
  // of bounds.
  RETURN_IF_NOT_OK(mono_config.Validate(audio_element_obu.num_substreams_));
  // Fill `substream_id_to_labels`. `channel_mapping` encodes the mapping of
  // Ambisonics Channel Number (ACN) to substream index.
  for (int ambisonics_channel_number = 0;
       ambisonics_channel_number < input_mono_config.channel_mapping_size();
       ++ambisonics_channel_number) {
    const uint8_t obu_substream_index =
        mono_config.channel_mapping[ambisonics_channel_number];
    if (obu_substream_index ==
        AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber) {
      LOG(INFO) << "Detected mixed-order ambisonics with  A"
                << ambisonics_channel_number << "dropped.";
      continue;
    }
    const DecodedUleb128 substream_id =
        audio_element_obu.audio_substream_ids_[obu_substream_index];

    // Add the associated ACN to the labels associated with that substream.
    substream_id_to_labels[substream_id].push_back(
        {absl::StrCat("A", ambisonics_channel_number)});
  }

  return absl::OkStatus();
}

absl::Status FillAmbisonicsProjectionConfig(
    const iamf_tools_cli_proto::AmbisonicsConfig& input_config,
    const DecodedUleb128 audio_element_id, AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (!input_config.has_ambisonics_projection_config()) {
    LOG(ERROR) << "Audio Element Metadata [" << audio_element_id
               << " is of mode AMBISONICS_MODE_PROJECTION but does not have"
               << " the `AMBISONICS_MODE_PROJECTION` field.";
    return absl::InvalidArgumentError("");
  }
  const auto& input_projection_config =
      input_config.ambisonics_projection_config();
  RETURN_IF_NOT_OK(audio_element_obu.InitializeAmbisonicsProjection(
      input_projection_config.output_channel_count(),
      input_projection_config.substream_count(),
      input_projection_config.coupled_substream_count()));
  auto& projection_config = std::get<AmbisonicsProjectionConfig>(
      std::get<AmbisonicsConfig>(audio_element_obu.config_).ambisonics_config);
  const int expected_demixing_matrix_size =
      (input_projection_config.substream_count() +
       input_projection_config.coupled_substream_count()) *
      input_projection_config.output_channel_count();
  if (input_projection_config.demixing_matrix_size() !=
      expected_demixing_matrix_size) {
    LOG(ERROR) << "Audio Element Metadata [" << audio_element_id
               << " expects demixing_matrix_size= "
               << expected_demixing_matrix_size
               << ", but `demixing_matrix` has "
               << input_projection_config.demixing_matrix_size()
               << " elements.";
    return absl::InvalidArgumentError("");
  }

  for (int i = 0; i < input_projection_config.demixing_matrix_size(); ++i) {
    RETURN_IF_NOT_OK(Int32ToInt16(input_projection_config.demixing_matrix(i),
                                  projection_config.demixing_matrix[i]));
  }

  // Fill `substream_id_to_labels`.
  if (audio_element_obu.num_substreams_ !=
      static_cast<uint32_t>(projection_config.substream_count)) {
    LOG(ERROR) << "`num_substreams` different from `substream_count`: ("
               << audio_element_obu.num_substreams_ << " vs "
               << static_cast<int>(projection_config.substream_count) << ")";
    return absl::InvalidArgumentError("");
  }

  // For projection mode, assume coupled substreams (using 2 channels) come
  // first and are followed by non-coupled substreams (using 1 channel each).
  for (int i = 0; i < audio_element_obu.num_substreams_; ++i) {
    substream_id_to_labels[audio_element_obu.audio_substream_ids_[i]] =
        (i < projection_config.coupled_substream_count
             ? std::list<std::string>{absl::StrCat("A", 2 * i),
                                      absl::StrCat("A", 2 * i + 1)}
             : std::list<std::string>{absl::StrCat(
                   "A", 2 * projection_config.coupled_substream_count + i)});
  }

  return absl::OkStatus();
}

absl::Status FillAmbisonicsConfig(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    AudioElementWithData& audio_element) {
  if (!audio_element_metadata.has_ambisonics_config()) {
    LOG(ERROR) << "Audio Element Metadata ["
               << audio_element_metadata.audio_element_id()
               << " is of type AUDIO_ELEMENT_SCENE_BASED but does not have"
               << " the `ambisonics_config` field.";
    return absl::InvalidArgumentError("");
  }

  const auto& input_config = audio_element_metadata.ambisonics_config();
  AmbisonicsConfig::AmbisonicsMode ambisonics_mode;
  switch (input_config.ambisonics_mode()) {
    using enum iamf_tools_cli_proto::AmbisonicsMode;
    using enum AmbisonicsConfig::AmbisonicsMode;
    case AMBISONICS_MODE_MONO:
      ambisonics_mode = kAmbisonicsModeMono;
      RETURN_IF_NOT_OK(FillAmbisonicsMonoConfig(
          input_config, audio_element_metadata.audio_element_id(),
          audio_element.obu, audio_element.substream_id_to_labels));
      break;
    case AMBISONICS_MODE_PROJECTION:
      ambisonics_mode = kAmbisonicsModeProjection;
      RETURN_IF_NOT_OK(FillAmbisonicsProjectionConfig(
          input_config, audio_element_metadata.audio_element_id(),
          audio_element.obu, audio_element.substream_id_to_labels));
      break;
    default:
      LOG(ERROR) << "Unrecognized ambisonics_mode: "
                 << input_config.ambisonics_mode();
      return absl::InvalidArgumentError("");
  }
  std::get<AmbisonicsConfig>(audio_element.obu.config_).ambisonics_mode =
      ambisonics_mode;

  return absl::OkStatus();
}

void LogAudioElements(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements) {
  // Examine Audio Element OBUs.
  for (const auto& [audio_element_id, audio_element] : audio_elements) {
    audio_element.obu.PrintObu();

    // Log `substream_id_to_labels` separately.
    for (const auto& [substream_id, labels] :
         audio_element.substream_id_to_labels) {
      LOG(INFO) << "Substream ID: " << substream_id;
      LOG(INFO) << "  num_channels= " << labels.size();
    }
  }
}

}  // namespace

absl::Status AudioElementGenerator::Generate(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_configs,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  for (const auto& audio_element_metadata : audio_element_metadata_) {
    // Common data.
    const auto audio_element_id = audio_element_metadata.audio_element_id();

    AudioElementObu::AudioElementType audio_element_type;
    switch (audio_element_metadata.audio_element_type()) {
      using enum iamf_tools_cli_proto::AudioElementType;
      using enum AudioElementObu::AudioElementType;
      case AUDIO_ELEMENT_CHANNEL_BASED:
        audio_element_type = kAudioElementChannelBased;
        break;
      case AUDIO_ELEMENT_SCENE_BASED:
        audio_element_type = kAudioElementSceneBased;
        break;
      default:
        return absl::InvalidArgumentError("");
    }
    uint8_t reserved;
    RETURN_IF_NOT_OK(
        Uint32ToUint8(audio_element_metadata.reserved(), reserved));
    const auto codec_config_id = audio_element_metadata.codec_config_id();

    AudioElementObu audio_element_obu(
        GetHeaderFromMetadata(audio_element_metadata.obu_header()),
        audio_element_id, audio_element_type, reserved, codec_config_id);

    // Audio Substreams.
    RETURN_IF_NOT_OK(
        GenerateAudioSubstreams(audio_element_metadata, audio_element_obu));

    // Parameter definitions.
    if (!codec_configs.contains(audio_element_metadata.codec_config_id())) {
      LOG(ERROR) << "Failed to find matching codec_config_id="
                 << audio_element_metadata.codec_config_id();
      return absl::InvalidArgumentError("");
    }
    const auto& codec_config_obu =
        codec_configs.at(audio_element_metadata.codec_config_id());
    RETURN_IF_NOT_OK(GenerateParameterDefinitions(
        audio_element_metadata, codec_config_obu, audio_element_obu));

    // Config data based on `audio_element_type`.
    // Insert first so even if the following operations fail, the OBU will be
    // destroyed by one of the transitive callers of this function.
    auto [new_audio_element_iter, inserted] = audio_elements.emplace(
        audio_element_id, AudioElementWithData{
                              .obu = std::move(audio_element_obu),
                              .codec_config = &codec_config_obu,
                          });
    if (!inserted) {
      LOG(ERROR) << "Inserting Audio Element with ID "
                 << audio_element_metadata.audio_element_id()
                 << " failed because there is a duplicated element with the "
                 << "same ID";
      return absl::InvalidArgumentError("");
    }

    switch (new_audio_element_iter->second.obu.audio_element_type_) {
      using enum AudioElementObu::AudioElementType;
      case kAudioElementChannelBased:
        RETURN_IF_NOT_OK(FillScalableChannelLayoutConfig(
            audio_element_metadata, codec_config_obu,
            new_audio_element_iter->second));
        break;
      case kAudioElementSceneBased:
        RETURN_IF_NOT_OK(FillAmbisonicsConfig(audio_element_metadata,
                                              new_audio_element_iter->second));
        break;
      default:
        // TODO(b/289541186): Support the extension fields here.
        return absl::InvalidArgumentError("");
    }
  }

  LogAudioElements(audio_elements);
  return absl::OkStatus();
}

}  // namespace iamf_tools
