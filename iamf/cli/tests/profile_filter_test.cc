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
#include <numeric>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using AudioElementsById = DescriptorObus::AudioElementsById;
using CodecConfigsById = DescriptorObus::CodecConfigsById;
using MixPresentationObus = DescriptorObus::MixPresentationObus;
using ::testing::Not;
using ::testing::UnorderedElementsAre;
using ::testing::Values;
using ::testing::ValuesIn;

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

const absl::flat_hash_set<ProfileVersion> kV1ProfileVersions = {
    kIamfSimpleProfile, kIamfBaseProfile, kIamfBaseEnhancedProfile};

const absl::flat_hash_set<ProfileVersion> kV2ProfileVersions = {
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
     KeepsObjectBasedAudioElementForSupportedProfiles) {
  auto audio_element_obu = AudioElementObu::CreateForObjects(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      kFirstSubstreamId, *ObjectsConfig::Create(1, {}));
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> supported_profiles = kV2ProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, supported_profiles),
              IsOk());

  EXPECT_EQ(supported_profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     KeepsObjectBasedAudioElementForSubsetOfProfiles) {
  auto audio_element_obu = AudioElementObu::CreateForObjects(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      kFirstSubstreamId, *ObjectsConfig::Create(1, {}));
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> input_profiles = kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, input_profiles),
              IsOk());

  EXPECT_EQ(input_profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForAudioElement,
     RemovesObjectBasedAudioElementForUnsupportedProfiles) {
  auto audio_element_obu = AudioElementObu::CreateForObjects(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      kFirstSubstreamId, *ObjectsConfig::Create(1, {}));
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> input_profiles = kV1ProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, input_profiles),
              Not(IsOk()));

  EXPECT_TRUE(input_profiles.empty());
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

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              Not(IsOk()));

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
  auto audio_element_obu = AudioElementObu::CreateForAmbisonics(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, MakeFullOrderAmbisonicsMonoConfig(0));
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
  auto projection_config = AmbisonicsProjectionConfig::Create(
      kOutputChannelCount, 1, kCoupledSubstreamCount, kDemixingMatrix);
  ASSERT_THAT(projection_config, IsOk());
  auto audio_element_obu = AudioElementObu::CreateForAmbisonics(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId},
      AmbisonicsConfig{.ambisonics_config = *projection_config});
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kAllKnownProfileVersions);
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
     RemovesBaseEnhancedProfileWhenFirstLayerIsExpandedLayoutReserved20) {
  const ScalableChannelLayoutConfig kOneLayerExpandedLayoutReserved20Config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutReserved20}}};
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutReserved20Config);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, base_enhanced_profile),
              Not(IsOk()));

  EXPECT_TRUE(base_enhanced_profile.empty());
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

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

void InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
    CodecConfigsById& codec_config_obus, AudioElementsById& audio_elements,
    MixPresentationObus& mix_presentation_obus) {
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
    CodecConfigsById& codec_config_obus, AudioElementsById& audio_elements,
    MixPresentationObus& mix_presentation_obus) {
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

void InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
    int num_audio_elements, CodecConfigsById& codec_config_obus,
    AudioElementsById& audio_elements,
    MixPresentationObus& mix_presentation_obus) {
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

AudioElementsById MakeAudioElementsWithCodecConfigIds(
    const absl::flat_hash_map<DecodedUleb128, DecodedUleb128>&
        audio_element_id_to_codec_config_id,
    const CodecConfigsById& codec_config_obus) {
  AudioElementsById audio_elements;
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
    const AudioElementsById& audio_elements) {
  MixGainParamDefinition common_mix_gain_param_definition(
      MakeScheduleInParameterBlockBaseArgs(kCommonMixGainParameterId,
                                           kCommonMixGainParameterRate));
  common_mix_gain_param_definition.default_mix_gain_ =
      QFormatOrFloatingPoint::MakeFromQ7_8(0);
  const std::vector<MixPresentationLayout> layouts = {
      {.loudness_layout =
           {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
            .specific_layout =
                LoudspeakersSsConventionLayout{
                    .sound_system =
                        LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
                    .reserved = 0}},
       .loudness = {
           .integrated_loudness = QFormatOrFloatingPoint::MakeFromQ7_8(0),
           .digital_peak = QFormatOrFloatingPoint::MakeFromQ7_8(0)}}};

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
    int num_submixes, CodecConfigsById& codec_config_obus,
    AudioElementsById& audio_elements,
    MixPresentationObus& mix_presentation_obus) {
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
     AllKnownProfilesDoNotSupportMultipleCodecConfigsInTheFirstSubmix) {
  CodecConfigsById codec_config_obus;
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
  CodecConfigsById codec_config_obus;
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

  EXPECT_EQ(all_known_profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForMixPresentation,
     SomeProfilesSupportMultipleCodecConfigsWithDifferentBitDepths) {
  CodecConfigsById codec_config_obus;
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

  EXPECT_EQ(all_known_profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForMixPresentation,
     SomeProfilesSupportHeadphonesRenderingModeBinauralHeadLocked) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeBinauralHeadLocked;
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), all_known_profiles),
      IsOk());

  EXPECT_EQ(all_known_profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllProfilesWhenCodecConfigsHaveDifferentSampleRates) {
  CodecConfigsById codec_config_obus;
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
  CodecConfigsById codec_config_obus;
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
  CodecConfigsById codec_config_obus;
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
  CodecConfigsById codec_config_obus;
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

TEST(FilterProfilesForMixPresentation, SomeProfilesSupportTwoSubmixes) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(2, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), all_known_profiles),
      IsOk());

  EXPECT_EQ(all_known_profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForMixPresentation,
     RemovesAllKnownProfilesForThreeSubmixes) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
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
     KeepsBaseEnhancedProfileWithAFourthOrderAmbisonicsAudioElement) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
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
     RemovesAllKnownProfilesWhenThereAreMoreThanTwentyEightAudioElements) {
  const int kNumAudioElements = 29;
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      kNumAudioElements, codec_config_obus, audio_elements,
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
     RemovesAllKnownProfilesThatDoNotMeetRequirements) {
  const int kNumAudioElements = 28;
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
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
     RemovesAllKnownProfilesWhenThereIsAnUnknownAudioElement) {
  constexpr DecodedUleb128 kUnknownAudioElementId = 1000;
  const AudioElementsById kNoAudioElements;
  // Omit adding an audio element.
  MixPresentationObus mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kUnknownAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          kNoAudioElements, mix_presentation_obus.front(), all_known_profiles),
      Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

const auto kPolarParam = RenderingConfigParamDefinition::Create(
    PolarParamDefinition(ParamDefinition::BaseArgs{}), {});
const auto kCart8Param = RenderingConfigParamDefinition::Create(
    Cart8ParamDefinition(ParamDefinition::BaseArgs{}), {});
const auto kCart16Param = RenderingConfigParamDefinition::Create(
    Cart16ParamDefinition(ParamDefinition::BaseArgs{}), {});
const auto kDualPolarParam = RenderingConfigParamDefinition::Create(
    DualPolarParamDefinition(ParamDefinition::BaseArgs{}), {});
const auto kDualCart8Param = RenderingConfigParamDefinition::Create(
    DualCart8ParamDefinition(ParamDefinition::BaseArgs{}), {});
const auto kDualCart16Param = RenderingConfigParamDefinition::Create(
    DualCart16ParamDefinition(ParamDefinition::BaseArgs{}), {});

