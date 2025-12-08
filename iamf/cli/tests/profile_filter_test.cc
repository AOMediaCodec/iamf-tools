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

#include <array>
#include <cstdint>
#include <initializer_list>
#include <list>
#include <numeric>
#include <optional>
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
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using ::testing::UnorderedElementsAre;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr DecodedUleb128 kSecondCodecConfigId = 2;

const uint32_t kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kSecondAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 1;
constexpr DecodedUleb128 kSecondSubstreamId = 2;
constexpr DecodedUleb128 kFirstMixPresentationId = 1;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
const uint32_t kCommonMixGainParameterRate = kSampleRate;
const uint8_t kAudioElementReserved = 0;
const int kOneLayer = 1;

constexpr std::array<DecodedUleb128, 1> kZerothOrderAmbisonicsSubstreamId{100};
constexpr std::array<DecodedUleb128, 25> kFourthOrderAmbisonicsSubstreamIds = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

constexpr std::array<uint8_t, 0> kEmptyExtensionConfig{};

using enum ProfileVersion;

// TODO(b/461488730): Diverge behavior of the IAMF v2.0.0 profiles as features
//                    are added.
const absl::flat_hash_set<ProfileVersion> kAllKnownProfileVersions = {
    kIamfSimpleProfile,       kIamfBaseProfile,      kIamfBaseEnhancedProfile,
    kIamfBaseAdvancedProfile, kIamfAdvanced1Profile, kIamfAdvanced2Profile};

const ScalableChannelLayoutConfig kOneLayerStereoConfig = {
    .channel_audio_layer_configs = {
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
         .substream_count = 1,
         .coupled_substream_count = 1}}};

const ScalableChannelLayoutConfig kOneLayerExpandedLayoutLFEConfig = {
    .channel_audio_layer_configs = {{
        .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
        .substream_count = 1,
        .coupled_substream_count = 1,
        .expanded_loudspeaker_layout =
            ChannelAudioLayerConfig::kExpandedLayoutLFE,
    }}};

TEST(FilterProfilesForAudioElement,
     KeepsChannelBasedAudioElementForAllKnownProfiles) {
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerStereoConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     RemovesAllKnownProfilesWhenFirstLayerIsLoudspeakerLayout10) {
  const ScalableChannelLayoutConfig kOneLayerReserved10Config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved10,
           .substream_count = 1,
           .coupled_substream_count = 1}}};
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerReserved10Config);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =

      kAllKnownProfileVersions;

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, all_known_profiles)
                   .ok());

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForAudioElement,
     KeepsChannelBasedAudioElementWhenSubsequentLayersAreReserved) {
  const ScalableChannelLayoutConfig kTwoLayerWithReservedConfig = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .substream_count = 1,
           .coupled_substream_count = 1},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved10,
           .substream_count = 1,
           .coupled_substream_count = 1}}};
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId}, kTwoLayerWithReservedConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     KeepsSceneBasedMonoAudioElementForAllKnownProfiles) {
  constexpr std::array<uint8_t, 1> kMonoChannelMapping{0};
  auto audio_element_obu = AudioElementObu::CreateForMonoAmbisonics(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kMonoChannelMapping);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     KeepsSceneBasedProjectionAudioElementForAllKnownProfiles) {
  constexpr uint8_t kOutputChannelCount = 1;
  constexpr uint8_t kCoupledSubstreamCount = 0;
  constexpr std::array<int16_t, 1> kDemixingMatrix{0};
  auto audio_element_obu = AudioElementObu::CreateForProjectionAmbisonics(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOutputChannelCount, kCoupledSubstreamCount,
      kDemixingMatrix);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     RemovesSimpleProfileWhenFirstLayerIsExpandedLayout) {
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutLFEConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, simple_profile)
                   .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesSimpleProfileForReservedAudioElementType) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementBeginReserved, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, simple_profile)
                   .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesSimpleProfileForReservedAmbisonicsMode) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementSceneBased, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, simple_profile)
                   .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseProfileWhenFirstLayerIsExpandedLayout) {
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutLFEConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseProfileForReservedAudioElementType) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementBeginReserved, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseProfileForReservedAmbisonicsMode) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementSceneBased, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileWhenFirstLayerIsExpandedLayout10_2_9_3) {
  const ScalableChannelLayoutConfig kOneLayerExpandedLayout10_2_9_3Config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayout10_2_9_3}}};

  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayout10_2_9_3Config);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutLfePair) {
  const ScalableChannelLayoutConfig kOneLayerExpandedLayoutLfePairConfig = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutLfePair}}};

  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutLfePairConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutBottom3Ch) {
  const ScalableChannelLayoutConfig kOneLayerExpandedLayoutBottom3ChConfig = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch}}};

  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutBottom3ChConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutReserved16) {
  const ScalableChannelLayoutConfig kOneLayerExpandedLayoutReserved16Config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutReserved16}}};

  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutReserved16Config);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     KeepsBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutLFE) {
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutLFEConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, base_enhanced_profile),
              IsOk());

  EXPECT_FALSE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileForReservedAudioElementType) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementBeginReserved, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileForReservedAmbisonicsMode) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementSceneBased, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

void InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, kZerothOrderAmbisonicsSubstreamId,
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
      kFirstAudioElementId, kCodecConfigId, kFourthOrderAmbisonicsSubstreamIds,
      codec_config_obus, audio_elements);

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

absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
MakeAudioElementsWithCodecConfigIds(
    const absl::flat_hash_map<DecodedUleb128, DecodedUleb128>&
        audio_element_id_to_codec_config_id,
    const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        codec_config_obus) {
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  for (const auto& [audio_element_id, codec_config_id] :
       audio_element_id_to_codec_config_id) {
    const DecodedUleb128 substream_id = audio_elements.size();
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        audio_element_id, codec_config_id, {substream_id}, codec_config_obus,
        audio_elements);
  }
  return audio_elements;
}

MixPresentationObu MakeMixPresentationObuWithAudioElementIdsInSubmixes(
    const std::vector<std::vector<DecodedUleb128>>& audio_element_ids_in_submix,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements) {
  MixGainParamDefinition common_mix_gain_param_definition;
  common_mix_gain_param_definition.parameter_id_ = kCommonMixGainParameterId;
  common_mix_gain_param_definition.parameter_rate_ =
      kCommonMixGainParameterRate;
  common_mix_gain_param_definition.param_definition_mode_ = true;
  common_mix_gain_param_definition.default_mix_gain_ = 0;
  const std::vector<MixPresentationLayout> layouts = {
      {.loudness_layout =
           {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
            .specific_layout =
                LoudspeakersSsConventionLayout{
                    .sound_system =
                        LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
                    .reserved = 0}},
       .loudness = {
           .info_type = 0, .integrated_loudness = 0, .digital_peak = 0}}};

  std::vector<MixPresentationSubMix> sub_mixes;
  for (const auto& audio_element_ids : audio_element_ids_in_submix) {
    std::vector<SubMixAudioElement> sub_mix_audio_elements;
    for (const auto& audio_element_id : audio_element_ids) {
      sub_mix_audio_elements.push_back(SubMixAudioElement{
          .audio_element_id = audio_element_id,
          .localized_element_annotations = {},
          .rendering_config =
              {.headphones_rendering_mode =
                   RenderingConfig::kHeadphonesRenderingModeStereo,
               .reserved = 0,
               .rendering_config_extension_bytes = {}},
          .element_mix_gain = common_mix_gain_param_definition,
      });
    }
    sub_mixes.emplace_back(MixPresentationSubMix{
        .audio_elements = sub_mix_audio_elements,
        .output_mix_gain = common_mix_gain_param_definition,
        .layouts = layouts});
  }

  return MixPresentationObu(ObuHeader(), kFirstMixPresentationId,
                            /*count_label=*/0, {}, {}, sub_mixes);
}

