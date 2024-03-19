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
#include "iamf/cli/mix_presentation_generator.h"

#include <cstdint>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/ia.h"
#include "iamf/mix_presentation.h"
#include "iamf/obu_util.h"
#include "iamf/param_definitions.h"

namespace iamf_tools {

namespace {

void FillLanguageLabelsAndAnnotations(
    const iamf_tools_cli_proto::MixPresentationObuMetadata&
        mix_presentation_metadata,
    DecodedUleb128& count_label, std::vector<std::string>& language_labels,
    std::vector<MixPresentationAnnotations>& mix_presentation_annotations) {
  count_label = mix_presentation_metadata.count_label();

  language_labels.reserve(mix_presentation_metadata.count_label());
  for (const auto& language_label :
       mix_presentation_metadata.language_labels()) {
    language_labels.push_back(language_label);
  }

  mix_presentation_annotations.reserve(mix_presentation_metadata.count_label());
  for (const auto& mix_presentation_annotation :
       mix_presentation_metadata.mix_presentation_annotations_array()) {
    mix_presentation_annotations.push_back(
        {mix_presentation_annotation.mix_presentation_friendly_label()});
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

absl::Status FillMixPresentationElementAnnotations(
    const iamf_tools_cli_proto::SubMixAudioElement& input_sub_mix_audio_element,
    SubMixAudioElement& sub_mix_audio_element) {
  for (const auto& input_audio_element_friendly_label :
       input_sub_mix_audio_element
           .mix_presentation_element_annotations_array()) {
    sub_mix_audio_element.mix_presentation_element_annotations.push_back(
        {input_audio_element_friendly_label.audio_element_friendly_label()});
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
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown headphones_rendering_mode= ",
                       input_rendering_config.headphones_rendering_mode()));
  }

  RETURN_IF_NOT_OK(Uint32ToUint8(input_rendering_config.reserved(),
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

absl::Status FillMixConfig(
    const iamf_tools_cli_proto::MixGainParamDefinition& input_mix_gain,
    MixGainParamDefinition& mix_gain) {
  RETURN_IF_NOT_OK(
      CopyParamDefinition(input_mix_gain.param_definition(), &mix_gain));
  RETURN_IF_NOT_OK(Int32ToInt16(input_mix_gain.default_mix_gain(),
                                mix_gain.default_mix_gain_));

  return absl::OkStatus();
}

absl::Status CopySoundSystem(
    const iamf_tools_cli_proto::SoundSystem input_sound_system,
    LoudspeakersSsConventionLayout::SoundSystem& output_sound_system) {
  using enum iamf_tools_cli_proto::SoundSystem;
  using enum LoudspeakersSsConventionLayout::SoundSystem;
  static const absl::NoDestructor<
      absl::flat_hash_map<iamf_tools_cli_proto::SoundSystem,
                          LoudspeakersSsConventionLayout::SoundSystem>>
      kInputSoundSystemToOutputSoundSystem(
          {{SOUND_SYSTEM_A_0_2_0, kSoundSystemA_0_2_0},
           {SOUND_SYSTEM_B_0_5_0, kSoundSystemB_0_5_0},
           {SOUND_SYSTEM_C_2_5_0, kSoundSystemC_2_5_0},
           {SOUND_SYSTEM_D_4_5_0, kSoundSystemD_4_5_0},
           {SOUND_SYSTEM_E_4_5_1, kSoundSystemE_4_5_1},
           {SOUND_SYSTEM_F_3_7_0, kSoundSystemF_3_7_0},
           {SOUND_SYSTEM_G_4_9_0, kSoundSystemG_4_9_0},
           {SOUND_SYSTEM_H_9_10_3, kSoundSystemH_9_10_3},
           {SOUND_SYSTEM_I_0_7_0, kSoundSystemI_0_7_0},
           {SOUND_SYSTEM_J_4_7_0, kSoundSystemJ_4_7_0},
           {SOUND_SYSTEM_10_2_7_0, kSoundSystem10_2_7_0},
           {SOUND_SYSTEM_11_2_3_0, kSoundSystem11_2_3_0},
           {SOUND_SYSTEM_12_0_1_0, kSoundSystem12_0_1_0}});

  if (!LookupInMap(*kInputSoundSystemToOutputSoundSystem, input_sound_system,
                   output_sound_system)
           .ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown input_sound_system= ", input_sound_system));
  }
  return absl::OkStatus();
}

absl::Status CopyReservedOrBinauralLayout(
    Layout::LayoutType layout,
    const ::iamf_tools_cli_proto::LoudspeakersReservedOrBinauralLayout&
        reserved_or_binaural_layout,
    Layout& obu_layout) {
  obu_layout.layout_type = layout;
  LoudspeakersReservedBinauralLayout obu_reserved_or_binaural_layout;
  RETURN_IF_NOT_OK(Uint32ToUint8(reserved_or_binaural_layout.reserved(),
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
        RETURN_IF_NOT_OK(
            CopySoundSystem(input_loudness_layout.ss_layout().sound_system(),
                            obu_ss_layout.sound_system));
        RETURN_IF_NOT_OK(
            Uint32ToUint8(input_loudness_layout.ss_layout().reserved(),
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

    sub_mix.layouts.push_back(layout);
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status MixPresentationGenerator::CopyInfoType(
    const iamf_tools_cli_proto::LoudnessInfo& input_loudness_info,
    uint8_t& loudness_info_type) {
  using enum iamf_tools_cli_proto::LoudnessInfoTypeBitMask;
  using enum LoudnessInfo::InfoTypeBitmask;
  static const absl::NoDestructor<
      absl::flat_hash_map<iamf_tools_cli_proto::LoudnessInfoTypeBitMask,
                          LoudnessInfo::InfoTypeBitmask>>
      kInputLoudnessInfoTypeBitMaskToOutputLoudnessInfoTypeBitMask({
          {LOUDNESS_INFO_TYPE_TRUE_PEAK, kTruePeak},
          {LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS, kAnchoredLoudness},
          {LOUDNESS_INFO_TYPE_RESERVED_4, kInfoTypeBitMask4},
          {LOUDNESS_INFO_TYPE_RESERVED_8, kInfoTypeBitMask8},
          {LOUDNESS_INFO_TYPE_RESERVED_16, kInfoTypeBitMask16},
          {LOUDNESS_INFO_TYPE_RESERVED_32, kInfoTypeBitMask32},
          {LOUDNESS_INFO_TYPE_RESERVED_64, kInfoTypeBitMask64},
          {LOUDNESS_INFO_TYPE_RESERVED_128, kInfoTypeBitMask128},
      });

  if (input_loudness_info.has_deprecated_info_type()) {
    LOG(WARNING) << "Please upgrade the `deprecated_info_type` "
                    "field to the new "
                    "`info_type_bit_masks` field.";

    RETURN_IF_NOT_OK(Uint32ToUint8(input_loudness_info.deprecated_info_type(),
                                   loudness_info_type));
    return absl::OkStatus();
  } else {
    uint8_t accumulated_info_type_bitmask = 0;
    for (int i = 0; i < input_loudness_info.info_type_bit_masks_size(); ++i) {
      LoudnessInfo::InfoTypeBitmask user_output_bit_mask;
      if (!LookupInMap(
               *kInputLoudnessInfoTypeBitMaskToOutputLoudnessInfoTypeBitMask,
               input_loudness_info.info_type_bit_masks(i), user_output_bit_mask)
               .ok()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown info_type_bit_masks(", i,
                         ")= ", input_loudness_info.info_type_bit_masks(i)));
      }

      // Track the accumulated bit mask.
      accumulated_info_type_bitmask |=
          static_cast<uint8_t>(user_output_bit_mask);
    }

    loudness_info_type = accumulated_info_type_bitmask;
    return absl::OkStatus();
  }
}

absl::Status MixPresentationGenerator::Generate(
    std::list<MixPresentationObu>& mix_presentation_obus) {
  for (const auto& mix_presentation_metadata : mix_presentation_metadata_) {
    struct {
      DecodedUleb128 mix_presentation_id;
      DecodedUleb128 count_label;
      std::vector<std::string> language_labels;
      // Length `count_label`.
      std::vector<MixPresentationAnnotations> mix_presentation_annotations;

      DecodedUleb128 num_sub_mixes;
      // Length `num_sub_mixes`.
      std::vector<MixPresentationSubMix> sub_mixes;
    } obu_args;

    obu_args.mix_presentation_id =
        mix_presentation_metadata.mix_presentation_id();

    FillLanguageLabelsAndAnnotations(
        mix_presentation_metadata, obu_args.count_label,
        obu_args.language_labels, obu_args.mix_presentation_annotations);

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

        RETURN_IF_NOT_OK(FillMixPresentationElementAnnotations(
            input_sub_mix_audio_element, sub_mix_audio_element));

        RETURN_IF_NOT_OK(
            FillRenderingConfig(input_sub_mix_audio_element.rendering_config(),
                                sub_mix_audio_element.rendering_config));

        RETURN_IF_NOT_OK(FillMixConfig(
            input_sub_mix_audio_element.element_mix_config().mix_gain(),
            sub_mix_audio_element.element_mix_config.mix_gain));
        sub_mix.audio_elements.push_back(sub_mix_audio_element);
      }

      RETURN_IF_NOT_OK(
          FillMixConfig(input_sub_mix.output_mix_config().output_mix_gain(),
                        sub_mix.output_mix_config.output_mix_gain));

      RETURN_IF_NOT_OK(FillLayouts(input_sub_mix, sub_mix));
      obu_args.sub_mixes.push_back(std::move(sub_mix));
    }

    mix_presentation_obus.emplace_back(
        GetHeaderFromMetadata(mix_presentation_metadata.obu_header()),
        obu_args.mix_presentation_id, obu_args.count_label,
        obu_args.language_labels, obu_args.mix_presentation_annotations,
        obu_args.num_sub_mixes, obu_args.sub_mixes);
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