void InitializeDescriptorObusForObjectAudioElement(
    uint8_t num_objects,
    const std::vector<RenderingConfigParamDefinition>& param_definitions,
    CodecConfigsById& codec_config_obus, AudioElementsById& audio_elements,
    MixPresentationObus& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  auto objects_config = ObjectsConfig::Create(num_objects, {});
  ASSERT_THAT(objects_config, IsOk());
  auto audio_element_obu = AudioElementObu::CreateForObjects(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      kFirstSubstreamId, *objects_config);
  ASSERT_THAT(audio_element_obu, IsOk());
  auto audio_element_with_data =
      ObuWithDataGenerator::GenerateAudioElementWithData(codec_config_obus,
                                                         *audio_element_obu);
  ASSERT_THAT(audio_element_with_data, IsOk());
  audio_elements.emplace(kFirstAudioElementId, *audio_element_with_data);

  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.rendering_config_param_definitions = param_definitions;
}

void AppendAudioElementToFirstSubmix(
    DecodedUleb128 audio_element_id,
    const std::vector<RenderingConfigParamDefinition>& param_definitions,
    MixPresentationObu& mix_presentation_obu) {
  auto sub_mix_audio_element =
      mix_presentation_obu.sub_mixes_.front().audio_elements.front();
  sub_mix_audio_element.audio_element_id = audio_element_id;
  sub_mix_audio_element.rendering_config.rendering_config_param_definitions =
      param_definitions;
  mix_presentation_obu.sub_mixes_.front().audio_elements.push_back(
      sub_mix_audio_element);
}

TEST(
    FilterProfilesForMixPresentation,
    RemovesAllKnownProfilesWhenNonObjectAudioElementHasRenderingConfigParamDefinitions) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.rendering_config_param_definitions.push_back(
          kPolarParam);
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), all_known_profiles),
      Not(IsOk()));

  EXPECT_TRUE(all_known_profiles.empty());
}

TEST(
    FilterProfilesForMixPresentation,
    KeepsBaseAdvancedProfileWhenMixPresentationHasMultipleObjectAudioElements) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  auto second_objects_config = ObjectsConfig::Create(/*num_objects=*/1, {});
  ASSERT_THAT(second_objects_config, IsOk());
  auto second_object_obu = AudioElementObu::CreateForObjects(
      ObuHeader(), kSecondAudioElementId, kAudioElementReserved, kCodecConfigId,
      kSecondSubstreamId, *second_objects_config);
  ASSERT_THAT(second_object_obu, IsOk());
  auto second_object_with_data =
      ObuWithDataGenerator::GenerateAudioElementWithData(codec_config_obus,
                                                         *second_object_obu);
  ASSERT_THAT(second_object_with_data, IsOk());
  audio_elements.emplace(kSecondAudioElementId, *second_object_with_data);
  AppendAudioElementToFirstSubmix(kSecondAudioElementId, {kPolarParam},
                                  mix_presentation_obus.front());
  absl::flat_hash_set<ProfileVersion> profiles = kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              IsOk());

  EXPECT_EQ(profiles, kV2ProfileVersions);
}

TEST(FilterProfilesForMixPresentation,
     RemovesBaseAdvancedProfileWhenMixingObjectAndSceneBasedAudioElements) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus, audio_elements);
  AppendAudioElementToFirstSubmix(kSecondAudioElementId, {},
                                  mix_presentation_obus.front());
  absl::flat_hash_set<ProfileVersion> profiles = kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              IsOk());

  EXPECT_THAT(profiles, UnorderedElementsAre(kIamfAdvanced1Profile,
                                             kIamfAdvanced2Profile));
}

TEST(
    FilterProfilesForMixPresentation,
    FailsForBaseAdvancedProfileOnlyWhenMixingObjectAndChannelBasedAudioElements) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId}, codec_config_obus, audio_elements);
  AppendAudioElementToFirstSubmix(kSecondAudioElementId, {},
                                  mix_presentation_obus.front());
  absl::flat_hash_set<ProfileVersion> base_advanced_profile = {
      kIamfBaseAdvancedProfile};

  EXPECT_THAT(
      ProfileFilter::FilterProfilesForMixPresentation(
          audio_elements, mix_presentation_obus.front(), base_advanced_profile),
      Not(IsOk()));

  EXPECT_TRUE(base_advanced_profile.empty());
}

