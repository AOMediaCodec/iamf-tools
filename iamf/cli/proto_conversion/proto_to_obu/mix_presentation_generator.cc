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
#include "iamf/cli/proto_conversion/proto_to_obu/mix_presentation_generator.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/proto/element_gain_offset_config.pb.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/types.pb.h"
#include "iamf/cli/proto_conversion/lookup_tables.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/element_gain_offset_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

absl::Status FillAnnotationsLanguageAndAnnotations(
    const iamf_tools_cli_proto::MixPresentationObuMetadata&
        mix_presentation_metadata,
    DecodedUleb128& count_label, std::vector<std::string>& annotations_language,
    std::vector<std::string>& localized_presentation_annotations) {
  if (mix_presentation_metadata.has_count_label()) {
    ABSL_LOG(WARNING) << "Ignoring deprecated `count_label` field."
                      << "Please remove it.";
  }

  // IAMF v1.1.0 renamed this from `language_labels`.
  annotations_language = {
      mix_presentation_metadata.annotations_language().begin(),
      mix_presentation_metadata.annotations_language().end()};

  count_label = annotations_language.size();

  // IAMF v1.1.0 renamed this from `mix_presentation_annotations_array`.
  localized_presentation_annotations = {
      mix_presentation_metadata.localized_presentation_annotations().begin(),
      mix_presentation_metadata.localized_presentation_annotations().end()};
  return ValidateContainerSizeEqual("localized_presentation_annotations",
                                    localized_presentation_annotations,
                                    count_label);
}

void ReserveNumSubMixes(const iamf_tools_cli_proto::MixPresentationObuMetadata&
                            mix_presentation_metadata,
                        std::vector<MixPresentationSubMix>& sub_mixes) {
  if (mix_presentation_metadata.has_num_sub_mixes()) {
    ABSL_LOG(WARNING) << "Ignoring deprecated `num_sub_mixes` field."
                      << "Please remove it.";
  }

  sub_mixes.reserve(mix_presentation_metadata.sub_mixes_size());
}

void ReserveSubMixNumAudioElements(
    const iamf_tools_cli_proto::MixPresentationSubMix& input_sub_mix,
    MixPresentationSubMix& sub_mix) {
  if (input_sub_mix.has_num_audio_elements()) {
    ABSL_LOG(WARNING) << "Ignoring deprecated `num_audio_elements` field."
                      << "Please remove it.";
  }
  sub_mix.audio_elements.reserve(input_sub_mix.audio_elements_size());
}

PolarParamDefinition CreatePolarParamDefinition(
    const iamf_tools_cli_proto::PolarParamDefinition& input_param_definition) {
  PolarParamDefinition param_definition;
  param_definition.parameter_id_ =
      input_param_definition.param_definition().parameter_id();
  param_definition.parameter_rate_ =
      input_param_definition.param_definition().parameter_rate();
  param_definition.param_definition_mode_ =
      input_param_definition.param_definition().param_definition_mode();
  param_definition.duration_ =
      input_param_definition.param_definition().duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.param_definition().constant_subblock_duration();
  param_definition.default_azimuth_ = input_param_definition.default_azimuth();
  param_definition.default_elevation_ =
      input_param_definition.default_elevation();
  param_definition.default_distance_ =
      input_param_definition.default_distance();
  return param_definition;
}

Cart8ParamDefinition CreateCart8ParamDefinition(
    const iamf_tools_cli_proto::Cart8ParamDefinition& input_param_definition) {
  Cart8ParamDefinition param_definition;
  param_definition.parameter_id_ =
      input_param_definition.param_definition().parameter_id();
  param_definition.parameter_rate_ =
      input_param_definition.param_definition().parameter_rate();
  param_definition.param_definition_mode_ =
      input_param_definition.param_definition().param_definition_mode();
  param_definition.duration_ =
      input_param_definition.param_definition().duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.param_definition().constant_subblock_duration();
  param_definition.default_x_ = input_param_definition.default_x();
  param_definition.default_y_ = input_param_definition.default_y();
  param_definition.default_z_ = input_param_definition.default_z();
  return param_definition;
}

