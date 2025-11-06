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
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto_conversion/lookup_tables.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using absl::InvalidArgumentError;
using absl::MakeConstSpan;
using absl::MakeSpan;
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

absl::Status GenerateParameterDefinitions(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu,
    AudioElementObu& audio_element_obu) {
  if (audio_element_metadata.has_num_parameters()) {
    ABSL_LOG(WARNING) << "Ignoring deprecated `num_parameters` field. "
                         "Please remove it.";
  }

  audio_element_obu.InitializeParams(
      audio_element_metadata.audio_element_params_size());
  for (int i = 0; i < audio_element_metadata.audio_element_params_size(); ++i) {
    const auto& user_data_parameter =
        audio_element_metadata.audio_element_params(i);

    ParamDefinition::ParameterDefinitionType copied_param_definition_type;
    RETURN_IF_NOT_OK(CopyAudioElementParamDefinitionType(
        user_data_parameter, copied_param_definition_type));
    switch (copied_param_definition_type) {
      using enum ParamDefinition::ParameterDefinitionType;
      case kParameterDefinitionDemixing: {
        DemixingParamDefinition demixing_param_definition;
        RETURN_IF_NOT_OK(CopyParamDefinition(
            user_data_parameter.demixing_param().param_definition(),
            demixing_param_definition));
        // Copy the `DemixingInfoParameterData` in the IAMF spec.
        RETURN_IF_NOT_OK(CopyDemixingInfoParameterData(
            user_data_parameter.demixing_param()
                .default_demixing_info_parameter_data(),
            demixing_param_definition.default_demixing_info_parameter_data_));
        // Copy the extension portion of `DefaultDemixingInfoParameterData` in
        // the IAMF spec.
        RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
            "DemixingParamDefinition.default_w",
            user_data_parameter.demixing_param().default_w(),
            demixing_param_definition.default_demixing_info_parameter_data_
                .default_w));
        RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
            "DemixingParamDefinition.reserved",
            user_data_parameter.demixing_param().reserved(),
            demixing_param_definition.default_demixing_info_parameter_data_
                .reserved_for_future_use));
        if (demixing_param_definition.duration_ !=
            codec_config_obu.GetCodecConfig().num_samples_per_frame) {
          return InvalidArgumentError(
              StrCat("Demixing parameter duration= ",
                     demixing_param_definition.duration_,
                     " is inconsistent with num_samples_per_frame=",
                     codec_config_obu.GetCodecConfig().num_samples_per_frame));
        }
        audio_element_obu.audio_element_params_.emplace_back(
            AudioElementParam{demixing_param_definition});
        break;
      }
      case kParameterDefinitionReconGain: {
        ReconGainParamDefinition recon_gain_param_definition(
            audio_element_obu.GetAudioElementId());
        RETURN_IF_NOT_OK(CopyParamDefinition(
            user_data_parameter.recon_gain_param().param_definition(),
            recon_gain_param_definition));
        if (recon_gain_param_definition.duration_ !=
            codec_config_obu.GetCodecConfig().num_samples_per_frame) {
          return InvalidArgumentError(
              StrCat("Recon gain parameter duration= ",
                     recon_gain_param_definition.duration_,
                     " is inconsistent with num_samples_per_frame=",
                     codec_config_obu.GetCodecConfig().num_samples_per_frame));
        }
        audio_element_obu.audio_element_params_.emplace_back(
            AudioElementParam{recon_gain_param_definition});
        break;
      }
      case kParameterDefinitionMixGain:
        return InvalidArgumentError(
            "Mix gain parameters are not permitted in audio elements.");
      default: {
        const auto& user_param_definition =
            user_data_parameter.param_definition_extension();
        ExtendedParamDefinition extended_param_definition(
            copied_param_definition_type);
        // Copy the extension bytes.
        if (user_param_definition.has_param_definition_size()) {
          ABSL_LOG(WARNING)
              << "Ignoring deprecated `param_definition_size` field. "
                 "Please remove it.";
        }
        extended_param_definition.param_definition_bytes_.resize(
            user_param_definition.param_definition_bytes().size());
        RETURN_IF_NOT_OK(StaticCastSpanIfInRange(
            "param_definition_bytes",
            absl::MakeConstSpan(user_param_definition.param_definition_bytes()),
            absl::MakeSpan(extended_param_definition.param_definition_bytes_)));

        audio_element_obu.audio_element_params_.emplace_back(
            AudioElementParam{extended_param_definition});
      } break;
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
  for (int i = 0; i < channel_config.GetNumLayers(); i++) {
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
    if (audio_element_param.GetType() ==
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
    // Ignore user input since it would not be in the bitstream as of IAMF
    // v1.1.0.
    output_expanded_loudspeaker_layout = std::nullopt;
  }

  return absl::OkStatus();
}

