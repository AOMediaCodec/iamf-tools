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
#include "iamf/opus_decoder_config.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/tests/test_utils.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {
namespace {

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

struct AudioRollDistanceTestCase {
  int16_t audio_roll_distance;
  uint32_t num_samples_per_frame;
  absl::StatusCode expected_status_code;
};

class OpusDecoderConfigTestForAudioRollDistance
    : public testing::TestWithParam<AudioRollDistanceTestCase> {
 protected:
  // A decoder config with reasonable default values. They are not relevant to
  // the test.
  const OpusDecoderConfig opus_decoder_config_ = {
      .version_ = 1, .pre_skip_ = 312, .input_sample_rate_ = 0};
};

TEST_P(OpusDecoderConfigTestForAudioRollDistance, TestOpusDecoderConfig) {
  WriteBitBuffer ignored_wb(128);

  EXPECT_EQ(opus_decoder_config_
                .ValidateAndWrite(GetParam().num_samples_per_frame,
                                  GetParam().audio_roll_distance, ignored_wb)
                .code(),
            GetParam().expected_status_code);
}

INSTANTIATE_TEST_SUITE_P(Legal, OpusDecoderConfigTestForAudioRollDistance,
                         testing::ValuesIn<AudioRollDistanceTestCase>({
                             {-3840, 1, absl::StatusCode::kOk},
                             {-1920, 2, absl::StatusCode::kOk},
                             {-1280, 3, absl::StatusCode::kOk},
                             {-549, 7, absl::StatusCode::kOk},
                             {-16, 240, absl::StatusCode::kOk},
                             {-5, 959, absl::StatusCode::kOk},
                             {-4, 960, absl::StatusCode::kOk},
                             {-3, 1280, absl::StatusCode::kOk},
                             {-2, 1920, absl::StatusCode::kOk},
                             {-1, 3840, absl::StatusCode::kOk},
                             {-1, 0xffffffff, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(
    Illegal, OpusDecoderConfigTestForAudioRollDistance,
    testing::ValuesIn<AudioRollDistanceTestCase>({
        {0, 0, absl::StatusCode::kInvalidArgument},
        {0, 1, absl::StatusCode::kInvalidArgument},
        {1, 0, absl::StatusCode::kInvalidArgument},
        {-5, 960, absl::StatusCode::kInvalidArgument},
        {4, 960, absl::StatusCode::kInvalidArgument},
        {4, 960, absl::StatusCode::kInvalidArgument},
        {-3, 960, absl::StatusCode::kInvalidArgument},
        {-32768, 0xffffffff, absl::StatusCode::kInvalidArgument},
        {32767, 0xffffffff, absl::StatusCode::kInvalidArgument},
    }));

}  // namespace
}  // namespace iamf_tools
