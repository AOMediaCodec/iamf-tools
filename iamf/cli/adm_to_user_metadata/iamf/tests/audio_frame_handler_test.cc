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
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/audio_frame.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

using testing::ElementsAreArray;

using enum ChannelLabel::Label;

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
  EXPECT_THAT(audio_frame_handler.PopulateAudioFrameMetadata(
                  kFileNameSuffix, audio_element_id, input_layout,
                  audio_element_metadata),
              IsOk());
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

template <class InputContainer>
void ExpectLabelsAreConvertibleToChannelLabels(
    InputContainer labels,
    const std::vector<ChannelLabel::Label>& expected_labels) {
  std::vector<ChannelLabel::Label> converted_labels;
  ASSERT_THAT(ChannelLabel::FillLabelsFromStrings(labels, converted_labels),
              IsOk());
  EXPECT_EQ(converted_labels, expected_labels);
}

TEST(PopulateAudioFrameMetadata, ConfiguresChannelIdsAndLabelsForStereoInput) {
  const auto audio_frame_obu_metadata =
      GetAudioFrameMetadataExpectOk(IamfInputLayout::kStereo);

  ExpectLabelsAreConvertibleToChannelLabels(
      audio_frame_obu_metadata.channel_labels(), {kL2, kR2});
  EXPECT_THAT(audio_frame_obu_metadata.channel_ids(), ElementsAreArray({0, 1}));
}

TEST(PopulateAudioFrameMetadata,
     ConfiguresChannelIdsAndLabelsForBinauralInput) {
  const auto audio_frame_obu_metadata =
      GetAudioFrameMetadataExpectOk(IamfInputLayout::kBinaural);

  ExpectLabelsAreConvertibleToChannelLabels(
      audio_frame_obu_metadata.channel_labels(), {kL2, kR2});
  EXPECT_THAT(audio_frame_obu_metadata.channel_ids(), ElementsAreArray({0, 1}));
}

TEST(PopulateAudioFrameMetadata,
     ConfiguresChannelIdsAndLabelsForAmbisonicsOrder1Input) {
  const auto audio_frame_obu_metadata =
      GetAudioFrameMetadataExpectOk(IamfInputLayout::kAmbisonicsOrder1);

  ExpectLabelsAreConvertibleToChannelLabels(
      audio_frame_obu_metadata.channel_labels(), {kA0, kA1, kA2, kA3});
  EXPECT_THAT(audio_frame_obu_metadata.channel_ids(),
              ElementsAreArray({0, 1, 2, 3}));
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
