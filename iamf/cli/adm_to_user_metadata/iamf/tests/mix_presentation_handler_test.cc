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
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

constexpr uint32_t kCommonParameterRate = 48000;
constexpr uint32_t kMixPresentationId = 99;

constexpr float kStereoGain = 3.0f / 256.0f;
constexpr int16_t kExpectedStereoGain = 3;
const AudioObject& GetStereoAudioObject() {
  constexpr absl::string_view kStereoAudioObjectId = "Stereo Audio Object";
  constexpr absl::string_view kStereoAudioPackFormatId = "AP_00010002";
  static const absl::NoDestructor<AudioObject> kStereoAudioObject(AudioObject(
      {.id = std::string(kStereoAudioObjectId),
       .gain = kStereoGain,
       .audio_pack_format_id_refs = {std::string(kStereoAudioPackFormatId)}}));
  return *kStereoAudioObject;
}

const AudioObject& GetBinauralAudioObject() {
  constexpr absl::string_view kBinauralAudioObjectId = "Binaural Audio Object";
  constexpr absl::string_view kBinauralAudioPackFormatId = "AP_00050001";
  static const absl::NoDestructor<AudioObject> kBinauralAudioObject(
      AudioObject({.id = std::string(kBinauralAudioObjectId),
                   .audio_pack_format_id_refs = {
                       std::string(kBinauralAudioPackFormatId)}}));
  return *kBinauralAudioObject;
}

const AudioObject& Get5_1AudioObject() {
  constexpr absl::string_view k5_1AudioObjectId = "5.1 Audio Object";
  constexpr absl::string_view k5_1AudioPackFormatId = "AP_00010003";
  static const absl::NoDestructor<AudioObject> k5_1AudioObject(AudioObject(
      {.id = std::string(k5_1AudioObjectId),
       .audio_pack_format_id_refs = {std::string(k5_1AudioPackFormatId)}}));
  return *k5_1AudioObject;
}

TEST(Constructor, DoesNotCrash) {
  MixPresentationHandler handler(kCommonParameterRate, {});
}

iamf_tools_cli_proto::MixPresentationObuMetadata GetMixObuMetataExpectOk(
    const std::vector<AudioObject>& audio_objects = {GetStereoAudioObject()},
    LoudnessMetadata loudness_metadata = LoudnessMetadata()) {
  std::map<std::string, uint32_t> audio_object_id_to_audio_element_id;
  // Assign audio element IDs sequentially.
  uint32_t audio_element_id = 0;
  for (const auto& audio_object : audio_objects) {
    audio_object_id_to_audio_element_id[audio_object.id] = audio_element_id++;
  }

  MixPresentationHandler handler(kCommonParameterRate,
                                 audio_object_id_to_audio_element_id);

  iamf_tools_cli_proto::MixPresentationObuMetadata mix_presentation_metadata;
  EXPECT_THAT(handler.PopulateMixPresentation(kMixPresentationId, audio_objects,
                                              loudness_metadata,
                                              mix_presentation_metadata),
              IsOk());

  return mix_presentation_metadata;
}

TEST(PopulateMixPresentation, PopulatesMixPresentationId) {
  EXPECT_EQ(GetMixObuMetataExpectOk().mix_presentation_id(),
            kMixPresentationId);
}

TEST(PopulateMixPresentation, PopulatesLabels) {
  const auto& mix_presentation_metadata = GetMixObuMetataExpectOk();

  EXPECT_FALSE(mix_presentation_metadata.annotations_language(0).empty());
  EXPECT_FALSE(
      mix_presentation_metadata.localized_presentation_annotations(0).empty());
}

