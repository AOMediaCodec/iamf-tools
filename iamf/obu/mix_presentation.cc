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
#include "iamf/obu/mix_presentation.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

namespace {

absl::Status ValidateUniqueAudioElementIds(
    const std::vector<MixPresentationSubMix>& sub_mixes) {
  std::vector<DecodedUleb128> collected_audio_element_ids;

  absl::flat_hash_set<DecodedUleb128> seen_audio_element_ids;
  // Audio Element IDs must be unique across all sub-mixes.
  for (const auto& sub_mix : sub_mixes) {
    for (const auto& audio_element : sub_mix.audio_elements) {
      collected_audio_element_ids.push_back(audio_element.audio_element_id);
    }
  }

  return ValidateUnique(collected_audio_element_ids.begin(),
                        collected_audio_element_ids.end(), "Audio element IDs");
}

absl::Status ValidateUniqueAnchorElements(
    const std::vector<AnchoredLoudnessElement>& anchor_elements) {
  std::vector<uint8_t> anchor_elements_as_uint8;
  anchor_elements_as_uint8.reserve(anchor_elements.size());
  for (const auto& anchor_element : anchor_elements) {
    anchor_elements_as_uint8.push_back(
        static_cast<uint8_t>(anchor_element.anchor_element));
  }
  return ValidateUnique(anchor_elements_as_uint8.begin(),
                        anchor_elements_as_uint8.end(),
                        "Anchored loudness types");
}

absl::Status ValidateAndWriteSubMixAudioElement(
    const SubMixAudioElement& element, WriteBitBuffer& wb) {
  // Write the main portion of an `SubMixAudioElement`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(element.audio_element_id));
  for (const auto& localized_element_annotation :
       element.localized_element_annotations) {
    RETURN_IF_NOT_OK(wb.WriteString(localized_element_annotation));
  }

  // Write out `rendering_config`.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint8_t>(element.rendering_config.headphones_rendering_mode),
      2));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint8_t>(element.rendering_config.reserved), 6));
  RETURN_IF_NOT_OK(wb.WriteUleb128(
      element.rendering_config.rendering_config_extension_size));
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "rendering_config_extension_bytes",
      element.rendering_config.rendering_config_extension_bytes.size(),
      element.rendering_config.rendering_config_extension_size));
  RETURN_IF_NOT_OK(wb.WriteUint8Vector(
      element.rendering_config.rendering_config_extension_bytes));

  RETURN_IF_NOT_OK(element.element_mix_gain.ValidateAndWrite(wb));
  return absl::OkStatus();
}

// Writes and validates a `MixPresentationLayout and sets `found_stereo_layout`
// to if it is a stereo layout.
absl::Status ValidateAndWriteLayout(const MixPresentationLayout& layout,
                                    bool& found_stereo_layout,
                                    WriteBitBuffer& wb) {
  // Write the `loudness_layout` portion of a `MixPresentationLayout`.
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(layout.loudness_layout.layout_type, 2));

  // Write the specific type of `Layout` dependent on `layout_type`.
  switch (layout.loudness_layout.layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeLoudspeakersSsConvention:
      RETURN_IF_NOT_OK(std::get<LoudspeakersSsConventionLayout>(
                           layout.loudness_layout.specific_layout)
                           .Write(found_stereo_layout, wb));
      break;
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    case kLayoutTypeBinaural:
      RETURN_IF_NOT_OK(std::get<LoudspeakersReservedOrBinauralLayout>(
                           layout.loudness_layout.specific_layout)
                           .Write(wb));
      break;
  }

  // Write the `loudness` portion of a `MixPresentationLayout`.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(layout.loudness.info_type, 8));
  RETURN_IF_NOT_OK(wb.WriteSigned16(layout.loudness.integrated_loudness));
  RETURN_IF_NOT_OK(wb.WriteSigned16(layout.loudness.digital_peak));

  // Conditionally write `true_peak` based on `info_type`.
  if ((layout.loudness.info_type & LoudnessInfo::kTruePeak) != 0) {
    RETURN_IF_NOT_OK(wb.WriteSigned16(layout.loudness.true_peak));
  }
  // Conditionally write `anchored_loudness` based on `info_type`.
  if ((layout.loudness.info_type & LoudnessInfo::kAnchoredLoudness) != 0) {
    RETURN_IF_NOT_OK(ValidateUniqueAnchorElements(
        layout.loudness.anchored_loudness.anchor_elements));
    const AnchoredLoudness& anchored_loudness =
        layout.loudness.anchored_loudness;
    RETURN_IF_NOT_OK(
        wb.WriteUnsignedLiteral(anchored_loudness.num_anchored_loudness, 8));
    RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
        "anchor_elements", anchored_loudness.anchor_elements.size(),
        static_cast<uint32_t>(anchored_loudness.num_anchored_loudness)));
    for (const auto& anchor_element : anchored_loudness.anchor_elements) {
      RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
          static_cast<uint8_t>(anchor_element.anchor_element), 8));
      RETURN_IF_NOT_OK(wb.WriteSigned16(anchor_element.anchored_loudness));
    }
  }
  // Conditionally write `layout_extension` based on `info_type`.
  if ((layout.loudness.info_type & LoudnessInfo::kAnyLayoutExtension) != 0) {
    RETURN_IF_NOT_OK(
        wb.WriteUleb128(layout.loudness.layout_extension.info_type_size));
    RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
        "info_type_bytes",
        layout.loudness.layout_extension.info_type_bytes.size(),
        layout.loudness.layout_extension.info_type_size));
    RETURN_IF_NOT_OK(
        wb.WriteUint8Vector(layout.loudness.layout_extension.info_type_bytes));
  }

  return absl::OkStatus();
}

