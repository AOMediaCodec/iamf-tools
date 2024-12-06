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
#include "iamf/cli/profile_filter.h"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

namespace {

constexpr int kSimpleProfileMaxAudioElements = 1;
constexpr int kBaseProfileMaxAudioElements = 2;
constexpr int kBaseEnhancedProfileMaxAudioElements = 28;

constexpr int kSimpleProfileMaxChannels = 16;
constexpr int kBaseProfileMaxChannels = 18;
constexpr int kBaseEnhancedProfileMaxChannels = 28;

absl::Status ClearAndReturnError(
    absl::string_view context,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  profile_versions.clear();
  return absl::InvalidArgumentError(context);
}

absl::Status FilterAudioElementType(
    absl::string_view debugging_context,
    AudioElementObu::AudioElementType audio_element_type,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  switch (audio_element_type) {
    using enum AudioElementObu::AudioElementType;
    case AudioElementObu::kAudioElementChannelBased:
    case AudioElementObu::kAudioElementSceneBased:
      break;
    default:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context, "has audio element type= ", audio_element_type,
        ". But the requested profiles do support not support this type."));
  }
  return absl::OkStatus();
}

// Filters out profiles that do not support specific expanded loudspeaker
// layouts. This function assumes profiles that do not support expanded layout
// have already been filtered out (e.g. kIamfSimpleProfile, kIamfBaseProfile).
absl::Status FilterExpandedLoudspeakerLayout(
    absl::string_view debugging_context,
    std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
        expanded_loudspeaker_layout,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (auto status = ValidateHasValue(expanded_loudspeaker_layout,
                                     "expanded_loudspeaker_layout");
      !status.ok()) {
    return ClearAndReturnError(
        absl::StrCat(debugging_context, status.message()), profile_versions);
  }

  switch (*expanded_loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
    case kExpandedLayoutLFE:
    case kExpandedLayoutStereoS:
    case kExpandedLayoutStereoSS:
    case kExpandedLayoutStereoRS:
    case kExpandedLayoutStereoTF:
    case kExpandedLayoutStereoTB:
    case kExpandedLayoutTop4Ch:
    case kExpandedLayout3_0_ch:
    case kExpandedLayout9_1_6_ch:
    case kExpandedLayoutStereoF:
    case kExpandedLayoutStereoSi:
    case kExpandedLayoutStereoTpSi:
    case kExpandedLayoutTop6Ch:
      break;
    case kExpandedLayoutReserved13:
    case kExpandedLayoutReserved255:
    default:
      // Other layouts are reserved and not supported by base-enhanced profile.
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
  }
  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context,
        "has expanded_loudspeaker_layout= ", *expanded_loudspeaker_layout,
        ". But the requested profiles do support not support this type."));
  }
  return absl::OkStatus();
}

absl::Status FilterChannelBasedConfig(
    absl::string_view debugging_context,
    const AudioElementObu& audio_element_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (!std::holds_alternative<ScalableChannelLayoutConfig>(
          audio_element_obu.config_)) {
    return ClearAndReturnError(
        absl::StrCat(
            debugging_context,
            "signals that it is a channel-based audio element, but it does not "
            "hold an `ScalableChannelLayoutConfig`."),
        profile_versions);
  }
  const auto& scalable_channel_layout_config =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_);
  if (scalable_channel_layout_config.channel_audio_layer_configs.empty()) {
    return ClearAndReturnError(
        absl::StrCat(debugging_context, ". Expected at least one layer."),
        profile_versions);
  }

  const auto& first_channel_audio_layer_config =
      scalable_channel_layout_config.channel_audio_layer_configs[0];

  switch (first_channel_audio_layer_config.loudspeaker_layout) {
    using enum ChannelAudioLayerConfig::LoudspeakerLayout;
    case kLayoutMono:
    case kLayoutStereo:
    case kLayout5_1_ch:
    case kLayout5_1_2_ch:
    case kLayout5_1_4_ch:
    case kLayout7_1_ch:
    case kLayout7_1_2_ch:
    case kLayout7_1_4_ch:
    case kLayout3_1_2_ch:
    case kLayoutBinaural:
      break;
    case kLayoutReserved10:
    case kLayoutReserved11:
    case kLayoutReserved12:
    case kLayoutReserved13:
    case kLayoutReserved14:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      break;
    case kLayoutExpanded:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      RETURN_IF_NOT_OK(FilterExpandedLoudspeakerLayout(
          debugging_context,
          first_channel_audio_layer_config.expanded_loudspeaker_layout,
          profile_versions));
      break;
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context, "has the first loudspeaker_layout= ",
        first_channel_audio_layer_config.loudspeaker_layout,
        ". But the requested profiles do support not support this type."));
  }

  return absl::OkStatus();
}

