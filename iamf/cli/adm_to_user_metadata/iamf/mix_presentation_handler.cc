/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/adm_to_user_metadata/iamf/mix_presentation_handler.h"

#include <cstdint>
#include <map>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/common/obu_util.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

absl::StatusOr<iamf_tools_cli_proto::SoundSystem>
LookupSoundSystemFromInputLayout(IamfInputLayout layout) {
  // Map which holds the valid loudspeaker layout and the reference sound system
  // corresponding to it.
  using enum IamfInputLayout;
  using enum iamf_tools_cli_proto::SoundSystem;
  static const absl::NoDestructor<
      absl::flat_hash_map<IamfInputLayout, iamf_tools_cli_proto::SoundSystem>>
      kInputLayoutToSoundSystem({
          {kMono, SOUND_SYSTEM_12_0_1_0},
          {kStereo, SOUND_SYSTEM_A_0_2_0},
          {k5_1, SOUND_SYSTEM_B_0_5_0},
          {k5_1_2, SOUND_SYSTEM_C_2_5_0},
          {k5_1_4, SOUND_SYSTEM_D_4_5_0},
          {k7_1, SOUND_SYSTEM_I_0_7_0},
          {k7_1_4, SOUND_SYSTEM_J_4_7_0},
      });

  auto it = kInputLayoutToSoundSystem->find(layout);
  if (it == kInputLayoutToSoundSystem->end()) {
    return absl::NotFoundError("Sound system not found for input_layout");
  }
  return it->second;
}

absl::Status CopyLoudness(const LoudnessMetadata& loudness_metadata,
                          iamf_tools_cli_proto::LoudnessInfo& loudness_info) {
  int16_t integrated_loudness_q7_8;
  if (const auto& status = FloatToQ7_8(loudness_metadata.integrated_loudness,
                                       integrated_loudness_q7_8);
      !status.ok()) {
    return status;
  }
  loudness_info.set_integrated_loudness(integrated_loudness_q7_8);

  // Configure the optional true peak value.
  if (loudness_metadata.max_true_peak.has_value()) {
    loudness_info.add_info_type_bit_masks(
        iamf_tools_cli_proto::LOUDNESS_INFO_TYPE_TRUE_PEAK);
    int16_t true_peak_q7_8;
    if (const auto& status =
            FloatToQ7_8(*loudness_metadata.max_true_peak, true_peak_q7_8);
        !status.ok()) {
      return status;
    }
    loudness_info.set_true_peak(true_peak_q7_8);
  }

  // Configure the optional dialogue loudness. IAMF supports other types of
  // anchored loudness, but ADM only encodes dialogue loudness.
  if (loudness_metadata.dialogue_loudness.has_value()) {
    loudness_info.add_info_type_bit_masks(
        iamf_tools_cli_proto::LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS);
    int16_t dialogue_loudness_q7_8;
    if (const auto& status = FloatToQ7_8(*loudness_metadata.dialogue_loudness,
                                         dialogue_loudness_q7_8);
        !status.ok()) {
      return status;
    }

    loudness_info.mutable_anchored_loudness()->set_num_anchored_loudness(1);
    auto* anchor_element =
        loudness_info.mutable_anchored_loudness()->add_anchor_elements();
    anchor_element->set_anchor_element(
        iamf_tools_cli_proto::ANCHOR_TYPE_DIALOGUE);
    anchor_element->set_anchored_loudness(dialogue_loudness_q7_8);
  }

  // ADM does not encode digital peak value, but it is required in IAMF.
  static const int16_t kQ7_8Zero = 0x0000;
  loudness_info.set_digital_peak(kQ7_8Zero);
  return absl::OkStatus();
}

absl::Status SetDefaultLoudnessLayout(
    const LoudnessMetadata& loudness_metadata,
    iamf_tools_cli_proto::MixPresentationSubMix& mix_presentation_sub_mix) {
  auto* layout = mix_presentation_sub_mix.add_layouts();
  const auto& loudness_layout = layout->mutable_loudness_layout();

  // Always set Stereo as a default loudness_layout for each sub-mix.
  loudness_layout->set_layout_type(
      iamf_tools_cli_proto::LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION);
  const auto& sound_system =
      LookupSoundSystemFromInputLayout(IamfInputLayout::kStereo);
  CHECK_OK(sound_system);
  loudness_layout->mutable_ss_layout()->set_sound_system(*sound_system);
  return CopyLoudness(loudness_metadata, *layout->mutable_loudness());
}

