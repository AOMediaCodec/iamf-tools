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
#include "iamf/cli/codec/aac_decoder.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

// This symbol conflicts with `aacenc_lib.h` and `aacdecoder_lib.h`.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/codec/aac_utils.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/common/macros.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "libAACdec/include/aacdecoder_lib.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

namespace {

absl::Status ConfigureAacDecoder(const AacDecoderConfig& raw_aac_decoder_config,
                                 int num_channels,
                                 AAC_DECODER_INSTANCE* decoder_) {
  // Configure `fdk_aac` with the audio specific config which has the correct
  // number of channels in it. IAMF may share a decoder config for several
  // substreams, so the raw value may not be accurate.
  AudioSpecificConfig fdk_audio_specific_config =
      raw_aac_decoder_config.decoder_specific_info_.audio_specific_config;
  fdk_audio_specific_config.channel_configuration_ = num_channels;

  // Serialize the modified config. Assume a reasonable default size, but let
  // the buffer be resizable to be safe.
  const size_t kMaxAudioSpecificConfigSize = 5;
  WriteBitBuffer wb(kMaxAudioSpecificConfigSize);
  const absl::Status status = fdk_audio_specific_config.ValidateAndWrite(wb);

  if (status.ok() && wb.IsByteAligned()) {
    // Transform data from `const uint_t*` to `UCHAR*` to match the `libaac`
    // interface.
    std::vector<UCHAR> libaac_audio_specific_config(wb.bit_buffer().size());
    std::transform(wb.bit_buffer().begin(), wb.bit_buffer().end(),
                   libaac_audio_specific_config.begin(),
                   [](uint8_t c) { return static_cast<UCHAR>(c); });

    // Configure `decoder_` with the serialized data.
    UCHAR* conf[] = {libaac_audio_specific_config.data()};
    const UINT length[] = {static_cast<UINT>(wb.bit_offset() / 8)};
    aacDecoder_ConfigRaw(decoder_, conf, length);
  } else {
    LOG(ERROR) << "Erroring writing audio specific config: " << status
               << " wrote " << wb.bit_offset() << " bits.";
  }

  return status;
}

}  // namespace

AacDecoder::AacDecoder(const CodecConfigObu& codec_config_obu, int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      aac_decoder_config_(std::get<AacDecoderConfig>(
          codec_config_obu.GetCodecConfig().decoder_config)) {}

AacDecoder::~AacDecoder() {
  if (decoder_ != nullptr) {
    aacDecoder_Close(decoder_);
  }
}

absl::Status AacDecoder::Initialize() {
  // Initialize the decoder.
  decoder_ = aacDecoder_Open(GetAacTransportationType(), /*nrOfLayers=*/1);

  if (decoder_ == nullptr) {
    LOG(ERROR) << "Failed to initialize AAC decoder.";
    return absl::UnknownError("");
  }

  RETURN_IF_NOT_OK(
      ConfigureAacDecoder(aac_decoder_config_, num_channels_, decoder_));

  const auto* stream_info = aacDecoder_GetStreamInfo(decoder_);
  LOG_FIRST_N(INFO, 1) << "Created an AAC encoder with "
                       << stream_info->numChannels << " channels.";

  return absl::OkStatus();
}

absl::Status AacDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame,
    std::vector<std::vector<int32_t>>& decoded_samples) {
  // Transform the data and feed it to the decoder.
  std::vector<UCHAR> input_data(encoded_frame.size());
  std::transform(encoded_frame.begin(), encoded_frame.end(), input_data.begin(),
                 [](uint8_t c) { return static_cast<UCHAR>(c); });

  UCHAR* in_buffer[] = {input_data.data()};
  const UINT buffer_size[] = {static_cast<UINT>(encoded_frame.size())};
  UINT bytes_valid = static_cast<UINT>(encoded_frame.size());
  aacDecoder_Fill(decoder_, in_buffer, buffer_size, &bytes_valid);
  if (bytes_valid != 0) {
    LOG(ERROR) << "The input frame failed to decode. It may not have been a "
                  "complete AAC frame.";
    return absl::UnknownError("");
  }

  // Retrieve the decoded frame. `fdk_aac` decodes to INT_PCM (usually 16-bits)
  // samples with channels interlaced.
  std::vector<INT_PCM> output_pcm;
  output_pcm.resize(num_samples_per_channel_ * num_channels_);
  auto aac_error_code = aacDecoder_DecodeFrame(decoder_, output_pcm.data(),
                                               output_pcm.size(), /*flags=*/0);
  if (aac_error_code != AAC_DEC_OK) {
    LOG(ERROR) << "AAC failed to decode: " << aac_error_code;
    return absl::UnknownError("");
  }

  // Transform the data to channels arranged in (time, channel) axes with
  // samples stored in the upper bytes of an `int32_t`. There can only be one or
  // two channels.
  decoded_samples.reserve(decoded_samples.size() +
                          output_pcm.size() / num_channels_);
  for (int i = 0; i < output_pcm.size(); i += num_channels_) {
    // Grab samples in all channels associated with this time instant and store
    // it in the upper bytes.
    std::vector<int32_t> time_sample(num_channels_, 0);
    for (int j = 0; j < num_channels_; ++j) {
      time_sample[j] = static_cast<int32_t>(output_pcm[i + j])
                       << (32 - GetFdkAacBitDepth());
    }
    decoded_samples.push_back(time_sample);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