absl::StatusOr<AudioElementWithData> CreateChannelBasedAudioElementWithData(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu) {
  if (!audio_element_metadata.has_scalable_channel_layout_config()) {
    return InvalidArgumentError(StrCat(
        "Audio Element Metadata [", audio_element_metadata.audio_element_id(),
        " is of type AUDIO_ELEMENT_CHANNEL_BASED but does not have",
        " the `scalable_channel_layout_config` field."));
  }

  const auto& input_config =
      audio_element_metadata.scalable_channel_layout_config();
  if (input_config.has_num_layers()) {
    ABSL_LOG(WARNING)
        << "Ignoring deprecated `num_layers` field. Please remove it.";
  }

  auto config = ScalableChannelLayoutConfig();
  config.channel_audio_layer_configs.reserve(
      input_config.channel_audio_layer_configs_size());
  for (const auto& input_layer_config :
       input_config.channel_audio_layer_configs()) {
    ChannelAudioLayerConfig layer_config;
    RETURN_IF_NOT_OK(CopyLoudspeakerLayoutAndExpandedLoudspeakerLayout(
        input_layer_config, layer_config.loudspeaker_layout,
        layer_config.expanded_loudspeaker_layout));
    RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, bool>(
        "ChannelAudioLayerConfig.output_gain_is_present_flag",
        input_layer_config.output_gain_is_present_flag(),
        layer_config.output_gain_is_present_flag));
    RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, bool>(
        "ChannelAudioLayerConfig.recon_gain_is_present_flag",
        input_layer_config.recon_gain_is_present_flag(),
        layer_config.recon_gain_is_present_flag));
    RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
        "ChannelAudioLayerConfig.reserved_a", input_layer_config.reserved_a(),
        layer_config.reserved_a));
    RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
        "ChannelAudioLayerConfig.substream_count",
        input_layer_config.substream_count(), layer_config.substream_count));
    RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
        "ChannelAudioLayerConfig.coupled_substream_count",
        input_layer_config.coupled_substream_count(),
        layer_config.coupled_substream_count));

    if (layer_config.output_gain_is_present_flag == 1) {
      RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
          "ChannelAudioLayerConfig.output_gain_flag",
          input_layer_config.output_gain_flag(),
          layer_config.output_gain_flag));
      RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
          "ChannelAudioLayerConfig.reserved_b", input_layer_config.reserved_b(),
          layer_config.reserved_b));
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "ChannelAudioLayerConfig.output_gain",
          input_layer_config.output_gain(), layer_config.output_gain));
    }
    config.channel_audio_layer_configs.emplace_back(std::move(layer_config));
  }

  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      GetHeaderFromMetadata(audio_element_metadata.obu_header()),
      audio_element_metadata.audio_element_id(),
      audio_element_metadata.reserved(),
      audio_element_metadata.codec_config_id(),
      MakeConstSpan(audio_element_metadata.audio_substream_ids()), config);
  if (!obu.ok()) {
    return obu.status();
  }
  RETURN_IF_NOT_OK(GenerateParameterDefinitions(audio_element_metadata,
                                                codec_config_obu, *obu));
  RETURN_IF_NOT_OK(ValidateReconGainDefined(codec_config_obu, *obu));

  SubstreamIdLabelsMap substream_id_to_labels;
  LabelGainMap label_to_output_gain;
  std::vector<ChannelNumbers> channel_numbers_for_layers;
  RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeScalableChannelLayoutConfig(
      obu->audio_substream_ids_, config, substream_id_to_labels,
      label_to_output_gain, channel_numbers_for_layers));
  return AudioElementWithData{
      .obu = *std::move(obu),
      .codec_config = &codec_config_obu,
      .substream_id_to_labels = std::move(substream_id_to_labels),
      .label_to_output_gain = std::move(label_to_output_gain),
      .channel_numbers_for_layers = std::move(channel_numbers_for_layers)};
}

