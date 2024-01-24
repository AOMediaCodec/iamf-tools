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
#include "iamf/cli/lpcm_encoder.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/ia.h"
#include "iamf/lpcm_decoder_config.h"

namespace iamf_tools {

absl::Status LpcmEncoder::InitializeEncoder() {
  if (decoder_config_.sample_size_ % 8 != 0) {
    // `EncodeAudioFrame` assume the `bit_depth` is a multiple of 8.
    LOG(ERROR) << "Expected lpcm_decoder_config->sample_size to be a "
                  "multiple of 8: "
               << decoder_config_.sample_size_;
    return absl::UnknownError("");
  }

  // `EncodeAudioFrame` assume there are only 2 possible values of
  // `sample_format_flags`. Even though the LPCM specification has this as an
  // extension point.
  if (decoder_config_.sample_format_flags_ !=
          LpcmDecoderConfig::kLpcmBigEndian &&
      decoder_config_.sample_format_flags_ !=
          LpcmDecoderConfig::kLpcmLittleEndian) {
    LOG(ERROR) << "Unrecognized sample_format_flags";
    return absl::InvalidArgumentError("");
  }

  LOG_FIRST_N(INFO, 1) << "  Configured LPCM encoder for "
                       << num_samples_per_frame_ << " samples of "
                       << num_channels_ << " channels as "
                       << static_cast<int>(decoder_config_.sample_size_)
                       << "-bit LPCM in "
                       << (decoder_config_.sample_format_flags_ ==
                                   LpcmDecoderConfig::kLpcmBigEndian
                               ? "big"
                               : "little")
                       << " endian";

  return absl::OkStatus();
}

absl::Status LpcmEncoder::EncodeAudioFrame(
    int /*input_bit_depth*/, const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  // Since this implementation supports partial frames get the number of samples
  // per channel from the input.
  const int num_samples_per_channel = static_cast<int>(samples.size());

  // The size of an LPCM frame can easily be calculated before encoding.
  // Frame size = (# time ticks) * (# channels) * (bit_depth / 8) bytes.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  audio_frame.resize(num_samples_per_channel * num_channels_ *
                         decoder_config_.sample_size_ / 8,
                     0);
  // Write the entire PCM frame the buffer. Nothing should be trimmed when
  // encoding the sample.
  RETURN_IF_NOT_OK(WritePcmFrameToBuffer(
      samples, /*samples_to_trim_at_start=*/0,
      /*samples_to_trim_at_end=*/0, decoder_config_.sample_size_,
      decoder_config_.sample_format_flags_ == LpcmDecoderConfig::kLpcmBigEndian,
      audio_frame.size(), audio_frame.data()));

  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  return absl::OkStatus();
}

}  // namespace iamf_tools
