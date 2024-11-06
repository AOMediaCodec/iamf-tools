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
#include "iamf/cli/proto_to_obu/audio_element_generator.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/lookup_tables.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using absl::InvalidArgumentError;
using absl::StrCat;

using enum ChannelLabel::Label;

namespace {
// Copies the `ParameterDefinitionType` based on the input data.
absl::Status CopyAudioElementParamDefinitionType(
    iamf_tools_cli_proto::AudioElementParam user_data_parameter,
    ParamDefinition::ParameterDefinitionType& output_param_definition_type) {
  if (user_data_parameter.has_deprecated_param_definition_type()) {
    return InvalidArgumentError(
        "Please upgrade the `deprecated_param_definition_type` "
        "field to the new `param_definition_type` field."
        "\nSuggested upgrades:\n"
        "- `deprecated_param_definition_type: 1` -> `param_definition_type: "
        "PARAM_DEFINITION_TYPE_DEMIXING`\n"
        "- `deprecated_param_definition_type: 2` -> `param_definition_type: "
        "PARAM_DEFINITION_TYPE_RECON_GAIN`\n");
  }
  if (!user_data_parameter.has_param_definition_type()) {
    return InvalidArgumentError("Missing `param_definition_type` field.");
  }

  switch (user_data_parameter.param_definition_type()) {
    using enum iamf_tools_cli_proto::ParamDefinitionType;
    using enum ParamDefinition::ParameterDefinitionType;
    case PARAM_DEFINITION_TYPE_DEMIXING:
      output_param_definition_type = kParameterDefinitionDemixing;
      return absl::OkStatus();
    case PARAM_DEFINITION_TYPE_RECON_GAIN:
      output_param_definition_type = kParameterDefinitionReconGain;
      return absl::OkStatus();
    case PARAM_DEFINITION_TYPE_MIX_GAIN:
      return InvalidArgumentError(absl::StrCat(
          "Mix gain parameters are not permitted in audio elements"));
    case PARAM_DEFINITION_TYPE_RESERVED_3:
      output_param_definition_type = kParameterDefinitionReservedStart;
      return absl::OkStatus();
    default:
      return InvalidArgumentError(
          StrCat("Unknown or invalid param_definition_type= ",
                 user_data_parameter.param_definition_type()));
  }
}

absl::Status GenerateAudioSubstreams(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    AudioElementObu& audio_element_obu) {
  if (audio_element_metadata.num_substreams() !=
      audio_element_metadata.audio_substream_ids_size()) {
    return InvalidArgumentError(
        StrCat("User data has inconsistent `num_substreams` and "
               "`audio_substream_ids`. User provided ",
               audio_element_metadata.audio_substream_ids_size(),
               " substreams in `audio_substream_ids`, and `num_substreams`= ",
               audio_element_metadata.num_substreams()));
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
    return InvalidArgumentError(StrCat(
        "User data has inconsistent `num_parameters`. Found: ",
        audio_element_metadata.audio_element_params_size(),
        " parameters, expected: ", audio_element_metadata.num_parameters()));
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
            *demixing_param_definition));
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
                .reserved_for_future_use));
        if (demixing_param_definition->duration_ !=
            codec_config_obu.GetCodecConfig().num_samples_per_frame) {
          return InvalidArgumentError(
              StrCat("Demixing parameter duration= ",
                     demixing_param_definition->duration_,
                     " is inconsistent with num_samples_per_frame=",
                     codec_config_obu.GetCodecConfig().num_samples_per_frame));
        }

        audio_element_param.param_definition =
            std::move(demixing_param_definition);
        break;
      }
      case kParameterDefinitionReconGain: {
        auto recon_gain_param_definition =
            std::make_unique<ReconGainParamDefinition>(
                audio_element_obu.GetAudioElementId());
        RETURN_IF_NOT_OK(CopyParamDefinition(
            user_data_parameter.recon_gain_param().param_definition(),
            *recon_gain_param_definition));
        if (recon_gain_param_definition->duration_ !=
            codec_config_obu.GetCodecConfig().num_samples_per_frame) {
          return InvalidArgumentError(
              StrCat("Recon gain parameter duration= ",
                     recon_gain_param_definition->duration_,
                     " is inconsistent with num_samples_per_frame=",
                     codec_config_obu.GetCodecConfig().num_samples_per_frame));
        }
        audio_element_param.param_definition =
            std::move(recon_gain_param_definition);
        break;
      }
      case kParameterDefinitionMixGain:
        return InvalidArgumentError(
            "Mix gain parameters are not permitted in audio elements.");
      default: {
        auto extended_param_definition =
            std::make_unique<ExtendedParamDefinition>(
                audio_element_param.param_definition_type);
        extended_param_definition->param_definition_size_ =
            user_data_parameter.param_definition_extension()
                .param_definition_size();

        auto& metadata_extension_bytes =
            user_data_parameter.param_definition_extension()
                .param_definition_bytes();

        extended_param_definition->param_definition_bytes_.reserve(
            metadata_extension_bytes.size());
        for (const char& c : metadata_extension_bytes) {
          extended_param_definition->param_definition_bytes_.push_back(
              static_cast<uint8_t>(c));
        }

        audio_element_param.param_definition =
            std::move(extended_param_definition);
      } break;
    }
  }

  return absl::OkStatus();
}

