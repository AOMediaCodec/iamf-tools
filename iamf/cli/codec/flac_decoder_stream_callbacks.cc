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

#include "absl/log/log.h"
#include "iamf/cli/codec/flac_decoder.h"
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

}  // namespace iamf_tools
