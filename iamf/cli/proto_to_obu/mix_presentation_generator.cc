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
#include "iamf/cli/proto_to_obu/mix_presentation_generator.h"

#include <cstdint>
#include <list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/lookup_tables.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

void FillAnnotationsLanguageAndAnnotations(
    const iamf_tools_cli_proto::MixPresentationObuMetadata&
        mix_presentation_metadata,
    DecodedUleb128& count_label, std::vector<std::string>& annotations_language,
    std::vector<std::string>& localized_presentation_annotations) {
  count_label = mix_presentation_metadata.count_label();

  annotations_language.reserve(mix_presentation_metadata.count_label());
  // Prioritize the `annotations_language` field from IAMF v1.1.0.
  if (!mix_presentation_metadata.annotations_language().empty()) {
    for (const auto& language :
         mix_presentation_metadata.annotations_language()) {
      annotations_language.push_back(language);
    }
  } else if (!mix_presentation_metadata.language_labels().empty()) {
    LOG(WARNING) << "Please upgrade `language_labels` to "
                    "`annotations_language`.";
    for (const auto& language_label :
         mix_presentation_metadata.language_labels()) {
      annotations_language.push_back(language_label);
    }
  }

  localized_presentation_annotations.reserve(
      mix_presentation_metadata.count_label());
  // Prioritize the `localized_presentation_annotations` field from
  // IAMF v1.1.0.
  if (!mix_presentation_metadata.localized_presentation_annotations().empty()) {
    for (const auto& localized_presentation_annotation :
         mix_presentation_metadata.localized_presentation_annotations()) {
      localized_presentation_annotations.push_back(
          localized_presentation_annotation);
    }
  } else if (!mix_presentation_metadata.mix_presentation_annotations_array()
                  .empty()) {
    LOG(WARNING) << "Please upgrade `mix_presentation_annotations_array` to "
                    "`localized_presentation_annotations`.";
    for (const auto& mix_presentation_annotation :
         mix_presentation_metadata.mix_presentation_annotations_array()) {
      localized_presentation_annotations.push_back(
          mix_presentation_annotation.mix_presentation_friendly_label());
    }
  }
}

void FillNumSubMixes(const iamf_tools_cli_proto::MixPresentationObuMetadata&
                         mix_presentation_metadata,
                     DecodedUleb128& num_sub_mixes,
                     std::vector<MixPresentationSubMix>& sub_mixes) {
  num_sub_mixes = mix_presentation_metadata.num_sub_mixes();

  sub_mixes.reserve(mix_presentation_metadata.num_sub_mixes());
}

void FillSubMixNumAudioElements(
    const iamf_tools_cli_proto::MixPresentationSubMix& input_sub_mix,
    MixPresentationSubMix& sub_mix) {
  sub_mix.num_audio_elements = input_sub_mix.num_audio_elements();
  sub_mix.audio_elements.reserve(input_sub_mix.num_audio_elements());
}

absl::Status FillLocalizedElementAnnotations(
    const iamf_tools_cli_proto::SubMixAudioElement& input_sub_mix_audio_element,
    SubMixAudioElement& sub_mix_audio_element) {
  if (!input_sub_mix_audio_element.localized_element_annotations().empty()) {
    for (const auto& localized_element_annotation :
         input_sub_mix_audio_element.localized_element_annotations()) {
      sub_mix_audio_element.localized_element_annotations.push_back(
          localized_element_annotation);
    }
  } else if (!input_sub_mix_audio_element
                  .mix_presentation_element_annotations_array()
                  .empty()) {
    LOG(WARNING)
        << "Please upgrade `mix_presentation_element_annotations_array` to "
           "`localized_element_annotations`.";
    for (const auto& input_audio_element_friendly_label :
         input_sub_mix_audio_element
             .mix_presentation_element_annotations_array()) {
      sub_mix_audio_element.localized_element_annotations.push_back(
          input_audio_element_friendly_label.audio_element_friendly_label());
    }
  }
  return absl::OkStatus();
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
    case HEADPHONES_RENDERING_MODE_BINAURAL:
      rendering_config.headphones_rendering_mode =
          kHeadphonesRenderingModeBinaural;
      break;
    case HEADPHONES_RENDERING_MODE_RESERVED_2:
      rendering_config.headphones_rendering_mode =
          kHeadphonesRenderingModeReserved2;
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

  rendering_config.rendering_config_extension_size =
      input_rendering_config.rendering_config_extension_size();

  rendering_config.rendering_config_extension_bytes.reserve(
      input_rendering_config.rendering_config_extension_size());
  for (const char& c :
       input_rendering_config.rendering_config_extension_bytes()) {
    rendering_config.rendering_config_extension_bytes.push_back(
        static_cast<uint8_t>(c));
  }

  return absl::OkStatus();
}

