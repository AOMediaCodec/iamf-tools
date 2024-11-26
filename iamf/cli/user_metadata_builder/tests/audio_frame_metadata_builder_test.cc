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

#include "iamf/cli/user_metadata_builder/audio_frame_metadata_builder.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

using enum ChannelLabel::Label;

constexpr absl::string_view kWavFilename = "prefix_convertedsuffix.wav";
constexpr IamfInputLayout kInputLayout = IamfInputLayout::kStereo;
constexpr uint32_t kFirstAudioElementId = 0;
constexpr uint32_t kLargeAudioElementId = std::numeric_limits<uint32_t>::max();

iamf_tools_cli_proto::AudioFrameObuMetadata GetAudioFrameMetadataExpectOk(
    absl::string_view wav_filename, IamfInputLayout input_layout,
    int32_t audio_element_id) {
  iamf_tools_cli_proto::AudioFrameObuMetadata audio_element_metadata;
  EXPECT_THAT(
      AudioFrameMetadataBuilder::PopulateAudioFrameMetadata(
          wav_filename, audio_element_id, input_layout, audio_element_metadata),
      IsOk());
  return audio_element_metadata;
}

TEST(PopulateAudioFrameMetadata, ConfiguresWavFilename) {
  constexpr absl::string_view kExpectedWavFilename = "custom_wav_filename.wav";
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kExpectedWavFilename, kInputLayout, kFirstAudioElementId);

  EXPECT_EQ(audio_frame_obu_metadata.wav_filename(), kExpectedWavFilename);
}

TEST(PopulateAudioFrameMetadata, ConfiguresAudioElementId) {
  constexpr int32_t kAudioElementId = 1;
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, kInputLayout, kAudioElementId);

  EXPECT_EQ(audio_frame_obu_metadata.audio_element_id(), kAudioElementId);
}

TEST(PopulateAudioFrameMetadata, ConfiguresLargeAudioElementId) {
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, kInputLayout, kLargeAudioElementId);

  EXPECT_EQ(audio_frame_obu_metadata.audio_element_id(), kLargeAudioElementId);
}

TEST(PopulateAudioFrameMetadata, ConfiguresSamplesToTrimAtEndToZero) {
  constexpr int32_t kExpectedNumSamplesToTrimAtEnd = 0;
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, kInputLayout, kFirstAudioElementId);

  EXPECT_EQ(audio_frame_obu_metadata.samples_to_trim_at_end(),
            kExpectedNumSamplesToTrimAtEnd);
}

TEST(PopulateAudioFrameMetadata, ConfiguresSamplesToTrimAtStartToZero) {
  constexpr int32_t kExpectedNumSamplesToTrimAtStart = 0;
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, kInputLayout, kFirstAudioElementId);

  EXPECT_EQ(audio_frame_obu_metadata.samples_to_trim_at_start(),
            kExpectedNumSamplesToTrimAtStart);
}

TEST(PopulateAudioFrameMetadata,
     ConfiguresSamplesToTrimAtStartIncludesCodecDelayToFalse) {
  constexpr bool kExpectedSamplesToTrimAtStartIncludesCodecDelay = false;
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, kInputLayout, kFirstAudioElementId);

  EXPECT_EQ(
      audio_frame_obu_metadata.samples_to_trim_at_start_includes_codec_delay(),
      kExpectedSamplesToTrimAtStartIncludesCodecDelay);
}

TEST(PopulateAudioFrameMetadata,
     ConfiguresSamplesToTrimAtEndIncludesPaddingToFalse) {
  constexpr bool kExpectedSamplesToTrimAtEndIncludesPadding = false;
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, kInputLayout, kFirstAudioElementId);

  EXPECT_EQ(audio_frame_obu_metadata.samples_to_trim_at_end_includes_padding(),
            kExpectedSamplesToTrimAtEndIncludesPadding);
}

template <class InputContainer>
void ExpectLabelsAreConvertibleToChannelLabels(
    InputContainer labels,
    const std::vector<ChannelLabel::Label>& expected_labels) {
  std::vector<ChannelLabel::Label> converted_labels;
  ASSERT_THAT(ChannelLabel::ConvertAndFillLabels(labels, converted_labels),
              IsOk());
  EXPECT_EQ(converted_labels, expected_labels);
}

TEST(PopulateAudioFrameMetadata, ConfiguresChannelIdsAndLabelsForStereoInput) {
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, IamfInputLayout::kStereo, kFirstAudioElementId);

  ExpectLabelsAreConvertibleToChannelLabels(
      audio_frame_obu_metadata.channel_metadatas(), {kL2, kR2});
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(0).channel_id(), 0);
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(1).channel_id(), 1);
}

TEST(PopulateAudioFrameMetadata,
     ConfiguresChannelIdsAndLabelsForBinauralInput) {
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, IamfInputLayout::kBinaural, kFirstAudioElementId);

  ExpectLabelsAreConvertibleToChannelLabels(
      audio_frame_obu_metadata.channel_metadatas(), {kL2, kR2});
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(0).channel_id(), 0);
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(1).channel_id(), 1);
}

TEST(PopulateAudioFrameMetadata,
     ConfiguresChannelIdsAndLabelsForAmbisonicsOrder1Input) {
  const auto audio_frame_obu_metadata = GetAudioFrameMetadataExpectOk(
      kWavFilename, IamfInputLayout::kAmbisonicsOrder1, kFirstAudioElementId);

  ExpectLabelsAreConvertibleToChannelLabels(
      audio_frame_obu_metadata.channel_metadatas(), {kA0, kA1, kA2, kA3});
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(0).channel_id(), 0);
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(1).channel_id(), 1);
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(2).channel_id(), 2);
  EXPECT_EQ(audio_frame_obu_metadata.channel_metadatas(3).channel_id(), 3);
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
