/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/audio_frame_decoder.h"

#include <array>
#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 44;
constexpr uint32_t kSampleRate = 16000;
constexpr DecodedUleb128 kAudioElementId = 13;
constexpr DecodedUleb128 kSubstreamId = 0;
constexpr DownMixingParams kDownMixingParams = {.alpha = 0.5, .beta = 0.5};
const int kNumChannels = 1;
const int kNumSamplesPerFrame = 8;
const int kBytesPerSample = 2;
constexpr std::array<uint8_t, 22> kFlacEncodedFrame = {
    0xff, 0xf8, 0x6a, 0xa8, 0x00, 0x0f, 0x42, 0x00, 0x00, 0x00, 0x13,
    0x80, 0x00, 0x80, 0x04, 0x92, 0x49, 0x00, 0x01, 0xfe, 0x81, 0xee};

AudioFrameWithData PrepareEncodedAudioFrame(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    CodecConfig::CodecId codec_id_type = CodecConfig::kCodecIdLpcm,
    std::vector<uint8_t> encoded_audio_frame_payload = {}) {
  if (codec_id_type == CodecConfig::kCodecIdLpcm) {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus);
  } else if (codec_id_type == CodecConfig::kCodecIdFlac) {
    AddFlacCodecConfigWithId(kCodecConfigId, codec_config_obus);
  }
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {kSubstreamId}, codec_config_obus,
      audio_elements);

  if (encoded_audio_frame_payload.empty()) {
    encoded_audio_frame_payload =
        std::vector<uint8_t>(kNumSamplesPerFrame * kBytesPerSample, 0);
  }
  return AudioFrameWithData({
      .obu = AudioFrameObu(ObuHeader(), kSubstreamId,
                           /*audio_frame=*/
                           encoded_audio_frame_payload),
      .start_timestamp = 0,
      .end_timestamp = kNumSamplesPerFrame,
      .down_mixing_params = kDownMixingParams,
      .audio_element_with_data = &audio_elements.at(kAudioElementId),
  });
}

TEST(Decode, RequiresSubstreamsAreInitialized) {
  AudioFrameDecoder decoder;
  // Encoded frames.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  const AudioFrameWithData encoded_audio_frame =
      PrepareEncodedAudioFrame(codec_config_obus, audio_elements);

  // Decoding fails before substreams are initialized.
  EXPECT_FALSE(decoder.Decode(encoded_audio_frame).ok());
  const auto& audio_element = audio_elements.at(kAudioElementId);
  // Decoding succeeds after substreams are initialized.
  EXPECT_THAT(
      decoder.InitDecodersForSubstreams(audio_element.substream_id_to_labels,
                                        *audio_element.codec_config),
      IsOk());
  EXPECT_THAT(decoder.Decode(encoded_audio_frame), IsOk());
}

TEST(InitDecodersForSubstreams,
     ShouldNotBeCalledTwiceWithTheSameSubstreamIdForStatefulEncoders) {
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  const auto& codec_config = codec_config_obus.at(kCodecConfigId);

  AudioFrameDecoder decoder;
  const SubstreamIdLabelsMap kLabelsForSubstreamZero = {
      {kSubstreamId, {ChannelLabel::kMono}}};
  EXPECT_THAT(
      decoder.InitDecodersForSubstreams(kLabelsForSubstreamZero, codec_config),
      IsOk());
  EXPECT_FALSE(
      decoder.InitDecodersForSubstreams(kLabelsForSubstreamZero, codec_config)
          .ok());

  const SubstreamIdLabelsMap kLabelsForSubstreamOne = {
      {kSubstreamId + 1, {ChannelLabel::kMono}}};
  EXPECT_THAT(
      decoder.InitDecodersForSubstreams(kLabelsForSubstreamOne, codec_config),
      IsOk());
}

void InitAllAudioElements(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    AudioFrameDecoder& decoder) {
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    EXPECT_THAT(decoder.InitDecodersForSubstreams(
                    audio_element_with_data.substream_id_to_labels,
                    *audio_element_with_data.codec_config),
                IsOk());
  }
}

