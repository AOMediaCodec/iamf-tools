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
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/common/macros.h"
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

absl::Status ClearAndReturnError(
    absl::string_view context,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  profile_versions.clear();
  return absl::InvalidArgumentError(context);
}

int GetNumberOfChannels(const AudioElementWithData& audio_element) {
  int num_channels = 0;
  for (const auto& [substream_id, labels] :
       audio_element.substream_id_to_labels) {
    num_channels += labels.size();
  }
  return num_channels;
}

absl::Status GetNumberOfAudioElementsAndChannels(
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

absl::Status ProfileFilter::FilterProfilesForMixPresentation(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const MixPresentationObu& mix_presentation_obu,
    absl::flat_hash_set<ProfileVersion>& profile_versions) {
  const std::string mix_presentation_id_for_debugging =
      absl::StrCat("Mix presentation with ID= ",
                   mix_presentation_obu.GetMixPresentationId());
  RETURN_IF_NOT_OK(FilterProfileForNumSubmixes(
      mix_presentation_id_for_debugging, mix_presentation_obu.GetNumSubMixes(),
      profile_versions));

  int num_audio_elements_in_mix_presentation;
  int num_channels_in_mix_presentation;
  RETURN_IF_NOT_OK(GetNumberOfAudioElementsAndChannels(
      mix_presentation_id_for_debugging, audio_elements, mix_presentation_obu,
      profile_versions, num_audio_elements_in_mix_presentation,
      num_channels_in_mix_presentation));

  RETURN_IF_NOT_OK(FilterProfilesForNumAudioElements(
      mix_presentation_id_for_debugging, num_audio_elements_in_mix_presentation,
      profile_versions));
  RETURN_IF_NOT_OK(FilterProfilesForNumChannels(
      mix_presentation_id_for_debugging, num_channels_in_mix_presentation,
      profile_versions));

  return absl::OkStatus();
}
}  // namespace iamf_tools
