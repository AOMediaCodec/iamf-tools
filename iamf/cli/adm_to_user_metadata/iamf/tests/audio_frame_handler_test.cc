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

#include "iamf/cli/adm_to_user_metadata/iamf/audio_frame_handler.h"

#include <cstdint>

#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/proto/audio_frame.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using testing::ElementsAreArray;

constexpr absl::string_view kFileNamePrefix = "prefix";
constexpr absl::string_view kFileNameSuffix = "suffix";
constexpr int32_t kNumSamplesToTrimAtEnd = 99;
const int32_t kFirstAudioElementId = 0;

iamf_tools_cli_proto::AudioFrameObuMetadata GetAudioFrameMetadataExpectOk(
    IamfInputLayout input_layout = IamfInputLayout::kStereo,
    int32_t audio_element_id = kFirstAudioElementId) {
  const AudioFrameHandler audio_frame_handler(kFileNamePrefix,
                                              kNumSamplesToTrimAtEnd);

  iamf_tools_cli_proto::AudioFrameObuMetadata audio_element_metadata;
  EXPECT_TRUE(audio_frame_handler
                  .PopulateAudioFrameMetadata(kFileNameSuffix, audio_element_id,
                                              input_layout,
                                              audio_element_metadata)
                  .ok());
  return audio_element_metadata;
}

TEST(Constructor, SetsMemberVariables) {
  const AudioFrameHandler audio_frame_handler(kFileNamePrefix,
                                              kNumSamplesToTrimAtEnd);
  EXPECT_EQ(audio_frame_handler.file_prefix_, kFileNamePrefix);
  EXPECT_EQ(audio_frame_handler.num_samples_to_trim_at_end_,
            kNumSamplesToTrimAtEnd);
}

TEST(PopulateAudioFrameMetadata, ConfiguresWavFilename) {
  constexpr absl::string_view kExpectedWavFilename =
      "prefix_convertedsuffix.wav";

  EXPECT_EQ(GetAudioFrameMetadataExpectOk().wav_filename(),
            kExpectedWavFilename);
}

TEST(PopulateAudioFrameMetadata, ConfiguresAudioElementId) {
  constexpr int32_t kAudioElementId = 1;

  EXPECT_EQ(
      GetAudioFrameMetadataExpectOk(IamfInputLayout::kStereo, kAudioElementId)
          .audio_element_id(),
      kAudioElementId);
}

TEST(PopulateAudioFrameMetadata, ConfiguresSamplesToTrimAtEnd) {
  EXPECT_EQ(GetAudioFrameMetadataExpectOk().samples_to_trim_at_end(),
            kNumSamplesToTrimAtEnd);
}

TEST(PopulateAudioFrameMetadata, ConfiguresSamplesToTrimAtStartToZero) {
  constexpr int32_t kExpectedNumSamplesToTrimAtStart = 0;

  EXPECT_EQ(GetAudioFrameMetadataExpectOk().samples_to_trim_at_start(),
            kExpectedNumSamplesToTrimAtStart);
}

TEST(PopulateAudioFrameMetadata, ConfiguresLabelsForChannelBasedInput) {
  const auto audio_frame_obu_metadata =
      GetAudioFrameMetadataExpectOk(IamfInputLayout::kStereo);

  EXPECT_THAT(audio_frame_obu_metadata.channel_labels(),
              ElementsAreArray({"L2", "R2"}));
  EXPECT_THAT(audio_frame_obu_metadata.channel_ids(), ElementsAreArray({0, 1}));
}

TEST(PopulateAudioFrameMetadata, ConfiguresLabelsForSceneBasedInput) {
  const auto audio_frame_obu_metadata =
      GetAudioFrameMetadataExpectOk(IamfInputLayout::kAmbisonicsOrder1);

  EXPECT_THAT(audio_frame_obu_metadata.channel_labels(),
              ElementsAreArray({"A0", "A1", "A2", "A3"}));
  EXPECT_THAT(audio_frame_obu_metadata.channel_ids(),
              ElementsAreArray({0, 1, 2, 3}));
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