absl::Status LoudspeakerLayoutToChannels(
    const ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    ChannelNumbers& channels) {
  switch (loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kLayoutMono:
      channels = {1, 0, 0};
      break;
    case kLayoutStereo:
      channels = {2, 0, 0};
      break;
    case kLayout5_1_ch:
      channels = {5, 1, 0};
      break;
    case kLayout5_1_2_ch:
      channels = {5, 1, 2};
      break;
    case kLayout5_1_4_ch:
      channels = {5, 1, 4};
      break;
    case kLayout7_1_ch:
      channels = {7, 1, 0};
      break;
    case kLayout7_1_2_ch:
      channels = {7, 1, 2};
      break;
    case kLayout7_1_4_ch:
      channels = {7, 1, 4};
      break;
    case kLayout3_1_2_ch:
      channels = {3, 1, 2};
      break;
    case kLayoutBinaural:
      channels = {2, 0, 0};
      break;
    default:
      return InvalidArgumentError(
          StrCat("Unknown loudspeaker_layout= ", loudspeaker_layout));
  }
  return absl::OkStatus();
}

// For the Base Channel Group (BCG). This is the first layer of a scalable audio
// element.
// https://aomediacodec.github.io/iamf/#scalablechannelaudio-channelgroupformat
absl::Status CollectBaseChannelGroupLabels(
    const ChannelNumbers& layer_channels,
    std::list<ChannelLabel::Label>* coupled_substream_labels,
    std::list<ChannelLabel::Label>* non_coupled_substream_labels) {
  switch (layer_channels.surround) {
    case 1:
      non_coupled_substream_labels->push_back(kMono);
      break;
    case 2:
      coupled_substream_labels->push_back(kL2);
      coupled_substream_labels->push_back(kR2);
      break;
    case 3:
      coupled_substream_labels->push_back(kL3);
      coupled_substream_labels->push_back(kR3);
      non_coupled_substream_labels->push_back(kCentre);
      break;
    case 5:
      coupled_substream_labels->push_back(kL5);
      coupled_substream_labels->push_back(kR5);
      coupled_substream_labels->push_back(kLs5);
      coupled_substream_labels->push_back(kRs5);
      non_coupled_substream_labels->push_back(kCentre);
      break;
    case 7:
      coupled_substream_labels->push_back(kL7);
      coupled_substream_labels->push_back(kR7);
      coupled_substream_labels->push_back(kLss7);
      coupled_substream_labels->push_back(kRss7);
      coupled_substream_labels->push_back(kLrs7);
      coupled_substream_labels->push_back(kRrs7);
      non_coupled_substream_labels->push_back(kCentre);
      break;
    default:
      LOG(ERROR) << "Unsupported number of surround channels: "
                 << layer_channels.surround;
      return InvalidArgumentError(
          StrCat("Unsupported number of surround channels: ",
                 layer_channels.surround));
  }
  switch (layer_channels.height) {
    case 0:
      // Not adding anything.
      break;
    case 2:
      if (layer_channels.surround == 3) {
        coupled_substream_labels->push_back(kLtf3);
        coupled_substream_labels->push_back(kRtf3);
      } else {
        coupled_substream_labels->push_back(kLtf2);
        coupled_substream_labels->push_back(kRtf2);
      }
      break;
    case 4:
      coupled_substream_labels->push_back(kLtf4);
      coupled_substream_labels->push_back(kRtf4);
      coupled_substream_labels->push_back(kLtb4);
      coupled_substream_labels->push_back(kRtb4);
      break;
    default:
      LOG(ERROR) << "Unsupported number of height channels: "
                 << layer_channels.height;
      return InvalidArgumentError(StrCat(
          "Unsupported number of height channels: ", layer_channels.height));
  }
  switch (layer_channels.lfe) {
    case 0:
      // Not adding anything.
      break;
    case 1:
      non_coupled_substream_labels->push_back(kLFE);
      break;
    default:
      return InvalidArgumentError(
          StrCat("Unsupported number of LFE channels: ", layer_channels.lfe));
  }

  return absl::OkStatus();
}