TEST(Decode, DecodesLpcmFrame) {
  AudioFrameDecoder decoder;

  // Encoded frames.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  const AudioFrameWithData encoded_audio_frame =
      PrepareEncodedAudioFrame(codec_config_obus, audio_elements);
  InitAllAudioElements(audio_elements, decoder);

  // Decode.
  const auto decoded_audio_frame = decoder.Decode(encoded_audio_frame);

  // Validate.
  ASSERT_THAT(decoded_audio_frame, IsOk());
  EXPECT_EQ(decoded_audio_frame->substream_id, kSubstreamId);
  EXPECT_EQ(decoded_audio_frame->start_timestamp, 0);
  EXPECT_EQ(decoded_audio_frame->end_timestamp, kNumSamplesPerFrame);
  EXPECT_EQ(decoded_audio_frame->down_mixing_params, kDownMixingParams);
  EXPECT_EQ(decoded_audio_frame->audio_element_with_data,
            &audio_elements.at(kAudioElementId));

  // For LPCM, the input bytes are all zeros, but we expect the decoder to
  // combine kBytesPerSample bytes each into one int32_t sample.
  // There are kNumSamplesPerFrame samples in the frame.
  EXPECT_EQ(decoded_audio_frame->decoded_samples.size(), kNumChannels);
  for (const auto& samples_for_channel : decoded_audio_frame->decoded_samples) {
    EXPECT_EQ(samples_for_channel.size(), kNumSamplesPerFrame);
    for (int32_t sample : samples_for_channel) {
      EXPECT_EQ(sample, 0);
    }
  }
}

TEST(Decode, DecodesFlacFrame) {
  AudioFrameDecoder decoder;

  // Encoded frames.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::vector<uint8_t> encoded_audio_frame_payload = {kFlacEncodedFrame.begin(),
                                                      kFlacEncodedFrame.end()};
  const AudioFrameWithData encoded_audio_frame = PrepareEncodedAudioFrame(
      codec_config_obus, audio_elements, CodecConfig::kCodecIdFlac,
      encoded_audio_frame_payload);
  InitAllAudioElements(audio_elements, decoder);

  // Decode.
  const auto decoded_audio_frame = decoder.Decode(encoded_audio_frame);

  // Validate.
  ASSERT_THAT(decoded_audio_frame, IsOk());
  EXPECT_EQ(decoded_audio_frame->substream_id, kSubstreamId);
  EXPECT_EQ(decoded_audio_frame->start_timestamp, 0);
  EXPECT_EQ(decoded_audio_frame->end_timestamp, kNumSamplesPerFrame);
  EXPECT_EQ(decoded_audio_frame->down_mixing_params, kDownMixingParams);
  EXPECT_EQ(decoded_audio_frame->audio_element_with_data,
            &audio_elements.at(kAudioElementId));
  const std::vector<std::vector<int32_t>> kExpectedDecodedSamplesInt32 = {
      {0x00010000, 0x00020000, 0x00030000, 0x00040000, 0x00050000, 0x00060000,
       0x00070000, 0x00080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
       0x00000000, 0x00000000, 0x00000000, 0x00000000},
      {static_cast<int32_t>(0xffff0000), static_cast<int32_t>(0xfffe0000),
       static_cast<int32_t>(0xfffd0000), static_cast<int32_t>(0xfffc0000),
       static_cast<int32_t>(0xfffb0000), static_cast<int32_t>(0xfffa0000),
       static_cast<int32_t>(0xfff90000), static_cast<int32_t>(0xfff80000),
       0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
       0x00000000, 0x00000000}};
  const auto kExpectedDecodedSamples =
      Int32ToInternalSampleType2D(kExpectedDecodedSamplesInt32);

  EXPECT_EQ(decoded_audio_frame->decoded_samples, kExpectedDecodedSamples);
}

TEST(Decode, DecodesMultipleFlacFrames) {
  AudioFrameDecoder decoder;

  // Encoded frames.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::vector<uint8_t> encoded_audio_frame_payload = {kFlacEncodedFrame.begin(),
                                                      kFlacEncodedFrame.end()};
  const AudioFrameWithData encoded_audio_frame = PrepareEncodedAudioFrame(
      codec_config_obus, audio_elements, CodecConfig::kCodecIdFlac,
      encoded_audio_frame_payload);
  InitAllAudioElements(audio_elements, decoder);

  // Decode the same frame twice.
  EXPECT_THAT(decoder.Decode(encoded_audio_frame), IsOk());
  EXPECT_THAT(decoder.Decode(encoded_audio_frame), IsOk());
}
// TODO(b/308073716): Add tests for more kinds of decoders.

}  // namespace
}  // namespace iamf_tools