Cart16ParamDefinition CreateCart16ParamDefinition(
    const iamf_tools_cli_proto::Cart16ParamDefinition& input_param_definition) {
  Cart16ParamDefinition param_definition;
  param_definition.parameter_id_ =
      input_param_definition.param_definition().parameter_id();
  param_definition.parameter_rate_ =
      input_param_definition.param_definition().parameter_rate();
  param_definition.param_definition_mode_ =
      input_param_definition.param_definition().param_definition_mode();
  param_definition.duration_ =
      input_param_definition.param_definition().duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.param_definition().constant_subblock_duration();
  param_definition.default_x_ = input_param_definition.default_x();
  param_definition.default_y_ = input_param_definition.default_y();
  param_definition.default_z_ = input_param_definition.default_z();
  return param_definition;
}

DualPolarParamDefinition CreateDualPolarParamDefinition(
    const iamf_tools_cli_proto::DualPolarParamDefinition&
        input_param_definition) {
  DualPolarParamDefinition param_definition;
  param_definition.parameter_id_ =
      input_param_definition.param_definition().parameter_id();
  param_definition.parameter_rate_ =
      input_param_definition.param_definition().parameter_rate();
  param_definition.param_definition_mode_ =
      input_param_definition.param_definition().param_definition_mode();
  param_definition.duration_ =
      input_param_definition.param_definition().duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.param_definition().constant_subblock_duration();
  param_definition.default_first_azimuth_ =
      input_param_definition.default_first_azimuth();
  param_definition.default_first_elevation_ =
      input_param_definition.default_first_elevation();
  param_definition.default_first_distance_ =
      input_param_definition.default_first_distance();
  param_definition.default_second_azimuth_ =
      input_param_definition.default_second_azimuth();
  param_definition.default_second_elevation_ =
      input_param_definition.default_second_elevation();
  param_definition.default_second_distance_ =
      input_param_definition.default_second_distance();
  return param_definition;
}

DualCart8ParamDefinition CreateDualCart8ParamDefinition(
    const iamf_tools_cli_proto::DualCart8ParamDefinition&
        input_param_definition) {
  DualCart8ParamDefinition param_definition;
  param_definition.parameter_id_ =
      input_param_definition.param_definition().parameter_id();
  param_definition.parameter_rate_ =
      input_param_definition.param_definition().parameter_rate();
  param_definition.param_definition_mode_ =
      input_param_definition.param_definition().param_definition_mode();
  param_definition.duration_ =
      input_param_definition.param_definition().duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.param_definition().constant_subblock_duration();
  param_definition.default_first_x_ = input_param_definition.default_first_x();
  param_definition.default_first_y_ = input_param_definition.default_first_y();
  param_definition.default_first_z_ = input_param_definition.default_first_z();
  param_definition.default_second_x_ =
      input_param_definition.default_second_x();
  param_definition.default_second_y_ =
      input_param_definition.default_second_y();
  param_definition.default_second_z_ =
      input_param_definition.default_second_z();
  return param_definition;
}

DualCart16ParamDefinition CreateDualCart16ParamDefinition(
    const iamf_tools_cli_proto::DualCart16ParamDefinition&
        input_param_definition) {
  DualCart16ParamDefinition param_definition;
  param_definition.parameter_id_ =
      input_param_definition.param_definition().parameter_id();
  param_definition.parameter_rate_ =
      input_param_definition.param_definition().parameter_rate();
  param_definition.param_definition_mode_ =
      input_param_definition.param_definition().param_definition_mode();
  param_definition.duration_ =
      input_param_definition.param_definition().duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.param_definition().constant_subblock_duration();
  param_definition.default_first_x_ = input_param_definition.default_first_x();
  param_definition.default_first_y_ = input_param_definition.default_first_y();
  param_definition.default_first_z_ = input_param_definition.default_first_z();
  param_definition.default_second_x_ =
      input_param_definition.default_second_x();
  param_definition.default_second_y_ =
      input_param_definition.default_second_y();
  param_definition.default_second_z_ =
      input_param_definition.default_second_z();
  return param_definition;
}

