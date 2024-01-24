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
#include "iamf/flac_decoder_config.h"

#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/ia.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

namespace {

absl::Status GetStreamInfo(const FlacDecoderConfig& decoder_config,
                           const FlacMetaBlockStreamInfo** stream_info) {
  if (decoder_config.metadata_blocks_.empty() ||
      decoder_config.metadata_blocks_.front().header.block_type !=
          FlacMetaBlockHeader::kFlacStreamInfo) {
    return absl::InvalidArgumentError(
        "FLAC always requires the first block is present and is a "
        "`STREAMINFO` block.");
  }

  *stream_info = &std::get<FlacMetaBlockStreamInfo>(
      decoder_config.metadata_blocks_.front().payload);
  return absl::OkStatus();
}

absl::Status ValidateSampleRate(uint32_t sample_rate) {
  // Validate restrictions from the FLAC specification.
  if (sample_rate < FlacMetaBlockStreamInfo::kMinSampleRate ||
      sample_rate > FlacMetaBlockStreamInfo::kMaxSampleRate) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid sample rate= ", sample_rate));
  }

  return absl::OkStatus();
}

absl::Status ValidateBitsPerSample(uint8_t bits_per_sample) {
  // Validate restrictions from the FLAC specification.
  if (bits_per_sample < FlacMetaBlockStreamInfo::kMinBitsPerSample ||
      bits_per_sample > FlacMetaBlockStreamInfo::kMaxBitsPerSample) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid bits_per_sample= ", bits_per_sample));
  }

  return absl::OkStatus();
}

absl::Status ValidateTotalSamplesInStream(uint64_t total_samples_in_stream) {
  // The FLAC specification treats this as a 36-bit value which is always valid,
  // but in `iamf_tools` it could be out of bounds because it is stored as a
  // `uint64_t`.
  if (total_samples_in_stream <
          FlacMetaBlockStreamInfo::kMinTotalSamplesInStream ||
      total_samples_in_stream >
          FlacMetaBlockStreamInfo::kMaxTotalSamplesInStream) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid total_samples_in_stream= ", total_samples_in_stream));
  }

  return absl::OkStatus();
}

// Validates the `FlacDecoderConfig`.
absl::Status ValidatePayload(uint32_t num_samples_per_frame,
                             const FlacDecoderConfig& decoder_config) {
  for (int i = 0; i < decoder_config.metadata_blocks_.size(); i++) {
    const bool last_metadata_block_flag =
        decoder_config.metadata_blocks_[i].header.last_metadata_block_flag;

    const bool last_block = (i == decoder_config.metadata_blocks_.size() - 1);

    if (last_metadata_block_flag != last_block) {
      return absl::InvalidArgumentError(
          "There MUST be exactly one FLAC metadata block with "
          "`last_metadata_block_flag == true` and it MUST be the final block.");
    }
  }

  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(decoder_config, &stream_info));

  // FLAC restricts some fields.
  RETURN_IF_NOT_OK(ValidateSampleRate(stream_info->sample_rate));
  RETURN_IF_NOT_OK(ValidateBitsPerSample(stream_info->bits_per_sample));

  if (stream_info->minimum_block_size < 16 ||
      stream_info->maximum_block_size < 16) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid minimum_block_size= ", stream_info->minimum_block_size,
        " or invalid maximum_block_size=", stream_info->maximum_block_size));
  }

  // IAMF restricts some fields.
  if (stream_info->minimum_block_size != num_samples_per_frame ||
      stream_info->maximum_block_size != num_samples_per_frame) {
    return absl::InvalidArgumentError(absl::StrCat(
        "IAMF requires maximum_block_size= ", stream_info->maximum_block_size,
        " and minimum_block_size= ", stream_info->minimum_block_size,
        " to be equal to num_samples_per_frame= ", num_samples_per_frame,
        " in the Codec Config OBU."));
  }
  if (stream_info->minimum_frame_size != 0 ||
      stream_info->maximum_frame_size != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid minimum_frame_size= ", stream_info->minimum_frame_size,
        " or invalid maximum_frame_size= ", stream_info->maximum_frame_size));
  }

  // FLAC represents the channels offset by 1. There must be 2 channels.
  if (stream_info->number_of_channels != 1) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Invalid number_of_channels= ", stream_info->number_of_channels));
  }

  RETURN_IF_NOT_OK(ValidateTotalSamplesInStream(stream_info->bits_per_sample));

  for (const auto byte : stream_info->md5_signature) {
    if (byte != 0) {
      return absl::InvalidArgumentError(absl::StrCat("Invalid md5_signature."));
    }
  }
  return absl::OkStatus();
}

absl::Status ValidateAudioRollDistance(int16_t audio_roll_distance) {
  if (audio_roll_distance != 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid audio_roll_distance= ", audio_roll_distance));
  }
  return absl::OkStatus();
}