absl::Status ValidateAndWriteSubMix(const MixPresentationSubMix& sub_mix,
                                    WriteBitBuffer& wb) {
  // IAMF requires there to be at least one audio element.
  RETURN_IF_NOT_OK(ValidateNotEqual(
      DecodedUleb128{0}, sub_mix.num_audio_elements, "num_audio_elements"));

  // Write the main portion of a `MixPresentationSubMix`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(sub_mix.num_audio_elements));

  // Loop to write the `audio_elements` array.
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual("audio_elements",
                                           sub_mix.audio_elements.size(),
                                           sub_mix.num_audio_elements));
  for (const auto& sub_mix_audio_element : sub_mix.audio_elements) {
    RETURN_IF_NOT_OK(
        ValidateAndWriteSubMixAudioElement(sub_mix_audio_element, wb));
  }

  RETURN_IF_NOT_OK(sub_mix.output_mix_gain.ValidateAndWrite(wb));
  RETURN_IF_NOT_OK(wb.WriteUleb128(sub_mix.num_layouts));

  // Loop to write the `layouts` array.
  bool found_stereo_layout = false;
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual("layouts", sub_mix.layouts.size(),
                                           sub_mix.num_layouts));
  for (const auto& layout : sub_mix.layouts) {
    RETURN_IF_NOT_OK(ValidateAndWriteLayout(layout, found_stereo_layout, wb));
  }
  if (!found_stereo_layout) {
    return absl::InvalidArgumentError(
        "Every sub-mix must have a stereo layout.");
  }

  return absl::OkStatus();
}

absl::Status ValidateNumSubMixes(DecodedUleb128 num_sub_mixes) {
  if (num_sub_mixes == 0) {
    return absl::InvalidArgumentError(
        "Expected a non-zero number of sub-mixes.");
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status Layout::ReadAndValidate(ReadBitBuffer& rb) {
  uint8_t layout_type_uint;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, layout_type_uint));
  layout_type = static_cast<Layout::LayoutType>(layout_type_uint);

  // Read the specific type of `Layout` dependent on `layout_type`.
  switch (layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeLoudspeakersSsConvention:
      specific_layout = LoudspeakersSsConventionLayout();
      return std::get<LoudspeakersSsConventionLayout>(specific_layout).Read(rb);
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    // Reserved layouts are identical to binaural layouts as of IAMF
    // v1.0-errata aomediacodec.github.io/iamf/v1.0.0-errata.html#syntax-layout.
    case kLayoutTypeBinaural:
      specific_layout = LoudspeakersReservedOrBinauralLayout();
      return std::get<LoudspeakersReservedOrBinauralLayout>(specific_layout)
          .Read(rb);
  }

  return absl::InternalError(absl::StrCat(
      "Unexpected value for 2-bit Layout::LayoutType = ", layout_type));
}

