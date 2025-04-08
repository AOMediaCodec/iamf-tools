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

#include "iamf/cli/codec/flac_decoder_stream_callbacks.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "absl/types/span.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

namespace flac_callbacks {

namespace {
using absl::MakeConstSpan;
}

FLAC__StreamDecoderReadStatus LibFlacReadCallback(
    const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes,
    void* client_data) {
  if (bytes == nullptr || buffer == nullptr || client_data == nullptr) {
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }

  auto libflac_callback_data = static_cast<LibFlacCallbackData*>(client_data);
  // We are contracted to fill in up to the next `*bytes` bytes of the buffer.
  // If there is more data, then there will be a subsequent call to this
  // callback.
  const auto encoded_frame_slice = libflac_callback_data->GetNextSlice(*bytes);
  if (encoded_frame_slice.empty()) {
    // No more data to read.
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }

  for (int i = 0; i < encoded_frame_slice.size(); ++i) {
    buffer[i] = encoded_frame_slice[i];
  }
  *bytes = encoded_frame_slice.size();
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus LibFlacWriteCallback(
    const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* client_data) {
  auto* libflac_callback_data = static_cast<LibFlacCallbackData*>(client_data);
  if (libflac_callback_data->num_samples_per_channel_ <
      frame->header.blocksize) {
    LOG(ERROR) << "Frame blocksize " << frame->header.blocksize
               << " does not match expected number of samples per channel "
               << libflac_callback_data->num_samples_per_channel_;
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  // Zero initialize a vector of the maximum number of samples per channel. But
  // only fill in based on the actual number of samples in the frame.
  std::vector<std::vector<int32_t>> decoded_samples(
      libflac_callback_data->num_samples_per_channel_,
      std::vector<int32_t>(frame->header.channels, 0.0));

  // Note: libFLAC represents data in a planar fashion, so each channel is
  // stored in a separate array, and the elements within those arrays represent
  // time ticks. However, we store samples in an interleaved fashion, which
  // means that each outer entry in decoded_samples represents a time tick, and
  // each element within represents a channel. So we need to transpose the data
  // from libFLAC's planar format into our interleaved format.
  for (int c = 0; c < frame->header.channels; ++c) {
    const FLAC__int32* const channel_buffer = buffer[c];
    for (int t = 0; t < frame->header.blocksize; ++t) {
      decoded_samples[t][c] = channel_buffer[t]
                              << (32 - frame->header.bits_per_sample);
    }
  }
  libflac_callback_data->decoded_frame_ = decoded_samples;
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void LibFlacErrorCallback(const FLAC__StreamDecoder* /*decoder*/,
                          FLAC__StreamDecoderErrorStatus status,
                          void* /*client_data*/) {
  switch (status) {
    case FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC:
      LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC";
      break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER:
      LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER";
      break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH:
      LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH";
      break;
    case FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM:
      LOG(ERROR) << "FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM";
      break;
    default:
      LOG(ERROR) << "Unknown FLAC__StreamDecoderErrorStatus= " << status;
      break;
  }
}

void LibFlacCallbackData::SetEncodedFrame(
    absl::Span<const uint8_t> raw_encoded_frame) {
  // Cache the frame, and reset the bookkeeping index.
  encoded_frame_ =
      std::vector<uint8_t>(raw_encoded_frame.begin(), raw_encoded_frame.end());
  next_byte_index_ = 0;
}

absl::Span<const uint8_t> LibFlacCallbackData::GetNextSlice(size_t chunk_size) {
  // Ok, the buffer is exhausted. Return an empty span.
  if (next_byte_index_ >= encoded_frame_.size()) {
    return {};
  }

  // Grab the next slice, advance the bookkeeping index; effectively consecutive
  // calls stride over the source frame.
  auto next_slice =
      MakeConstSpan(encoded_frame_).subspan(next_byte_index_, chunk_size);
  next_byte_index_ += next_slice.size();
  return next_slice;
}

}  // namespace flac_callbacks

}  // namespace iamf_tools