absl::Status FilterAmbisonicsConfig(
    absl::string_view debugging_context,
    const AudioElementObu& audio_element_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (!std::holds_alternative<AmbisonicsConfig>(audio_element_obu.config_)) {
    return ClearAndReturnError(
        absl::StrCat(
            debugging_context,
            "signals that it is a scene-based audio element, but it does not "
            "hold an `AmbisonicsConfig`."),
        profile_versions);
  }

  auto ambisonics_mode =
      std::get<AmbisonicsConfig>(audio_element_obu.config_).ambisonics_mode;
  switch (ambisonics_mode) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case AmbisonicsConfig::kAmbisonicsModeMono:
    case AmbisonicsConfig::kAmbisonicsModeProjection:
      break;
    default:
      profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseProfile);
      profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
      break;
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        debugging_context, "has ambisonics_mode= ", ambisonics_mode,
        ". But the requested profiles do support not support this mode."));
  }
  return absl::OkStatus();
}

absl::Status FilterProfileForNumSubmixes(
    absl::string_view mix_presentation_id_for_debugging,
    int num_submixes_in_mix_presentation,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (num_submixes_in_mix_presentation > 1) {
    profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
    profile_versions.erase(ProfileVersion::kIamfBaseProfile);
    profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_submixes_in_mix_presentation,
                     " sub mixes, but the requested profiles "
                     "do not support this number of sub-mixes."));
  }
  return absl::OkStatus();
}

absl::Status FilterProfileForHeadphonesRenderingMode(
    absl::string_view mix_presentation_id_for_debugging,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
    for (const auto& sub_mix_audio_element : sub_mix.audio_elements) {
      switch (
          sub_mix_audio_element.rendering_config.headphones_rendering_mode) {
        using enum RenderingConfig::HeadphonesRenderingMode;
        case kHeadphonesRenderingModeReserved2:
        case kHeadphonesRenderingModeReserved3:
          profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseProfile);
          profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
          break;
        default:
          break;
      }

      if (profile_versions.empty()) {
        return absl::InvalidArgumentError(absl::StrCat(
            mix_presentation_id_for_debugging,
            " has an audio element with headphones rendering mode= ",
            sub_mix_audio_element.rendering_config.headphones_rendering_mode,
            " sub mixes, but the requested profiles do support not this "
            "mode."));
      }
    }
  }

  return absl::OkStatus();
}

int GetNumberOfChannels(const AudioElementWithData& audio_element) {
  int num_channels = 0;
  for (const auto& [substream_id, labels] :
       audio_element.substream_id_to_labels) {
    num_channels += labels.size();
  }
  return num_channels;
}

absl::Status FilterAudioElementsAndGetNumberOfAudioElementsAndChannels(
    absl::string_view mix_presentation_id_for_debugging,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions,
    int& num_audio_elements_in_mix_presentation,
    int& num_channels_in_mix_presentation) {
  num_audio_elements_in_mix_presentation = 0;
  num_channels_in_mix_presentation = 0;
  for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
    num_audio_elements_in_mix_presentation += sub_mix.num_audio_elements;
    for (const auto& sub_mix_audio_element : sub_mix.audio_elements) {
      auto iter = audio_elements.find(sub_mix_audio_element.audio_element_id);
      if (iter == audio_elements.end()) {
        return ClearAndReturnError(
            absl::StrCat(mix_presentation_id_for_debugging,
                         " has Audio Element ID= ",
                         sub_mix_audio_element.audio_element_id,
                         " , but there is no Audio Element with that ID."),
            profile_versions);
      }
      RETURN_IF_NOT_OK(ProfileFilter::FilterProfilesForAudioElement(
          mix_presentation_id_for_debugging, iter->second.obu,
          profile_versions));

      num_channels_in_mix_presentation += GetNumberOfChannels(iter->second);
    }
  }
  return absl::OkStatus();
}