// TODO(b/456733934): Simplify `GetAmbisonicsMonoConfig`. Some fields are
//                    not needed, or are checked at OBU creation.
absl::StatusOr<AmbisonicsMonoConfig> GetAmbisonicsMonoConfig(
    DecodedUleb128 audio_element_id_for_debugging,
    DecodedUleb128 num_substreams_in_obu,
    const iamf_tools_cli_proto::AmbisonicsConfig& input_config) {
  if (!input_config.has_ambisonics_mono_config()) {
    return InvalidArgumentError(
        StrCat("Audio Element Metadata [", audio_element_id_for_debugging,
               " is of mode AMBISONICS_MODE_MONO but does not have the "
               "`ambisonics_mono_config` field."));
  }
  const auto& input_mono_config = input_config.ambisonics_mono_config();
  AmbisonicsMonoConfig mono_config;
  if (input_mono_config.channel_mapping_size() !=
      input_mono_config.output_channel_count()) {
    return InvalidArgumentError(StrCat(
        "Audio Element Metadata [", audio_element_id_for_debugging,
        " has output_channel_count= ", input_mono_config.output_channel_count(),
        ", but `channel_mapping` has ",
        input_mono_config.channel_mapping_size(), " elements."));
  }
  // Set output channel count, etc.
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AmbisonicsMonoConfig.output_channel_count",
      input_mono_config.output_channel_count(),
      mono_config.output_channel_count));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AmbisonicsMonoConfig.substream_count",
      input_mono_config.substream_count(), mono_config.substream_count));
  mono_config.channel_mapping.resize(input_mono_config.channel_mapping_size());
  RETURN_IF_NOT_OK(StaticCastSpanIfInRange<uint32_t, uint8_t>(
      "AmbisonicsMonoConfig.channel_mapping",
      input_mono_config.channel_mapping(),
      MakeSpan(mono_config.channel_mapping)));
  // Validate the mono config. This ensures no substream indices should be out
  // of bounds.
  RETURN_IF_NOT_OK(mono_config.Validate(num_substreams_in_obu));
  return mono_config;
}