absl::Status CollectChannelLayersAndLabelsForExpandedLoudspeakerLayout(
    int layer_index,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout,
    ChannelNumbers& channel_numbers,
    std::list<ChannelLabel::Label>& coupled_substream_labels,
    std::list<ChannelLabel::Label>& non_coupled_substream_labels) {
  if (layer_index != 0) {
    return absl::InvalidArgumentError(
        "Expanded layout is only permitted when there is a single layer.");
  }
  RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                    "Expanded layout is required."));

  switch (*expanded_loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    case kExpandedLayoutLFE:
      channel_numbers = {0, 1, 0};
      non_coupled_substream_labels = {kLFE};
      break;
    case kExpandedLayoutStereoS:
      channel_numbers = {2, 0, 0};
      coupled_substream_labels = {kLs5, kRs5};
      break;
    case kExpandedLayoutStereoSS:
      channel_numbers = {2, 0, 0};
      coupled_substream_labels = {kLss7, kRss7};
      break;
    case kExpandedLayoutStereoRS:
      channel_numbers = {2, 0, 0};
      coupled_substream_labels = {kLrs7, kRrs7};
      break;
    case kExpandedLayoutStereoTF:
      channel_numbers = {0, 0, 2};
      coupled_substream_labels = {kLtf4, kRtf4};
      break;
    case kExpandedLayoutStereoTB:
      channel_numbers = {0, 0, 2};
      coupled_substream_labels = {kLtb4, kRtb4};
      break;
    case kExpandedLayoutTop4Ch:
      channel_numbers = {0, 0, 4};
      coupled_substream_labels = {kLtf4, kRtf4, kLtb4, kRtb4};
      break;
    case kExpandedLayout3_0_ch:
      channel_numbers = {3, 0, 0};
      coupled_substream_labels = {kL7, kR7};
      non_coupled_substream_labels = {kCentre};
      break;
    case kExpandedLayout9_1_6_ch:
      channel_numbers = {9, 1, 6};
      coupled_substream_labels = {kFLc,   kFRc,   kFL,   kFR,   kSiL,
                                  kSiR,   kBL,    kBR,   kTpFL, kTpFR,
                                  kTpSiL, kTpSiR, kTpBL, kTpBR};
      non_coupled_substream_labels = {kFC, kLFE};
      break;
    case kExpandedLayoutStereoF:
      channel_numbers = {2, 0, 0};
      coupled_substream_labels = {kFL, kFR};
      break;
    case kExpandedLayoutStereoSi:
      channel_numbers = {2, 0, 0};
      coupled_substream_labels = {kSiL, kSiR};
      break;
    case kExpandedLayoutStereoTpSi:
      channel_numbers = {0, 0, 2};
      coupled_substream_labels = {kTpSiL, kTpSiR};
      break;
    case kExpandedLayoutTop6Ch:
      channel_numbers = {0, 0, 6};
      coupled_substream_labels = {kTpFL, kTpFR, kTpSiL, kTpSiR, kTpBL, kTpBR};
      break;
    default:
      return absl::InvalidArgumentError(
          StrCat("Unsupported expanded loudspeaker layout= ",
                 *expanded_loudspeaker_layout));
  }

  LOG(INFO) << "Layer[" << layer_index << "]:";
  LogChannelNumbers("  layer_channels", channel_numbers);

  return absl::OkStatus();
}

// For the Demixed Channel Groups (DCG). This all layers after the first layer
// in a scalable audio element.
// https://aomediacodec.github.io/iamf/#scalablechannelaudio-channelgroupformat
absl::Status CollectDemixedChannelGroupLabels(
    const ChannelNumbers& accumulated_channels,
    const ChannelNumbers& layer_channels,
    std::list<ChannelLabel::Label>* coupled_substream_labels,
    std::list<ChannelLabel::Label>* non_coupled_substream_labels) {
  bool push_l2_in_the_end = false;
  for (int surround = accumulated_channels.surround + 1;
       surround <= layer_channels.surround; surround++) {
    switch (surround) {
      case 2:
        // This is the special case where layer 1 is Mono and layer 2 is
        // Stereo. According to the Spec 3.7.2
        // (https://aomediacodec.github.io/iamf/#syntax-scalable-channel-layout-config):
        // "The Centre (or Front Centre) channel comes first and is followed by
        // the LFE (or LFE1) channel, and then the L channel.". Save pushing
        // kL2 till the end.
        push_l2_in_the_end = true;
        break;
      case 3:
        non_coupled_substream_labels->push_back(kCentre);
        break;
      case 5:
        coupled_substream_labels->push_back(kL5);
        coupled_substream_labels->push_back(kR5);
        break;
      case 7:
        coupled_substream_labels->push_back(kLss7);
        coupled_substream_labels->push_back(kRss7);
        break;
      default:
        if (surround > 7) {
          return InvalidArgumentError(
              StrCat("Unsupported number of surround channels: ", surround));
        }
        break;
    }
  }

  if (layer_channels.height > accumulated_channels.height) {
    if (accumulated_channels.height == 0) {
      if (layer_channels.height == 4) {
        coupled_substream_labels->push_back(kLtf4);
        coupled_substream_labels->push_back(kRtf4);
        coupled_substream_labels->push_back(kLtb4);
        coupled_substream_labels->push_back(kRtb4);
      } else if (layer_channels.height == 2) {
        if (layer_channels.surround == 3) {
          coupled_substream_labels->push_back(kLtf3);
          coupled_substream_labels->push_back(kRtf3);
        } else {
          coupled_substream_labels->push_back(kLtf2);
          coupled_substream_labels->push_back(kRtf2);
        }
      } else {
        return InvalidArgumentError(StrCat(
            "Unsupported number of height channels: ", layer_channels.height));
      }
    } else if (accumulated_channels.height == 2) {
      coupled_substream_labels->push_back(kLtf4);
      coupled_substream_labels->push_back(kRtf4);
    } else {
      return InvalidArgumentError(
          absl::StrCat("Unsupported number of height channels: ",
                       accumulated_channels.height));
    }
  }

  if (layer_channels.lfe > accumulated_channels.lfe) {
    if (layer_channels.lfe == 1) {
      non_coupled_substream_labels->push_back(kLFE);
    } else {
      return InvalidArgumentError(
          StrCat("Unsupported number of LFE channels: ", layer_channels.lfe));
    }
  }

  if (push_l2_in_the_end) {
    non_coupled_substream_labels->push_back(kL2);
  }

  return absl::OkStatus();
}

