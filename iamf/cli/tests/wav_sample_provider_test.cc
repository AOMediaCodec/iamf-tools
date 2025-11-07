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
#include "iamf/cli/wav_sample_provider.h"

#include <array>
#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelLabel::Label;
using testing::Pointwise;

constexpr absl::string_view kTestdataPath = "iamf/cli/testdata/";

constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kSubstreamId = 0;
constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr uint32_t kSampleRate = 48000;

constexpr auto kExpectedSamplesL2 = std::to_array<InternalSampleType>(
    {1 << 16, 2 << 16, 3 << 16, 4 << 16, 5 << 16, 6 << 16, 7 << 16, 8 << 16});
constexpr auto kExpectedSamplesR2 =
    std::to_array({65535 << 16, 65534 << 16, 65533 << 16, 65532 << 16,
                   65531 << 16, 65530 << 16, 65529 << 16, 65528 << 16});

using iamf_tools_cli_proto::AudioFrameObuMetadata;

void FillStereoDataForAudioElementId(
    uint32_t audio_element_id, AudioFrameObuMetadata& audio_frame_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        wav_filename: "stereo_8_samples_48khz_s16le.wav"
        samples_to_trim_at_end: 0
        samples_to_trim_at_start: 0
      )pb",
      &audio_frame_metadata));
  audio_frame_metadata.set_audio_element_id(audio_element_id);
  using enum iamf_tools_cli_proto::ChannelLabel;
  auto* channel_metadata = audio_frame_metadata.add_channel_metadatas();
  channel_metadata->set_channel_id(0);
  channel_metadata->set_channel_label(CHANNEL_LABEL_L_2);
  channel_metadata = audio_frame_metadata.add_channel_metadatas();
  channel_metadata->set_channel_id(1);
  channel_metadata->set_channel_label(CHANNEL_LABEL_R_2);
}

void InitializeTestData(
    const uint32_t sample_rate,
    iamf_tools_cli_proto::UserMetadata& user_metadata,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  FillStereoDataForAudioElementId(kAudioElementId,
                                  *user_metadata.add_audio_frame_metadata());
  static absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  codec_config_obus.clear();
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, sample_rate,
                                        codec_config_obus);
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {kSubstreamId},
      codec_config_obus, audio_elements);
}

std::string GetInputWavDir() {
  static const std::string kInputWavDir = GetRunfilesPath(kTestdataPath);
  return kInputWavDir;
}

TEST(Create, SucceedsForStereoInputWithChannelMetadatas) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);

  EXPECT_THAT(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                        GetInputWavDir(), audio_elements),
              IsOk());
}

TEST(Create, FailsWhenUserMetadataContainsDuplicateAudioElementIds) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);
  FillStereoDataForAudioElementId(kAudioElementId,
                                  *user_metadata.add_audio_frame_metadata());

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

TEST(Create, FailsWhenMatchingAudioElementObuIsMissing) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kNoAudioElements = {};
  FillStereoDataForAudioElementId(kAudioElementId,
                                  *user_metadata.add_audio_frame_metadata());

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), kNoAudioElements)
                   .ok());
}

TEST(Create, FailsWhenCodecConfigIsMissing) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);

  // Corrupt the audio element by clearing the codec config pointer.
  ASSERT_TRUE(audio_elements.contains(kAudioElementId));
  audio_elements.at(kAudioElementId).codec_config = nullptr;

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

TEST(Create, FailsForUnknownLabels) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);
  user_metadata.mutable_audio_frame_metadata(0)
      ->mutable_channel_metadatas(0)
      ->set_channel_label(iamf_tools_cli_proto::CHANNEL_LABEL_INVALID);

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

TEST(Create, SucceedsForDuplicateChannelMetadatasChannelIds) {
  constexpr uint32_t kDuplicateChannelId = 0;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);
  user_metadata.mutable_audio_frame_metadata(0)
      ->mutable_channel_metadatas(0)
      ->set_channel_id(kDuplicateChannelId);
  user_metadata.mutable_audio_frame_metadata(0)
      ->mutable_channel_metadatas(1)
      ->set_channel_id(kDuplicateChannelId);

  EXPECT_THAT(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                        GetInputWavDir(), audio_elements),
              IsOk());
}