absl::Status MixPresentationLayout::ReadAndValidate(ReadBitBuffer& rb) {
  // Read the `loudness_layout` portion of a `MixPresentationLayout`.
  RETURN_IF_NOT_OK(loudness_layout.ReadAndValidate(rb));

  // Read the `loudness` portion of a `MixPresentationLayout`.
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, loudness.info_type));
  RETURN_IF_NOT_OK(rb.ReadSigned16(loudness.integrated_loudness));
  RETURN_IF_NOT_OK(rb.ReadSigned16(loudness.digital_peak));

  // Conditionally read `true_peak` based on `info_type`.
  if (loudness.info_type & LoudnessInfo::kTruePeak) {
    RETURN_IF_NOT_OK(rb.ReadSigned16(loudness.true_peak));
  }
  // Conditionally read `anchored_loudness` based on `info_type`.
  if (loudness.info_type & LoudnessInfo::kAnchoredLoudness) {
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(
        8, loudness.anchored_loudness.num_anchored_loudness));

    for (int i = 0; i < loudness.anchored_loudness.num_anchored_loudness; ++i) {
      AnchoredLoudnessElement anchor_loudness_element;
      uint8_t anchor_element;
      RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, anchor_element));
      anchor_loudness_element.anchor_element =
          static_cast<AnchoredLoudnessElement::AnchorElement>(anchor_element);
      RETURN_IF_NOT_OK(
          rb.ReadSigned16(anchor_loudness_element.anchored_loudness));
      loudness.anchored_loudness.anchor_elements.push_back(
          anchor_loudness_element);
    }
    RETURN_IF_NOT_OK(ValidateUniqueAnchorElements(
        loudness.anchored_loudness.anchor_elements));
  }
  // Conditionally read `layout_extension` based on `info_type`.
  if (loudness.info_type & LoudnessInfo::kAnyLayoutExtension) {
    RETURN_IF_NOT_OK(rb.ReadULeb128(loudness.layout_extension.info_type_size));
    RETURN_IF_NOT_OK(
        rb.ReadUint8Vector(loudness.layout_extension.info_type_size,
                           loudness.layout_extension.info_type_bytes));
  }

  return absl::OkStatus();
}

absl::Status SubMixAudioElement::ReadAndValidate(const int32_t& count_label,
                                                 ReadBitBuffer& rb) {
  // Read the main portion of an `SubMixAudioElement`.
  RETURN_IF_NOT_OK(rb.ReadULeb128(audio_element_id));
  for (int i = 0; i < count_label; ++i) {
    std::string localized_element_annotation;
    RETURN_IF_NOT_OK(rb.ReadString(localized_element_annotation));
    localized_element_annotations.push_back(localized_element_annotation);
  }

  // Read `rendering_config`.
  uint8_t headphones_rendering_mode;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, headphones_rendering_mode));
  rendering_config.headphones_rendering_mode =
      static_cast<RenderingConfig::HeadphonesRenderingMode>(
          headphones_rendering_mode);

  uint8_t reserved;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, reserved));
  rendering_config.reserved = reserved;
  RETURN_IF_NOT_OK(
      rb.ReadULeb128(rendering_config.rendering_config_extension_size));
  RETURN_IF_NOT_OK(
      rb.ReadUint8Vector(rendering_config.rendering_config_extension_size,
                         rendering_config.rendering_config_extension_bytes));

  RETURN_IF_NOT_OK(element_mix_gain.ReadAndValidate(rb));
  return absl::OkStatus();
}

absl::Status MixPresentationSubMix::ReadAndValidate(const int32_t& count_label,
                                                    ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_audio_elements));
  // IAMF requires there to be at least one audio element.
  RETURN_IF_NOT_OK(ValidateNotEqual(DecodedUleb128{0}, num_audio_elements,
                                    "num_audio_elements"));
  for (int i = 0; i < num_audio_elements; ++i) {
    SubMixAudioElement sub_mix_audio_element;
    RETURN_IF_NOT_OK(sub_mix_audio_element.ReadAndValidate(count_label, rb));
    audio_elements.push_back(sub_mix_audio_element);
  }

  RETURN_IF_NOT_OK(output_mix_gain.ReadAndValidate(rb));
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_layouts));
  for (int i = 0; i < num_layouts; ++i) {
    MixPresentationLayout mix_presentation_layout;
    RETURN_IF_NOT_OK(mix_presentation_layout.ReadAndValidate(rb));
    layouts.push_back(mix_presentation_layout);
  }
  return absl::OkStatus();
}