absl::Status AddSubstreamLabels(
    const std::list<ChannelLabel::Label>& coupled_substream_labels,
    const std::list<ChannelLabel::Label>& non_coupled_substream_labels,
    const std::vector<DecodedUleb128>& substream_ids,
    SubstreamIdLabelsMap& substream_id_to_labels, int& substream_index) {
  CHECK_EQ(coupled_substream_labels.size() % 2, 0);
  // Determine how many substream IDs will be used below. This helps prevent
  // indexing `substream_ids` out of bounds.
  const auto substreams_to_add =
      coupled_substream_labels.size() / 2 + non_coupled_substream_labels.size();
  if (substream_index + substreams_to_add > substream_ids.size()) {
    return absl::OutOfRangeError(
        absl::StrCat("Too few substream IDs are present to assign all labels. "
                     "substream_ids.size()= ",
                     substream_ids.size()));
  }

  // First add coupled substream labels, two at a time.
  for (auto iter = coupled_substream_labels.begin();
       iter != coupled_substream_labels.end() &&
       substream_index < substream_ids.size();) {
    const auto substream_id = substream_ids[substream_index++];
    auto& labels_for_substream_id = substream_id_to_labels[substream_id];
    const auto first_label = *iter++;
    const auto second_label = *iter++;

    labels_for_substream_id.push_back(first_label);
    labels_for_substream_id.push_back(second_label);
    LOG(INFO) << "  substream_id_to_labels[" << substream_id
              << "]: " << first_label << "/" << second_label;
  }

  // Then add non-coupled substream labels.
  for (auto iter = non_coupled_substream_labels.begin();
       iter != non_coupled_substream_labels.end();) {
    const auto substream_id = substream_ids[substream_index++];
    substream_id_to_labels[substream_id].push_back(*iter++);
    LOG(INFO) << "  substream_id_to_labels[" << substream_id
              << "]: " << substream_id_to_labels[substream_id].back();
  }
  return absl::OkStatus();
}

absl::Status ValidateSubstreamCounts(
    const std::list<ChannelLabel::Label>& coupled_substream_labels,
    const std::list<ChannelLabel::Label>& non_coupled_substream_labels,
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
    return InvalidArgumentError(StrCat(
        "Coupled substream count different from the required number. In OBU: ",
        coupled_substream_count_in_obu,
        " vs expected: ", num_required_coupled_channels));
  }

  // The sum of coupled and non-coupled channels must be the same as
  // the `substream_count` recorded in the OBU.
  if (substream_count_in_obu !=
      (num_required_non_coupled_channels + num_required_coupled_channels)) {
    return InvalidArgumentError(StrCat(
        "Substream count different from the #non-coupled substreams. In OBU: ",
        substream_count_in_obu, " vs expected: ",
        num_required_non_coupled_channels + num_required_coupled_channels));
  }

  return absl::OkStatus();
}

