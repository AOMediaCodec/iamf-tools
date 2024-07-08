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
#include <initializer_list>
#include <list>
#include <numeric>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 1;
const uint32_t kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kSecondAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 1;
constexpr DecodedUleb128 kSecondSubstreamId = 2;
constexpr DecodedUleb128 kFirstMixPresentationId = 1;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
const uint32_t kCommonMixGainParameterRate = kSampleRate;

const std::vector<DecodedUleb128> kSubstreamIdsForFourthOrderAmbisonics = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

using enum ProfileVersion;

const absl::flat_hash_set<ProfileVersion> kAllKnownProfileVersions = {
    kIamfSimpleProfile, kIamfBaseProfile, kIamfBaseEnhancedProfile};

void InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
}

void InitializeDescriptorObusForOneFourthOrderAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId,
      kSubstreamIdsForFourthOrderAmbisonics, codec_config_obus, audio_elements);

  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
}

void InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus, audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId, kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
}

void InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
    int num_audio_elements,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  // Create audio elements where the audio element IDs match the sole substream
  // IDs.
  std::vector<DecodedUleb128> ids(num_audio_elements, 0);
  std::iota(ids.begin(), ids.end(), 0);
  for (const auto& id : ids) {
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        /*audio_element_id=*/id, kCodecConfigId, /*substream_ids=*/{id},
        codec_config_obus, audio_elements);
  }
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, /*audio_element_ids=*/ids,
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
}

TEST(FilterProfilesForMixPresentation,
     KeepsSimpleProfileWhenThereIsOnlyOneAudioElement) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile),
      IsOk());

  EXPECT_TRUE(simple_profile.contains(kIamfSimpleProfile));
}

TEST(FilterProfilesForMixPresentation,
     RemovesSimpleProfileWhenThereAreMultipleAudioElements) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile)
          .ok());
  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesSimpleProfileWhenThereAreMoreThanSixteenChannels) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneFourthOrderAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile)
          .ok());
  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     KeepsBaseProfileWhenThereIsOnlyOneAudioElement) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), base_profile),
              IsOk());

  EXPECT_TRUE(base_profile.contains(kIamfBaseProfile));
}

TEST(FilterProfilesForMixPresentation,
     RemovesBaseProfileWhenThereAreMoreThanEighteenChannels) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneFourthOrderAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForMixPresentation(
                   audio_elements, mix_presentation_obus.front(), base_profile)
                   .ok());
  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     KeepsBaseEnhancedProfileWhenThereAreTwoAudioElements) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), base_profile),
              IsOk());

  EXPECT_TRUE(base_profile.contains(kIamfBaseProfile));
}

TEST(FilterProfilesForMixPresentation,
     KeepsBaseEnhancedProfileWhenThereIsOnlyOneAudioElement) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile),
      IsOk());

  EXPECT_TRUE(base_enhanced_profile.contains(kIamfBaseEnhancedProfile));
}

TEST(FilterProfilesForMixPresentation,
     KeepsBaseEnhancedProfileWhenWithAFourthOrderAmbisonicsAudioElement) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneFourthOrderAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile),
      IsOk());

  EXPECT_TRUE(base_enhanced_profile.contains(kIamfBaseEnhancedProfile));
}

TEST(FilterProfilesForMixPresentation,
     KeepsBaseEnhancedProfileWhenWhenThereAreTwentyEightOrFewerAudioElements) {
  const int kNumAudioElements = 28;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile),
      IsOk());

  EXPECT_TRUE(base_enhanced_profile.contains(kIamfBaseEnhancedProfile));
}

TEST(
    FilterProfilesForMixPresentation,
    RemoveBaseEnhancedProfileWhenWhenThereAreMoreThanTwentyEightAudioElements) {
  const int kNumAudioElements = 29;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile)
          .ok());

  EXPECT_FALSE(base_enhanced_profile.contains(kIamfBaseEnhancedProfile));
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesThatDoNotMeetRequirements) {
  const int kNumAudioElements = 28;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles_to_filter =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), profiles_to_filter),
      IsOk());

  EXPECT_TRUE(profiles_to_filter.contains(kIamfBaseEnhancedProfile));
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllKnownProfilesWhenThereAreMoreThanTwentyEightAudioElements) {
  const int kNumAudioElements = 29;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), all_known_profiles)
          .ok());

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesWhenThereIsAnUnknownAudioElement) {
  constexpr DecodedUleb128 kUnknownAudioElementId = 1000;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  // Omit adding an audio element.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kUnknownAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          kNoAudioElements, mix_presentation_obus.front(), all_known_profiles)
          .ok());

  EXPECT_TRUE(all_known_profiles.empty());
}

}  // namespace

}  // namespace iamf_tools