absl::Status WriteStreamInfo(const FlacMetaBlockStreamInfo& stream_info,
                             WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.minimum_block_size, 16));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.maximum_block_size, 16));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.minimum_frame_size, 24));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.maximum_frame_size, 24));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.sample_rate, 20));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.number_of_channels, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_info.bits_per_sample, 5));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral64(stream_info.total_samples_in_stream, 36));
  RETURN_IF_NOT_OK(wb.WriteUint8Vector(std::vector<uint8_t>(
      stream_info.md5_signature.begin(), stream_info.md5_signature.end())));
  return absl::OkStatus();
}

void PrintStreamInfo(const FlacMetaBlockStreamInfo& stream_info) {
  LOG(INFO) << "      metadata_block(stream_info):";

  LOG(INFO) << "        minimum_block_size= " << stream_info.minimum_block_size;
  LOG(INFO) << "        maximum_block_size= " << stream_info.maximum_block_size;
  LOG(INFO) << "        minimum_frame_size= " << stream_info.minimum_frame_size;
  LOG(INFO) << "        maximum_frame_size= " << stream_info.maximum_frame_size;
  LOG(INFO) << "        sample_rate= " << stream_info.sample_rate;
  LOG(INFO) << "        number_of_channels= "
            << static_cast<int>(stream_info.number_of_channels);
  LOG(INFO) << "        bits_per_sample= "
            << static_cast<int>(stream_info.bits_per_sample);
  LOG(INFO) << "        total_samples_in_stream= "
            << stream_info.total_samples_in_stream;
}

}  // namespace

absl::Status FlacDecoderConfig::ValidateAndWrite(uint32_t num_samples_per_frame,
                                                 int16_t audio_roll_distance,
                                                 WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));

  RETURN_IF_NOT_OK(ValidatePayload(num_samples_per_frame, *this));

  for (const auto& metadata_block : metadata_blocks_) {
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
        metadata_block.header.last_metadata_block_flag, 1));
    RETURN_IF_NOT_OK(
        wb.WriteUnsignedLiteral(metadata_block.header.block_type, 7));
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
        metadata_block.header.metadata_data_block_length, 24));

    int64_t expected_end =
        wb.bit_offset() + metadata_block.header.metadata_data_block_length * 8;

    switch (metadata_block.header.block_type) {
      case FlacMetaBlockHeader::kFlacStreamInfo:
        RETURN_IF_NOT_OK(WriteStreamInfo(
            std::get<FlacMetaBlockStreamInfo>(metadata_block.payload), wb));
        break;
      default:
        RETURN_IF_NOT_OK(wb.WriteUint8Vector(
            std::get<std::vector<uint8_t>>(metadata_block.payload)));
        break;
    }

    if (expected_end != wb.bit_offset()) {
      LOG(ERROR) << "`FlacDecoderConfig` was expected to be using "
                 << metadata_block.header.metadata_data_block_length
                 << " bytes, but it was not.";
      return absl::UnknownError("");
    }
  }

  return absl::OkStatus();
}

absl::Status FlacDecoderConfig::GetOutputSampleRate(
    uint32_t& output_sample_rate) const {
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(*this, &stream_info));

  output_sample_rate = stream_info->sample_rate;
  return ValidateSampleRate(output_sample_rate);
}

absl::Status FlacDecoderConfig::GetBitDepthToMeasureLoudness(
    uint8_t& bit_depth_to_measure_loudness) const {
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(*this, &stream_info));

  // The raw bit-depth field for FLAC represents bit-depth - 1.
  bit_depth_to_measure_loudness = stream_info->bits_per_sample + 1;
  return ValidateBitsPerSample(stream_info->bits_per_sample);
}

absl::Status FlacDecoderConfig::GetTotalSamplesInStream(
    uint64_t& total_samples_in_stream) const {
  const FlacMetaBlockStreamInfo* stream_info;
  RETURN_IF_NOT_OK(GetStreamInfo(*this, &stream_info));

  total_samples_in_stream = stream_info->total_samples_in_stream;
  return ValidateTotalSamplesInStream(stream_info->total_samples_in_stream);
}

void FlacDecoderConfig::Print() const {
  LOG(INFO) << "    decoder_config(flac):";

  for (const auto& metadata_block : metadata_blocks_) {
    LOG(INFO) << "      header:";
    LOG(INFO) << "        last_metadata_block_flag= "
              << metadata_block.header.last_metadata_block_flag;
    LOG(INFO) << "        block_type= "
              << static_cast<int>(metadata_block.header.block_type);
    LOG(INFO) << "        metadata_data_block_length= "
              << metadata_block.header.metadata_data_block_length;
    switch (metadata_block.header.block_type) {
      case FlacMetaBlockHeader::kFlacStreamInfo:
        PrintStreamInfo(
            std::get<FlacMetaBlockStreamInfo>(metadata_block.payload));
        break;
      default: {
        const auto& generic_block =
            std::get<std::vector<uint8_t>>(metadata_block.payload);
        LOG(INFO) << "      metadata_block(generic_block):";
        LOG(INFO) << "        size= " << generic_block.size();
        LOG(INFO) << "        payload omitted.";
      }
    }
  }
}

}  // namespace iamf_tools
