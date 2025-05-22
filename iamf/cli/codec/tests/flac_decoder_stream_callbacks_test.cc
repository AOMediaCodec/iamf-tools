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
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/types.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {
namespace {

using ::testing::ElementsAreArray;

constexpr uint32_t kNumSamplesPerFrame = 1024;
constexpr int kNumChannels = 2;

using flac_callbacks::LibFlacCallbackData;

TEST(LibFlacCallbackData, ConstructorSetsNumSamplesPerChannel) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);

  EXPECT_EQ(callback_data.num_samples_per_channel_, kNumSamplesPerFrame);
}

TEST(LibFlacCallbackData, SetEncodedFrameRemovesPreviouslySetFrame) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  // Intentionally get the buffer to a state where it was partially exhausted.
  callback_data.SetEncodedFrame({99, 100});
  // Intentionally avoid exhausting the buffer.
  callback_data.GetNextSlice(1);

  // Resetting it gets rid of any trace of the previous frame.
  const std::vector<uint8_t> encoded_frame = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  callback_data.SetEncodedFrame(encoded_frame);
  EXPECT_EQ(callback_data.GetNextSlice(10), encoded_frame);
}

TEST(LibFlacCallbackData, GetNextSliceCapsOutputToAtMostRemainingSize) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  const std::vector<uint8_t> kEncodedFrame = {99, 100};
  callback_data.SetEncodedFrame(kEncodedFrame);

  // It's ok to request more bytes than are available, fewer will be returned if
  // there are not enough left.
  EXPECT_EQ(callback_data.GetNextSlice(kEncodedFrame.size() + 1),
            kEncodedFrame);
}

TEST(LibFlacCallbackData, RepeatedCallsToGetNextSliceReturnNextSlice) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  const std::vector<uint8_t> encoded_frame = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  callback_data.SetEncodedFrame(encoded_frame);

  EXPECT_EQ(callback_data.GetNextSlice(5),
            std::vector<uint8_t>({1, 2, 3, 4, 5}));
  EXPECT_EQ(callback_data.GetNextSlice(5),
            std::vector<uint8_t>({6, 7, 8, 9, 10}));
}

TEST(LibFlacCallbackData, CallsWhenBufferIsExhaustedReturnEmptySpan) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  constexpr int kNumBytes = 5;
  const std::vector<uint8_t> encoded_frame(kNumBytes, 0);
  callback_data.SetEncodedFrame(encoded_frame);
  callback_data.GetNextSlice(kNumBytes);

  EXPECT_TRUE(callback_data.GetNextSlice(kNumBytes).empty());
}

TEST(LibFlacReadCallback, ReadCallbackReturnsEndOfStreamForEmptyFrame) {
  const std::vector<uint8_t> kEmptyFrame;
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  FLAC__byte buffer[1024];
  size_t bytes = 1024;

  auto status = flac_callbacks::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM);
  EXPECT_EQ(bytes, 0);
}

TEST(LibFlacReadCallback, ReadCallbackReturnsAbortForNullPtrs) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  FLAC__byte buffer[1024];
  size_t bytes = 1024;

  // Various `nullptr` arguments will force the callback to abort.
  EXPECT_EQ(flac_callbacks::LibFlacReadCallback(
                /*stream_decoder=*/nullptr, /*buffer=*/nullptr, &bytes,
                &callback_data),
            FLAC__STREAM_DECODER_READ_STATUS_ABORT);
  EXPECT_EQ(flac_callbacks::LibFlacReadCallback(
                /*stream_decoder=*/nullptr, buffer, /*bytes=*/nullptr,
                &callback_data),
            FLAC__STREAM_DECODER_READ_STATUS_ABORT);
  EXPECT_EQ(flac_callbacks::LibFlacReadCallback(/*stream_decoder=*/nullptr,
                                                buffer, &bytes,
                                                /*client_data=*/nullptr),
            FLAC__STREAM_DECODER_READ_STATUS_ABORT);
}