absl::StatusOr<RenderingConfigParamDefinition>
CreateRenderingConfigParamDefinition(
    const iamf_tools_cli_proto::RenderingConfigParamDefinition&
        input_rendering_config_param_definition) {
  switch (input_rendering_config_param_definition.param_definition_type()) {
    using enum iamf_tools_cli_proto::ParamDefinitionType;
    case PARAM_DEFINITION_TYPE_POLAR:
      return RenderingConfigParamDefinition::Create(
          CreatePolarParamDefinition(
              input_rendering_config_param_definition.polar_param_definition()),
          /*param_definition_bytes=*/{});
    case PARAM_DEFINITION_TYPE_CART_8:
      return RenderingConfigParamDefinition::Create(
          CreateCart8ParamDefinition(
              input_rendering_config_param_definition.cart8_param_definition()),
          /*param_definition_bytes=*/{});
    case PARAM_DEFINITION_TYPE_CART_16:
      return RenderingConfigParamDefinition::Create(
          CreateCart16ParamDefinition(input_rendering_config_param_definition
                                          .cart16_param_definition()),
          /*param_definition_bytes=*/{});
    case PARAM_DEFINITION_TYPE_DUAL_POLAR:
      return RenderingConfigParamDefinition::Create(
          CreateDualPolarParamDefinition(input_rendering_config_param_definition
                                             .dual_polar_param_definition()),
          /*param_definition_bytes=*/{});
    case PARAM_DEFINITION_TYPE_DUAL_CART_8:
      return RenderingConfigParamDefinition::Create(
          CreateDualCart8ParamDefinition(input_rendering_config_param_definition
                                             .dual_cart8_param_definition()),
          /*param_definition_bytes=*/{});
    case PARAM_DEFINITION_TYPE_DUAL_CART_16:
      return RenderingConfigParamDefinition::Create(
          CreateDualCart16ParamDefinition(
              input_rendering_config_param_definition
                  .dual_cart16_param_definition()),
          /*param_definition_bytes=*/{});
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown param_definition_type= ",
          input_rendering_config_param_definition.param_definition_type()));
  }
}

absl::StatusOr<ElementGainOffsetConfig> CreateElementGainOffsetConfig(
    const iamf_tools_cli_proto::ElementGainOffsetConfig&
        input_element_gain_offset_config) {
  if (input_element_gain_offset_config.has_value_type()) {
    const auto element_gain_offset = ProtoToQFormatOrFloatingPoint(
        input_element_gain_offset_config.value_type().element_gain_offset());
    if (!element_gain_offset.ok()) {
      return element_gain_offset.status();
    }
    return ElementGainOffsetConfig::MakeValueType(*element_gain_offset);
  } else if (input_element_gain_offset_config.has_range_type()) {
    const auto default_element_gain_offset = ProtoToQFormatOrFloatingPoint(
        input_element_gain_offset_config.range_type()
            .default_element_gain_offset());
    if (!default_element_gain_offset.ok()) {
      return default_element_gain_offset.status();
    }
    const auto min_element_gain_offset = ProtoToQFormatOrFloatingPoint(
        input_element_gain_offset_config.range_type()
            .min_element_gain_offset());
    if (!min_element_gain_offset.ok()) {
      return min_element_gain_offset.status();
    }
    const auto max_element_gain_offset = ProtoToQFormatOrFloatingPoint(
        input_element_gain_offset_config.range_type()
            .max_element_gain_offset());
    if (!max_element_gain_offset.ok()) {
      return max_element_gain_offset.status();
    }

    return ElementGainOffsetConfig::CreateRangeType(
        *default_element_gain_offset, *min_element_gain_offset,
        *max_element_gain_offset);
  } else {
    return absl::InvalidArgumentError(
        "ElementGainOffsetConfig must have one of value_type or "
        "range_type set.");
  }
}