// Validates and writes a `LoudspeakersSsConventionLayout` and sets
// `found_stereo_layout` to true if it is a stereo layout.
absl::Status LoudspeakersSsConventionLayout::Write(bool& found_stereo_layout,
                                                   WriteBitBuffer& wb) const {
  if (sound_system == kSoundSystemA_0_2_0) {
    found_stereo_layout = true;
  }
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(sound_system, 4));

  return wb.WriteUnsignedLiteral(reserved, 2);
}

// Reads and validates a `LoudspeakersSsConventionLayout`
// TODO(b/339855338): Set `found_stereo_layout` to true if it is a stereo layout
// and check that its been found in MixPresentationSubMix::Read.
absl::Status LoudspeakersSsConventionLayout::Read(ReadBitBuffer& rb) {
  uint8_t sound_system_uint;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, sound_system_uint));
  sound_system = static_cast<SoundSystem>(sound_system_uint);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, reserved));
  return absl::OkStatus();
}

absl::Status LoudspeakersReservedOrBinauralLayout::Write(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved, 6));
  return absl::OkStatus();
}

absl::Status LoudspeakersReservedOrBinauralLayout::Read(ReadBitBuffer& rb) {
  return rb.ReadUnsignedLiteral(6, reserved);
}

absl::Status MixPresentationObu::GetNumChannelsFromLayout(
    const Layout& loudness_layout, int32_t& num_channels) {
  switch (loudness_layout.layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeBinaural:
      num_channels = 2;
      return absl::OkStatus();
    case kLayoutTypeLoudspeakersSsConvention: {
      using enum LoudspeakersSsConventionLayout::SoundSystem;
      static const absl::NoDestructor<absl::flat_hash_map<
          LoudspeakersSsConventionLayout::SoundSystem, int32_t>>
          kSoundSystemToNumChannels({
              {kSoundSystemA_0_2_0, 2},
              {kSoundSystemB_0_5_0, 6},
              {kSoundSystemC_2_5_0, 8},
              {kSoundSystemD_4_5_0, 10},
              {kSoundSystemE_4_5_1, 11},
              {kSoundSystemF_3_7_0, 12},
              {kSoundSystemG_4_9_0, 14},
              {kSoundSystemH_9_10_3, 24},
              {kSoundSystemI_0_7_0, 8},
              {kSoundSystemJ_4_7_0, 12},
              {kSoundSystem10_2_7_0, 10},
              {kSoundSystem11_2_3_0, 6},
              {kSoundSystem12_0_1_0, 1},
              {kSoundSystem13_6_9_0, 16},
          });

      const auto sound_system = std::get<LoudspeakersSsConventionLayout>(
                                    loudness_layout.specific_layout)
                                    .sound_system;

      if (!LookupInMap(*kSoundSystemToNumChannels, sound_system, num_channels)
               .ok()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown number of channels for reserved or out of "
                         "bounds sound_system= ",
                         sound_system));
      }
      return absl::OkStatus();
    }
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown layout_type= ", loudness_layout.layout_type));
  }
}

