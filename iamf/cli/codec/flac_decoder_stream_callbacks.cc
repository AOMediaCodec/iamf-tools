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
#include "iamf/cli/codec/flac_decoder.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

FLAC__StreamDecoderReadStatus LibFlacReadCallback(
    const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes,
    void* client_data) {
  auto flac_decoder = static_cast<FlacDecoder*>(client_data);
  const auto& encoded_frame = flac_decoder->GetEncodedFrame();
  if (encoded_frame.empty()) {
    // No more data to read.
    *bytes = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }
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
  return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus LibFlacWriteCallback(
    const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* client_data) {
  std::vector<std::vector<int32_t>> decoded_samples(frame->header.channels);
  auto* flac_decoder = static_cast<FlacDecoder*>(client_data);
  const auto num_samples_per_channel = frame->header.blocksize;
  // Note: libFLAC represents data in a planar fashion, so each channel is
  // stored in a separate array.
  for (int i = 0; i < frame->header.channels; ++i) {
    decoded_samples[i].resize(num_samples_per_channel);
    const FLAC__int32* const channel_buffer = buffer[i];
    for (int j = 0; j < num_samples_per_channel; ++j) {
      decoded_samples[i][j] = static_cast<int32_t>(channel_buffer[j]);
    }
  }
  flac_decoder->SetDecodedFrame(decoded_samples);
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

}  // namespace iamf_tools