absl::Status FillRenderingConfig(
    const iamf_tools_cli_proto::RenderingConfig& input_rendering_config,
    RenderingConfig& rendering_config) {
  switch (input_rendering_config.headphones_rendering_mode()) {
    using enum iamf_tools_cli_proto::HeadPhonesRenderingMode;
    using enum RenderingConfig::HeadphonesRenderingMode;
    case HEADPHONES_RENDERING_MODE_STEREO:
      rendering_config.headphones_rendering_mode =
          kHeadphonesRenderingModeStereo;
      break;
    case HEADPHONES_RENDERING_MODE_BINAURAL_WORLD_LOCKED:
      rendering_config.headphones_rendering_mode =
          kHeadphonesRenderingModeBinauralWorldLocked;
      break;
    case HEADPHONES_RENDERING_MODE_BINAURAL_HEAD_LOCKED:
      rendering_config.headphones_rendering_mode =
          kHeadphonesRenderingModeBinauralHeadLocked;
      break;
    case HEADPHONES_RENDERING_MODE_RESERVED_3:
      rendering_config.headphones_rendering_mode =
          kHeadphonesRenderingModeReserved3;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown headphones_rendering_mode= ",
                       input_rendering_config.headphones_rendering_mode()));
  }

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "RenderingConfig.reserved", input_rendering_config.reserved(),
      rendering_config.reserved));

  if (input_rendering_config.has_rendering_config_extension_size()) {
    ABSL_LOG(WARNING)
        << "Ignoring deprecated `rendering_config_extension_size` "
           "field. Please remove it.";
  }

  for (const auto& input_rendering_config_param_definition :
       input_rendering_config.rendering_config_param_definitions()) {
    auto rendering_config_param_definition =
        CreateRenderingConfigParamDefinition(
            input_rendering_config_param_definition);
    if (!rendering_config_param_definition.ok()) {
      return rendering_config_param_definition.status();
    }
    rendering_config.rendering_config_param_definitions.push_back(
        *rendering_config_param_definition);
  }

  if (input_rendering_config.has_element_gain_offset_config()) {
    auto element_gain_offset_config = CreateElementGainOffsetConfig(
        input_rendering_config.element_gain_offset_config());
    if (!element_gain_offset_config.ok()) {
      return element_gain_offset_config.status();
    }
    rendering_config.element_gain_offset_config = *element_gain_offset_config;
  }

  rendering_config.rendering_config_extension_bytes.resize(
      input_rendering_config.rendering_config_extension_bytes().size());
  return StaticCastSpanIfInRange(
      "rendering_config_extension_bytes",
      absl::MakeConstSpan(
          input_rendering_config.rendering_config_extension_bytes()),
      absl::MakeSpan(rendering_config.rendering_config_extension_bytes));
}

absl::Status FillMixConfig(
    const iamf_tools_cli_proto::MixGainParamDefinition& input_mix_gain,
    MixGainParamDefinition& mix_gain) {
  RETURN_IF_NOT_OK(
      CopyParamDefinition(input_mix_gain.param_definition(), mix_gain));
  int16_t default_mix_gain_q78;
  RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
      "MixGainParamDefinition.default_mix_gain",
      input_mix_gain.default_mix_gain(), default_mix_gain_q78));
  mix_gain.default_mix_gain_ =
      QFormatOrFloatingPoint::MakeFromQ7_8(default_mix_gain_q78);

  return absl::OkStatus();
}

absl::Status CopyReservedOrBinauralLayout(
    Layout::LayoutType layout,
    const ::iamf_tools_cli_proto::LoudspeakersReservedOrBinauralLayout&
        reserved_or_binaural_layout,
    Layout& obu_layout) {
  obu_layout.layout_type = layout;
  LoudspeakersReservedOrBinauralLayout obu_reserved_or_binaural_layout;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "LoudspeakersReservedOrBinauralLayout.reserved",
      reserved_or_binaural_layout.reserved(),
      obu_reserved_or_binaural_layout.reserved));

  obu_layout.specific_layout = obu_reserved_or_binaural_layout;
  return absl::OkStatus();
}