bool OutputGainApplies(const uint8_t output_gain_flag,
                       ChannelLabel::Label label) {
  switch (label) {
    case kMono:
    case kL2:
    case kL3:
      return output_gain_flag & (1 << 5);
    case kR2:
    case kR3:
      return output_gain_flag & (1 << 4);
    case kLs5:
      return output_gain_flag & (1 << 3);
    case kRs5:
      return output_gain_flag & (1 << 2);
    case kLtf2:
    case kLtf3:
      return output_gain_flag & (1 << 1);
    case kRtf2:
    case kRtf3:
      return output_gain_flag & 1;
    default:
      return false;
  }
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
    } else if (codec_config_obu.IsLossless()) {
      // Lossless codec does not require recon gain.
      expected_recon_gain_is_present_flag = 0;
    } else {
      expected_recon_gain_is_present_flag = 1;
      recon_gain_required = true;
    }
    if (channel_audio_layer_configs[i].recon_gain_is_present_flag !=
        expected_recon_gain_is_present_flag) {
      return InvalidArgumentError(
          StrCat("`recon_gain_is_present_flag` for layer ", i, " should be ",
                 expected_recon_gain_is_present_flag, " but is ",
                 channel_audio_layer_configs[i].recon_gain_is_present_flag));
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
    return InvalidArgumentError(
        StrCat("Recon gain is ", (recon_gain_required ? "" : "not "),
               "required but is ", (recon_gain_defined ? "" : "not "),
               "defined in Audio Element OBU ID= ",
               audio_element_obu.GetAudioElementId()));
  }

  return absl::OkStatus();
}

// Copies the `LoudspeakerLayout` based on the input data.
absl::Status CopyLoudspeakerLayout(
    const iamf_tools_cli_proto::ChannelAudioLayerConfig&
        input_channel_audio_layer_config,
    ChannelAudioLayerConfig::LoudspeakerLayout& output_loudspeaker_layout) {
  if (input_channel_audio_layer_config.has_deprecated_loudspeaker_layout()) {
    return InvalidArgumentError(
        "Please upgrade the `deprecated_loudspeaker_layout` field to the new "
        "`loudspeaker_layout` field.\n"
        "Suggested upgrades:\n"
        "- `deprecated_loudspeaker_layout: 0` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_MONO`\n"
        "- `deprecated_loudspeaker_layout: 1` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_STEREO`\n"
        "- `deprecated_loudspeaker_layout: 2` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_5_1_CH`\n"
        "- `deprecated_loudspeaker_layout: 3` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_5_1_2_CH`\n"
        "- `deprecated_loudspeaker_layout: 4` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_5_1_4_CH`\n"
        "- `deprecated_loudspeaker_layout: 5` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_7_1_CH`\n"
        "- `deprecated_loudspeaker_layout: 6` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_7_1_2_CH`\n"
        "- `deprecated_loudspeaker_layout: 7` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_7_1_4_CH`\n"
        "- `deprecated_loudspeaker_layout: 8` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_3_1_2_CH`\n"
        "- `deprecated_loudspeaker_layout: 9` -> `loudspeaker_layout: "
        "LOUDSPEAKER_LAYOUT_BINAURAL`\n");
  }

  static const auto kProtoToInternalLoudspeakerLayout = BuildStaticMapFromPairs(
      LookupTables::kProtoAndInternalLoudspeakerLayouts);

  return CopyFromMap(*kProtoToInternalLoudspeakerLayout,
                     input_channel_audio_layer_config.loudspeaker_layout(),
                     "Internal version of proto `LoudspeakerLayout`= ",
                     output_loudspeaker_layout);
}

// Copies the `ExpandedLoudspeakerLayout` based on the input data.
absl::Status CopyExpandedLoudspeakerLayout(
    iamf_tools_cli_proto::ExpandedLoudspeakerLayout
        input_expanded_loudspeaker_layout,
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout&
        output_expanded_loudspeaker_layout) {
  static const auto kProtoToInternalExpandedLoudspeakerLayout =
      BuildStaticMapFromPairs(
          LookupTables::kProtoAndInternalExpandedLoudspeakerLayouts);

  return CopyFromMap(*kProtoToInternalExpandedLoudspeakerLayout,
                     input_expanded_loudspeaker_layout,
                     "Internal version of proto `ExpandedLoudspeakerLayout`= ",
                     output_expanded_loudspeaker_layout);
}

// Copies the `LoudspeakerLayout` and `ExpandedLoudspeakerLayout` based on the
// input data.
absl::Status CopyLoudspeakerLayoutAndExpandedLoudspeakerLayout(
    const iamf_tools_cli_proto::ChannelAudioLayerConfig& input_layer_config,
    ChannelAudioLayerConfig::LoudspeakerLayout& output_loudspeaker_layout,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>&
        output_expanded_loudspeaker_layout) {
  RETURN_IF_NOT_OK(
      CopyLoudspeakerLayout(input_layer_config, output_loudspeaker_layout));

  if (output_loudspeaker_layout == ChannelAudioLayerConfig::kLayoutExpanded) {
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout
        expanded_loudspeaker_layout;
    RETURN_IF_NOT_OK(CopyExpandedLoudspeakerLayout(
        input_layer_config.expanded_loudspeaker_layout(),
        expanded_loudspeaker_layout));
    output_expanded_loudspeaker_layout = expanded_loudspeaker_layout;
  } else {
    // Ignore user input since it would not be in the bitstream as of IAMF v1.1.
    output_expanded_loudspeaker_layout = std::nullopt;
  }

  return absl::OkStatus();
}

