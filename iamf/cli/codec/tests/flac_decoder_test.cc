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

#include "iamf/cli/codec/flac_decoder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAreArray;
using ::testing::Test;

// Derived from iamf/cli/testdata/stereo_8_samples_48khz_s16le.wav @ 16 samples
// per frame.
constexpr std::array<uint8_t, 22> kFlacEncodedFrame = {
    0xff, 0xf8, 0x6a, 0xa8, 0x00, 0x0f, 0x42, 0x00, 0x00, 0x00, 0x13,
    0x80, 0x00, 0x80, 0x04, 0x92, 0x49, 0x00, 0x01, 0xfe, 0x81, 0xee};

class FlacDecoderTest : public Test {
 public:
  FlacDecoder CreateFlacDecoder(int num_samples_per_frame = 16) {
    auto codec_config_obu = CodecConfigObu(
        ObuHeader(), 0,
        {.codec_id = CodecConfig::kCodecIdFlac,
         .num_samples_per_frame = DecodedUleb128(num_samples_per_frame),
         .audio_roll_distance = -1,
         .decoder_config = FlacDecoderConfig{
             {{{.header = {.last_metadata_block_flag = true,
                           .block_type = FlacMetaBlockHeader::kFlacStreamInfo,
                           .metadata_data_block_length = 34},
                .payload =
                    FlacMetaBlockStreamInfo{.minimum_block_size = 16,
                                            .maximum_block_size = 16,
                                            .sample_rate = 48000,
                                            .bits_per_sample = 15,
                                            .total_samples_in_stream = 0}}}}}});
    EXPECT_THAT(codec_config_obu.Initialize(), IsOk());
    return FlacDecoder(codec_config_obu,
                       /*num_channels=*/2);
  }
};

TEST_F(FlacDecoderTest, ReadCallbackEmptyFrame) {
  FlacDecoder flac_decoder = CreateFlacDecoder();
  FLAC__byte buffer[1024];
  size_t bytes = 1024;
  auto status = FlacDecoder::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &flac_decoder);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM);
  EXPECT_EQ(bytes, 0);
}

TEST_F(FlacDecoderTest, ReadCallbackFrameTooLarge) {
  FlacDecoder flac_decoder = CreateFlacDecoder();
  FLAC__byte buffer[1024];
  size_t bytes = 1024;
  flac_decoder.SetEncodedFrame(std::vector<uint8_t>(1025));
  auto status = FlacDecoder::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &flac_decoder);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_ABORT);
  EXPECT_EQ(bytes, 0);
}

TEST_F(FlacDecoderTest, ReadCallbackSuccess) {
  FlacDecoder flac_decoder = CreateFlacDecoder();
  FLAC__byte buffer[1024];
  size_t bytes = 1028;
  const std::vector<uint8_t> encoded_frame(1024, 1);
  flac_decoder.SetEncodedFrame(encoded_frame);
  auto status = FlacDecoder::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &flac_decoder);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_EQ(bytes, 1024);
  EXPECT_THAT(buffer, ElementsAreArray(encoded_frame));
}

TEST_F(FlacDecoderTest, WriteCallbackSuccess32BitSamples) {
  FlacDecoder flac_decoder = CreateFlacDecoder(3);
  FLAC__Frame frame;
  frame.header.channels = 2;
  frame.header.blocksize = 3;
  frame.header.bits_per_sample = 32;
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};
  auto status = FlacDecoder::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &frame, buffer, &flac_decoder);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_THAT(flac_decoder.GetDecodedFrame(),
              ElementsAreArray(std::vector<std::vector<int32_t>>(
                  {{1, 2}, {0x7fffffff, 3}, {3, 4}})));
}

TEST_F(FlacDecoderTest, WriteCallbackSuccess16BitSamples) {
  FlacDecoder flac_decoder = CreateFlacDecoder(2);
  FLAC__Frame frame;
  frame.header.channels = 2;
  frame.header.blocksize = 2;
  frame.header.bits_per_sample = 16;
  FLAC__int32 channel_0[] = {0x00001111, 0x0000ffff};
  FLAC__int32 channel_1[] = {0x00000101, 0x00002222};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};
  auto status = FlacDecoder::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &frame, buffer, &flac_decoder);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_THAT(flac_decoder.GetDecodedFrame(),
              ElementsAreArray(std::vector<std::vector<int32_t>>(
                  {{0x11110000, 0x01010000},
                   {static_cast<int32_t>(0xffff0000), 0x22220000}})));
}

TEST_F(FlacDecoderTest, WriteCallbackFailsOnMismatchedBlocksize) {
  // num_samples_per_channel = 5, but the encoded frame has 3 samples per
  // channel.
  FlacDecoder flac_decoder = CreateFlacDecoder(5);
  FLAC__Frame frame;
  frame.header.channels = 2;
  frame.header.blocksize = 3;
  frame.header.bits_per_sample = 32;
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};
  auto status = FlacDecoder::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &frame, buffer, &flac_decoder);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_ABORT);
}

TEST_F(FlacDecoderTest, InitializeSuccess) {
  FlacDecoder flac_decoder = CreateFlacDecoder();
  EXPECT_THAT(flac_decoder.Initialize(), IsOk());
}

TEST_F(FlacDecoderTest, DecodeAudioFrameSuccess) {
  FlacDecoder flac_decoder = CreateFlacDecoder();

  EXPECT_THAT(flac_decoder.Initialize(), IsOk());
  auto status = flac_decoder.DecodeAudioFrame(
      std::vector(kFlacEncodedFrame.begin(), kFlacEncodedFrame.end()));
  EXPECT_THAT(status, IsOk());

  const std::vector<std::vector<int32_t>> kExpectedDecodedSamples = {
      {0x00010000, static_cast<int32_t>(0xffff0000)},
      {0x00020000, static_cast<int32_t>(0xfffe0000)},
      {0x00030000, static_cast<int32_t>(0xfffd0000)},
      {0x00040000, static_cast<int32_t>(0xfffc0000)},
      {0x00050000, static_cast<int32_t>(0xfffb0000)},
      {0x00060000, static_cast<int32_t>(0xfffa0000)},
      {0x00070000, static_cast<int32_t>(0xfff90000)},
      {0x00080000, static_cast<int32_t>(0xfff80000)},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000}};
  EXPECT_EQ(flac_decoder.ValidDecodedSamples(), kExpectedDecodedSamples);

  // Decode again.
  status = flac_decoder.DecodeAudioFrame(
      std::vector(kFlacEncodedFrame.begin(), kFlacEncodedFrame.end()));
  EXPECT_THAT(status, IsOk());
  EXPECT_EQ(flac_decoder.ValidDecodedSamples(), kExpectedDecodedSamples);
}

TEST_F(FlacDecoderTest, DecodeAudioFrameFailsOnMismatchedBlocksize) {
  // num_samples_per_channel = 32, but the encoded frame has 16 samples per
  // channel.
  FlacDecoder flac_decoder = CreateFlacDecoder(32);
  EXPECT_THAT(flac_decoder.Initialize(), IsOk());
  auto status = flac_decoder.DecodeAudioFrame(
      std::vector(kFlacEncodedFrame.begin(), kFlacEncodedFrame.end()));
  EXPECT_FALSE(status.ok());
}

}  // namespace

}  // namespace iamf_tools