// TODO(b/456734866): Simplify `GetAmbisonicsProjectionConfig`. Some fields are
//                    not needed, or are checked at OBU creation.
absl::StatusOr<AmbisonicsProjectionConfig> GetAmbisonicsProjectionConfig(
    DecodedUleb128 audio_element_id_for_debugging,
    DecodedUleb128 num_substreams_in_obu,
    const iamf_tools_cli_proto::AmbisonicsConfig& input_config) {
  if (!input_config.has_ambisonics_projection_config()) {
    return InvalidArgumentError(
        StrCat("Audio Element Metadata [", audio_element_id_for_debugging,
               " is of mode AMBISONICS_MODE_PROJECTION but does not have"
               " the `AMBISONICS_MODE_PROJECTION` field."));
  }
  const auto& input_projection_config =
      input_config.ambisonics_projection_config();
  AmbisonicsProjectionConfig projection_config;
  const int expected_demixing_matrix_size =
      (input_projection_config.substream_count() +
       input_projection_config.coupled_substream_count()) *
      input_projection_config.output_channel_count();
  if (input_projection_config.demixing_matrix_size() !=
      expected_demixing_matrix_size) {
    return InvalidArgumentError(
        StrCat("Audio Element Metadata [", audio_element_id_for_debugging,
               " expects demixing_matrix_size= ", expected_demixing_matrix_size,
               ", but `demixing_matrix` has ",
               input_projection_config.demixing_matrix_size(), " elements."));
  }
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AmbisonicsProjectionConfig.output_channel_count",
      input_projection_config.output_channel_count(),
      projection_config.output_channel_count));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AmbisonicsProjectionConfig.substream_count",
      input_projection_config.substream_count(),
      projection_config.substream_count));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AmbisonicsProjectionConfig.coupled_substream_count",
      input_projection_config.coupled_substream_count(),
      projection_config.coupled_substream_count));
  projection_config.demixing_matrix.resize(expected_demixing_matrix_size);

  RETURN_IF_NOT_OK(StaticCastSpanIfInRange<int32_t, int16_t>(
      "AmbisonicsProjectionConfig.demixing_matrix",
      input_projection_config.demixing_matrix(),
      MakeSpan(projection_config.demixing_matrix)));
  RETURN_IF_NOT_OK(projection_config.Validate(num_substreams_in_obu));
  return projection_config;
}

absl::StatusOr<AudioElementWithData> CreateAmbisonicsMonoAudioElementWithData(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu) {
  const auto& input_config = audio_element_metadata.ambisonics_config();
  auto mono_config = GetAmbisonicsMonoConfig(
      audio_element_metadata.audio_element_id(),
      audio_element_metadata.audio_substream_ids_size(), input_config);
  if (!mono_config.ok()) {
    return mono_config.status();
  }

  absl::StatusOr<AudioElementObu> obu =
      AudioElementObu::CreateForMonoAmbisonics(
          GetHeaderFromMetadata(audio_element_metadata.obu_header()),
          audio_element_metadata.audio_element_id(),
          audio_element_metadata.reserved(),
          audio_element_metadata.codec_config_id(),
          audio_element_metadata.audio_substream_ids(),
          mono_config->channel_mapping);

  if (!obu.ok()) {
    return obu.status();
  }
  RETURN_IF_NOT_OK(GenerateParameterDefinitions(audio_element_metadata,
                                                codec_config_obu, *obu));

  SubstreamIdLabelsMap substream_id_to_labels;
  RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeAmbisonicsConfig(
      *obu, substream_id_to_labels));
  return AudioElementWithData{
      .obu = *std::move(obu),
      .codec_config = &codec_config_obu,
      .substream_id_to_labels = std::move(substream_id_to_labels)};
}

absl::StatusOr<AudioElementWithData>
CreateAmbisonicsProjectionAudioElementWithData(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu) {
  auto projection_config = GetAmbisonicsProjectionConfig(
      audio_element_metadata.audio_element_id(),
      audio_element_metadata.audio_substream_ids_size(),
      audio_element_metadata.ambisonics_config());
  if (!projection_config.ok()) {
    return projection_config.status();
  }
  absl::StatusOr<AudioElementObu> obu =
      AudioElementObu::CreateForProjectionAmbisonics(
          GetHeaderFromMetadata(audio_element_metadata.obu_header()),
          audio_element_metadata.audio_element_id(),
          audio_element_metadata.reserved(),
          audio_element_metadata.codec_config_id(),
          audio_element_metadata.audio_substream_ids(),
          projection_config->output_channel_count,
          projection_config->coupled_substream_count,
          projection_config->demixing_matrix);
  if (!obu.ok()) {
    return obu.status();
  }

  RETURN_IF_NOT_OK(GenerateParameterDefinitions(audio_element_metadata,
                                                codec_config_obu, *obu));

  SubstreamIdLabelsMap substream_id_to_labels;
  RETURN_IF_NOT_OK(ObuWithDataGenerator::FinalizeAmbisonicsConfig(
      *obu, substream_id_to_labels));
  return AudioElementWithData{
      .obu = *std::move(obu),
      .codec_config = &codec_config_obu,
      .substream_id_to_labels = std::move(substream_id_to_labels)};
}