absl::Status MixPresentationObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  // Write the main portion of the OBU.
  RETURN_IF_NOT_OK(wb.WriteUleb128(mix_presentation_id_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(count_label_));

  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "language_labels", annotations_language_.size(), count_label_));
  RETURN_IF_NOT_OK(ValidateUnique(annotations_language_.begin(),
                                  annotations_language_.end(),
                                  "Language labels"));

  for (const auto& annotations_language : annotations_language_) {
    RETURN_IF_NOT_OK(wb.WriteString(annotations_language));
  }

  RETURN_IF_NOT_OK(ValidateVectorSizeEqual("mix presentation annotations",
                                           annotations_language_.size(),
                                           count_label_));
  for (const auto& localized_presentation_annotation :
       localized_presentation_annotations_) {
    RETURN_IF_NOT_OK(wb.WriteString(localized_presentation_annotation));
  }

  RETURN_IF_NOT_OK(wb.WriteUleb128(num_sub_mixes_));

  // Loop to write the `sub_mixes` array.
  RETURN_IF_NOT_OK(ValidateNumSubMixes(num_sub_mixes_));
  RETURN_IF_NOT_OK(ValidateUniqueAudioElementIds(sub_mixes_));
  RETURN_IF_NOT_OK(
      ValidateVectorSizeEqual("sub_mixes", sub_mixes_.size(), num_sub_mixes_));
  for (const auto& sub_mix : sub_mixes_) {
    RETURN_IF_NOT_OK(ValidateAndWriteSubMix(sub_mix, wb));
  }

  return absl::OkStatus();
}

absl::StatusOr<MixPresentationObu> MixPresentationObu::CreateFromBuffer(
    const ObuHeader& header, ReadBitBuffer& rb) {
  MixPresentationObu mix_presentation_obu(header);
  RETURN_IF_NOT_OK(mix_presentation_obu.ValidateAndReadPayload(rb));
  return mix_presentation_obu;
}