absl::Status FillScalableChannelLayoutConfig(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu,
    AudioElementWithData& audio_element) {
  if (!audio_element_metadata.has_scalable_channel_layout_config()) {
    return InvalidArgumentError(StrCat(
        "Audio Element Metadata [", audio_element_metadata.audio_element_id(),
        " is of type AUDIO_ELEMENT_CHANNEL_BASED but does not have",
        " the `scalable_channel_layout_config` field."));
  }

  const auto& input_config =
      audio_element_metadata.scalable_channel_layout_config();
  RETURN_IF_NOT_OK(audio_element.obu.InitializeScalableChannelLayout(
      input_config.num_layers(), input_config.reserved()));
  auto& config =
      std::get<ScalableChannelLayoutConfig>(audio_element.obu.config_);
  if (config.num_layers != input_config.channel_audio_layer_configs_size()) {
    return InvalidArgumentError(StrCat(
        "Expected ", config.num_layers, " layers in the metadata. Found ",
        input_config.channel_audio_layer_configs_size(), " layers."));
  }
  for (int i = 0; i < config.num_layers; ++i) {
    ChannelAudioLayerConfig* const layer_config =
        &config.channel_audio_layer_configs[i];

    const auto& input_layer_config =
        input_config.channel_audio_layer_configs(i);

    RETURN_IF_NOT_OK(CopyLoudspeakerLayoutAndExpandedLoudspeakerLayout(
        input_layer_config, layer_config->loudspeaker_layout,
        layer_config->expanded_loudspeaker_layout));
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

  return AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
      audio_element.obu.audio_substream_ids_, config,
      audio_element.substream_id_to_labels, audio_element.label_to_output_gain,
      audio_element.channel_numbers_for_layers);
}

absl::Status FinalizeAmbisonicsMonoConfig(
    const AudioElementObu& audio_element_obu,
    const AmbisonicsMonoConfig& mono_config,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  // Fill `substream_id_to_labels`. `channel_mapping` encodes the mapping of
  // Ambisonics Channel Number (ACN) to substream index.
  for (int ambisonics_channel_number = 0;
       ambisonics_channel_number < mono_config.channel_mapping.size();
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
    const auto ambisonics_label =
        ChannelLabel::AmbisonicsChannelNumberToLabel(ambisonics_channel_number);
    if (!ambisonics_label.ok()) {
      return ambisonics_label.status();
    }
    substream_id_to_labels[substream_id].push_back(*ambisonics_label);
  }
  return absl::OkStatus();
}

absl::Status FinalizeAmbisonicsProjectionConfig(
    const AudioElementObu& audio_element_obu,
    const AmbisonicsProjectionConfig& projection_config,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (audio_element_obu.num_substreams_ !=
      static_cast<uint32_t>(projection_config.substream_count)) {
    return InvalidArgumentError(
        StrCat("`num_substreams` different from `substream_count`: (",
               audio_element_obu.num_substreams_, " vs ",
               projection_config.substream_count, ")"));
  }

  // For projection mode, assume coupled substreams (using 2 channels) come
  // first and are followed by non-coupled substreams (using 1 channel each).
  for (int i = 0; i < audio_element_obu.num_substreams_; ++i) {
    const std::list<int> ambisonic_channel_numbers =
        i < projection_config.coupled_substream_count
            ? std::list<int>{2 * i, 2 * i + 1}
            : std::list<int>{2 * projection_config.coupled_substream_count + i};
    for (const auto ambisonic_channel_number : ambisonic_channel_numbers) {
      const auto ambisonics_label =
          ChannelLabel::AmbisonicsChannelNumberToLabel(
              ambisonic_channel_number);
      if (!ambisonics_label.ok()) {
        return ambisonics_label.status();
      }
      substream_id_to_labels[audio_element_obu.audio_substream_ids_[i]]
          .push_back(*ambisonics_label);
    }
  }
  return absl::OkStatus();
}