TEST(ValidateMixPresentationsForPrimaryProfile,
     SucceedsWhenFirstMixPresentationIsObjectsOnly) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId}, codec_config_obus, audio_elements);
  constexpr DecodedUleb128 kSecondMixPresentationId = 2;
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, {kFirstAudioElementId, kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);

  EXPECT_THAT(
      ProfileFilter::ValidateMixPresentationsForPrimaryProfile(
          kIamfBaseAdvancedProfile, audio_elements, mix_presentation_obus),
      IsOk());
}

TEST(ValidateMixPresentationsForPrimaryProfile,
     FailsWhenObjectBasedMixPresentationIsSecondAndFirstIsChannelBased) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId}, codec_config_obus, audio_elements);
  // Put the channel-based Mix Presentation first and the object-based one
  // second.
  mix_presentation_obus.clear();
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
  constexpr DecodedUleb128 kSecondMixPresentationId = 2;
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);

  EXPECT_THAT(
      ProfileFilter::ValidateMixPresentationsForPrimaryProfile(
          kIamfBaseAdvancedProfile, audio_elements, mix_presentation_obus),
      Not(IsOk()));
}

TEST(ValidateMixPresentationsForPrimaryProfile,
     FailsWhenFirstMixPresentationMixesObjectAndChannel) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId}, codec_config_obus, audio_elements);
  AppendAudioElementToFirstSubmix(kSecondAudioElementId, {},
                                  mix_presentation_obus.front());

  EXPECT_THAT(
      ProfileFilter::ValidateMixPresentationsForPrimaryProfile(
          kIamfBaseAdvancedProfile, audio_elements, mix_presentation_obus),
      Not(IsOk()));
}

TEST(ValidateMixPresentationsForPrimaryProfile,
     SucceedsWhenObjectBasedMixPresentationHasMoreThan18AudioElements) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId}, codec_config_obus, audio_elements);
  // First Mix Presentation is channel-based; second has 19 object Audio
  // Elements (exceeding the 18-Audio-Element limit for Base-Advanced).
  mix_presentation_obus.clear();
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
  constexpr DecodedUleb128 kSecondMixPresentationId = 2;
  const std::vector<DecodedUleb128> kNineteenObjectIds(19,
                                                       kFirstAudioElementId);
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, kNineteenObjectIds, kCommonMixGainParameterId,
      kCommonMixGainParameterRate, mix_presentation_obus);

  EXPECT_THAT(
      ProfileFilter::ValidateMixPresentationsForPrimaryProfile(
          kIamfBaseAdvancedProfile, audio_elements, mix_presentation_obus),
      IsOk());
}

TEST(ValidateMixPresentationsForPrimaryProfile,
     SucceedsWhenPrimaryProfileIsNotBaseAdvanced) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      /*num_objects=*/1, {kPolarParam}, codec_config_obus, audio_elements,
      mix_presentation_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId}, codec_config_obus, audio_elements);
  AppendAudioElementToFirstSubmix(kSecondAudioElementId, {},
                                  mix_presentation_obus.front());

  EXPECT_THAT(ProfileFilter::ValidateMixPresentationsForPrimaryProfile(
                  kIamfAdvanced1Profile, audio_elements, mix_presentation_obus),
              IsOk());
}

