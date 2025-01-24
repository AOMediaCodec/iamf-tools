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
#include "iamf/cli/codec/opus_decoder.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/opus_utils.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/obu_util.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "include/opus.h"
#include "include/opus_types.h"

namespace iamf_tools {

namespace {

// Performs validation for values that this implementation assumes are
// restricted because they are restricted in IAMF v1.1.0.
absl::Status ValidateDecoderConfig(
    const OpusDecoderConfig& opus_decoder_config) {
  // Validate the input. Reject values that would need to be added to this
  // function if they were ever supported.
  if (opus_decoder_config.output_gain_ != 0 ||
      opus_decoder_config.mapping_family_ != 0) {
    const auto error_message = absl::StrCat(
        "IAMF v1.1.0 expects output_gain: ", opus_decoder_config.output_gain_,
        " and mapping_family: ", opus_decoder_config.mapping_family_,
        " to be 0.");
    return absl::InvalidArgumentError(error_message);
  }

  return absl::OkStatus();
}

}  // namespace

OpusDecoder::OpusDecoder(const CodecConfigObu& codec_config_obu,
                         int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      opus_decoder_config_(std::get<OpusDecoderConfig>(
          codec_config_obu.GetCodecConfig().decoder_config)),
      output_sample_rate_(codec_config_obu.GetOutputSampleRate()) {}

OpusDecoder::~OpusDecoder() {
  if (decoder_ != nullptr) {
    opus_decoder_destroy(decoder_);
  }
}

absl::Status OpusDecoder::Initialize() {
  MAYBE_RETURN_IF_NOT_OK(ValidateDecoderConfig(opus_decoder_config_));

  // Initialize the decoder.
  int opus_error_code;
  decoder_ = opus_decoder_create(static_cast<opus_int32>(output_sample_rate_),
                                 num_channels_, &opus_error_code);
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(
      opus_error_code, "Failed to initialize Opus decoder."));

  return absl::OkStatus();
}

absl::Status OpusDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame) {
  num_valid_ticks_ = 0;

  // `opus_decode_float` decodes to `float` samples with channels interlaced.
  // Typically these values are in the range of [-1, +1] (always for
  // `iamf_tools`-encoded data). Values outside of that range will be clipped in
  // `NormalizedFloatToInt32`.
  std::vector<float> output_pcm_float(num_samples_per_channel_ * num_channels_);

  // Transform the data and feed it to the decoder.
  std::vector<unsigned char> input_data(encoded_frame.size());
  std::transform(encoded_frame.begin(), encoded_frame.end(), input_data.begin(),
                 [](uint8_t c) { return static_cast<unsigned char>(c); });

  const int num_output_samples = opus_decode_float(
      decoder_, input_data.data(), static_cast<opus_int32>(input_data.size()),
      output_pcm_float.data(),
      /*frame_size=*/num_samples_per_channel_,
      /*decode_fec=*/0);
  if (num_output_samples < 0) {
    // When `num_output_samples` is negative, it is a non-OK Opus error code.
    return OpusErrorCodeToAbslStatus(num_output_samples,
                                     "Failed to decode Opus frame.");
  }
  LOG_FIRST_N(INFO, 3) << "Opus decoded " << num_output_samples
                       << " samples per channel. With " << num_channels_
                       << " channels.";
  // Convert the interleaved data to (time, channel) axes.
  return ConvertInterleavedToTimeChannel(
      absl::MakeConstSpan(output_pcm_float)
          .first(num_output_samples * num_channels_),
      num_channels_,
      absl::AnyInvocable<absl::Status(float, int32_t&) const>(
          NormalizedFloatingPointToInt32<float>),
      decoded_samples_, num_valid_ticks_);
}

}  // namespace iamf_tools