void InitializeDescriptorObusWithNSubmixes(
    int num_submixes,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);

  std::vector<std::vector<DecodedUleb128>> audio_element_ids_in_submix;
  for (int i = 0; i < num_submixes; ++i) {
    const DecodedUleb128 kAudioElementId = i;
    const DecodedUleb128 kSubstreamId = i;
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kAudioElementId, kCodecConfigId, {kSubstreamId}, codec_config_obus,
        audio_elements);
    audio_element_ids_in_submix.push_back({kAudioElementId});
  }
  mix_presentation_obus.push_back(
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          audio_element_ids_in_submix, audio_elements));
}

TEST(FilterProfilesForMixPresentation,
     RemovesSimpleProfileWhenThereAreTwoSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(2, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile)
          .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     AllKnownProfilesDoNotSupportMultipleCodecConfigsInTheFirstSubmix) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus);
  const auto audio_elements_with_different_codec_configs =
      MakeAudioElementsWithCodecConfigIds(
          {{kFirstAudioElementId, kCodecConfigId},
           {kSecondAudioElementId, kSecondCodecConfigId}},
          codec_config_obus);
  const std::vector<DecodedUleb128> kTwoAudioElementsInFirstSubmix = {
      kFirstAudioElementId, kSecondAudioElementId};
  const auto mix_presentation_with_different_codec_configs_in_first_submix =
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          {kTwoAudioElementsInFirstSubmix},
          audio_elements_with_different_codec_configs);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements_with_different_codec_configs,
                  mix_presentation_with_different_codec_configs_in_first_submix,
                  all_known_profiles),
              Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForMixPresentation,
     SomeProfilesSupportMultipleCodecConfigsInDifferentSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus);
  const auto audio_elements_with_different_codec_configs =
      MakeAudioElementsWithCodecConfigIds(
          {{kFirstAudioElementId, kCodecConfigId},
           {kSecondAudioElementId, kSecondCodecConfigId}},
          codec_config_obus);
  const auto
      mix_presentation_with_different_codec_configs_in_different_submixes =
          MakeMixPresentationObuWithAudioElementIdsInSubmixes(
              {{kFirstAudioElementId}, {kSecondAudioElementId}},
              audio_elements_with_different_codec_configs);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements_with_different_codec_configs,
          mix_presentation_with_different_codec_configs_in_different_submixes,
          all_known_profiles),
      IsOk());

  EXPECT_THAT(all_known_profiles, UnorderedElementsAre(kIamfBaseAdvancedProfile,
                                                       kIamfAdvanced1Profile,
                                                       kIamfAdvanced2Profile));
}