TEST(ValidateAudioElementsForAdditionalProfile,
     IgnoresUnrecognizedAudioElements) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      /*num_audio_elements=*/18, codec_config_obus, audio_elements,
      mix_presentation_obus);
  // Add an unrecognized reserved Audio Element type; its channels should not
  // count toward the 18-channel limit.
  auto unrecognized_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), /*audio_element_id=*/99,
      AudioElementObu::kAudioElementEndReserved, /*reserved=*/0, kCodecConfigId,
      /*audio_substream_ids=*/{99}, /*audio_element_config_bytes=*/{});
  ASSERT_THAT(unrecognized_obu, IsOk());
  audio_elements.emplace(
      99, AudioElementWithData{
              .obu = *std::move(unrecognized_obu),
              .substream_id_to_labels = {{99, {ChannelLabel::kMono}}}});

  EXPECT_THAT(ProfileFilter::ValidateAudioElementsForAdditionalProfile(
                  kIamfBaseAdvancedProfile, audio_elements),
              IsOk());
}

// Begin parameterized tests.

using FilterProfilesForAudioElementV1ProfileTest =
    ::testing::TestWithParam<ProfileVersion>;

TEST_P(FilterProfilesForAudioElementV1ProfileTest,
       RemovesProfileForReservedAudioElementType) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementBeginReserved, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

TEST_P(FilterProfilesForAudioElementV1ProfileTest,
       RemovesProfileForReservedAmbisonicsMode) {
  auto audio_element_obu = AudioElementObu::CreateForExtension(
      ObuHeader(), kFirstAudioElementId,
      AudioElementObu::kAudioElementSceneBased, kAudioElementReserved,
      kCodecConfigId, {kFirstSubstreamId}, kEmptyExtensionConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

INSTANTIATE_TEST_SUITE_P(V1Profiles, FilterProfilesForAudioElementV1ProfileTest,
                         ValuesIn(kV1ProfileVersions));

using FilterProfilesForAudioElementSimpleAndBaseProfileTest =
    ::testing::TestWithParam<ProfileVersion>;

TEST_P(FilterProfilesForAudioElementSimpleAndBaseProfileTest,
       RemovesProfileWhenFirstLayerIsExpandedLayout) {
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, kOneLayerExpandedLayoutLFEConfig);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

INSTANTIATE_TEST_SUITE_P(SimpleAndBaseProfiles,
                         FilterProfilesForAudioElementSimpleAndBaseProfileTest,
                         Values(kIamfSimpleProfile, kIamfBaseProfile));

using FilterProfilesForAudioElementV2ExpandedLayoutTest =
    ::testing::TestWithParam<
        ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>;

TEST_P(FilterProfilesForAudioElementV2ExpandedLayoutTest,
       RemovesBaseEnhancedProfileWhenFirstLayerIsV2ExpandedLayout) {
  const ScalableChannelLayoutConfig config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout = GetParam()}}};
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, config);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> base_enhanced_profile = {
      kIamfBaseEnhancedProfile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, base_enhanced_profile),
              Not(IsOk()));

  EXPECT_TRUE(base_enhanced_profile.empty());
}

TEST_P(FilterProfilesForAudioElementV2ExpandedLayoutTest,
       SomeProfilesSupportV2ExpandedLayout) {
  const ScalableChannelLayoutConfig config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout = GetParam()}}};
  auto audio_element_obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kFirstAudioElementId, kAudioElementReserved, kCodecConfigId,
      {kFirstSubstreamId}, config);
  ASSERT_THAT(audio_element_obu, IsOk());
  absl::flat_hash_set<ProfileVersion> all_known_profiles =
      kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForAudioElement(
                  "", *audio_element_obu, all_known_profiles),
              IsOk());

  EXPECT_EQ(all_known_profiles, kV2ProfileVersions);
}

INSTANTIATE_TEST_SUITE_P(
    V2ExpandedLayouts, FilterProfilesForAudioElementV2ExpandedLayoutTest,
    Values(ChannelAudioLayerConfig::kExpandedLayout10_2_9_3,
           ChannelAudioLayerConfig::kExpandedLayoutLfePair,
           ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch,
           ChannelAudioLayerConfig::kExpandedLayout7_1_5_4Ch,
           ChannelAudioLayerConfig::kExpandedLayoutBottom4Ch,
           ChannelAudioLayerConfig::kExpandedLayoutTop1Ch,
           ChannelAudioLayerConfig::kExpandedLayoutTop5Ch));

