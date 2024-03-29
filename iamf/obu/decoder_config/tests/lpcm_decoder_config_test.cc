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
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

class LpcmTest : public testing::Test {
 public:
  LpcmTest()
      : lpcm_decoder_config_(
            {LpcmDecoderConfig::kLpcmLittleEndian, 16, 48000}) {}
  ~LpcmTest() = default;

 protected:
  void TestWriteDecoderConfig() {
    WriteBitBuffer wb(expected_decoder_config_payload_.size());

    EXPECT_EQ(
        lpcm_decoder_config_.ValidateAndWrite(audio_roll_distance_, wb).code(),
        expected_write_status_code_);

    if (expected_write_status_code_ == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_decoder_config_payload_);
    }
  }

  // `audio_roll_distance_` would typically come from the associated Codec
  // Config OBU. The IAMF specification REQUIRES this be 0.
  int audio_roll_distance_ = 0;

  LpcmDecoderConfig lpcm_decoder_config_;

  absl::StatusCode expected_write_status_code_ = absl::StatusCode::kOk;
  std::vector<uint8_t> expected_decoder_config_payload_;
};

TEST_F(LpcmTest, WriteDefault) {
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      1,
                                      // `sample_size`.
                                      16,
                                      // `sample_rate`.
                                      0x00, 0x00, 0xbb, 0x80};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, WriteSampleFormatFlags) {
  lpcm_decoder_config_.sample_format_flags_ = LpcmDecoderConfig::kLpcmBigEndian;
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      0,
                                      // `sample_size`.
                                      16,
                                      // `sample_rate`.
                                      0x00, 0x00, 0xbb, 0x80};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleFormatFlagsMin) {
  lpcm_decoder_config_.sample_format_flags_ =
      static_cast<LpcmDecoderConfig::LpcmFormatFlags>(
          LpcmDecoderConfig::kLpcmBeginReserved);
  expected_write_status_code_ = absl::StatusCode::kUnimplemented;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleFormatFlagsMax) {
  lpcm_decoder_config_.sample_format_flags_ =
      static_cast<LpcmDecoderConfig::LpcmFormatFlags>(
          LpcmDecoderConfig::kLpcmEndReserved);
  expected_write_status_code_ = absl::StatusCode::kUnimplemented;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, WriteSampleSize24) {
  lpcm_decoder_config_.sample_size_ = 24;
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      1,
                                      // `sample_size`.
                                      24,
                                      // `sample_rate`.
                                      0x00, 0x00, 0xbb, 0x80};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, WriteSampleSize32) {
  lpcm_decoder_config_.sample_size_ = 32;
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      1,
                                      // `sample_size`.
                                      32,
                                      // `sample_rate`.
                                      0x00, 0x00, 0xbb, 0x80};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalAudioRollDistanceMustBeZero) {
  audio_roll_distance_ = -1;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleSizeZero) {
  lpcm_decoder_config_.sample_size_ = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleSizeEight) {
  lpcm_decoder_config_.sample_size_ = 8;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleSizeOverMax) {
  lpcm_decoder_config_.sample_size_ = 33;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, WriteSampleRateMin) {
  lpcm_decoder_config_.sample_rate_ = 16000;
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      1,
                                      // `sample_size`.
                                      16,
                                      // `sample_rate`.
                                      0x00, 0x00, 0x3e, 0x80};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, WriteSampleRate44_1kHz) {
  lpcm_decoder_config_.sample_rate_ = 44100;
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      1,
                                      // `sample_size`.
                                      16,
                                      // `sample_rate`.
                                      0x00, 0x00, 0xac, 0x44};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, WriteSampleRateMax) {
  lpcm_decoder_config_.sample_rate_ = 96000;
  expected_decoder_config_payload_ = {// `sample_format_flags`.
                                      1,
                                      // `sample_size`.
                                      16,
                                      // `sample_rate`.
                                      0x00, 0x01, 0x77, 0x00};
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleRateZero) {
  lpcm_decoder_config_.sample_rate_ = 0;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleRate192kHz) {
  lpcm_decoder_config_.sample_rate_ = 192000;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

TEST_F(LpcmTest, IllegalSampleRateMax) {
  lpcm_decoder_config_.sample_rate_ = std::numeric_limits<uint32_t>::max();
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWriteDecoderConfig();
}

}  // namespace
}  // namespace iamf_tools
