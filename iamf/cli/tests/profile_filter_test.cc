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

constexpr DecodedUleb128 kCodecConfigId = 1;
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

using enum ProfileVersion;

const absl::flat_hash_set<ProfileVersion> kAllKnownProfileVersions = {
    kIamfSimpleProfile, kIamfBaseProfile, kIamfBaseEnhancedProfile};

TEST(FilterProfilesForAudioElement,
     KeepsChannelBasedAudioElementForAllKnownProfiles) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo;
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     RemovesAllKnownProfilesWhenFirstLayerIsLoudspeakerLayout10) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved10;
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, all_known_profiles)
                   .ok());

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(FilterProfilesForAudioElement,
     KeepsChannelBasedAudioElementWhenSubsequentLayersAreReserved) {
  const int kNumSubstreams = 2;
  const int kNumLayers = 2;
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(kNumSubstreams);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kNumLayers, kAudioElementReserved),
              IsOk());
  std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
      .channel_audio_layer_configs[0]
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo;
  std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
      .channel_audio_layer_configs[1]
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved10;

  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     KeepsSceneBasedMonoAudioElementForAllKnownProfiles) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementSceneBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeAmbisonicsMono(1, 1), IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     KeepsSceneBasedProjectionAudioElementForAllKnownProfiles) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementSceneBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeAmbisonicsProjection(1, 1, 0),
              IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     RemovesSimpleProfileWhenFirstLayerIsExpandedLayout) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded;
  first_layer.expanded_loudspeaker_layout =
      ChannelAudioLayerConfig::kExpandedLayoutLFE;
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, simple_profile)
                   .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesSimpleProfileForReservedAudioElementType) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementBeginReserved,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeExtensionConfig();
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, simple_profile)
                   .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesSimpleProfileForReservedAmbisonicsMode) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementSceneBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeExtensionConfig();
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, simple_profile)
                   .ok());

  EXPECT_TRUE(simple_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseProfileWhenFirstLayerIsExpandedLayout) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded;
  first_layer.expanded_loudspeaker_layout =
      ChannelAudioLayerConfig::kExpandedLayoutLFE;
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseProfileForReservedAudioElementType) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementBeginReserved,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeExtensionConfig();
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseProfileForReservedAmbisonicsMode) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementSceneBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeExtensionConfig();
  absl::flat_hash_set<ProfileVersion> base_profile = {kIamfBaseProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, base_profile)
                   .ok());

  EXPECT_TRUE(base_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutReserved13) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded;
  first_layer.expanded_loudspeaker_layout =
      ChannelAudioLayerConfig::kExpandedLayoutReserved13;
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     KeepsBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutLFE) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded;
  first_layer.expanded_loudspeaker_layout =
      ChannelAudioLayerConfig::kExpandedLayoutLFE;
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", audio_element_obu, base_enhanced_profile),
              IsOk());

  EXPECT_FALSE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileForReservedAudioElementType) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementBeginReserved,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeExtensionConfig();
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, base_enhanced_profile)
                   .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST(FilterProfilesForAudioElement,
     RemovesBaseEnhancedProfileForReservedAmbisonicsMode) {
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementSceneBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeExtensionConfig();
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, base_enhanced_profile)
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

void InitializeDescriptorObusWithTwoSubmixes(
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
  MixGainParamDefinition common_mix_gain_param_definition;
  common_mix_gain_param_definition.parameter_id_ = kCommonMixGainParameterId;
  common_mix_gain_param_definition.parameter_rate_ =
      kCommonMixGainParameterRate;
  common_mix_gain_param_definition.param_definition_mode_ = true;
  common_mix_gain_param_definition.default_mix_gain_ = 0;
  const RenderingConfig kRenderingConfig = {
      .headphones_rendering_mode =
          RenderingConfig::kHeadphonesRenderingModeStereo,
      .reserved = 0,
      .rendering_config_extension_bytes = {}};
  const MixPresentationLayout kStereoLayout = {
      .loudness_layout =
          {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
           .specific_layout =
               LoudspeakersSsConventionLayout{
                   .sound_system =
                       LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
                   .reserved = 0}},
      .loudness = {
          .info_type = 0, .integrated_loudness = 0, .digital_peak = 0}};
  std::vector<MixPresentationSubMix> sub_mixes;
  sub_mixes.push_back({.audio_elements = {{
                           .audio_element_id = kFirstAudioElementId,
                           .localized_element_annotations = {},
                           .rendering_config = kRenderingConfig,
                           .element_mix_gain = common_mix_gain_param_definition,
                       }},
                       .output_mix_gain = common_mix_gain_param_definition,
                       .layouts = {kStereoLayout}});
  sub_mixes.push_back({.audio_elements = {{
                           .audio_element_id = kSecondAudioElementId,
                           .localized_element_annotations = {},
                           .rendering_config = kRenderingConfig,
                           .element_mix_gain = common_mix_gain_param_definition,
                       }},
                       .output_mix_gain = common_mix_gain_param_definition,
                       .layouts = {kStereoLayout}});

  mix_presentation_obus.push_back(
      MixPresentationObu(ObuHeader(), kFirstMixPresentationId,
                         /*count_label=*/0, {}, {}, sub_mixes));
}

TEST(FilterProfilesForMixPresentation,
     RemovesSimpleProfileWhenThereAreTwoSubmixes) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusWithTwoSubmixes(codec_config_obus, audio_elements,
                                          mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> simple_profile = {kIamfSimpleProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), simple_profile)
          .ok());

  EXPECT_TRUE(simple_profile.empty());
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
  InitializeDescriptorObusWithTwoSubmixes(codec_config_obus, audio_elements,
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
  InitializeDescriptorObusWithTwoSubmixes(codec_config_obus, audio_elements,
                                          mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_FALSE(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_enhanced_profile)
          .ok());

  EXPECT_TRUE(base_enhanced_profile.empty());
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
  AudioElementObu audio_element_obu(ObuHeader(), kFirstAudioElementId,
                                    AudioElementObu::kAudioElementChannelBased,
                                    kAudioElementReserved, kCodecConfigId);
  audio_element_obu.InitializeAudioSubstreams(1);
  ASSERT_THAT(audio_element_obu.InitializeScalableChannelLayout(
                  kOneLayer, kAudioElementReserved),
              IsOk());
  auto& first_layer =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs[0];
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded;
  first_layer.expanded_loudspeaker_layout = std::nullopt;
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_FALSE(ProfileFilter::FilterProfilesForAudioElement(
                   "", audio_element_obu, all_known_profiles)
                   .ok());

  EXPECT_TRUE(all_known_profiles.empty());
}

}  // namespace

}  // namespace iamf_tools
