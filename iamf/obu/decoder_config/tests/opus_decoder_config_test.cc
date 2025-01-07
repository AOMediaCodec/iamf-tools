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
#include "iamf/obu/decoder_config/opus_decoder_config.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

class OpusTest : public testing::Test {
 public:
  OpusTest() : opus_decoder_config_({.version_ = 1, .pre_skip_ = 0}) {}
  ~OpusTest() = default;

 protected:
  void TestWriteDecoderConfig() {
    WriteBitBuffer wb(expected_decoder_config_payload_.size());

    // `num_samples_per_frame` and `audio_roll_distance` would typically come
    // from the associated Codec Config OBU. Choose arbitrary legal values as
    // default.
    static constexpr uint32_t kNumSamplesPerFrame = 960;
    static constexpr int16_t kAudioRollDistance = -4;

    EXPECT_EQ(opus_decoder_config_
                  .ValidateAndWrite(kNumSamplesPerFrame, kAudioRollDistance, wb)
                  .code(),
              expected_write_status_code_);

    if (expected_write_status_code_ == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_decoder_config_payload_);
    }
  }

  OpusDecoderConfig opus_decoder_config_;

  absl::StatusCode expected_write_status_code_ = absl::StatusCode::kOk;
  std::vector<uint8_t> expected_decoder_config_payload_;
};

TEST(OpusDecoderConfig, IamfFixedFieldsAreDefault) {
  OpusDecoderConfig decoder_config;
  // The IAMF spec REQUIRES fixed fields for all Opus Decoder Configs. Verify
  // the default constructor configures these to the fixed values.
  EXPECT_EQ(decoder_config.output_channel_count_,
            OpusDecoderConfig::kOutputChannelCount);
  EXPECT_EQ(decoder_config.output_gain_, OpusDecoderConfig::kOutputGain);
  EXPECT_EQ(decoder_config.mapping_family_, OpusDecoderConfig::kMappingFamily);
}