absl::StatusOr<AudioElementWithData> CreateAudioElementWithData(
    const iamf_tools_cli_proto::AudioElementObuMetadata& audio_element_metadata,
    const CodecConfigObu& codec_config_obu) {
  // Dispatch to specific functions, depending on the type (or sub-type) of the
  // Audio Element OBU.
  switch (audio_element_metadata.audio_element_type()) {
    using enum iamf_tools_cli_proto::AudioElementType;
    case AUDIO_ELEMENT_CHANNEL_BASED:
      return CreateChannelBasedAudioElementWithData(audio_element_metadata,
                                                    codec_config_obu);
    case AUDIO_ELEMENT_SCENE_BASED: {
      const std::string error_prefix = StrCat(
          "Audio Element Metadata [", audio_element_metadata.audio_element_id(),
          "] is type ", audio_element_metadata.audio_element_type(), " but ");
      if (!audio_element_metadata.has_ambisonics_config()) {
        return InvalidArgumentError(
            StrCat(error_prefix, "has missing `ambisonics_config` field."));
      }
      // Dispatch between the different sub-types of Ambisonics.
      switch (audio_element_metadata.ambisonics_config().ambisonics_mode()) {
        using enum iamf_tools_cli_proto::AmbisonicsMode;
        case AMBISONICS_MODE_MONO:
          return CreateAmbisonicsMonoAudioElementWithData(
              audio_element_metadata, codec_config_obu);
        case AMBISONICS_MODE_PROJECTION:
          return CreateAmbisonicsProjectionAudioElementWithData(
              audio_element_metadata, codec_config_obu);
        default:
          return InvalidArgumentError(
              StrCat(error_prefix, "has unknown `ambisonics_mode`"));
      }
    }
    default:
      return InvalidArgumentError(
          StrCat("Unrecognized audio_element_type: ",
                 audio_element_metadata.audio_element_type()));
  }
}

void LogAudioElement(const AudioElementWithData& audio_element) {
  // Examine Audio Element OBU.
  audio_element.obu.PrintObu();

  // Log `substream_id_to_labels` separately.
  for (const auto& [substream_id, labels] :
       audio_element.substream_id_to_labels) {
    ABSL_VLOG(1) << "Substream ID: " << substream_id;
    ABSL_VLOG(1) << "  num_channels= " << labels.size();
  }
}

}  // namespace

absl::Status AudioElementGenerator::Generate(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_configs,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  for (const auto& audio_element_metadata : audio_element_metadata_) {
    // Ensure the ID is unique before generating data for it.
    if (audio_elements.contains(audio_element_metadata.audio_element_id())) {
      return InvalidArgumentError(StrCat(
          "Inserting Audio Element with ID ",
          audio_element_metadata.audio_element_id(),
          " failed because there is a duplicated element with the same ID"));
    }

    const auto codec_config_iter =
        codec_configs.find(audio_element_metadata.codec_config_id());
    if (codec_config_iter == codec_configs.end()) {
      return InvalidArgumentError(
          StrCat("Failed to find matching codec_config_id=",
                 audio_element_metadata.codec_config_id()));
    }
    if (audio_element_metadata.has_num_substreams()) {
      ABSL_LOG(WARNING) << "Ignoring deprecated `num_substreams` field. "
                           "Please remove it.";
    }

    absl::StatusOr<AudioElementWithData> audio_element_with_data =
        CreateAudioElementWithData(audio_element_metadata,
                                   codec_config_iter->second);
    if (!audio_element_with_data.ok()) {
      return audio_element_with_data.status();
    }
    LogAudioElement(*audio_element_with_data);
    audio_elements.emplace(audio_element_metadata.audio_element_id(),
                           *std::move(audio_element_with_data));
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