TEST(FilterProfilesForMixPresentation,
     SomeProfilesSupportMultipleCodecConfigsWithDifferentBitDepths) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  constexpr uint32_t kSecondCodecConfigBitDepth = 32;
  AddLpcmCodecConfig(
      kSecondCodecConfigId,
      codec_config_obus.at(kCodecConfigId).GetNumSamplesPerFrame(),
      kSecondCodecConfigBitDepth,
      codec_config_obus.at(kCodecConfigId).GetOutputSampleRate(),
      codec_config_obus);

  const auto audio_elements_with_different_bit_depths =
      MakeAudioElementsWithCodecConfigIds(
          {{kFirstAudioElementId, kCodecConfigId},
           {kSecondAudioElementId, kSecondCodecConfigId}},
          codec_config_obus);
  const auto mix_presentation_with_different_bit_depths =
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          {{kFirstAudioElementId}, {kSecondAudioElementId}},
          audio_elements_with_different_bit_depths);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements_with_different_bit_depths,
          mix_presentation_with_different_bit_depths, all_known_profiles),
      IsOk());

  EXPECT_THAT(all_known_profiles, UnorderedElementsAre(kIamfBaseAdvancedProfile,
                                                       kIamfAdvanced1Profile,
                                                       kIamfAdvanced2Profile));
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesWhenCodecConfigsHaveDifferentSampleRates) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  constexpr uint32_t kSecondCodecConfigSampleRate = 96000;
  AddLpcmCodecConfig(
      kSecondCodecConfigId,
      codec_config_obus.at(kCodecConfigId).GetNumSamplesPerFrame(),
      codec_config_obus.at(kCodecConfigId).GetBitDepthToMeasureLoudness(),
      kSecondCodecConfigSampleRate, codec_config_obus);

  const auto audio_elements_with_different_sample_rates =
      MakeAudioElementsWithCodecConfigIds(
          {{kFirstAudioElementId, kCodecConfigId},
           {kSecondAudioElementId, kSecondCodecConfigId}},
          codec_config_obus);
  const auto mix_presentation_with_different_sample_rates =
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          {{kFirstAudioElementId}, {kSecondAudioElementId}},
          audio_elements_with_different_sample_rates);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements_with_different_sample_rates,
          mix_presentation_with_different_sample_rates, all_known_profiles),
      Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesWhenCodecConfigsHaveDifferentSamplesPerFrame) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  constexpr uint32_t kSecondCodecConfigNumSamplesPerFrame = 1024;
  AddLpcmCodecConfig(
      kSecondCodecConfigId, kSecondCodecConfigNumSamplesPerFrame,
      codec_config_obus.at(kCodecConfigId).GetBitDepthToMeasureLoudness(),
      codec_config_obus.at(kCodecConfigId).GetOutputSampleRate(),
      codec_config_obus);

  const auto audio_elements_with_different_samples_per_frame =
      MakeAudioElementsWithCodecConfigIds(
          {{kFirstAudioElementId, kCodecConfigId},
           {kSecondAudioElementId, kSecondCodecConfigId}},
          codec_config_obus);
  const auto mix_presentation_with_different_samples_per_frame =
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          {{kFirstAudioElementId}, {kSecondAudioElementId}},
          audio_elements_with_different_samples_per_frame);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements_with_different_samples_per_frame,
                  mix_presentation_with_different_samples_per_frame,
                  all_known_profiles),
              Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesWhenThereAreTwoNonLpcmCodecConfigs) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  AddFlacCodecConfig(
      kSecondCodecConfigId,
      codec_config_obus.at(kCodecConfigId).GetNumSamplesPerFrame(),
      codec_config_obus.at(kCodecConfigId).GetBitDepthToMeasureLoudness(),
      codec_config_obus.at(kCodecConfigId).GetOutputSampleRate(),
      codec_config_obus);

  const auto audio_elements_with_two_non_lpcm_codec_configs =
      MakeAudioElementsWithCodecConfigIds(
          {{kFirstAudioElementId, kCodecConfigId},
           {kSecondAudioElementId, kSecondCodecConfigId}},
          codec_config_obus);
  const auto mix_presentation_with_two_non_lpcm_codec_configs =
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          {{kFirstAudioElementId}, {kSecondAudioElementId}},
          audio_elements_with_two_non_lpcm_codec_configs);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements_with_two_non_lpcm_codec_configs,
          mix_presentation_with_two_non_lpcm_codec_configs, all_known_profiles),
      Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesWhenThereAreThreeCodecConfigs) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus);
  constexpr DecodedUleb128 kThirdCodecConfigId = 1024;
  AddLpcmCodecConfigWithIdAndSampleRate(kThirdCodecConfigId, kSampleRate,
                                        codec_config_obus);

  constexpr DecodedUleb128 kThirdAudioElementId = 1025;
  const auto audio_elements_with_three_different_codec_configs =
      MakeAudioElementsWithCodecConfigIds(
          {
              {kFirstAudioElementId, kCodecConfigId},
              {kSecondAudioElementId, kSecondCodecConfigId},
              {kThirdAudioElementId, kThirdCodecConfigId},
          },
          codec_config_obus);
  const auto mix_presentation_with_three_different_codec_configs =
      MakeMixPresentationObuWithAudioElementIdsInSubmixes(
          {{kFirstAudioElementId},
           {kSecondAudioElementId, kThirdAudioElementId}},
          audio_elements_with_three_different_codec_configs);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements_with_three_different_codec_configs,
                  mix_presentation_with_three_different_codec_configs,
                  all_known_profiles),
              Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
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
     RemovesSimpleProfileWithReservedHeadphonesRenderingMode2) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved2;
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile)
          .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesSimpleProfileWithReservedHeadphonesRenderingMode3) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved3;
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile)
          .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesBaseProfileWhenThereAreTwoSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(2, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForMixPresentation(
                   audio_elements, mix_presentation_obus.front(), base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
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
     RemovesBaseProfileWithReservedHeadphonesRenderingMode2) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved2;
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForMixPresentation(
                   audio_elements, mix_presentation_obus.front(), base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesBaseProfileWithReservedHeadphonesRenderingMode3) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved3;
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForMixPresentation(
                   audio_elements, mix_presentation_obus.front(), base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesBaseEnhancedProfileWhenThereAreTwoSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(2, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile)
          .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForMixPresentation, SomeProfilesSupportTwoSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(2, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), all_known_profiles),
      IsOk());

  EXPECT_THAT(all_known_profiles, UnorderedElementsAre(kIamfBaseAdvancedProfile,
                                                       kIamfAdvanced1Profile,
                                                       kIamfAdvanced2Profile));
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllKnownProfilesForThreeSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(3, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), all_known_profiles),
      Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
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
     KeepsBaseEnhancedProfileWithAFourthOrderAmbisonicsAudioElement) {
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
     KeepsBaseEnhancedProfileWhenThereAreTwentyEightOrFewerAudioElements) {
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

TEST(FilterProfilesForMixPresentation,
     RemoveBaseEnhancedProfileWhenThereAreMoreThanTwentyEightAudioElements) {
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
     RemovesBaseEnhancedProfileWithReservedHeadphonesRenderingMode2) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved2;
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile)
          .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesBaseEnhancedProfileWithReservedHeadphonesRenderingMode3) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved3;
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile)
          .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllKnownProfilesThatDoNotMeetRequirements) {
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
     KeepsBaseAdvancedProfileWhenThereAreEighteenOrFewerAudioElements) {
  const int kNumAudioElements = 18;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_advanced_profile = {
      kIamfBaseAdvancedProfile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_advanced_profile),
      IsOk());

  EXPECT_TRUE(base_advanced_profile.contains(kIamfBaseAdvancedProfile));
}