absl::Status SubMixAudioElementHandler(
    const AudioObject& audio_object, uint32_t audio_element_id,
    uint32_t common_parameter_rate,
    iamf_tools_cli_proto::SubMixAudioElement& sub_mix_audio_element) {
  sub_mix_audio_element.set_audio_element_id(audio_element_id);

  sub_mix_audio_element.add_localized_element_annotations(
      audio_object.audio_object_label);

  auto* rendering_config = sub_mix_audio_element.mutable_rendering_config();

  const auto& input_layout = LookupInputLayoutFromAudioPackFormatId(
      audio_object.audio_pack_format_id_refs[0]);
  if (!input_layout.ok()) {
    return input_layout.status();
  }

  // Set 'headphones_rendering_mode' to HEADPHONES_RENDERING_MODE_BINAURAL if
  // typeDefinition is binaural, else set it to
  // HEADPHONES_RENDERING_MODE_STEREO.
  if (*input_layout == IamfInputLayout::kBinaural) {
    rendering_config->set_headphones_rendering_mode(
        iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_BINAURAL);
  } else {
    rendering_config->set_headphones_rendering_mode(
        iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_STEREO);
  }

  auto* mix_gain_param_definition =
      sub_mix_audio_element.mutable_element_mix_gain();
  // 'default_mix_gain' for each audio element in a mix presentation is
  // initialized to 0. If the corresponding audioObject in ADM has the 'gain'
  // parameter present, set it to the same.
  int16_t mix_gain_q7_8;
  if (const auto& status = FloatToQ7_8(audio_object.gain, mix_gain_q7_8);
      !status.ok()) {
    return status;
  }
  mix_gain_param_definition->set_default_mix_gain(mix_gain_q7_8);

  auto* param_definition =
      mix_gain_param_definition->mutable_param_definition();
  param_definition->set_parameter_id(0);
  param_definition->set_parameter_rate(common_parameter_rate);
  param_definition->set_param_definition_mode(1);

  return absl::OkStatus();
}

absl::Status MixPresentationLayoutHandler(
    IamfInputLayout input_layout, const LoudnessMetadata& loudness_metadata,
    iamf_tools_cli_proto::MixPresentationSubMix& mix_presentation_sub_mix) {
  auto* layout = mix_presentation_sub_mix.add_layouts();
  const auto& loudness_layout = layout->mutable_loudness_layout();

  // If the input is binaural, configure a "reserved or binaural" layout.
  // Otherwise configure an "ss convention" layout.
  if (input_layout == IamfInputLayout::kBinaural) {
    loudness_layout->set_layout_type(
        iamf_tools_cli_proto::LAYOUT_TYPE_BINAURAL);
    loudness_layout->mutable_reserved_or_binaural_layout()->set_reserved(0);
  } else {
    loudness_layout->set_layout_type(
        iamf_tools_cli_proto::LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION);
    const auto& sound_system = LookupSoundSystemFromInputLayout(input_layout);
    if (!sound_system.ok()) {
      return sound_system.status();
    }
    loudness_layout->mutable_ss_layout()->set_sound_system(*sound_system);
  }

  return CopyLoudness(loudness_metadata, *layout->mutable_loudness());
}

bool IsChannelBasedAndNotStereo(IamfInputLayout input_layout) {
  switch (input_layout) {
    using enum IamfInputLayout;
    case kMono:
    case k5_1:
    case k5_1_2:
    case k5_1_4:
    case k7_1:
    case k7_1_4:
    case kBinaural:
      return true;
    case kStereo:
    case kAmbisonicsOrder1:
    case kAmbisonicsOrder2:
    case kAmbisonicsOrder3:
      return false;
  }
}

}  // namespace

// Sets the required textproto fields for mix_presentation_metadata.
absl::Status MixPresentationHandler::PopulateMixPresentation(
    int32_t mix_presentation_id, const std::vector<AudioObject>& audio_objects,
    const LoudnessMetadata& loudness_metadata,
    iamf_tools_cli_proto::MixPresentationObuMetadata&
        mix_presentation_obu_metadata) {
  mix_presentation_obu_metadata.set_mix_presentation_id(mix_presentation_id);
  mix_presentation_obu_metadata.set_count_label(1);
  mix_presentation_obu_metadata.add_annotations_language("en-us");
  mix_presentation_obu_metadata.add_localized_presentation_annotations(
      "test_mix_pres");
  mix_presentation_obu_metadata.set_num_sub_mixes(1);

  auto& mix_presentation_sub_mix =
      *mix_presentation_obu_metadata.add_sub_mixes();
  mix_presentation_sub_mix.set_num_audio_elements(audio_objects.size());
  for (const auto& audio_object : audio_objects) {
    const auto status = SubMixAudioElementHandler(
        audio_object, audio_object_id_to_audio_element_id_[audio_object.id],
        common_parameter_rate_, *mix_presentation_sub_mix.add_audio_elements());
    if (!status.ok()) {
      return status;
    }
  }

  auto* mix_gain_param_definition =
      mix_presentation_sub_mix.mutable_output_mix_gain();
  auto* param_definition =
      mix_gain_param_definition->mutable_param_definition();

  param_definition->set_parameter_id(0);
  param_definition->set_parameter_rate(common_parameter_rate_);
  param_definition->set_param_definition_mode(1);
  mix_gain_param_definition->set_default_mix_gain(0);
  int32_t num_layouts = 1;

  // A stereo loudness layout is always required by IAMF.
  if (const auto& status =
          SetDefaultLoudnessLayout(loudness_metadata, mix_presentation_sub_mix);
      !status.ok()) {
    return status;
  }

  for (const auto& audio_object : audio_objects) {
    if (num_layouts > 3) {
      break;
    }

    const auto& iamf_input_layout = LookupInputLayoutFromAudioPackFormatId(
        audio_object.audio_pack_format_id_refs[0]);
    if (!iamf_input_layout.ok()) {
      return iamf_input_layout.status();
    }

    if (IsChannelBasedAndNotStereo(*iamf_input_layout)) {
      num_layouts++;
      if (const auto& status = MixPresentationLayoutHandler(
              *iamf_input_layout, loudness_metadata, mix_presentation_sub_mix);
          !status.ok()) {
        return status;
      }
    }
  }

  mix_presentation_sub_mix.set_num_layouts(num_layouts);

  return absl::OkStatus();
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
