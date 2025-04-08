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
#include <memory>
#include <variant>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"

namespace iamf_tools {

absl::StatusOr<std::unique_ptr<DecoderBase>> LpcmDecoder::Create(
    const CodecConfigObu& codec_config_obu, int num_channels) {
  const LpcmDecoderConfig* decoder_config = std::get_if<LpcmDecoderConfig>(
      &codec_config_obu.GetCodecConfig().decoder_config);
  if (decoder_config == nullptr) {
    return absl::InvalidArgumentError(
        "CodecConfigObu does not contain an `LpcmDecoderConfig`.");
  }

  RETURN_IF_NOT_OK(decoder_config->Validate(
      codec_config_obu.GetCodecConfig().audio_roll_distance));

  uint8_t bit_depth;
  auto status = decoder_config->GetBitDepthToMeasureLoudness(bit_depth);
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

  return absl::WrapUnique(
      new LpcmDecoder(num_channels, codec_config_obu.GetNumSamplesPerFrame(),
                      decoder_config->IsLittleEndian(), bytes_per_sample));
}

absl::Status LpcmDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame) {
  num_valid_ticks_ = 0;

  // Make sure we have a valid number of bytes.  There needs to be an equal
  // number of samples for each channel.
  if (encoded_frame.size() % bytes_per_sample_ != 0 ||
      (encoded_frame.size() / bytes_per_sample_) % num_channels_ != 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "LpcmDecoder::DecodeAudioFrame() failed: encoded_frame has ",
        encoded_frame.size(),
        " bytes, which is not a multiple of the bytes per sample (",
        bytes_per_sample_, ") * number of channels (", num_channels_, ")."));
  }
  // Each time tick has one sample for each channel.
  const size_t num_ticks =
      encoded_frame.size() / bytes_per_sample_ / num_channels_;
  if (num_ticks > num_samples_per_channel_) {
    return absl::InvalidArgumentError(
        absl::StrCat("Detected num_ticks= ", num_ticks,
                     ", but the decoder is only configured for up to "
                     "num_samples_per_channel_= ",
                     num_samples_per_channel_, "."));
  }
  num_valid_ticks_ = num_ticks;

  int32_t sample_result;
  for (size_t t = 0; t < num_valid_ticks_; ++t) {
    // One sample for each channel in this time tick.
    for (size_t c = 0; c < num_channels_; ++c) {
      const size_t offset = (t * num_channels_ + c) * bytes_per_sample_;
      absl::Span<const uint8_t> input_bytes(encoded_frame.data() + offset,
                                            bytes_per_sample_);
      if (little_endian_) {
        RETURN_IF_NOT_OK(LittleEndianBytesToInt32(input_bytes, sample_result));
      } else {
        RETURN_IF_NOT_OK(BigEndianBytesToInt32(input_bytes, sample_result));
      }
      decoded_samples_[t][c] = sample_result;
    }
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