using FilterProfilesForMixPresentationV1ProfileTest =
    ::testing::TestWithParam<ProfileVersion>;

TEST_P(FilterProfilesForMixPresentationV1ProfileTest,
       RemovesProfileWhenThereAreTwoSubmixes) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusWithNSubmixes(2, codec_config_obus, audio_elements,
                                        mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

TEST_P(FilterProfilesForMixPresentationV1ProfileTest,
       RemovesProfileWithReservedHeadphonesRenderingModeBinauralHeadLocked) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeBinauralHeadLocked;
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

TEST_P(FilterProfilesForMixPresentationV1ProfileTest,
       RemovesProfileWithReservedHeadphonesRenderingMode3) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  mix_presentation_obus.front()
      .sub_mixes_.front()
      .audio_elements.front()
      .rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeReserved3;
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

INSTANTIATE_TEST_SUITE_P(V1Profiles,
                         FilterProfilesForMixPresentationV1ProfileTest,
                         ValuesIn(kV1ProfileVersions));

using FilterProfilesForMixPresentationFourthOrderAmbisonicsUnsupportedTest =
    ::testing::TestWithParam<ProfileVersion>;

TEST_P(FilterProfilesForMixPresentationFourthOrderAmbisonicsUnsupportedTest,
       RemovesProfileWhenFourthOrderAmbisonicsExceedsMaxChannels) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForOneFourthOrderAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam()};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

INSTANTIATE_TEST_SUITE_P(
    SimpleAndBaseProfiles,
    FilterProfilesForMixPresentationFourthOrderAmbisonicsUnsupportedTest,
    Values(kIamfSimpleProfile, kIamfBaseProfile));

struct ProfileAndMaxCountTestCase {
  ProfileVersion profile;
  int max_count;
};

using FilterProfilesForMixPresentationAudioElementLimitTest =
    ::testing::TestWithParam<ProfileAndMaxCountTestCase>;

TEST_P(FilterProfilesForMixPresentationAudioElementLimitTest,
       KeepsProfileAtMaxAudioElementLimit) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      GetParam().max_count, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam().profile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              IsOk());

  EXPECT_TRUE(profiles.contains(GetParam().profile));
}

TEST_P(FilterProfilesForMixPresentationAudioElementLimitTest,
       RemovesProfileAboveMaxAudioElementLimit) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      GetParam().max_count + 1, codec_config_obus, audio_elements,
      mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles = {GetParam().profile};

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

INSTANTIATE_TEST_SUITE_P(
    AudioElementLimits, FilterProfilesForMixPresentationAudioElementLimitTest,
    ValuesIn<ProfileAndMaxCountTestCase>({
        {.profile = kIamfSimpleProfile, .max_count = 1},
        {.profile = kIamfBaseProfile, .max_count = 2},
        {.profile = kIamfBaseEnhancedProfile, .max_count = 28},
        {.profile = kIamfBaseAdvancedProfile, .max_count = 18},
        {.profile = kIamfAdvanced1Profile, .max_count = 18},
        {.profile = kIamfAdvanced2Profile, .max_count = 28},
    }));

struct ObjectParamTestCase {
  uint8_t num_objects;
  std::vector<RenderingConfigParamDefinition> param_definitions;
};

using FilterProfilesForMixPresentationValidObjectParamTest =
    ::testing::TestWithParam<ObjectParamTestCase>;

TEST_P(FilterProfilesForMixPresentationValidObjectParamTest,
       KeepsSupportedProfilesForObjectsWithValidPositionParamDefinition) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      GetParam().num_objects, GetParam().param_definitions, codec_config_obus,
      audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles = kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              IsOk());

  EXPECT_EQ(profiles, kV2ProfileVersions);
}

