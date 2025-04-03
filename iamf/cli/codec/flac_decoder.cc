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

#include "iamf/cli/codec/flac_decoder.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/flac_decoder_stream_callbacks.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

FlacDecoder::FlacDecoder(int num_channels, uint32_t num_samples_per_frame)
    : DecoderBase(num_channels, num_samples_per_frame),
      callback_data_(num_samples_per_frame) {}

FlacDecoder::~FlacDecoder() {
  if (decoder_ != nullptr) {
    FLAC__stream_decoder_delete(decoder_);
  }
}

absl::Status FlacDecoder::Initialize() {
  decoder_ = FLAC__stream_decoder_new();
  if (decoder_ == nullptr) {
    return absl::InternalError("Failed to create FLAC stream decoder.");
  }
  FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_stream(
      decoder_, flac_callbacks::LibFlacReadCallback, /*seek_callback=*/nullptr,
      /*tell_callback=*/nullptr, /*length_callback=*/nullptr,
      /*eof_callback=*/nullptr, flac_callbacks::LibFlacWriteCallback,
      /*metadata_callback=*/nullptr, flac_callbacks::LibFlacErrorCallback,
      static_cast<void*>(&callback_data_));

  if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    return absl::InternalError(
        absl::StrCat("Failed to initialize FLAC stream decoder: ", status));
  }
  return absl::OkStatus();
}

absl::Status FlacDecoder::Finalize() {
  // Signal to `libflac` the decoder is finished.
  if (!FLAC__stream_decoder_finish(decoder_)) {
    return absl::InternalError("Failed to finalize Flac stream decoder.");
  }
  return absl::OkStatus();
}

absl::Status FlacDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame) {
  num_valid_ticks_ = 0;

  // Set the encoded frame to be decoded; the libflac decoder will copy the
  // data using LibFlacReadCallback.
  callback_data_.SetEncodedFrame(encoded_frame);
  if (!FLAC__stream_decoder_process_single(decoder_)) {
    // More specific error information is logged in LibFlacErrorCallback.
    return absl::InternalError("Failed to decode FLAC frame.");
  }
  // Get the decoded frame, which will have been set by LibFlacWriteCallback.
  // Copy the first `num_valid_ticks_` time samples to `decoded_samples_`.
  const auto& decoded_frame = callback_data_.decoded_frame_;
  num_valid_ticks_ = decoded_frame.size();
  std::copy(decoded_frame.begin(), decoded_frame.begin() + num_valid_ticks_,
            decoded_samples_.begin());
  return absl::OkStatus();
}

}  // namespace iamf_tools
