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

constexpr int16_t kAudioRollDistance = 0;

TEST(GetRequiredAudioRollDistance, ReturnsFixedValue) {
  EXPECT_EQ(LpcmDecoderConfig::GetRequiredAudioRollDistance(),
            kAudioRollDistance);
}

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

TEST(LpcmDecoderConfigTest, Validate_ValidLittleEndian) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_ValidBigEndian) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmBigEndian,
                                           16, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      0,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleFormatFlagsMin) {
  LpcmDecoderConfig lpcm_decoder_config = {
      LpcmDecoderConfig::kLpcmBeginReserved, 16, 48000};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_IllegalSampleFormatFlagsMax) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmEndReserved,
                                           16, 48000};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_SampleSize24) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           24, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      24,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_SampleSize32) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_AudioRollDistanceMustBeZero_A) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  int16_t audio_roll_distance = -1;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_AudioRollDistanceMustBeZero_B) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  int16_t audio_roll_distance = 1;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleSizeZero) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           0, 48000};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleSizeEight) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           8, 48000};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleSizeOverMax) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           40, 48000};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_SampleRateMin_16kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 16000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x00, 0x3e, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_SampleRate44_1kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 44100};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x00, 0xac, 0x44  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_SampleRateMax_96kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           32, 96000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      32,                     // sample_size
      0x00, 0x01, 0x77, 0x00  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.Validate(audio_roll_distance), IsOk());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleRateZero) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 0};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleRate192kHz) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 192000};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Validate_InvalidSampleRateMax) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16,
                                           std::numeric_limits<int16_t>::max()};
  int16_t audio_roll_distance = 0;

  EXPECT_FALSE(lpcm_decoder_config.Validate(audio_roll_distance).ok());
}

TEST(LpcmDecoderConfigTest, Write_AllValid) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_THAT(lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb),
              IsOk());
  ValidateWriteResults(wb, expected_decoder_config_payload_);
}

TEST(LpcmDecoderConfigTest, Write_InvalidDoesNotWrite) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           8, 48000};
  int16_t audio_roll_distance = 0;
  WriteBitBuffer wb(48);  // Arbitrary size.  We'll fail before writing.

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(LpcmDecoderConfigTest, Write_InvalidRollDistance) {
  LpcmDecoderConfig lpcm_decoder_config = {LpcmDecoderConfig::kLpcmLittleEndian,
                                           16, 48000};
  std::vector<uint8_t> expected_decoder_config_payload_ = {
      1,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 1;
  WriteBitBuffer wb(expected_decoder_config_payload_.size());

  EXPECT_FALSE(
      lpcm_decoder_config.ValidateAndWrite(audio_roll_distance, wb).ok());
}

TEST(ReadAndValidateTest, ReadAllFields) {
  std::vector<uint8_t> source = {
      1,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 0;
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  LpcmDecoderConfig lpcm_decoder_config;
  EXPECT_THAT(
      lpcm_decoder_config.ReadAndValidate(audio_roll_distance, *read_buffer),
      IsOk());
  LpcmDecoderConfig expected_lpcm_decoder_config = {
      LpcmDecoderConfig::kLpcmLittleEndian, 16, 48000};
  EXPECT_EQ(lpcm_decoder_config, expected_lpcm_decoder_config);
}

TEST(ReadAndValidateTest, RejectInvalidAudioRollDistance) {
  std::vector<uint8_t> source = {
      1,                      // sample_format_flags
      16,                     // sample_size
      0x00, 0x00, 0xbb, 0x80  // sample_rate
  };
  int16_t audio_roll_distance = 1;
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  LpcmDecoderConfig lpcm_decoder_config;
  EXPECT_FALSE(
      lpcm_decoder_config.ReadAndValidate(audio_roll_distance, *read_buffer)
          .ok());
}

}  // namespace
}  // namespace iamf_tools