TEST(FilterProfilesForMixPresentation,
     KeepsAdvanced1ProfileWhenThereAreEighteenOrFewerAudioElements) {
  const int kNumAudioElements = 18;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> advanced1_profile = {
      kIamfAdvanced1Profile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), advanced1_profile),
      IsOk());

  EXPECT_TRUE(advanced1_profile.contains(kIamfAdvanced1Profile));
}

TEST(FilterProfilesForMixPresentation,
     KeepsAdvanced2ProfileWhenThereAreTwentyEightOrFewerAudioElements) {
  const int kNumAudioElements = 28;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> advanced2_profile = {
      kIamfAdvanced2Profile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), advanced2_profile),
      IsOk());

  EXPECT_TRUE(advanced2_profile.contains(kIamfAdvanced2Profile));
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
     RemovesAllKnownProfilesWhenThereIsAnUnknownAudioElement) {
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

TEST(FilterProfilesForAudioElement,
     RemovesAllKnownProfilesWhenExpandedLayoutIsSignalledButNotPresent) {
  const ScalableChannelLayoutConfig kInvalidExpandedLayout = {
      .channel_audio_layer_configs = {{
          .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
          .substream_count = 1,
          .coupled_substream_count = 1,
          .expanded_loudspeaker_layout = std::nullopt,
      }}};
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, /*reserved=*/0, kCodecConfigId,
      {kFirstSubstreamId}, kInvalidExpandedLayout);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", *audio_element_obu, all_known_profiles)
                   .ok());

  EXPECT_TRUE(all_known_profiles.empty());
}

}  // namespace

}  // namespace iamf_tools