absl::Status FillLayouts(
    const iamf_tools_cli_proto::MixPresentationSubMix& input_sub_mix,
    MixPresentationSubMix& sub_mix) {
  if (input_sub_mix.has_num_layouts()) {
    ABSL_LOG(WARNING) << "Ignoring deprecated `num_layouts` field."
                      << "Please remove it.";
  }

  // Reserve the layouts vector and copy in the layouts.
  sub_mix.layouts.reserve(input_sub_mix.layouts_size());

  for (const auto& input_layout : input_sub_mix.layouts()) {
    const auto& input_loudness_layout = input_layout.loudness_layout();
    MixPresentationLayout layout;

    switch (input_loudness_layout.layout_type()) {
      using enum iamf_tools_cli_proto::LayoutType;
      using enum Layout::LayoutType;
      case LAYOUT_TYPE_RESERVED_0:
        RETURN_IF_NOT_OK(CopyReservedOrBinauralLayout(
            kLayoutTypeReserved0,
            input_loudness_layout.reserved_or_binaural_layout(),
            layout.loudness_layout));
        break;
      case LAYOUT_TYPE_RESERVED_1:
        RETURN_IF_NOT_OK(CopyReservedOrBinauralLayout(
            kLayoutTypeReserved1,
            input_loudness_layout.reserved_or_binaural_layout(),
            layout.loudness_layout));
        break;
      case LAYOUT_TYPE_BINAURAL:
        RETURN_IF_NOT_OK(CopyReservedOrBinauralLayout(
            kLayoutTypeBinaural,
            input_loudness_layout.reserved_or_binaural_layout(),
            layout.loudness_layout));
        break;
      case LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION: {
        layout.loudness_layout.layout_type =
            kLayoutTypeLoudspeakersSsConvention;
        LoudspeakersSsConventionLayout obu_ss_layout;
        RETURN_IF_NOT_OK(MixPresentationGenerator::CopySoundSystem(
            input_loudness_layout.ss_layout().sound_system(),
            obu_ss_layout.sound_system));
        RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
            "LoudspeakersSsConventionLayout.reserved",
            input_loudness_layout.ss_layout().reserved(),
            obu_ss_layout.reserved));
        layout.loudness_layout.specific_layout = obu_ss_layout;
        break;
      }
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "Unknown layout_type= ", input_loudness_layout.layout_type()));
    }

    RETURN_IF_NOT_OK(MixPresentationGenerator::CopyInfoType(
        input_layout.loudness(), layout.loudness.info_type));

    RETURN_IF_NOT_OK(
        MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
            input_layout.loudness(), layout.loudness));
    RETURN_IF_NOT_OK(MixPresentationGenerator::CopyUserAnchoredLoudness(
        input_layout.loudness(), layout.loudness));
    RETURN_IF_NOT_OK(MixPresentationGenerator::CopyUserLayoutExtension(
        input_layout.loudness(), layout.loudness));

    sub_mix.layouts.push_back(layout);
  }

  return absl::OkStatus();
}

absl::Status FillMixPresentationTags(
    bool append_build_information_tag,
    const iamf_tools_cli_proto::MixPresentationTags& mix_presentation_tags,
    std::optional<MixPresentationTags>& obu_mix_presentation_tags) {
  if (mix_presentation_tags.has_num_tags()) {
    ABSL_LOG(WARNING)
        << "Ignoring deprecated `num_tags` field. Please remove it.";
  }
  obu_mix_presentation_tags = MixPresentationTags{};

  // Calculate the total number of tags, including automatically added ones.
  const size_t num_tags = mix_presentation_tags.tags().size() +
                          (append_build_information_tag ? 1 : 0);
  // At the OBU it must fit into a `uint8_t`.
  uint8_t obu_num_tags;
  RETURN_IF_NOT_OK(StaticCastIfInRange<size_t, uint8_t>(
      "Total number of MixPresentationTags.tags", num_tags, obu_num_tags));
  obu_mix_presentation_tags->tags.reserve(obu_num_tags);
  for (const auto& input_tag : mix_presentation_tags.tags()) {
    obu_mix_presentation_tags->tags.emplace_back(MixPresentationTags::Tag{
        .tag_name = input_tag.tag_name(),
        .tag_value = input_tag.tag_value(),
    });
  }
  // Append the build information tag.
  if (append_build_information_tag) {
    // TODO(b/388577499): Include the commit hash at build time, in the
    //                    `iamf_encoder` tag value.
    obu_mix_presentation_tags->tags.emplace_back(MixPresentationTags::Tag{
        .tag_name = "iamf_encoder",
        .tag_value = "GitHub/iamf-tools",
    });
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status MixPresentationGenerator::CopySoundSystem(
    iamf_tools_cli_proto::SoundSystem input_sound_system,
    LoudspeakersSsConventionLayout::SoundSystem& output_sound_system) {
  static const auto kProtoToInternalSoundSystem =
      BuildStaticMapFromPairs(LookupTables::kProtoAndInternalSoundSystems);

  return CopyFromMap(
      *kProtoToInternalSoundSystem, input_sound_system,
      "Internal version of proto `SoundSystem`= ", output_sound_system);
}

absl::Status MixPresentationGenerator::CopyInfoType(
    const iamf_tools_cli_proto::LoudnessInfo& input_loudness_info,
    uint8_t& loudness_info_type) {
  if (input_loudness_info.has_deprecated_info_type()) {
    return absl::InvalidArgumentError(
        "Please upgrade the `deprecated_info_type` "
        "field to the new `info_type_bit_masks` field."
        "\nSuggested upgrades:\n"
        "- `deprecated_info_type: 0` -> `info_type_bit_masks: []`\n"
        "- `deprecated_info_type: 1` -> `info_type_bit_masks: "
        "[LOUDNESS_INFO_TYPE_TRUE_PEAK]`\n"
        "- `deprecated_info_type: 2` -> `info_type_bit_masks: "
        "[LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS]`\n"
        "- `deprecated_info_type: 3` -> `info_type_bit_masks: "
        "[LOUDNESS_INFO_TYPE_TRUE_PEAK, "
        "LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS]`\n");
  }

  static const auto kProtoToInternalInfoTypeBitmask =
      BuildStaticMapFromPairs(LookupTables::kProtoAndInternalInfoTypeBitmasks);

  uint8_t accumulated_info_type_bitmask = 0;
  for (int i = 0; i < input_loudness_info.info_type_bit_masks_size(); ++i) {
    LoudnessInfo::InfoTypeBitmask user_output_bit_mask;
    RETURN_IF_NOT_OK(CopyFromMap(
        *kProtoToInternalInfoTypeBitmask,
        input_loudness_info.info_type_bit_masks(i),
        absl::StrCat("Internal version of proto `LoudnessInfoTypeBitMask(", i,
                     ")= "),
        user_output_bit_mask));

    // Track the accumulated bit mask.
    accumulated_info_type_bitmask |= static_cast<uint8_t>(user_output_bit_mask);
  }

  loudness_info_type = accumulated_info_type_bitmask;
  return absl::OkStatus();
}

absl::Status MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
    const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
    LoudnessInfo& output_loudness) {
  RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
      "LoudnessInfo.integrated_loudness", user_loudness.integrated_loudness(),
      output_loudness.integrated_loudness));
  RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
      "LoudnessInfo.digital_peak", user_loudness.digital_peak(),
      output_loudness.digital_peak));

  if ((output_loudness.info_type & LoudnessInfo::kTruePeak) != 0) {
    RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
        "LoudnessInfo.true_peak", user_loudness.true_peak(),
        output_loudness.true_peak));
  }

  return absl::OkStatus();
}