TEST(LibFlacReadCallback, EachCallToReadCallbackWritesUpToBufferSize) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  // Simulate `libFLAC` requestes 8 bytes at a time.
  const size_t kFlacBufferSize = 8;
  FLAC__byte buffer[kFlacBufferSize];
  // But raw frame has 9 bytes.
  callback_data.SetEncodedFrame({1, 2, 3, 4, 5, 6, 7, 8, 9});

  // The first call loads the first 8 bytes.
  size_t bytes = kFlacBufferSize;
  auto status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                                    &callback_data);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_EQ(bytes, 8);
  EXPECT_THAT(buffer, ElementsAreArray({1, 2, 3, 4, 5, 6, 7, 8}));

  // The second call loads the last byte.
  bytes = kFlacBufferSize;
  status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                               &callback_data);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_EQ(bytes, 1);
  // Expect the first byte is 9 using a matcher
  EXPECT_THAT(buffer[0], 9);

  // Finally the frame is exhausted, subsequent calls return end of stream.
  bytes = kFlacBufferSize;
  status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                               &callback_data);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM);
  EXPECT_EQ(bytes, 0);
}

TEST(LibFlacReadCallback, ConsumesEncodedFrame) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  FLAC__byte buffer[1024];
  size_t bytes = 1028;
  const std::vector<uint8_t> encoded_frame(1024, 1);
  callback_data.SetEncodedFrame(encoded_frame);

  auto status = flac_callbacks::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  // Any further reads are safe, but will return an empty span.
  const size_t kChunkSize = 1;
  EXPECT_TRUE(callback_data.GetNextSlice(kChunkSize).empty());
}

TEST(LibFlacReadCallback, Success) {
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kNumSamplesPerFrame, decoded_frame);
  FLAC__byte buffer[1024];
  size_t bytes = 1028;
  const std::vector<uint8_t> encoded_frame(1024, 1);
  callback_data.SetEncodedFrame(encoded_frame);

  auto status = flac_callbacks::LibFlacReadCallback(
      /*stream_decoder=*/nullptr, buffer, &bytes, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_EQ(bytes, 1024);
  EXPECT_THAT(buffer, ElementsAreArray(encoded_frame));
}

TEST(LibFlacWriteCallback, SucceedsFor32BitSamples) {
  constexpr int kThreeSamplesPerFrame = 3;
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kThreeSamplesPerFrame, decoded_frame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = 3,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 32}};
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  const auto kExpectedDecodedSamples = Int32ToInternalSampleType2D(
      std::vector<std::vector<int32_t>>({{1, 0x7fffffff, 3}, {2, 3, 4}}));
  EXPECT_THAT(callback_data.decoded_frame_,
              ElementsAreArray(kExpectedDecodedSamples));
}

TEST(LibFlacWriteCallback, SucceedsFor16BitSamples) {
  constexpr int kTwoSamplesPerFrame = 2;
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kTwoSamplesPerFrame, decoded_frame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = 2,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 16}};
  FLAC__int32 channel_0[] = {0x00001111, 0x0000ffff};
  FLAC__int32 channel_1[] = {0x00000101, 0x00002222};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  const auto kExpectedDecodedSamples =
      Int32ToInternalSampleType2D(std::vector<std::vector<int32_t>>(
          {{0x11110000, static_cast<int32_t>(0xffff0000)},
           {0x01010000, 0x22220000}}));
  EXPECT_THAT(callback_data.decoded_frame_,
              ElementsAreArray(kExpectedDecodedSamples));
}

TEST(LibFlacWriteCallback, ReturnsStatusAbortForTooSmallBlockSize) {
  constexpr int kTwoSamplesPerFrame = 2;
  constexpr int kLargerBlockSize = 3;
  // num_samples_per_channel = 2, but the encoded frame has 3 samples per
  // channel.
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kTwoSamplesPerFrame, decoded_frame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = kLargerBlockSize,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 32}};
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_ABORT);
}

TEST(LibFlacWriteCallback, FillsExtraSamplesWithZeros) {
  constexpr int kFourSamplesPerFrame = 4;
  constexpr int kSmallerBlockSize = 3;
  // num_samples_per_channel = 4, but the encoded frame has 3 samples per
  // channel.
  std::vector<std::vector<InternalSampleType>> decoded_frame;
  LibFlacCallbackData callback_data(kFourSamplesPerFrame, decoded_frame);
  const FLAC__Frame kFlacFrame = {.header = {.blocksize = kSmallerBlockSize,
                                             .channels = kNumChannels,
                                             .bits_per_sample = 32}};
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};

  auto status = flac_callbacks::LibFlacWriteCallback(
      /*stream_decoder=*/nullptr, &kFlacFrame, buffer, &callback_data);

  // The last sample is extra, and should be filled with zeros.
  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_EQ(callback_data.decoded_frame_.size(), kNumChannels);
  for (int c = 0; c < kNumChannels; c++) {
    EXPECT_EQ(callback_data.decoded_frame_[c][3], 0);
  }
}

}  // namespace
}  // namespace iamf_tools