absl::Status FillAmbisonicsMonoConfig(
    const iamf_tools_cli_proto::AmbisonicsConfig& input_config,
    const DecodedUleb128 audio_element_id, AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (!input_config.has_ambisonics_mono_config()) {
    return InvalidArgumentError(
        StrCat("Audio Element Metadata [", audio_element_id,
               " is of mode AMBISONICS_MODE_MONO but does not have the "
               "`ambisonics_mono_config` field."));
  }
  const auto& input_mono_config = input_config.ambisonics_mono_config();
  RETURN_IF_NOT_OK(audio_element_obu.InitializeAmbisonicsMono(
      input_mono_config.output_channel_count(),
      input_mono_config.substream_count()));
  auto& mono_config = std::get<AmbisonicsMonoConfig>(
      std::get<AmbisonicsConfig>(audio_element_obu.config_).ambisonics_config);
  if (input_mono_config.channel_mapping_size() !=
      input_mono_config.output_channel_count()) {
    return InvalidArgumentError(StrCat(
        "Audio Element Metadata [", audio_element_id,
        " has output_channel_count= ", input_mono_config.output_channel_count(),
        ", but `channel_mapping` has ",
        input_mono_config.channel_mapping_size(), " elements."));
  }

  for (int i = 0; i < input_mono_config.channel_mapping_size(); ++i) {
    RETURN_IF_NOT_OK(Uint32ToUint8(input_mono_config.channel_mapping(i),
                                   mono_config.channel_mapping[i]));
  }

  // Validate the mono config. This ensures no substream indices should be out
  // of bounds.
  RETURN_IF_NOT_OK(mono_config.Validate(audio_element_obu.num_substreams_));
  // Populate substream_id_to_labels.
  RETURN_IF_NOT_OK(FinalizeAmbisonicsMonoConfig(audio_element_obu, mono_config,
                                                substream_id_to_labels));
  return absl::OkStatus();
}

absl::Status FillAmbisonicsProjectionConfig(
    const iamf_tools_cli_proto::AmbisonicsConfig& input_config,
    const DecodedUleb128 audio_element_id, AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (!input_config.has_ambisonics_projection_config()) {
    return InvalidArgumentError(
        StrCat("Audio Element Metadata [", audio_element_id,
               " is of mode AMBISONICS_MODE_PROJECTION but does not have"
               " the `AMBISONICS_MODE_PROJECTION` field."));
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
    return InvalidArgumentError(
        StrCat("Audio Element Metadata [", audio_element_id,
               " expects demixing_matrix_size= ", expected_demixing_matrix_size,
               ", but `demixing_matrix` has ",
               input_projection_config.demixing_matrix_size(), " elements."));
  }

  for (int i = 0; i < input_projection_config.demixing_matrix_size(); ++i) {
    RETURN_IF_NOT_OK(Int32ToInt16(input_projection_config.demixing_matrix(i),
                                  projection_config.demixing_matrix[i]));
  }
  RETURN_IF_NOT_OK(FinalizeAmbisonicsProjectionConfig(
      audio_element_obu, projection_config, substream_id_to_labels));
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
    return InvalidArgumentError(StrCat(
        "Audio Element Metadata [", audio_element_metadata.audio_element_id(),
        " is of type AUDIO_ELEMENT_SCENE_BASED but does not have"
        " the `ambisonics_config` field."));
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
      return InvalidArgumentError(StrCat("Unrecognized ambisonics_mode: ",
                                         input_config.ambisonics_mode()));
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

absl::Status CollectChannelLayersAndLabelsForLoudspeakerLayout(
    int layer_index,
    ChannelAudioLayerConfig::LoudspeakerLayout loudspeaker_layout,
    const ChannelNumbers& accumulated_channels, ChannelNumbers& layer_channels,
    std::list<ChannelLabel::Label>& coupled_substream_labels,
    std::list<ChannelLabel::Label>& non_coupled_substream_labels) {
  // Figure out the `ChannelNumber` representation of ChannelGroup #i, i.e.
  // the additional channels presented in this layer.
  RETURN_IF_NOT_OK(
      LoudspeakerLayoutToChannels(loudspeaker_layout, layer_channels));

  // Channel number in each group can only grow or stay the same.
  if (layer_channels.surround < accumulated_channels.surround ||
      layer_channels.lfe < accumulated_channels.lfe ||
      layer_channels.height < accumulated_channels.height) {
    LogChannelNumbers("From", accumulated_channels);
    LogChannelNumbers("To", layer_channels);
    return InvalidArgumentError(
        StrCat("At least one channel number decreased from "
               "accumulated_channels to layer_channels"));
  }

  LOG(INFO) << "Layer[" << layer_index << "]:";
  LogChannelNumbers("  layer_channels", layer_channels);
  LogChannelNumbers("  accumulated_channels", accumulated_channels);

  if (layer_index == 0) {
    return CollectBaseChannelGroupLabels(layer_channels,
                                         &coupled_substream_labels,
                                         &non_coupled_substream_labels);
  } else {
    return CollectDemixedChannelGroupLabels(
        accumulated_channels, layer_channels, &coupled_substream_labels,
        &non_coupled_substream_labels);
  }
}

}  // namespace