absl::Status MixPresentationGenerator::CopyUserAnchoredLoudness(
    const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
    LoudnessInfo& output_loudness) {
  if ((output_loudness.info_type & LoudnessInfo::kAnchoredLoudness) == 0) {
    // Not using anchored loudness.
    return absl::OkStatus();
  }
  if (user_loudness.anchored_loudness().has_num_anchored_loudness()) {
    ABSL_LOG(WARNING)
        << "Ignoring deprecated `num_anchored_loudness` field. Please "
           "remove it.";
  }

  uint8_t num_anchored_loudness;
  RETURN_IF_NOT_OK(StaticCastIfInRange<size_t, uint8_t>(
      "Number of LoudnessInfo.anchored_loudness",
      user_loudness.anchored_loudness().anchor_elements_size(),
      num_anchored_loudness));

  for (const auto& metadata_anchor_element :
       user_loudness.anchored_loudness().anchor_elements()) {
    AnchoredLoudnessElement::AnchorElement obu_anchor_element;
    switch (metadata_anchor_element.anchor_element()) {
      using enum iamf_tools_cli_proto::AnchorType;
      using enum AnchoredLoudnessElement::AnchorElement;
      case ANCHOR_TYPE_UNKNOWN:
        obu_anchor_element = kAnchorElementUnknown;
        break;
      case ANCHOR_TYPE_DIALOGUE:
        obu_anchor_element = kAnchorElementDialogue;
        break;
      case ANCHOR_TYPE_ALBUM:
        obu_anchor_element = kAnchorElementAlbum;
        break;
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown anchor_element= ",
                         metadata_anchor_element.anchor_element()));
    }

    int16_t obu_anchored_loudness;
    RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
        "AnchorElement.anchored_loudness",
        metadata_anchor_element.anchored_loudness(), obu_anchored_loudness));
    output_loudness.anchored_loudness.anchor_elements.push_back(
        {obu_anchor_element, obu_anchored_loudness});
  }

  return absl::OkStatus();
}

