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
#include "iamf/obu/decoder_config/flac_decoder_config.h"

#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr int16_t kAudioRollDistance = 0;

TEST(GetRequiredAudioRollDistance, ReturnsFixedValue) {
  EXPECT_EQ(FlacDecoderConfig::GetRequiredAudioRollDistance(),
            kAudioRollDistance);
}

class FlacTest : public testing::Test {
 public:
  FlacTest()
      : num_samples_per_frame_(16),
        flac_decoder_config_{
            {{.header = {.last_metadata_block_flag = true,
                         .block_type = FlacMetaBlockHeader::kFlacStreamInfo,
                         .metadata_data_block_length = 34},
              .payload =
                  FlacMetaBlockStreamInfo{.minimum_block_size = 16,
                                          .maximum_block_size = 16,
                                          .sample_rate = 48000,
                                          .bits_per_sample = 15,
                                          .total_samples_in_stream = 0}}}} {
    first_stream_info_payload_ = &std::get<FlacMetaBlockStreamInfo>(
        flac_decoder_config_.metadata_blocks_[0].payload);
  }
  ~FlacTest() = default;

 protected:
  void TestWriteDecoderConfig() {
    WriteBitBuffer wb(expected_decoder_config_payload_.size());

    EXPECT_EQ(
        flac_decoder_config_
            .ValidateAndWrite(num_samples_per_frame_, audio_roll_distance_, wb)
            .code(),
        expected_write_status_code_);

    if (expected_write_status_code_ == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_decoder_config_payload_);
    }
  }

  // `num_samples_per_frame_` would typically come from the associated Codec
  // Config OBU. Some fields in the decoder config must be consistent with it,
  uint32_t num_samples_per_frame_;

  // `audio_roll_distance_` would typically come from the associated Codec
  // Config OBU. The IAMF specification REQUIRES this be 0.
  int16_t audio_roll_distance_ = 0;

  FlacDecoderConfig flac_decoder_config_;
  // A pointer which is initialized to point to the `FlacMetaBlockStreamInfo` in
  // `flac_decoder_config_`.
  FlacMetaBlockStreamInfo* first_stream_info_payload_;

  absl::StatusCode expected_write_status_code_ = absl::StatusCode::kOk;
  std::vector<uint8_t> expected_decoder_config_payload_;
};