TEST_F(OpusTest, WriteDefault) {
  expected_decoder_config_payload_ = {// `version`.
                                      1,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0, 0,
                                      // `input_sample_rate`.
                                      0, 0, 0, 0,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, VaryAllLegalFields) {
  opus_decoder_config_ = {
      .version_ = 2, .pre_skip_ = 3, .input_sample_rate_ = 4};
  expected_decoder_config_payload_ = {// `version`.
                                      2,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0, 3,
                                      // `input_sample_rate`.
                                      0, 0, 0, 4,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, MaxAllLegalFields) {
  opus_decoder_config_ = {
      .version_ = 15, .pre_skip_ = 0xffff, .input_sample_rate_ = 0xffffffff};
  expected_decoder_config_payload_ = {// `version`.
                                      15,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0xff, 0xff,
                                      // `input_sample_rate`.
                                      0xff, 0xff, 0xff, 0xff,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, MinorVersion) {
  opus_decoder_config_.version_ = 2;
  expected_decoder_config_payload_ = {// `version`.
                                      2,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0, 0,
                                      // `input_sample_rate`.
                                      0, 0, 0, 0,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalVersionZero) {
  opus_decoder_config_.version_ = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalVersionFuture) {
  opus_decoder_config_.version_ = 16;
  expected_write_status_code_ = absl::StatusCode::kUnimplemented;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalVersionMax) {
  opus_decoder_config_.version_ = 255;
  expected_write_status_code_ = absl::StatusCode::kUnimplemented;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalChannelCountZero) {
  opus_decoder_config_.output_channel_count_ = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalChannelCountEdgeBelow) {
  opus_decoder_config_.output_channel_count_ = 1;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalChannelCountEdgeAbove) {
  opus_decoder_config_.output_channel_count_ = 3;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, WritePreSkip) {
  opus_decoder_config_.pre_skip_ = 1;
  expected_decoder_config_payload_ = {// `version`.
                                      1,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0, 1,
                                      // `input_sample_rate`.
                                      0, 0, 0, 0,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, WritePreSkip312) {
  opus_decoder_config_.pre_skip_ = 312;
  expected_decoder_config_payload_ = {// `version`.
                                      1,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0x01, 0x38,
                                      // `input_sample_rate`.
                                      0, 0, 0, 0,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, WriteSampleRate48kHz) {
  opus_decoder_config_.input_sample_rate_ = 48000;
  expected_decoder_config_payload_ = {// `version`.
                                      1,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0, 0,
                                      // `input_sample_rate`.
                                      0, 0, 0xbb, 0x80,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, WriteSampleRate192kHz) {
  opus_decoder_config_.input_sample_rate_ = 192000;
  expected_decoder_config_payload_ = {// `version`.
                                      1,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`.
                                      0, 0,
                                      // `input_sample_rate`.
                                      0, 0x2, 0xee, 0x00,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, GetInputSampleRateZero) {
  opus_decoder_config_.input_sample_rate_ = 0;
  EXPECT_EQ(opus_decoder_config_.GetInputSampleRate(), 0);
}

TEST_F(OpusTest, GetInputSampleRate96kHz) {
  opus_decoder_config_.input_sample_rate_ = 96000;
  EXPECT_EQ(opus_decoder_config_.GetInputSampleRate(), 96000);
}

TEST_F(OpusTest, AlwaysReturns48kHz) {
  EXPECT_EQ(opus_decoder_config_.GetOutputSampleRate(), 48000);
}

TEST(GetBitDepthToMeasureLoudness, AlwaysReturns32) {
  EXPECT_EQ(OpusDecoderConfig::GetBitDepthToMeasureLoudness(), 32);
}

TEST_F(OpusTest, IllegalOutputGainNotZero) {
  opus_decoder_config_.output_gain_ = 1;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(OpusTest, IllegalMappingFamilyNotZero) {
  opus_decoder_config_.mapping_family_ = 1;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

// --- Begin ReadAndValidate Tests ---

TEST(ReadAndValidate, VaryAllLegalFields) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 2,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 3,
                                 // `input_sample_rate`.
                                 0, 0, 0, 4,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_THAT(opus_decoder_config.ReadAndValidate(
                  num_samples_per_frame, audio_roll_distance, *read_buffer),
              IsOk());

  EXPECT_EQ(opus_decoder_config.version_, 2);
  EXPECT_EQ(opus_decoder_config.pre_skip_, 3);
  EXPECT_EQ(opus_decoder_config.input_sample_rate_, 4);
}

TEST(ReadAndValidate, MaxAllLegalFields) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 15,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0xff, 0xff,
                                 // `input_sample_rate`.
                                 0xff, 0xff, 0xff, 0xff,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_THAT(opus_decoder_config.ReadAndValidate(
                  num_samples_per_frame, audio_roll_distance, *read_buffer),
              IsOk());

  EXPECT_EQ(opus_decoder_config.version_, 15);
  EXPECT_EQ(opus_decoder_config.pre_skip_, 0xffff);
  EXPECT_EQ(opus_decoder_config.input_sample_rate_, 0xffffffff);
}

TEST(ReadAndValidate, MinorVersion) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 2,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0, 0, 0,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_THAT(opus_decoder_config.ReadAndValidate(
                  num_samples_per_frame, audio_roll_distance, *read_buffer),
              IsOk());

  EXPECT_EQ(opus_decoder_config.version_, 2);
}

TEST(ReadAndValidate, IllegalVersionZero) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 0,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0, 0, 0,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_FALSE(opus_decoder_config
                   .ReadAndValidate(num_samples_per_frame, audio_roll_distance,
                                    *read_buffer)
                   .ok());
}

TEST(ReadAndValidate, IllegalVersionFuture) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 16,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0, 0, 0,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_FALSE(opus_decoder_config
                   .ReadAndValidate(num_samples_per_frame, audio_roll_distance,
                                    *read_buffer)
                   .ok());
}

TEST(ReadAndValidate, IllegalVersionmax) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 255,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0, 0, 0,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_FALSE(opus_decoder_config
                   .ReadAndValidate(num_samples_per_frame, audio_roll_distance,
                                    *read_buffer)
                   .ok());
}

TEST(ReadAndValidate, IllegalChannelCountZero) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 2,
                                 // `output_channel_count`.
                                 0,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0, 0, 0,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_FALSE(opus_decoder_config
                   .ReadAndValidate(num_samples_per_frame, audio_roll_distance,
                                    *read_buffer)
                   .ok());
}

TEST(ReadAndValidate, ReadPreSkip312) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 1,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0x01, 0x38,
                                 // `input_sample_rate`.
                                 0, 0, 0, 0,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_THAT(opus_decoder_config.ReadAndValidate(
                  num_samples_per_frame, audio_roll_distance, *read_buffer),
              IsOk());

  EXPECT_EQ(opus_decoder_config.version_, 1);
  EXPECT_EQ(opus_decoder_config.pre_skip_, 312);
}

TEST(ReadAndValidate, ReadSampleRate48kHz) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 1,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0, 0xbb, 0x80,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_THAT(opus_decoder_config.ReadAndValidate(
                  num_samples_per_frame, audio_roll_distance, *read_buffer),
              IsOk());

  EXPECT_EQ(opus_decoder_config.version_, 1);
  EXPECT_EQ(opus_decoder_config.input_sample_rate_, 48000);
}

TEST(ReadAndValidate, ReadSampleRate192kHz) {
  OpusDecoderConfig opus_decoder_config;
  uint32_t num_samples_per_frame = 960;
  int16_t audio_roll_distance = -4;
  std::vector<uint8_t> source = {// `version`.
                                 1,
                                 // `output_channel_count`.
                                 OpusDecoderConfig::kOutputChannelCount,
                                 // `pre_skip`.
                                 0, 0,
                                 // `input_sample_rate`.
                                 0, 0x2, 0xee, 0x00,
                                 // `output_gain`.
                                 0, 0,
                                 // `mapping_family`.
                                 OpusDecoderConfig::kMappingFamily};
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  EXPECT_THAT(opus_decoder_config.ReadAndValidate(
                  num_samples_per_frame, audio_roll_distance, *read_buffer),
              IsOk());

  EXPECT_EQ(opus_decoder_config.version_, 1);
  EXPECT_EQ(opus_decoder_config.input_sample_rate_, 192000);
}

struct GetRequiredAudioRollDistanceTestCase {
  uint32_t num_samples_per_frame;
  int16_t expected_audio_roll_distance;
};

class GetRequiredAudioRollDistanceTest
    : public testing::TestWithParam<GetRequiredAudioRollDistanceTestCase> {};

TEST_P(GetRequiredAudioRollDistanceTest, ValidAudioRollDistance) {
  EXPECT_THAT(OpusDecoderConfig::GetRequiredAudioRollDistance(
                  GetParam().num_samples_per_frame),
              IsOkAndHolds(GetParam().expected_audio_roll_distance));
}

INSTANTIATE_TEST_SUITE_P(
    Legal, GetRequiredAudioRollDistanceTest,
    testing::ValuesIn<GetRequiredAudioRollDistanceTestCase>({
        {1, -3840},
        {2, -1920},
        {3, -1280},
        {7, -549},
        {240, -16},
        {959, -5},
        {960, -4},
        {1280, -3},
        {1920, -2},
        {3840, -1},
        {0xffffffff, -1},
    }));

TEST(GetRequiredAudioRollDistance, IsInvalidWhenNumSamplesPerFrameIsZero) {
  constexpr uint32_t kInvalidNumSamplesPerFrame = 0;
  EXPECT_FALSE(OpusDecoderConfig::GetRequiredAudioRollDistance(
                   kInvalidNumSamplesPerFrame)
                   .ok());
}

TEST(ValidateAndWrite, ValidatesAudioRollDistance) {
  constexpr OpusDecoderConfig opus_decoder_config_ = {
      .version_ = 1, .pre_skip_ = 312, .input_sample_rate_ = 0};
  constexpr uint32_t kNumSamplesPerFrame = 960;
  constexpr int16_t kAudioRollDistance = -4;
  constexpr int16_t kInvalidAudioRollDistance = -5;
  WriteBitBuffer ignored_wb(128);

  EXPECT_THAT(opus_decoder_config_.ValidateAndWrite(
                  kNumSamplesPerFrame, kAudioRollDistance, ignored_wb),
              IsOk());
  EXPECT_FALSE(opus_decoder_config_
                   .ValidateAndWrite(kNumSamplesPerFrame,
                                     kInvalidAudioRollDistance, ignored_wb)
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