// Prefers selecting `element_mix_gain` (IAMF v1.1.0 field) if it present over
// `element_mix_config.mix_gain` (deprecated in the proto based on IAMF v1.0
// spec).
const iamf_tools_cli_proto::MixGainParamDefinition& SelectElementMixConfig(
    const iamf_tools_cli_proto::SubMixAudioElement& sub_mix_audio_element) {
  if (sub_mix_audio_element.has_element_mix_gain()) {
    return sub_mix_audio_element.element_mix_gain();
  } else {
    LOG(WARNING)
        << "Please upgrade `element_mix_config` to `element_mix_gain`.";
    return sub_mix_audio_element.element_mix_config().mix_gain();
  }
}

// Prefers selecting `output_mix_gain` (IAMF v1.1.0 field) if it present over
// `output_mix_config.output_mix_gain` (deprecated in the proto based on IAMF
// v1.0 spec).
const iamf_tools_cli_proto::MixGainParamDefinition& SelectOutputMixConfig(
    const iamf_tools_cli_proto::MixPresentationSubMix&
        mix_presentation_sub_mix) {
  if (mix_presentation_sub_mix.has_output_mix_gain()) {
    return mix_presentation_sub_mix.output_mix_gain();
  } else {
    LOG(WARNING) << "Please upgrade `output_mix_config` to `output_mix_gain`.";
    return mix_presentation_sub_mix.output_mix_config().output_mix_gain();
  }
}

absl::Status FillMixConfig(
    const iamf_tools_cli_proto::MixGainParamDefinition& input_mix_gain,
    MixGainParamDefinition& mix_gain) {
  RETURN_IF_NOT_OK(
      CopyParamDefinition(input_mix_gain.param_definition(), mix_gain));
  RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
      "MixGainParamDefinition.default_mix_gain",
      input_mix_gain.default_mix_gain(), mix_gain.default_mix_gain_));

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
  sub_mix.num_layouts = input_sub_mix.num_layouts();

  if (input_sub_mix.layouts().size() != input_sub_mix.num_layouts()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Inconsistent number of layouts in user input. "
        "input_sub_mix.num_layouts()= ",
        input_sub_mix.num_layouts(), " vs  input_sub_mix.layouts().size()= ",
        input_sub_mix.layouts().size()));
  }

  // Reserve the layouts vector and copy in the layouts.
  sub_mix.layouts.reserve(input_sub_mix.num_layouts());

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
    const iamf_tools_cli_proto::MixPresentationTags& mix_presentation_tags,
    std::optional<MixPresentationTags>& obu_mix_presentation_tags) {
  obu_mix_presentation_tags = MixPresentationTags{};
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "MixPresentationTags.num_tags", mix_presentation_tags.num_tags(),
      obu_mix_presentation_tags->num_tags));
  for (const auto& input_tag : mix_presentation_tags.tags()) {
    obu_mix_presentation_tags->tags.push_back(MixPresentationTags::Tag{
        .tag_name = input_tag.tag_name(),
        .tag_value = input_tag.tag_value(),
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

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "LoudnessInfo.anchored_loudness.num_anchored_loudness",
      user_loudness.anchored_loudness().num_anchored_loudness(),
      output_loudness.anchored_loudness.num_anchored_loudness));

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

  output_loudness.layout_extension.info_type_size =
      user_loudness.info_type_size();
  output_loudness.layout_extension.info_type_bytes.reserve(
      user_loudness.info_type_bytes().size());
  for (const char& c : user_loudness.info_type_bytes()) {
    output_loudness.layout_extension.info_type_bytes.push_back(
        static_cast<uint8_t>(c));
  }
  return absl::OkStatus();
}