absl::Status AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
    const std::vector<DecodedUleb128>& audio_substream_ids,
    const ScalableChannelLayoutConfig& config,
    SubstreamIdLabelsMap& substream_id_to_labels,
    LabelGainMap& label_to_output_gain,
    std::vector<ChannelNumbers>& channel_numbers_for_layers) {
  RETURN_IF_NOT_OK(ValidateUnique(audio_substream_ids.begin(),
                                  audio_substream_ids.end(),
                                  "audio_substream_ids"));
  // Starting from no channel at all.
  ChannelNumbers accumulated_channels = {0, 0, 0};
  int substream_index = 0;
  channel_numbers_for_layers.reserve(config.num_layers);
  for (int i = 0; i < config.num_layers; ++i) {
    const int previous_layer_substream_index = substream_index;

    ChannelNumbers layer_channels;
    std::list<ChannelLabel::Label> coupled_substream_labels;
    std::list<ChannelLabel::Label> non_coupled_substream_labels;
    const auto& layer_config = config.channel_audio_layer_configs[i];
    if (layer_config.loudspeaker_layout ==
        ChannelAudioLayerConfig::kLayoutExpanded) {
      RETURN_IF_NOT_OK(
          CollectChannelLayersAndLabelsForExpandedLoudspeakerLayout(
              i, layer_config.expanded_loudspeaker_layout, layer_channels,
              coupled_substream_labels, non_coupled_substream_labels));
    } else {
      RETURN_IF_NOT_OK(CollectChannelLayersAndLabelsForLoudspeakerLayout(
          i, layer_config.loudspeaker_layout, accumulated_channels,
          layer_channels, coupled_substream_labels,
          non_coupled_substream_labels));
    }

    channel_numbers_for_layers.push_back(layer_channels);

    RETURN_IF_NOT_OK(AddSubstreamLabels(
        coupled_substream_labels, non_coupled_substream_labels,
        audio_substream_ids, substream_id_to_labels, substream_index));
    RETURN_IF_NOT_OK(ValidateSubstreamCounts(
        coupled_substream_labels, non_coupled_substream_labels, layer_config));

    accumulated_channels = layer_channels;

    // Handle output gains.
    if (layer_config.output_gain_is_present_flag == 1) {
      // Loop through all substream IDs added in this layer.
      for (int i = previous_layer_substream_index; i < substream_index; i++) {
        const auto substream_id = audio_substream_ids[i];

        LOG(INFO) << "Output gain for substream ID: " << substream_id << ":";
        for (const auto& label : substream_id_to_labels.at(substream_id)) {
          if (OutputGainApplies(layer_config.output_gain_flag, label)) {
            label_to_output_gain[label] = Q7_8ToFloat(layer_config.output_gain);
            LOG(INFO) << "  " << label << ": Q7.8= " << layer_config.output_gain
                      << "; dB= " << label_to_output_gain[label];
          } else {
            LOG(INFO) << "  " << label << ": (not found)";
          }
        }
      }
    }
  }

  // Validate that all substreams were assigned at least one label.
  RETURN_IF_NOT_OK(ValidateEqual(
      audio_substream_ids.size(), substream_id_to_labels.size(),
      "audio_substream_ids.size() vs. substream_id_to_labels.size()"));

  return absl::OkStatus();
}

// TODO(b/340540080): Add tests for this function and remove fragility, for
// example, null pointers, get<> that can fail, etc.
absl::Status AudioElementGenerator::FinalizeAmbisonicsConfig(
    const AudioElementObu& audio_element_obu,
    SubstreamIdLabelsMap& substream_id_to_labels) {
  if (audio_element_obu.GetAudioElementType() !=
      AudioElementObu::AudioElementType::kAudioElementSceneBased) {
    return InvalidArgumentError(
        "Cannot finalize AmbisonicsMonoConfig for a non-scene-based Audio "
        "Element OBU.");
  }
  const auto& ambisonics_config =
      std::get<AmbisonicsConfig>(audio_element_obu.config_);
  switch (ambisonics_config.ambisonics_mode) {
    case AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeMono:
      return FinalizeAmbisonicsMonoConfig(
          audio_element_obu,
          std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config),
          substream_id_to_labels);
    case AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeProjection:
      return FinalizeAmbisonicsProjectionConfig(
          audio_element_obu,
          std::get<AmbisonicsProjectionConfig>(
              ambisonics_config.ambisonics_config),
          substream_id_to_labels);
    default:
      return absl::UnimplementedError(
          StrCat("Unimplemented Ambisonics mode: ",
                 ambisonics_config.ambisonics_mode));
  }
}

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
        return InvalidArgumentError(
            StrCat("Unrecognized audio_element_type= ",
                   audio_element_metadata.audio_element_type()));
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
      return InvalidArgumentError(
          StrCat("Failed to find matching codec_config_id=",
                 audio_element_metadata.codec_config_id()));
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
      return InvalidArgumentError(StrCat(
          "Inserting Audio Element with ID ",
          audio_element_metadata.audio_element_id(),
          " failed because there is a duplicated element with the same ID"));
    }

    switch (new_audio_element_iter->second.obu.GetAudioElementType()) {
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
        return InvalidArgumentError(
            StrCat("Unrecognized audio_element_type= ",
                   new_audio_element_iter->second.obu.GetAudioElementType()));
    }
  }

  LogAudioElements(audio_elements);
  return absl::OkStatus();
}

}  // namespace iamf_tools