absl::Status MixPresentationGenerator::CopyUserLayoutExtension(
    const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
    LoudnessInfo& output_loudness) {
  if ((output_loudness.info_type & LoudnessInfo::kAnyLayoutExtension) == 0) {
    // Not using layout extension.
    return absl::OkStatus();
  }
  if (user_loudness.has_info_type_size()) {
    ABSL_LOG(WARNING) << "Ignoring deprecated `info_type_size` field."
                      << "Please remove it.";
  }

  output_loudness.layout_extension.info_type_bytes.resize(
      user_loudness.info_type_bytes().size());
  return StaticCastSpanIfInRange(
      "layout_extension_bytes",
      absl::MakeConstSpan(user_loudness.info_type_bytes()),
      absl::MakeSpan(output_loudness.layout_extension.info_type_bytes));
}

absl::Status MixPresentationGenerator::Generate(
    bool append_build_information_tag,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  for (const auto& mix_presentation_metadata : mix_presentation_metadata_) {
    struct {
      DecodedUleb128 mix_presentation_id;
      DecodedUleb128 count_label;
      std::vector<std::string> annotations_language;
      // Length `count_label`.
      std::vector<std::string> localized_presentation_annotations;

      // Length `num_sub_mixes`.
      std::vector<MixPresentationSubMix> sub_mixes;

      std::optional<MixPresentationTags> mix_presentation_tags;
    } obu_args;

    obu_args.mix_presentation_id =
        mix_presentation_metadata.mix_presentation_id();

    RETURN_IF_NOT_OK(FillAnnotationsLanguageAndAnnotations(
        mix_presentation_metadata, obu_args.count_label,
        obu_args.annotations_language,
        obu_args.localized_presentation_annotations));

    ReserveNumSubMixes(mix_presentation_metadata, obu_args.sub_mixes);
    for (const auto& input_sub_mix : mix_presentation_metadata.sub_mixes()) {
      MixPresentationSubMix sub_mix;

      ReserveSubMixNumAudioElements(input_sub_mix, sub_mix);
      for (const auto& input_sub_mix_audio_element :
           input_sub_mix.audio_elements()) {
        SubMixAudioElement sub_mix_audio_element;
        sub_mix_audio_element.audio_element_id =
            input_sub_mix_audio_element.audio_element_id();

        // IAMF v1.1.0 renamed this from
        // `mix_presentation_element_annotations_array`.
        RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
            "localized_element_annotations",
            input_sub_mix_audio_element.localized_element_annotations(),
            obu_args.count_label));
        sub_mix_audio_element.localized_element_annotations = {
            input_sub_mix_audio_element.localized_element_annotations().begin(),
            input_sub_mix_audio_element.localized_element_annotations().end()};

        RETURN_IF_NOT_OK(
            FillRenderingConfig(input_sub_mix_audio_element.rendering_config(),
                                sub_mix_audio_element.rendering_config));

        RETURN_IF_NOT_OK(
            FillMixConfig(input_sub_mix_audio_element.element_mix_gain(),
                          sub_mix_audio_element.element_mix_gain));
        sub_mix.audio_elements.push_back(sub_mix_audio_element);
      }

      RETURN_IF_NOT_OK(FillMixConfig(input_sub_mix.output_mix_gain(),
                                     sub_mix.output_mix_gain));

      RETURN_IF_NOT_OK(FillLayouts(input_sub_mix, sub_mix));
      obu_args.sub_mixes.push_back(std::move(sub_mix));
    }
    if (mix_presentation_metadata.include_mix_presentation_tags() ||
        append_build_information_tag) {
      RETURN_IF_NOT_OK(FillMixPresentationTags(
          append_build_information_tag,
          mix_presentation_metadata.mix_presentation_tags(),
          obu_args.mix_presentation_tags));
    } else {
      obu_args.mix_presentation_tags = std::nullopt;
    }

    MixPresentationObu obu(
        GetHeaderFromMetadata(mix_presentation_metadata.obu_header()),
        obu_args.mix_presentation_id, obu_args.count_label,
        obu_args.annotations_language,
        obu_args.localized_presentation_annotations, obu_args.sub_mixes);
    obu.mix_presentation_tags_ = obu_args.mix_presentation_tags;
    mix_presentation_obus.emplace_back(std::move(obu));
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
