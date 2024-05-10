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

TEST(LpcmDecoderConfigTest, IsLittleEndian_True) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};

  EXPECT_TRUE(lpcm_decoder_config.IsLittleEndian());
}

TEST(LpcmDecoderConfigTest, IsLittleEndian_False) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmBigEndian,
                                           16, 48000};

  EXPECT_FALSE(lpcm_decoder_config.IsLittleEndian());
}

TEST(LpcmDecoderConfigTest, WriteValidLittleEndian) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, WriteValidBigEndian) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmBigEndian,
                                           16, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      0,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, IllegalSampleFormatFlagsMin) {
  LpcmDecoderConfig lpcm_decoder_config = {
      LpcmDecoderConfig::kLpcmBeginReserved, 16, 48000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, IllegalSampleFormatFlagsMax) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmEndReserved,
                                           16, 48000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, WriteSampleSize24) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           24, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      24,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, WriteSampleSize32) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, AudioRollDistanceMustBeZero_A) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  int16_t audio_roll_distance = -1;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, AudioRollDistanceMustBeZero_B) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  int16_t audio_roll_distance = 1;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, IllegalSampleSizeZero) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           0, 48000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, IllegalSampleSizeEight) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           8, 48000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, IllegalSampleSizeOverMax) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           40, 48000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, WriteSampleRateMin_16kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 16000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x00, 0x3e, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, WriteSampleRate44_1kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 44100};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x00, 0xac, 0x44  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, WriteSampleRateMax_96kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 96000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x01, 0x77, 0x00  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_TRUE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, IllegalSampleRateZero) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 0};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, IllegalSampleRate192kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 192000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, IllegalSampleRateMax) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16,
                                           std::numeric_limits<int16_t>::max()};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size, we'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

}  // namespace
}  // namespace iamf_tools
