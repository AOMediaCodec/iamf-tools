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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"

namespace iamf_tools {

FLAC__StreamDecoderReadStatus FlacDecoder::LibFlacReadCallback(
    const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[], size_t* bytes,
    void* client_data) {
  auto flac_decoder = static_cast<FlacDecoder*>(client_data);
  auto encoded_frame = flac_decoder->GetEncodedFrame();
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

FLAC__StreamDecoderWriteStatus FlacDecoder::LibFlacWriteCallback(
    const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
    const FLAC__int32* const buffer[], void* client_data) {
  auto* flac_decoder = static_cast<FlacDecoder*>(client_data);
  const auto num_samples_per_channel = frame->header.blocksize;
  if (flac_decoder->GetNumSamplesPerChannel() != frame->header.blocksize) {
    LOG(ERROR) << "Frame blocksize " << frame->header.blocksize
               << " does not match expected number of samples per channel "
               << flac_decoder->GetNumSamplesPerChannel();
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
  flac_decoder->SetDecodedFrame(decoded_samples);
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::LibFlacErrorCallback(const FLAC__StreamDecoder* /*decoder*/,
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

FlacDecoder::FlacDecoder(const CodecConfigObu& codec_config_obu,
                         int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      decoder_config_(std::get<FlacDecoderConfig>(
          codec_config_obu.GetCodecConfig().decoder_config)) {}

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
      decoder_, LibFlacReadCallback, /*seek_callback=*/nullptr,
      /*tell_callback=*/nullptr, /*length_callback=*/nullptr,
      /*eof_callback=*/nullptr, LibFlacWriteCallback,
      /*metadata_callback=*/nullptr, LibFlacErrorCallback,
      static_cast<void*>(this));

  if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    return absl::InternalError(
        absl::StrCat("Failed to initialize FLAC stream decoder: ", status));
  }
  return absl::OkStatus();
}

absl::Status FlacDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame,
    std::vector<std::vector<int32_t>>& decoded_samples) {
  // Set the encoded frame to be decoded; the libflac decoder will copy the
  // data using LibFlacReadCallback.
  encoded_frame_ = encoded_frame;
  if (!FLAC__stream_decoder_process_single(decoder_)) {
    // More specific error information is logged in LibFlacErrorCallback.
    return absl::InternalError("Failed to decode FLAC frame.");
  }
  // Get the decoded frame, which will have been set by LibFlacWriteCallback.
  decoded_samples = decoded_frame_;
  return absl::OkStatus();
}

}  // namespace iamf_tools
