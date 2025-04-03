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

#include "iamf/cli/codec/flac_decoder_stream_callbacks.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {
namespace {

using ::testing::ElementsAreArray;

constexpr uint32_t kNumSamplesPerFrame = 1024;
constexpr int kNumChannels = 2;

using flac_callbacks::LibFlacCallbackData;

TEST(LibFlacReadCallback, ConstructorSetsNumSamplesPerChannel) {
  LibFlacCallbackData callback_data(kNumSamplesPerFrame);

  EXPECT_EQ(callback_data.num_samples_per_channel_, kNumSamplesPerFrame);
}

TEST(LibFlacReadCallback, ReadCallbackReturnsEndOfStreamForEmptyFrame) {
  const std::vector<uint8_t> kEmptyFrame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame);
  FLAC__byte buffer[1024];
  size_t bytes = 1024;

  auto status = flac_callbacks::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM);
  EXPECT_EQ(bytes, 0);
}

TEST(LibFlacReadCallback, ReturnsAbortForOversizedFrame) {
  LibFlacCallbackData callback_data(kNumSamplesPerFrame);
  FLAC__byte buffer[1024];
  size_t bytes = 1024;
  callback_data.encoded_frame_ = std::vector<uint8_t>(1025);

  auto status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                                    &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_ABORT);
  EXPECT_EQ(bytes, 0);
}

TEST(LibFlacReadCallback, ConsumesEncodedFrame) {
  LibFlacCallbackData callback_data(kNumSamplesPerFrame);
  FLAC__byte buffer[1024];
  size_t bytes = 1028;
  const std::vector<uint8_t> encoded_frame(1024, 1);
  callback_data.encoded_frame_ = encoded_frame;

  auto status = flac_callbacks::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_TRUE(callback_data.encoded_frame_.empty());
}

TEST(LibFlacReadCallback, Success) {
  LibFlacCallbackData callback_data(kNumSamplesPerFrame);
  FLAC__byte buffer[1024];
  size_t bytes = 1028;
  const std::vector<uint8_t> encoded_frame(1024, 1);
  callback_data.encoded_frame_ = encoded_frame;

  auto status = flac_callbacks::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_EQ(bytes, 1024);
  EXPECT_THAT(buffer, ElementsAreArray(encoded_frame));
}

TEST(LibFlacWriteCallback, SucceedsFor32BitSamples) {
  constexpr int kThreeSamplesPerFrame = 3;
  LibFlacCallbackData callback_data(kThreeSamplesPerFrame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = 3,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 32}};
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_THAT(callback_data.decoded_frame_,
              ElementsAreArray(std::vector<std::vector<int32_t>>(
                  {{1, 2}, {0x7fffffff, 3}, {3, 4}})));
}

TEST(LibFlacWriteCallback, SucceedsFor16BitSamples) {
  constexpr int kTwoSamplesPerFrame = 2;
  LibFlacCallbackData callback_data(kTwoSamplesPerFrame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = 2,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 16}};
  FLAC__int32 channel_0[] = {0x00001111, 0x0000ffff};
  FLAC__int32 channel_1[] = {0x00000101, 0x00002222};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_THAT(callback_data.decoded_frame_,
              ElementsAreArray(std::vector<std::vector<int32_t>>(
                  {{0x11110000, 0x01010000},
                   {static_cast<int32_t>(0xffff0000), 0x22220000}})));
}

TEST(LibFlacWriteCallback, ReturnsStatusAbortForTooSmallBlockSize) {
  constexpr int kFiveSamplesPerFrame = 2;
  // num_samples_per_channel = 5, but the encoded frame has 3 samples per
  // channel.
  LibFlacCallbackData callback_data(kFiveSamplesPerFrame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = 3,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 32}};
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_ABORT);
}

}  // namespace
}  // namespace iamf_tools
