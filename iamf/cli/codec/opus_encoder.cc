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
#include "iamf/cli/codec/opus_encoder.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/opus_utils.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "include/opus.h"
#include "include/opus_defines.h"
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
    auto error_message = absl::StrCat(
        "IAMF v1.1.0 expects output_gain: ", opus_decoder_config.output_gain_,
        " and mapping_family: ", opus_decoder_config.mapping_family_,
        " to be 0.");
    return absl::InvalidArgumentError(error_message);
  }

  return absl::OkStatus();
}

// `opus_encode_float` recommends the input is normalized to the range [-1, 1].
const absl::AnyInvocable<absl::Status(int32_t, float&) const>
    kInt32ToNormalizedFloat = [](int32_t input, float& output) {
      output = Int32ToNormalizedFloatingPoint<float>(input);
      return absl::OkStatus();
    };

absl::StatusOr<int> EncodeFloat(
    const std::vector<std::vector<int32_t>>& samples,
    int num_samples_per_channel, ::OpusEncoder* encoder,
    std::vector<uint8_t>& audio_frame) {
  std::vector<float> encoder_input_pcm;
  RETURN_IF_NOT_OK(ConvertTimeChannelToInterleaved(absl::MakeConstSpan(samples),
                                                   kInt32ToNormalizedFloat,
                                                   encoder_input_pcm));

  // TODO(b/311655037): Test that samples are passed to `opus_encode_float` in
  //                    the correct order. Maybe also check they are in the
  //                    correct [-1, +1] range. This may requiring mocking a
  //                    simple version of `opus_encode_float`.
  return opus_encode_float(encoder, encoder_input_pcm.data(),
                           num_samples_per_channel, audio_frame.data(),
                           static_cast<opus_int32>(audio_frame.size()));
}

absl::StatusOr<int> EncodeInt16(
    const std::vector<std::vector<int32_t>>& samples,
    int num_samples_per_channel, int num_channels, ::OpusEncoder* encoder,
    std::vector<uint8_t>& audio_frame) {
  // `libopus` requires the native system endianness as input.
  const bool big_endian = IsNativeBigEndian();
  // Convert input to the array that will be passed to `opus_encode`.
  std::vector<opus_int16> encoder_input_pcm(
      num_samples_per_channel * num_channels, 0);
  int write_position = 0;
  for (int t = 0; t < samples.size(); t++) {
    for (int c = 0; c < samples[0].size(); ++c) {
      // Convert all frames to 16-bit samples for input to Opus.
      // Write the 16-bit samples directly into the pcm vector.
      RETURN_IF_NOT_OK(
          WritePcmSample(static_cast<uint32_t>(samples[t][c]), 16, big_endian,
                         reinterpret_cast<uint8_t*>(encoder_input_pcm.data()),
                         write_position));
    }
  }

  return opus_encode(encoder, encoder_input_pcm.data(), num_samples_per_channel,
                     audio_frame.data(),
                     static_cast<opus_int32>(audio_frame.size()));
}

}  // namespace

absl::Status OpusEncoder::SetNumberOfSamplesToDelayAtStart(
    bool validate_codec_delay) {
  opus_int32 lookahead;
  opus_encoder_ctl(encoder_, OPUS_GET_LOOKAHEAD(&lookahead));
  LOG_FIRST_N(INFO, 1) << "Opus lookahead=" << lookahead;
  // Opus calls the number of samples that should be trimmed/pre-skipped
  // `lookahead`.
  required_samples_to_delay_at_start_ = static_cast<uint32_t>(lookahead);
  if (validate_codec_delay) {
    MAYBE_RETURN_IF_NOT_OK(
        ValidateEqual(static_cast<uint32_t>(decoder_config_.pre_skip_),
                      required_samples_to_delay_at_start_, "Opus `pre_skip`"));
  }

  return absl::OkStatus();
}

absl::Status OpusEncoder::InitializeEncoder() {
  MAYBE_RETURN_IF_NOT_OK(ValidateDecoderConfig(decoder_config_));

  int application;
  switch (encoder_metadata_.application()) {
    using enum iamf_tools_cli_proto::OpusApplicationFlag;
    case APPLICATION_VOIP:
      application = OPUS_APPLICATION_VOIP;
      break;
    case APPLICATION_AUDIO:
      application = OPUS_APPLICATION_AUDIO;
      break;
    case APPLICATION_RESTRICTED_LOWDELAY:
      application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unrecognized application.", encoder_metadata_.application()));
  }

  int opus_error_code;
  encoder_ = opus_encoder_create(input_sample_rate_, num_channels_, application,
                                 &opus_error_code);
  RETURN_IF_NOT_OK(OpusErrorCodeToAbslStatus(
      opus_error_code, "Failed to initialize Opus encoder."));

  // `OPUS_SET_BITRATE` treats this as the bit-rate for the entire substream.
  // Configure `libopus` so coupled substreams and mono substreams have equally
  // effective bit-rate per channel.
  float opus_rate;
  if (encoder_metadata_.substream_id_to_bitrate_override().contains(
          substream_id_)) {
    opus_rate =
        encoder_metadata_.substream_id_to_bitrate_override().at(substream_id_);
  } else if (num_channels_ > 1) {
    opus_rate = encoder_metadata_.target_bitrate_per_channel() * num_channels_ *
                encoder_metadata_.coupling_rate_adjustment();
  } else {
    opus_rate = encoder_metadata_.target_bitrate_per_channel();
  }
  opus_encoder_ctl(encoder_,
                   OPUS_SET_BITRATE(static_cast<opus_int32>(opus_rate + 0.5f)));

  return absl::OkStatus();
}

OpusEncoder::~OpusEncoder() { opus_encoder_destroy(encoder_); }

absl::Status OpusEncoder::EncodeAudioFrame(
    int /*input_bit_depth*/, const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  RETURN_IF_NOT_OK(ValidateNotFinalized());
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  const int num_samples_per_channel = static_cast<int>(num_samples_per_frame_);

  // Opus output could take up to 4 bytes per sample. Reserve an output vector
  // of the maximum possible size.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  audio_frame.resize(num_samples_per_channel * num_channels_ * 4, 0);

  const auto encoded_length_bytes =
      encoder_metadata_.use_float_api()
          ? EncodeFloat(samples, num_samples_per_channel, encoder_, audio_frame)
          : EncodeInt16(samples, num_samples_per_channel, num_channels_,
                        encoder_, audio_frame);

  if (!encoded_length_bytes.ok()) {
    return encoded_length_bytes.status();
  }

  if (*encoded_length_bytes < 0) {
    // When `encoded_length_bytes` is negative, it is a non-OK Opus error code.
    return OpusErrorCodeToAbslStatus(*encoded_length_bytes,
                                     "Failed to encode samples.");
  }

  // Shrink output vector to actual size.
  audio_frame.resize(*encoded_length_bytes);

  absl::MutexLock lock(&mutex_);
  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  return absl::OkStatus();
}

}  // namespace iamf_tools