TEST_F(FlacTest, WriteDefault) {
  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, CanContainAdditionalBlocks) {
  flac_decoder_config_.metadata_blocks_[0].header.last_metadata_block_flag =
      false;

  flac_decoder_config_.metadata_blocks_.push_back(FlacMetadataBlock{
      .header = {.last_metadata_block_flag = false,
                 .block_type = FlacMetaBlockHeader::kFlacPicture,
                 .metadata_data_block_length = 3},
      .payload = std::vector<uint8_t>{'a', 'b', 'c'}});

  flac_decoder_config_.metadata_blocks_.push_back(FlacMetadataBlock{
      .header = {.last_metadata_block_flag = true,
                 .block_type = FlacMetaBlockHeader::kFlacApplication,
                 .metadata_data_block_length = 3},
      .payload = std::vector<uint8_t>{'d', 'e', 'f'}});

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      0 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00,
      // `last_metadata_block_flag` and `block_type` fields.
      0 << 7 | FlacMetaBlockHeader::kFlacPicture,
      // `metadata_data_block_length`.
      0, 0, 3,
      // Payload.
      'a', 'b', 'c',
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacApplication,
      // `metadata_data_block_length`.
      0, 0, 3,
      // Payload.
      'd', 'e', 'f'};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMetadataBlockLengthInconsistent) {
  flac_decoder_config_.metadata_blocks_[0].header.last_metadata_block_flag =
      false;

  // `metadata_data_block_length` is inconsistent with the payload.
  flac_decoder_config_.metadata_blocks_.push_back(FlacMetadataBlock{
      .header = {.last_metadata_block_flag = true,
                 .block_type = FlacMetaBlockHeader::kFlacPicture,
                 .metadata_data_block_length = 10},
      .payload = std::vector<uint8_t>{'a', 'b', 'c'}});

  expected_write_status_code_ = absl::StatusCode::kUnknown;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalExtraneousLastMetadataBlockFlag) {
  // The final block and only the final block MUST have
  // `last_metadata_block_flag` set.
  flac_decoder_config_.metadata_blocks_[0].header.last_metadata_block_flag =
      true;
  flac_decoder_config_.metadata_blocks_.push_back(FlacMetadataBlock{
      .header = {.last_metadata_block_flag = true,
                 .block_type = FlacMetaBlockHeader::kFlacPicture,
                 .metadata_data_block_length = 3},
      .payload = std::vector<uint8_t>{'a', 'b', 'c'}});

  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalStreamInfoMustBeFirstBlock) {
  flac_decoder_config_.metadata_blocks_.insert(
      flac_decoder_config_.metadata_blocks_.begin(),
      FlacMetadataBlock{
          .header = {.last_metadata_block_flag = true,
                     .block_type = FlacMetaBlockHeader::kFlacPicture,
                     .metadata_data_block_length = 3},
          .payload = std::vector<uint8_t>{'a', 'b', 'c'}});

  ASSERT_EQ(flac_decoder_config_.metadata_blocks_.back().header.block_type,
            FlacMetaBlockHeader::kFlacStreamInfo);
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalStreamInfoMustBePresent) {
  flac_decoder_config_.metadata_blocks_[0].header = {
      .last_metadata_block_flag = true,
      .block_type = FlacMetaBlockHeader::kFlacPadding,
      .metadata_data_block_length = 0};
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteBitsPerSampleMin) {
  first_stream_info_payload_->bits_per_sample =
      FlacMetaBlockStreamInfo::kMinBitsPerSample;

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      3 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteBitsPerSampleMax) {
  first_stream_info_payload_->bits_per_sample =
      FlacMetaBlockStreamInfo::kMaxBitsPerSample;

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1) | 1,
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteVaryMostLegalFields) {
  num_samples_per_frame_ = 64;
  flac_decoder_config_.metadata_blocks_[0].payload =
      FlacMetaBlockStreamInfo{.minimum_block_size = 64,
                              .maximum_block_size = 64,
                              .sample_rate = 48000,
                              .bits_per_sample = 7,
                              .total_samples_in_stream = 100};

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 64,
      // `maximum_block_size`.
      0, 64,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          FlacMetaBlockStreamInfo::kNumberOfChannels << 1,
      7 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 100,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteSampleRateMin) {
  first_stream_info_payload_->sample_rate =
      FlacMetaBlockStreamInfo::kMinSampleRate;

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x00, 0x00,
      (0x1 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteSampleRateMax) {
  first_stream_info_payload_->sample_rate =
      FlacMetaBlockStreamInfo::kMaxSampleRate;

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x9f, 0xff,
      (0x6 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, InvalidSampleRateTooLow) {
  first_stream_info_payload_->sample_rate = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, InvalidSampleRateTooHigh) {
  first_stream_info_payload_->sample_rate = 655351;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, InvalidBitsPerSampleZero) {
  first_stream_info_payload_->bits_per_sample = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, InvalidBitsPerSampleTooLow) {
  first_stream_info_payload_->bits_per_sample = 2;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, InvalidLastMetadataFlag) {
  flac_decoder_config_.metadata_blocks_[0].header.last_metadata_block_flag =
      false;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteMinimumMaximumBlockSizeMax) {
  num_samples_per_frame_ = 65535;
  first_stream_info_payload_->minimum_block_size = 65535;
  first_stream_info_payload_->maximum_block_size = 65535;

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0xff, 0xff,
      // `maximum_block_size`.
      0xff, 0xff,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 0x00,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalAudioRollDistanceMustBeZero) {
  audio_roll_distance_ = -1;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMinimumMaximumBlockSizeZero) {
  first_stream_info_payload_->minimum_block_size = 0;
  first_stream_info_payload_->maximum_block_size = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMinimumMaximumBlockSizeEdge) {
  first_stream_info_payload_->minimum_block_size = 15;
  first_stream_info_payload_->maximum_block_size = 15;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMinimumMaximumBlockSizeNotEqualToSamplesInFrame) {
  ASSERT_NE(num_samples_per_frame_, 32);

  first_stream_info_payload_->minimum_block_size = 32;
  first_stream_info_payload_->maximum_block_size = 32;

  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMinimumMaximumBlockSizeNotEqualToEachOther) {
  first_stream_info_payload_->minimum_block_size = 16;
  first_stream_info_payload_->maximum_block_size = 32;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMinimumFrameSizeNotEqualToZero) {
  const uint32_t kInvalidMinimumFrameSize = 16;
  ASSERT_NE(kInvalidMinimumFrameSize,
            FlacMetaBlockStreamInfo::kMinimumFrameSize);
  first_stream_info_payload_->minimum_frame_size = kInvalidMinimumFrameSize;

  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMaximumFrameSizeNotEqualToZero) {
  const uint32_t kInvalidMaximumFrameSize = 16;
  ASSERT_NE(kInvalidMaximumFrameSize,
            FlacMetaBlockStreamInfo::kMaximumFrameSize);
  first_stream_info_payload_->maximum_frame_size = kInvalidMaximumFrameSize;

  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalNumberOfChannelsNotEqualToOne) {
  const uint8_t kInvalidNumberOfChannels = 2;
  ASSERT_NE(kInvalidNumberOfChannels,
            FlacMetaBlockStreamInfo::kNumberOfChannels);
  first_stream_info_payload_->number_of_channels = kInvalidNumberOfChannels;

  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, WriteTotalSamplesInStreamMax) {
  first_stream_info_payload_->total_samples_in_stream =
      FlacMetaBlockStreamInfo::kMaxTotalSamplesInStream;

  expected_decoder_config_payload_ = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 16,
      // `maximum_block_size`.
      0, 16,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          (FlacMetaBlockStreamInfo::kNumberOfChannels << 1),
      15 << 4 |
          // `total_samples_in_stream` (36 bits).
          0xf,
      0xff, 0xff, 0xff, 0xff,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, IllegalMd5SumNonZero) {
  const uint8_t kInvalidMd5SumFirstByte = 0x01;
  ASSERT_NE(FlacMetaBlockStreamInfo::kMd5Signature[0], kInvalidMd5SumFirstByte);
  first_stream_info_payload_->md5_signature[0] = kInvalidMd5SumFirstByte;

  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(FlacTest, GetOutputSampleRateMin) {
  first_stream_info_payload_->sample_rate =
      FlacMetaBlockStreamInfo::kMinSampleRate;

  uint32_t output_sample_rate;
  EXPECT_THAT(flac_decoder_config_.GetOutputSampleRate(output_sample_rate),
              IsOk());
  EXPECT_EQ(output_sample_rate, FlacMetaBlockStreamInfo::kMinSampleRate);
}

TEST_F(FlacTest, GetOutputSampleRateMax) {
  first_stream_info_payload_->sample_rate =
      FlacMetaBlockStreamInfo::kMaxSampleRate;

  uint32_t output_sample_rate;
  EXPECT_THAT(flac_decoder_config_.GetOutputSampleRate(output_sample_rate),
              IsOk());
  EXPECT_EQ(output_sample_rate, FlacMetaBlockStreamInfo::kMaxSampleRate);
}

TEST_F(FlacTest, InvalidGetOutputSampleRateTooLow) {
  ASSERT_GT(FlacMetaBlockStreamInfo::kMinSampleRate, 0);
  first_stream_info_payload_->sample_rate =
      FlacMetaBlockStreamInfo::kMinSampleRate - 1;

  uint32_t output_sample_rate;
  EXPECT_FALSE(
      flac_decoder_config_.GetOutputSampleRate(output_sample_rate).ok());
}

TEST_F(FlacTest, InvalidGetOutputSampleRateTooHigh) {
  ASSERT_LT(FlacMetaBlockStreamInfo::kMaxSampleRate,
            std::numeric_limits<uint32_t>::max());
  first_stream_info_payload_->sample_rate =
      FlacMetaBlockStreamInfo::kMaxSampleRate + 1;

  uint32_t output_sample_rate;
  EXPECT_FALSE(
      flac_decoder_config_.GetOutputSampleRate(output_sample_rate).ok());
}

TEST_F(FlacTest, InvalidGetOutputSampleRateWithNoStreamInfo) {
  flac_decoder_config_.metadata_blocks_.clear();

  uint32_t output_sample_rate;
  EXPECT_FALSE(
      flac_decoder_config_.GetOutputSampleRate(output_sample_rate).ok());
}

TEST_F(FlacTest, GetBitsPerSampleMin) {
  first_stream_info_payload_->bits_per_sample =
      FlacMetaBlockStreamInfo::kMinBitsPerSample;

  uint8_t output_bit_depth;
  EXPECT_THAT(
      flac_decoder_config_.GetBitDepthToMeasureLoudness(output_bit_depth),
      IsOk());
  EXPECT_EQ(output_bit_depth, FlacMetaBlockStreamInfo::kMinBitsPerSample + 1);
}

TEST_F(FlacTest, GetBitsPerSampleMax) {
  first_stream_info_payload_->bits_per_sample =
      FlacMetaBlockStreamInfo::kMaxBitsPerSample;

  uint8_t output_bit_depth;
  EXPECT_THAT(
      flac_decoder_config_.GetBitDepthToMeasureLoudness(output_bit_depth),
      IsOk());
  EXPECT_EQ(output_bit_depth, FlacMetaBlockStreamInfo::kMaxBitsPerSample + 1);
}

TEST_F(FlacTest, GetBitsPerSampleMinTooLow) {
  ASSERT_GT(FlacMetaBlockStreamInfo::kMinBitsPerSample, 0);
  first_stream_info_payload_->bits_per_sample =
      FlacMetaBlockStreamInfo::kMinBitsPerSample - 1;
  uint8_t unused_output_bit_depth;
  EXPECT_FALSE(
      flac_decoder_config_.GetBitDepthToMeasureLoudness(unused_output_bit_depth)
          .ok());
}

TEST_F(FlacTest, GetBitsPerSampleMaxTooHigh) {
  ASSERT_LT(FlacMetaBlockStreamInfo::kMaxBitsPerSample,
            std::numeric_limits<uint32_t>::max());
  first_stream_info_payload_->bits_per_sample =
      FlacMetaBlockStreamInfo::kMaxBitsPerSample + 1;

  uint8_t unused_output_bit_depth;
  EXPECT_FALSE(
      flac_decoder_config_.GetBitDepthToMeasureLoudness(unused_output_bit_depth)
          .ok());
}

TEST_F(FlacTest, InvalidGetBitsPerSampleWithNoStreamInfo) {
  flac_decoder_config_.metadata_blocks_.clear();

  uint8_t unused_output_bit_depth;
  EXPECT_FALSE(
      flac_decoder_config_.GetBitDepthToMeasureLoudness(unused_output_bit_depth)
          .ok());
}

TEST_F(FlacTest, GetTotalNumSamplesInStreamMin) {
  first_stream_info_payload_->total_samples_in_stream =
      FlacMetaBlockStreamInfo::kMinTotalSamplesInStream;

  uint64_t output_total_samples_in_stream;
  EXPECT_THAT(flac_decoder_config_.GetTotalSamplesInStream(
                  output_total_samples_in_stream),
              IsOk());
  EXPECT_EQ(output_total_samples_in_stream,
            FlacMetaBlockStreamInfo::kMinTotalSamplesInStream);
}

TEST_F(FlacTest, GetTotalNumSamplesInStreamMax) {
  first_stream_info_payload_->total_samples_in_stream =
      FlacMetaBlockStreamInfo::kMaxTotalSamplesInStream;

  uint64_t output_total_samples_in_stream;
  EXPECT_THAT(flac_decoder_config_.GetTotalSamplesInStream(
                  output_total_samples_in_stream),
              IsOk());
  EXPECT_EQ(output_total_samples_in_stream,
            FlacMetaBlockStreamInfo::kMaxTotalSamplesInStream);
}

TEST_F(FlacTest, InvalidGetTotalNumSamplesInStreamTooHigh) {
  ASSERT_LT(FlacMetaBlockStreamInfo::kMaxTotalSamplesInStream,
            std::numeric_limits<uint64_t>::max());
  first_stream_info_payload_->total_samples_in_stream =
      FlacMetaBlockStreamInfo::kMaxTotalSamplesInStream + 1;

  uint64_t output_total_samples_in_stream;
  EXPECT_FALSE(flac_decoder_config_
                   .GetTotalSamplesInStream(output_total_samples_in_stream)
                   .ok());
}

TEST_F(FlacTest, InvalidGetTotalNumSamplesInStreamWithNoStreamInfo) {
  flac_decoder_config_.metadata_blocks_.clear();

  uint64_t output_total_samples_in_stream;
  EXPECT_FALSE(flac_decoder_config_
                   .GetTotalSamplesInStream(output_total_samples_in_stream)
                   .ok());
}

TEST(ReadAndValidateTest, ReadAndValidateStreamInfoSuccess) {
  std::vector<uint8_t> payload = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 64,
      // `maximum_block_size`.
      0, 64,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          FlacMetaBlockStreamInfo::kNumberOfChannels << 1,
      7 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 100,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  ;
  auto rb = MemoryBasedReadBitBuffer::CreateFromVector(1024, payload);
  FlacDecoderConfig decoder_config;
  EXPECT_THAT(decoder_config.ReadAndValidate(
                  /*num_samples_per_frame=*/64, /*audio_roll_distance=*/0, *rb),
              IsOk());
  EXPECT_EQ(decoder_config.metadata_blocks_.size(), 1);
  FlacMetaBlockHeader header = decoder_config.metadata_blocks_[0].header;
  EXPECT_EQ(header.block_type, FlacMetaBlockHeader::kFlacStreamInfo);
  EXPECT_EQ(header.metadata_data_block_length, 34);
  FlacMetaBlockStreamInfo stream_info = std::get<FlacMetaBlockStreamInfo>(
      decoder_config.metadata_blocks_[0].payload);
  EXPECT_EQ(stream_info.minimum_block_size, 64);
  EXPECT_EQ(stream_info.maximum_block_size, 64);
  EXPECT_EQ(stream_info.minimum_frame_size, 0);
  EXPECT_EQ(stream_info.maximum_frame_size, 0);
  EXPECT_EQ(stream_info.sample_rate, 48000);
  EXPECT_EQ(stream_info.number_of_channels,
            FlacMetaBlockStreamInfo::kNumberOfChannels);
  EXPECT_EQ(stream_info.bits_per_sample, 7);
  EXPECT_EQ(stream_info.total_samples_in_stream, 100);
  EXPECT_EQ(stream_info.md5_signature, FlacMetaBlockStreamInfo::kMd5Signature);
}

TEST(ReadAndValidateTest, ReadAndValidateStreamInfoFailsOnInvalidMd5Signature) {
  std::vector<uint8_t> payload = {
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 64,
      // `maximum_block_size`.
      0, 64,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          FlacMetaBlockStreamInfo::kNumberOfChannels << 1,
      7 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 100,
      // MD5 sum (invalid bit at end)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x01};
  ;
  auto rb = MemoryBasedReadBitBuffer::CreateFromVector(1024, payload);
  FlacDecoderConfig decoder_config;
  EXPECT_FALSE(
      decoder_config
          .ReadAndValidate(
              /*num_samples_per_frame=*/64, /*audio_roll_distance=*/0, *rb)
          .ok());
}

}  // namespace
}  // namespace iamf_tools
