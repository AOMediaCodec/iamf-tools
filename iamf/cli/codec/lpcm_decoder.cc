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

#include "iamf/cli/codec/lpcm_decoder.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"

namespace iamf_tools {

LpcmDecoder::LpcmDecoder(const CodecConfigObu& codec_config_obu,
                         int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      decoder_config_(std::get<LpcmDecoderConfig>(
          codec_config_obu.GetCodecConfig().decoder_config)),
      audio_roll_distance_(
          codec_config_obu.GetCodecConfig().audio_roll_distance) {}

absl::Status LpcmDecoder::Initialize() {
  RETURN_IF_NOT_OK(decoder_config_.Validate(audio_roll_distance_));
  return absl::OkStatus();
}

absl::Status LpcmDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame,
    std::vector<std::vector<int32_t>>& decoded_samples) {
  uint8_t bit_depth;
  auto status = decoder_config_.GetBitDepthToMeasureLoudness(bit_depth);
  if (!status.ok()) {
    return status;
  }
  // The LpcmDecoderConfig should have checked for valid values before returning
  // the bit depth, but we defensively check that it's a multiple of 8 here.
  if (bit_depth % 8 != 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("LpcmDecoder::DecodeAudioFrame() failed: bit_depth (",
                     bit_depth, ") is not a multiple of 8."));
  }
  const size_t bytes_per_sample = bit_depth / 8;
  // Make sure we have a valid number of bytes.  There needs to be an equal
  // number of samples for each channel.
  if (encoded_frame.size() % bytes_per_sample != 0 ||
      (encoded_frame.size() / bytes_per_sample) % num_channels_ != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "LpcmDecoder::DecodeAudioFrame() failed: encoded_frame has ",
        encoded_frame.size(),
        " bytes, which is not a multiple of the bytes per sample (",
        bytes_per_sample, ") * number of channels (", num_channels_, ")."));
  }
  // Each time tick has one sample for each channel.
  const size_t num_time_ticks =
      encoded_frame.size() / bytes_per_sample / num_channels_;
  const bool little_endian = decoder_config_.IsLittleEndian();

  decoded_samples.reserve(decoded_samples.size() + num_time_ticks);
  int32_t sample_result;
  for (size_t t = 0; t < num_time_ticks; ++t) {
    // One sample for each channel in this time tick.
    std::vector<int32_t> time_tick_samples(num_channels_);
    for (size_t c = 0; c < num_channels_; ++c) {
      const size_t offset = (t * num_channels_ + c) * bytes_per_sample;
      absl::Span<const uint8_t> input_bytes(encoded_frame.data() + offset,
                                            bytes_per_sample);
      if (little_endian) {
        status = LittleEndianBytesToInt32(input_bytes, sample_result);
      } else {
        status = BigEndianBytesToInt32(input_bytes, sample_result);
      }
      RETURN_IF_NOT_OK(status);
      time_tick_samples[c] = sample_result;
    }
    decoded_samples.push_back(std::move(time_tick_samples));
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