absl::Status MixPresentationObu::ValidateAndReadPayload(ReadBitBuffer& rb) {
  // Read the main portion of the OBU.
  RETURN_IF_NOT_OK(rb.ReadULeb128(mix_presentation_id_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(count_label_));

  for (int i = 0; i < count_label_; ++i) {
    std::string annotations_language;
    RETURN_IF_NOT_OK(rb.ReadString(annotations_language));
    annotations_language_.push_back(annotations_language);
  }
  RETURN_IF_NOT_OK(ValidateUnique(annotations_language_.begin(),
                                  annotations_language_.end(),
                                  "Annotation languages"));

  for (int i = 0; i < count_label_; ++i) {
    std::string localized_presentation_annotation;
    RETURN_IF_NOT_OK(rb.ReadString(localized_presentation_annotation));
    localized_presentation_annotations_.push_back(
        localized_presentation_annotation);
  }

  RETURN_IF_NOT_OK(rb.ReadULeb128(num_sub_mixes_));

  // Loop to read the `sub_mixes` array.
  for (int i = 0; i < num_sub_mixes_; ++i) {
    MixPresentationSubMix sub_mix;
    RETURN_IF_NOT_OK(sub_mix.ReadAndValidate(count_label_, rb));
    sub_mixes_.push_back(sub_mix);
  }

  RETURN_IF_NOT_OK(ValidateNumSubMixes(num_sub_mixes_));
  RETURN_IF_NOT_OK(ValidateUniqueAudioElementIds(sub_mixes_));

  return absl::OkStatus();
}

void LoudspeakersSsConventionLayout::Print() const {
  LOG(INFO) << "        sound_system= " << absl::StrCat(sound_system);
  LOG(INFO) << "        reserved= " << absl::StrCat(reserved);
}

void LoudspeakersReservedOrBinauralLayout::Print() const {
  LOG(INFO) << "        reserved= " << absl::StrCat(reserved);
}

void MixPresentationObu::PrintObu() const {
  LOG(INFO) << "Mix Presentation OBU:";
  LOG(INFO) << "  mix_presentation_id= " << mix_presentation_id_;
  LOG(INFO) << "  count_label= " << count_label_;
  LOG(INFO) << "  language_labels:";
  for (int i = 0; i < count_label_; ++i) {
    LOG(INFO) << "    annotations_languages[" << i << "]= \""
              << annotations_language_[i] << "\"";
  }
  LOG(INFO) << "  mix_presentation_annotations:";
  for (int i = 0; i < count_label_; ++i) {
    LOG(INFO) << "    localized_presentation_annotations[" << i << "]= \""
              << localized_presentation_annotations_[i] << "\"";
  }
  LOG(INFO) << "  num_sub_mixes= " << num_sub_mixes_;

  // Submixes.
  for (int i = 0; i < num_sub_mixes_; ++i) {
    const auto& sub_mix = sub_mixes_[i];
    LOG(INFO) << "  // sub_mixes[" << i << "]:";
    LOG(INFO) << "    num_audio_elements= " << sub_mix.num_audio_elements;
    // Audio elements.
    for (int j = 0; j < sub_mix.num_audio_elements; ++j) {
      const auto& audio_element = sub_mix.audio_elements[j];
      LOG(INFO) << "    // audio_elements[" << j << "]:";
      LOG(INFO) << "      audio_element_id= " << audio_element.audio_element_id;
      LOG(INFO) << "      localized_presentation_annotations:";
      for (int k = 0; k < count_label_; ++k) {
        LOG(INFO) << "        localized_element_annotations= \""
                  << audio_element.localized_element_annotations[k] << "\"";
      }
      LOG(INFO) << "        rendering_config:";
      LOG(INFO) << "          headphones_rendering_mode= "
                << absl::StrCat(audio_element.rendering_config
                                    .headphones_rendering_mode);
      LOG(INFO) << "          reserved= "
                << absl::StrCat(audio_element.rendering_config.reserved);
      LOG(INFO)
          << "          rendering_config_extension_size= "
          << audio_element.rendering_config.rendering_config_extension_size;
      LOG(INFO) << "          rendering_config_extension_bytes omitted.";
      LOG(INFO) << "        element_mix_gain:";
      audio_element.element_mix_gain.Print();
    }

    LOG(INFO) << "    output_mix_gain:";
    sub_mix.output_mix_gain.Print();

    LOG(INFO) << "    num_layouts= " << sub_mix.num_layouts;

    // Layouts.
    for (int j = 0; j < sub_mix.num_layouts; j++) {
      const auto& layout = sub_mix.layouts[j];
      LOG(INFO) << "    // layouts[" << j << "]:";
      LOG(INFO) << "      loudness_layout:";
      LOG(INFO) << "        layout_type= "
                << absl::StrCat(layout.loudness_layout.layout_type);

      // SpecificLayout.
      switch (layout.loudness_layout.layout_type) {
        using enum Layout::LayoutType;
        case kLayoutTypeLoudspeakersSsConvention:
          std::get<LoudspeakersSsConventionLayout>(
              layout.loudness_layout.specific_layout)
              .Print();
          break;
        case kLayoutTypeReserved0:
        case kLayoutTypeReserved1:
        case kLayoutTypeBinaural:
          std::get<LoudspeakersReservedOrBinauralLayout>(
              layout.loudness_layout.specific_layout)
              .Print();
      }

      const auto& loudness = layout.loudness;
      LOG(INFO) << "      loudness:";
      LOG(INFO) << "        info_type= " << absl::StrCat(loudness.info_type);
      LOG(INFO) << "        integrated_loudness= "
                << loudness.integrated_loudness;
      LOG(INFO) << "        digital_peak= " << loudness.digital_peak;
      if ((loudness.info_type & LoudnessInfo::kTruePeak) != 0) {
        LOG(INFO) << "        true_peak= " << layout.loudness.true_peak;
      }

      if ((loudness.info_type & LoudnessInfo::kAnchoredLoudness) != 0) {
        const auto& anchored_loudness = loudness.anchored_loudness;
        LOG(INFO) << "        anchored_loudness: ";
        LOG(INFO) << "          num_anchored_loudness= "
                  << absl::StrCat(anchored_loudness.num_anchored_loudness);
        for (int i = 0; i < anchored_loudness.anchor_elements.size(); i++) {
          LOG(INFO) << "          anchor_element[" << i << "]= "
                    << absl::StrCat(
                           anchored_loudness.anchor_elements[i].anchor_element);
          LOG(INFO) << "          anchored_loudness[" << i << "]= "
                    << anchored_loudness.anchor_elements[i].anchored_loudness;
        }
      }

      if ((loudness.info_type & LoudnessInfo::kAnyLayoutExtension) != 0) {
        const auto& layout_extension = loudness.layout_extension;
        LOG(INFO) << "        layout_extension: ";
        LOG(INFO) << "          info_type_size= "
                  << layout_extension.info_type_size;
        for (int i = 0; i < layout_extension.info_type_bytes.size(); ++i) {
          LOG(INFO) << "          info_type_bytes[" << i
                    << "]= " << layout_extension.info_type_bytes[i];
        }
      }
    }
  }
}

}  // namespace iamf_tools