TEST(PopulateMixPresentation, PopulatesStereoSubmix) {
  const auto& mix_presentation_metadata = GetMixObuMetataExpectOk();

  EXPECT_EQ(mix_presentation_metadata.sub_mixes_size(), 1);
  const auto& submix = mix_presentation_metadata.sub_mixes(0);
  EXPECT_EQ(submix.audio_elements_size(), 1);
  const auto& audio_element = submix.audio_elements(0);

  const uint32_t kExpectedAudioElementId = 0;
  EXPECT_EQ(audio_element.audio_element_id(), kExpectedAudioElementId);
  EXPECT_FALSE(audio_element.localized_element_annotations().empty());

  EXPECT_EQ(audio_element.element_mix_gain().default_mix_gain(),
            kExpectedStereoGain);

  EXPECT_EQ(submix.output_mix_gain().default_mix_gain(), 0);
}

TEST(PopulateMixPresentation,
     SetsBinauralWorldLockedRenderingModeForStereoAudioObject) {
  const auto& mix_presentation_metadata =
      GetMixObuMetataExpectOk({GetStereoAudioObject()});

  EXPECT_EQ(
      mix_presentation_metadata.sub_mixes(0)
          .audio_elements(0)
          .rendering_config()
          .headphones_rendering_mode(),
      iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_BINAURAL_WORLD_LOCKED);
}

TEST(PopulateMixPresentation,
     SetsBinauralWorldLockedRenderingModeForBinauralAudioObject) {
  const auto& mix_presentation_metadata =
      GetMixObuMetataExpectOk({GetBinauralAudioObject()});

  EXPECT_EQ(
      mix_presentation_metadata.sub_mixes(0)
          .audio_elements(0)
          .rendering_config()
          .headphones_rendering_mode(),
      iamf_tools_cli_proto::HEADPHONES_RENDERING_MODE_BINAURAL_WORLD_LOCKED);
}

void ExpectSsLayout(const auto& layout) {
  EXPECT_EQ(layout.loudness_layout().layout_type(),
            iamf_tools_cli_proto::LAYOUT_TYPE_LOUDSPEAKERS_SS_CONVENTION);
  ASSERT_TRUE(layout.loudness_layout().has_ss_layout());
}

TEST(PopulateMixPresentation, PopulatesLayout) {
  const float kIntegratedLoudness = 99.0 / 256.0f;
  const int16_t kExpectedIntegratedLoudness = 99;

  const auto& mix_presentation_metadata = GetMixObuMetataExpectOk(
      {GetStereoAudioObject()},
      LoudnessMetadata({.integrated_loudness = kIntegratedLoudness}));

  // Ignoring the deprecated `num_layouts` field.
  EXPECT_EQ(mix_presentation_metadata.sub_mixes(0).layouts_size(), 1);
  const auto& layout = mix_presentation_metadata.sub_mixes(0).layouts(0);
  ExpectSsLayout(layout);
  EXPECT_EQ(layout.loudness_layout().ss_layout().sound_system(),
            iamf_tools_cli_proto::SOUND_SYSTEM_A_0_2_0);

  EXPECT_TRUE(layout.loudness().info_type_bit_masks().empty());
  EXPECT_EQ(layout.loudness().integrated_loudness(),
            kExpectedIntegratedLoudness);
}