INSTANTIATE_TEST_SUITE_P(
    ValidObjectParamDefinitions,
    FilterProfilesForMixPresentationValidObjectParamTest,
    ValuesIn<ObjectParamTestCase>({
        {.num_objects = 1, .param_definitions = {kPolarParam}},
        {.num_objects = 1, .param_definitions = {kCart8Param}},
        {.num_objects = 1, .param_definitions = {kCart16Param}},
        {.num_objects = 2, .param_definitions = {kDualPolarParam}},
        {.num_objects = 2, .param_definitions = {kDualCart8Param}},
        {.num_objects = 2, .param_definitions = {kDualCart16Param}},
    }));

using FilterProfilesForMixPresentationInvalidObjectParamTest =
    ::testing::TestWithParam<ObjectParamTestCase>;

TEST_P(FilterProfilesForMixPresentationInvalidObjectParamTest,
       RemovesAllKnownProfilesForObjectsWithInvalidPositionParamDefinitions) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForObjectAudioElement(
      GetParam().num_objects, GetParam().param_definitions, codec_config_obus,
      audio_elements, mix_presentation_obus);
  absl::flat_hash_set<ProfileVersion> profiles = kAllKnownProfileVersions;

  EXPECT_THAT(ProfileFilter::FilterProfilesForMixPresentation(
                  audio_elements, mix_presentation_obus.front(), profiles),
              Not(IsOk()));

  EXPECT_TRUE(profiles.empty());
}

INSTANTIATE_TEST_SUITE_P(
    InvalidObjectParamDefinitions,
    FilterProfilesForMixPresentationInvalidObjectParamTest,
    ValuesIn<ObjectParamTestCase>({
        {.num_objects = 1, .param_definitions = {}},
        {.num_objects = 1, .param_definitions = {kPolarParam, kPolarParam}},
        {.num_objects = 1, .param_definitions = {kDualPolarParam}},
        {.num_objects = 1, .param_definitions = {kDualCart8Param}},
        {.num_objects = 1, .param_definitions = {kDualCart16Param}},
        {.num_objects = 2, .param_definitions = {kPolarParam}},
        {.num_objects = 2, .param_definitions = {kCart8Param}},
        {.num_objects = 2, .param_definitions = {kCart16Param}},
    }));

using ValidateAudioElementsForAdditionalProfileChannelLimitTest =
    ::testing::TestWithParam<ProfileAndMaxCountTestCase>;

TEST_P(ValidateAudioElementsForAdditionalProfileChannelLimitTest,
       ValidatesAtMaxChannelLimit) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      GetParam().max_count, codec_config_obus, audio_elements,
      mix_presentation_obus);

  EXPECT_THAT(ProfileFilter::ValidateAudioElementsForAdditionalProfile(
                  GetParam().profile, audio_elements),
              IsOk());
}

TEST_P(ValidateAudioElementsForAdditionalProfileChannelLimitTest,
       RejectsAboveMaxChannelLimit) {
  CodecConfigsById codec_config_obus;
  AudioElementsById audio_elements;
  MixPresentationObus mix_presentation_obus;
  InitializeDescriptorObusForNMonoAmbisonicsAudioElements(
      GetParam().max_count + 1, codec_config_obus, audio_elements,
      mix_presentation_obus);

  EXPECT_THAT(ProfileFilter::ValidateAudioElementsForAdditionalProfile(
                  GetParam().profile, audio_elements),
              Not(IsOk()));
}

INSTANTIATE_TEST_SUITE_P(
    ChannelLimits, ValidateAudioElementsForAdditionalProfileChannelLimitTest,
    ValuesIn<ProfileAndMaxCountTestCase>({
        {.profile = kIamfBaseAdvancedProfile, .max_count = 18},
        {.profile = kIamfAdvanced1Profile, .max_count = 18},
        {.profile = kIamfAdvanced2Profile, .max_count = 28},
    }));

}  // namespace

}  // namespace iamf_tools
