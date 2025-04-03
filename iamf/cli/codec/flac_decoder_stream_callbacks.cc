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
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

namespace flac_callbacks {

FLAC__StreamDecoderReadStatus LibFlacReadCallback(
    const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes,
    void* client_data) {
  auto libflac_callback_data = static_cast<LibFlacCallbackData*>(client_data);
  const auto& encoded_frame = libflac_callback_data->encoded_frame_;
  if (encoded_frame.empty()) {
    // No more data to read.
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }
  // TODO(b/407732471): Support reading larger frames, by pushing in chunks of
  //                    the raw data.
  if (encoded_frame.size() > *bytes) {
    LOG(ERROR) << "Encoded frame size " << encoded_frame.size()
               << " is larger than the libflac buffer size " << *bytes;
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
  }
  for (int i = 0; i < encoded_frame.size(); ++i) {
    buffer[i] = encoded_frame[i];
  }
  *bytes = encoded_frame.size();
  // Entire frame is processed, clear it so it is not read again.
  libflac_callback_data->encoded_frame_.clear();
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus LibFlacWriteCallback(
    const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* client_data) {
  auto* libflac_callback_data = static_cast<LibFlacCallbackData*>(client_data);
  const auto num_samples_per_channel = frame->header.blocksize;
  if (libflac_callback_data->num_samples_per_channel_ !=
      frame->header.blocksize) {
    LOG(ERROR) << "Frame blocksize " << frame->header.blocksize
               << " does not match expected number of samples per channel "
               << libflac_callback_data->num_samples_per_channel_;
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  std::vector<std::vector<int32_t>> decoded_samples(
      num_samples_per_channel, std::vector<int32_t>(frame->header.channels));
  // Note: libFLAC represents data in a planar fashion, so each channel is
  // stored in a separate array, and the elements within those arrays represent
  // time ticks. However, we store samples in an interleaved fashion, which
  // means that each outer entry in decoded_samples represents a time tick, and
  // each element within represents a channel. So we need to transpose the data
  // from libFLAC's planar format into our interleaved format.
  for (int c = 0; c < frame->header.channels; ++c) {
    const FLAC__int32* const channel_buffer = buffer[c];
    for (int t = 0; t < num_samples_per_channel; ++t) {
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

}  // namespace flac_callbacks

}  // namespace iamf_tools