TEST(Create, FailsForDuplicateChannelMetadatasChannelLabels) {
  constexpr auto kDuplicateLabel = iamf_tools_cli_proto::CHANNEL_LABEL_L_2;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);
  user_metadata.mutable_audio_frame_metadata(0)
      ->mutable_channel_metadatas(0)
      ->set_channel_label(kDuplicateLabel);
  user_metadata.mutable_audio_frame_metadata(0)
      ->mutable_channel_metadatas(1)
      ->set_channel_label(kDuplicateLabel);

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

TEST(Create, FailsForChannelMetadataChannelIdTooLarge) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);
  // Channel IDs are indexed from zero; a stereo wav file must not have a
  // channel ID greater than 1.
  constexpr auto kFirstChannelIndex = 0;
  constexpr uint32_t kChannelIdTooLargeForStereoWavFile = 2;
  user_metadata.mutable_audio_frame_metadata(0)
      ->mutable_channel_metadatas(kFirstChannelIndex)
      ->set_channel_id(kChannelIdTooLargeForStereoWavFile);

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

TEST(Create, FailsForBitDepthLowerThanFile) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);

  // Try to load a 24-bit WAV file with a codec config whose bit depth is 16.
  // The `Initialize()` would refuse to lower the bit depth and fail.
  user_metadata.mutable_audio_frame_metadata(0)->set_wav_filename(
      "stereo_8_samples_48khz_s24le.wav");
  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

TEST(Create, FailsForMismatchingSampleRates) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;

  // Set the sample rate of the codec config to a different one than the WAV
  // file, causing the `Initialize()` to fail.
  const uint32_t kWrongSampleRate = 16000;
  InitializeTestData(kWrongSampleRate, user_metadata, audio_elements);

  EXPECT_FALSE(WavSampleProvider::Create(user_metadata.audio_frame_metadata(),
                                         GetInputWavDir(), audio_elements)
                   .ok());
}

void ReadOneFrameExpectFinished(WavSampleProvider& wav_sample_provider,
                                LabelSamplesMap& labeled_samples) {
  bool finished_reading = false;
  EXPECT_THAT(wav_sample_provider.ReadFrames(kAudioElementId, labeled_samples,
                                             finished_reading),
              IsOk());
  EXPECT_TRUE(finished_reading);
}

TEST(WavSampleProviderTest, ReadFrameSucceedsWithChannelMetadatas) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);

  auto wav_sample_provider = WavSampleProvider::Create(
      user_metadata.audio_frame_metadata(), GetInputWavDir(), audio_elements);
  ASSERT_THAT(wav_sample_provider, IsOk());

  LabelSamplesMap labeled_samples;
  ReadOneFrameExpectFinished(*wav_sample_provider, labeled_samples);

  // Validate samples read from the WAV file.
  EXPECT_THAT(
      labeled_samples[kL2],
      Pointwise(InternalSampleMatchesIntegralSample(), kExpectedSamplesL2));
  EXPECT_THAT(
      labeled_samples[kR2],
      Pointwise(InternalSampleMatchesIntegralSample(), kExpectedSamplesR2));
}

TEST(WavSampleProviderTest, ReadFrameFailsWithWrongAudioElementId) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializeTestData(kSampleRate, user_metadata, audio_elements);

  auto wav_sample_provider = WavSampleProvider::Create(
      user_metadata.audio_frame_metadata(), GetInputWavDir(), audio_elements);
  ASSERT_THAT(wav_sample_provider, IsOk());

  // Try to read frames using a wrong Audio Element ID.
  const auto kWrongAudioElementId = kAudioElementId + 99;
  LabelSamplesMap labeled_samples;
  bool finished_reading = false;
  EXPECT_FALSE(
      wav_sample_provider
          ->ReadFrames(kWrongAudioElementId, labeled_samples, finished_reading)
          .ok());
}

}  // namespace
}  // namespace iamf_tools