absl::Status MixPresentationGenerator::Generate(
    std::list<MixPresentationObu>& mix_presentation_obus) {
  for (const auto& mix_presentation_metadata : mix_presentation_metadata_) {
    struct {
      DecodedUleb128 mix_presentation_id;
      DecodedUleb128 count_label;
      std::vector<std::string> annotations_language;
      // Length `count_label`.
      std::vector<std::string> localized_presentation_annotations;

      DecodedUleb128 num_sub_mixes;
      // Length `num_sub_mixes`.
      std::vector<MixPresentationSubMix> sub_mixes;

      std::optional<MixPresentationTags> mix_presentation_tags;
    } obu_args;

    obu_args.mix_presentation_id =
        mix_presentation_metadata.mix_presentation_id();

    FillAnnotationsLanguageAndAnnotations(
        mix_presentation_metadata, obu_args.count_label,
        obu_args.annotations_language,
        obu_args.localized_presentation_annotations);

    FillNumSubMixes(mix_presentation_metadata, obu_args.num_sub_mixes,
                    obu_args.sub_mixes);
    for (const auto& input_sub_mix : mix_presentation_metadata.sub_mixes()) {
      MixPresentationSubMix sub_mix;

      FillSubMixNumAudioElements(input_sub_mix, sub_mix);
      for (const auto& input_sub_mix_audio_element :
           input_sub_mix.audio_elements()) {
        SubMixAudioElement sub_mix_audio_element;
        sub_mix_audio_element.audio_element_id =
            input_sub_mix_audio_element.audio_element_id();

        RETURN_IF_NOT_OK(FillLocalizedElementAnnotations(
            input_sub_mix_audio_element, sub_mix_audio_element));

        RETURN_IF_NOT_OK(
            FillRenderingConfig(input_sub_mix_audio_element.rendering_config(),
                                sub_mix_audio_element.rendering_config));

        RETURN_IF_NOT_OK(
            FillMixConfig(SelectElementMixConfig(input_sub_mix_audio_element),
                          sub_mix_audio_element.element_mix_gain));
        sub_mix.audio_elements.push_back(sub_mix_audio_element);
      }

      RETURN_IF_NOT_OK(FillMixConfig(SelectOutputMixConfig(input_sub_mix),
                                     sub_mix.output_mix_gain));

      RETURN_IF_NOT_OK(FillLayouts(input_sub_mix, sub_mix));
      obu_args.sub_mixes.push_back(std::move(sub_mix));
    }
    if (mix_presentation_metadata.include_mix_presentation_tags()) {
      RETURN_IF_NOT_OK(FillMixPresentationTags(
          mix_presentation_metadata.mix_presentation_tags(),
          obu_args.mix_presentation_tags));
    } else {
      obu_args.mix_presentation_tags = std::nullopt;
    }

    MixPresentationObu obu(
        GetHeaderFromMetadata(mix_presentation_metadata.obu_header()),
        obu_args.mix_presentation_id, obu_args.count_label,
        obu_args.annotations_language,
        obu_args.localized_presentation_annotations, obu_args.num_sub_mixes,
        obu_args.sub_mixes);
    obu.mix_presentation_tags_ = obu_args.mix_presentation_tags;
    mix_presentation_obus.emplace_back(std::move(obu));
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
