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
#include <memory>
#include <variant>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/opus_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/sample_processing_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/types.h"
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

absl::StatusOr<std::unique_ptr<DecoderBase>> OpusDecoder::Create(
    const CodecConfigObu& codec_config_obu, int num_channels) {
  const OpusDecoderConfig* decoder_config = std::get_if<OpusDecoderConfig>(
      &codec_config_obu.GetCodecConfig().decoder_config);
  if (decoder_config == nullptr) {
    return absl::InvalidArgumentError(
        "CodecConfigObu does not contain an `OpusDecoderConfig`.");
  }
  MAYBE_RETURN_IF_NOT_OK(ValidateDecoderConfig(*decoder_config));

  // Initialize the decoder.
  int opus_error_code;
  LibOpusDecoder* decoder = opus_decoder_create(
      static_cast<opus_int32>(codec_config_obu.GetOutputSampleRate()),
      num_channels, &opus_error_code);
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(
      opus_error_code, "Failed to initialize Opus decoder."));
  if (decoder == nullptr) {
    return absl::UnknownError("Unexpected null decoder after initialization.");
  }

  return absl::WrapUnique(new OpusDecoder(
      num_channels, codec_config_obu.GetNumSamplesPerFrame(), decoder));
}

OpusDecoder::~OpusDecoder() {
  // The factory function prevents `decoder_` from ever being null.
  CHECK_NE(decoder_, nullptr);
  opus_decoder_destroy(decoder_);
}

absl::Status OpusDecoder::DecodeAudioFrame(
    absl::Span<const uint8_t> encoded_frame) {
  // TODO(b/382197581): Pre-allocate working buffers like `output_pcm_float` and
  //                    `input_data`.

  // `opus_decode_float` decodes to `float` samples with channels interlaced.
  // Typically these values are in the range of [-1, +1] (always for
  // `iamf_tools`-encoded data). Values outside of that range will be clipped in
  // `NormalizedFloatingPointToInt32`.
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
  LOG_FIRST_N(INFO, 1) << "Opus decoded " << num_output_samples
                       << " samples per channel. With " << num_channels_
                       << " channels.";

  // Convert the interleaved data to (channel, time) axes
  const absl::AnyInvocable<absl::Status(float, InternalSampleType&) const>
      kFloatToInternalSampleType = [](float input, InternalSampleType& output) {
        output = static_cast<InternalSampleType>(input);
        return absl::OkStatus();
      };
  return ConvertInterleavedToChannelTime(
      absl::MakeConstSpan(output_pcm_float)
          .first(num_output_samples * num_channels_),
      num_channels_, decoded_samples_, kFloatToInternalSampleType);
}

}  // namespace iamf_tools
