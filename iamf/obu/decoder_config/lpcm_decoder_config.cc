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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

namespace {

absl::Status ValidateSampleSize(uint8_t sample_size) {
  switch (sample_size) {
    case 16:
    case 24:
    case 32:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid sample_size= ", sample_size));
  }
}

absl::Status ValidateSampleRate(uint32_t sample_rate) {
  switch (sample_rate) {
    case 16000:
    case 32000:
    case 44100:
    case 48000:
    case 96000:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid sample_rate= ", sample_rate));
  }
}

// Validates the `LpcmDecoderConfig`.
absl::Status ValidatePayload(const LpcmDecoderConfig& decoder_config) {
  // Only 2 enumerations are defined for the 8-bit `sample_format_flags` field.
  switch (decoder_config.sample_format_flags_) {
    using enum LpcmDecoderConfig::LpcmFormatFlags;
    case kLpcmBigEndian:
    case kLpcmLittleEndian:
      break;
    default:
      return absl::UnimplementedError(
          absl::StrCat("Invalid sample_format_flags= ",
                       decoder_config.sample_format_flags_));
  }

  RETURN_IF_NOT_OK(ValidateSampleSize(decoder_config.sample_size_));
  RETURN_IF_NOT_OK(ValidateSampleRate(decoder_config.sample_rate_));

  return absl::OkStatus();
}

absl::Status ValidateAudioRollDistance(int16_t audio_roll_distance) {
  if (audio_roll_distance != 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid audio_roll_distance= ", audio_roll_distance));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status LpcmDecoderConfig::ValidateAndWrite(int16_t audio_roll_distance,
                                                 WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));
  RETURN_IF_NOT_OK(ValidatePayload(*this));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(sample_format_flags_, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(sample_size_, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(sample_rate_, 32));

  return absl::OkStatus();
}

absl::Status LpcmDecoderConfig::GetOutputSampleRate(
    uint32_t& output_sample_rate) const {
  output_sample_rate = sample_rate_;
  return ValidateSampleRate(sample_rate_);
}

absl::Status LpcmDecoderConfig::GetBitDepthToMeasureLoudness(
    uint8_t& bit_depth_to_measure_loudness) const {
  bit_depth_to_measure_loudness = sample_size_;
  return ValidateSampleSize(sample_size_);
}

void LpcmDecoderConfig::Print() const {
  LOG(INFO) << "    decoder_config(ipcm):";
  LOG(INFO) << "      sample_format_flags= "
            << static_cast<int>(sample_format_flags_);
  LOG(INFO) << "      sample_size= " << static_cast<int>(sample_size_);
  LOG(INFO) << "      sample_rate= " << sample_rate_;
}

}  // namespace iamf_tools