TEST(PopulateMixPresentation, PopulatesOptionalLoudnessValues) {
  const float kIntegratedLoudness = 1.0 / 256.0f;
  const int16_t kExpectedIntegratedLoudness = 1;
  const float kMaxTruePeak = 2.0 / 256.0f;
  const int16_t kExpectedMaxTruePeak = 2;
  const float kDialogueLoudness = 3.0 / 256.0f;
  const int16_t kExpectedDialogueLoudness = 3;

  const auto& mix_presentation_metadata = GetMixObuMetataExpectOk(
      {GetStereoAudioObject()},
      LoudnessMetadata({.integrated_loudness = kIntegratedLoudness,
                        .max_true_peak = kMaxTruePeak,
                        .dialogue_loudness = kDialogueLoudness}));

  const auto& layout = mix_presentation_metadata.sub_mixes(0).layouts(0);
  // Integrated loudness is always populated.
  EXPECT_EQ(layout.loudness().integrated_loudness(),
            kExpectedIntegratedLoudness);
  // Check all optional values.
  EXPECT_THAT(
      layout.loudness().info_type_bit_masks(),
      ::testing::UnorderedElementsAreArray(
          {iamf_tools_cli_proto::LOUDNESS_INFO_TYPE_TRUE_PEAK,
           iamf_tools_cli_proto::LOUDNESS_INFO_TYPE_ANCHORED_LOUDNESS}));
  EXPECT_EQ(layout.loudness().true_peak(), kExpectedMaxTruePeak);
  const auto& anchored_loudness = layout.loudness().anchored_loudness();
  // ADM only supports dialogue anchored loudness.
  // Ignoring the deprecated `num_anchored_loudness` field.
  EXPECT_EQ(anchored_loudness.anchor_elements_size(), 1);
  EXPECT_EQ(anchored_loudness.anchor_elements(0).anchor_element(),
            iamf_tools_cli_proto::ANCHOR_TYPE_DIALOGUE);
  EXPECT_EQ(anchored_loudness.anchor_elements(0).anchored_loudness(),
            kExpectedDialogueLoudness);
}

TEST(PopulateMixPresentation, PopulatesStereoAndHighestLayout) {
  const auto& mix_presentation_metadata =
      GetMixObuMetataExpectOk({Get5_1AudioObject()});

  // Ignoring the deprecated `num_layouts` field.
  EXPECT_EQ(mix_presentation_metadata.sub_mixes(0).layouts_size(), 2);
  const auto& layout_stereo = mix_presentation_metadata.sub_mixes(0).layouts(0);
  ExpectSsLayout(layout_stereo);
  EXPECT_EQ(layout_stereo.loudness_layout().ss_layout().sound_system(),
            iamf_tools_cli_proto::SOUND_SYSTEM_A_0_2_0);
  const auto& layout_5_1 = mix_presentation_metadata.sub_mixes(0).layouts(1);
  EXPECT_EQ(layout_5_1.loudness_layout().ss_layout().sound_system(),
            iamf_tools_cli_proto::SOUND_SYSTEM_B_0_5_0);
}

TEST(PopulateMixPresentation,
     PopulatesExactlyOneBinauralLayoutForBinauralAudioObject) {
  const auto& mix_presentation_metadata =
      GetMixObuMetataExpectOk({GetBinauralAudioObject()});

  // Find and validate the singular binaural layout. The spec does not require
  // it to be in any particular index.
  bool found_binaural_layout = false;
  for (const auto& layout : mix_presentation_metadata.sub_mixes(0).layouts()) {
    if (layout.loudness_layout().layout_type() !=
        iamf_tools_cli_proto::LAYOUT_TYPE_BINAURAL) {
      continue;
    }
    EXPECT_FALSE(found_binaural_layout)
        << "Found more than one binaural layout.";
    found_binaural_layout = true;
    EXPECT_TRUE(layout.loudness_layout().has_reserved_or_binaural_layout());
    EXPECT_EQ(layout.loudness_layout().reserved_or_binaural_layout().reserved(),
              0);
  }
  EXPECT_TRUE(found_binaural_layout) << "Found no binaural layouts.";
}

TEST(PopulateMixPresentation, AlwaysPopulatesExactlyOneSubmix) {
  EXPECT_EQ(
      GetMixObuMetataExpectOk({GetStereoAudioObject(), Get5_1AudioObject()})
          .sub_mixes_size(),
      1);
}
TEST(PopulateMixPresentation, PopulatesOneAudioElementPerAudioObject) {
  EXPECT_EQ(
      GetMixObuMetataExpectOk({GetStereoAudioObject(), Get5_1AudioObject()})
          .sub_mixes(0)
          .audio_elements_size(),
      2);
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