absl::Status FilterProfilesForNumAudioElements(
    absl::string_view mix_presentation_id_for_debugging,
    int num_audio_elements_in_mix_presentation,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (num_audio_elements_in_mix_presentation > kSimpleProfileMaxAudioElements) {
    profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
  }
  if (num_audio_elements_in_mix_presentation > kBaseProfileMaxAudioElements) {
    profile_versions.erase(ProfileVersion::kIamfBaseProfile);
  }
  if (num_audio_elements_in_mix_presentation >
      kBaseEnhancedProfileMaxAudioElements) {
    profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_audio_elements_in_mix_presentation,
                     " audio elements, but no "
                     "profile supports this number of audio elements."));
  }

  return absl::OkStatus();
}

absl::Status FilterProfilesForNumChannels(
    absl::string_view mix_presentation_id_for_debugging,
    int num_channels_in_mix_presentation,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  if (num_channels_in_mix_presentation > kSimpleProfileMaxChannels) {
    profile_versions.erase(ProfileVersion::kIamfSimpleProfile);
  }
  if (num_channels_in_mix_presentation > kBaseProfileMaxChannels) {
    profile_versions.erase(ProfileVersion::kIamfBaseProfile);
  }
  if (num_channels_in_mix_presentation > kBaseEnhancedProfileMaxChannels) {
    profile_versions.erase(ProfileVersion::kIamfBaseEnhancedProfile);
  }

  if (profile_versions.empty()) {
    return absl::InvalidArgumentError(
        absl::StrCat(mix_presentation_id_for_debugging, " has ",
                     num_channels_in_mix_presentation,
                     " channels, but no "
                     "profile supports this number of channels."));
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status ProfileFilter::FilterProfilesForAudioElement(
    absl::string_view debugging_context,
    const AudioElementObu& audio_element_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  const std::string context_and_audio_element_id_for_debugging = absl::StrCat(
      debugging_context,
      " Audio element ID= ", audio_element_obu.GetAudioElementId(), " ");

  RETURN_IF_NOT_OK(FilterAudioElementType(
      context_and_audio_element_id_for_debugging,
      audio_element_obu.GetAudioElementType(), profile_versions));
  // Filter any type-specific properties.
  switch (audio_element_obu.GetAudioElementType()) {
    using enum AudioElementObu::AudioElementType;
    case AudioElementObu::kAudioElementChannelBased:
      RETURN_IF_NOT_OK(
          FilterChannelBasedConfig(context_and_audio_element_id_for_debugging,
                                   audio_element_obu, profile_versions));
      break;
    case AudioElementObu::kAudioElementSceneBased:
      RETURN_IF_NOT_OK(
          FilterAmbisonicsConfig(context_and_audio_element_id_for_debugging,
                                 audio_element_obu, profile_versions));
      break;
    default:
      break;
  }

  return absl::OkStatus();
}

absl::Status ProfileFilter::FilterProfilesForMixPresentation(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  const std::string mix_presentation_id_for_debugging =
      absl::StrCat("Mix presentation with ID= ",
                   mix_presentation_obu.GetMixPresentationId());
  MAYBE_RETURN_IF_NOT_OK(FilterProfileForNumSubmixes(
      mix_presentation_id_for_debugging, mix_presentation_obu.GetNumSubMixes(),
      profile_versions));

  MAYBE_RETURN_IF_NOT_OK(FilterProfileForHeadphonesRenderingMode(
      mix_presentation_id_for_debugging, mix_presentation_obu,
      profile_versions));

  int num_audio_elements_in_mix_presentation;
  int num_channels_in_mix_presentation;
  MAYBE_RETURN_IF_NOT_OK(
      FilterAudioElementsAndGetNumberOfAudioElementsAndChannels(
          mix_presentation_id_for_debugging, audio_elements,
          mix_presentation_obu, profile_versions,
          num_audio_elements_in_mix_presentation,
          num_channels_in_mix_presentation));

  MAYBE_RETURN_IF_NOT_OK(FilterProfilesForNumAudioElements(
      mix_presentation_id_for_debugging, num_audio_elements_in_mix_presentation,
      profile_versions));
  MAYBE_RETURN_IF_NOT_OK(FilterProfilesForNumChannels(
      mix_presentation_id_for_debugging, num_channels_in_mix_presentation,
      profile_versions));

  return absl::OkStatus();
}

}  // namespace iamf_tools
