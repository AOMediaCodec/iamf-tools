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
#include "iamf/cli/codec/flac_decoder.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/obu_header.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {
namespace {

using ::testing::ElementsAreArray;

class FlacDecoderStreamCallbacksTest : public testing::Test {
 public:
  FlacDecoderStreamCallbacksTest()
      : flac_decoder_(FlacDecoder(
            (CodecConfigObu(ObuHeader(), 0,
                            {.codec_id = CodecConfig::kCodecIdFlac,
                             .num_samples_per_frame = 1024,
                             .audio_roll_distance = -1,
                             .decoder_config = FlacDecoderConfig{}})),
            /*num_channels=*/2)) {}

 protected:
  FlacDecoder flac_decoder_;
};

TEST_F(FlacDecoderStreamCallbacksTest, ReadCallbackEmptyFrame) {
  FLAC__byte buffer[1024];
  size_t bytes = 1024;
  auto status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                                    &flac_decoder_);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM);
  EXPECT_EQ(bytes, 0);
}

TEST_F(FlacDecoderStreamCallbacksTest, ReadCallbackFrameTooLarge) {
  FLAC__byte buffer[1024];
  size_t bytes = 1024;
  flac_decoder_.SetEncodedFrame(std::vector<uint8_t>(1025));
  auto status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                                    &flac_decoder_);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_ABORT);
  EXPECT_EQ(bytes, 0);
}

TEST_F(FlacDecoderStreamCallbacksTest, ReadCallbackSuccess) {
  FLAC__byte buffer[1024];
  size_t bytes = 1028;
  const std::vector<uint8_t> encoded_frame(1024, 1);
  flac_decoder_.SetEncodedFrame(encoded_frame);
  auto status = LibFlacReadCallback(/*stream_decoder=*/nullptr, buffer, &bytes,
                                    &flac_decoder_);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_READ_STATUS_CONTINUE);
  EXPECT_EQ(bytes, 1024);
  EXPECT_THAT(buffer, ElementsAreArray(encoded_frame));
}

TEST_F(FlacDecoderStreamCallbacksTest, WriteCallbackSuccess32BitSamples) {
  FLAC__Frame frame;
  frame.header.channels = 2;
  frame.header.blocksize = 3;
  frame.header.bits_per_sample = 32;
  FLAC__int32 channel_0[] = {1, 0x7fffffff, 3};
  FLAC__int32 channel_1[] = {2, 3, 4};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};
  auto status = LibFlacWriteCallback(/*stream_decoder=*/nullptr, &frame, buffer,
                                     &flac_decoder_);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_THAT(flac_decoder_.GetDecodedFrame(),
              ElementsAreArray(std::vector<std::vector<int32_t>>(
                  {{1, 0x7fffffff, 3}, {2, 3, 4}})));
}

TEST_F(FlacDecoderStreamCallbacksTest, WriteCallbackSuccess16BitSamples) {
  FLAC__Frame frame;
  frame.header.channels = 2;
  frame.header.blocksize = 2;
  frame.header.bits_per_sample = 16;
  FLAC__int32 channel_0[] = {0x11110000, 0x7fff0000};
  FLAC__int32 channel_1[] = {0x01010000, 0x22220000};
  const FLAC__int32 *const buffer[] = {channel_0, channel_1};
  auto status = LibFlacWriteCallback(/*stream_decoder=*/nullptr, &frame, buffer,
                                     &flac_decoder_);
  EXPECT_EQ(status, FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE);
  EXPECT_THAT(flac_decoder_.GetDecodedFrame(),
              ElementsAreArray(std::vector<std::vector<int32_t>>(
                  {{0x11110000, 0x7fff0000}, {0x01010000, 0x22220000}})));
}

}  // namespace
}  // namespace iamf_tools
