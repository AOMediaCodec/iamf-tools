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
#include "iamf/cli/opus_encoder_decoder.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/decoder_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"
#include "iamf/opus_decoder_config.h"
#include "include/opus.h"
#include "include/opus_defines.h"
#include "include/opus_types.h"

namespace iamf_tools {

namespace {

absl::StatusCode OpusErrorCodeToAbslStatusCode(int opus_error_code) {
  switch (opus_error_code) {
    case OPUS_OK:
      return absl::StatusCode::kOk;
    case OPUS_BAD_ARG:
      return absl::StatusCode::kInvalidArgument;
    case OPUS_BUFFER_TOO_SMALL:
    case OPUS_INVALID_STATE:
      return absl::StatusCode::kFailedPrecondition;
    case OPUS_INTERNAL_ERROR:
      return absl::StatusCode::kInternal;
    case OPUS_INVALID_PACKET:
      return absl::StatusCode::kDataLoss;
    case OPUS_UNIMPLEMENTED:
      return absl::StatusCode::kUnimplemented;
    case OPUS_ALLOC_FAIL:
      return absl::StatusCode::kResourceExhausted;
    default:
      return absl::StatusCode::kUnknown;
  }
}

// Performs validation for values that this implementation assumes are
// restricted because they are restricted in IAMF V1.
absl::Status ValidateDecoderConfig(
    const OpusDecoderConfig& opus_decoder_config) {
  // Validate the input. Reject values that would need to be added to this
  // function if they were ever supported.
  if (opus_decoder_config.output_gain_ != 0 ||
      opus_decoder_config.mapping_family_ != 0) {
    LOG(ERROR) << "IAMF V1 expects output_gain: "
               << opus_decoder_config.output_gain_ << " and mapping_family: "
               << static_cast<int>(opus_decoder_config.mapping_family_)
               << " to be 0.";
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

absl::StatusOr<int> EncodeFloat(
    const std::vector<std::vector<int32_t>>& samples,
    int num_samples_per_channel, int num_channels, ::OpusEncoder* encoder,
    std::vector<uint8_t>& audio_frame) {
  //  `opus_encode_float` usually recommends the input is normalized to the
  //  range [-1, 1].
  std::vector<float> encoder_input_pcm(num_samples_per_channel * num_channels,
                                       0.0);
  for (int t = 0; t < samples.size(); t++) {
    for (int c = 0; c < num_channels; ++c) {
      encoder_input_pcm[t * num_channels + c] =
          Int32ToNormalizedFloat(samples[t][c]);
    }
  }

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

OpusDecoder::OpusDecoder(const CodecConfigObu& codec_config_obu,
                         int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      opus_decoder_config_(std::get<OpusDecoderConfig>(
          codec_config_obu.codec_config_.decoder_config)),
      output_sample_rate_(codec_config_obu.GetOutputSampleRate()) {}

OpusDecoder::~OpusDecoder() {
  if (decoder_ != nullptr) {
    opus_decoder_destroy(decoder_);
  }
}

absl::Status OpusDecoder::Initialize() {
  RETURN_IF_NOT_OK(ValidateDecoderConfig(opus_decoder_config_));

  // Initialize the decoder.
  int opus_error_code;
  decoder_ = opus_decoder_create(static_cast<opus_int32>(output_sample_rate_),
                                 num_channels_, &opus_error_code);

  const auto status_code = OpusErrorCodeToAbslStatusCode(opus_error_code);
  if (status_code != absl::StatusCode::kOk) {
    decoder_ = nullptr;
    return absl::Status(
        status_code,
        absl::StrCat("Failed to initialize Opus decoder: opus_error_code= ",
                     opus_error_code));
  }

  return absl::OkStatus();
}

absl::Status OpusDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame,
    std::vector<std::vector<int32_t>>& decoded_frames) {
  // `opus_decode_float` decodes to `float` samples with channels interlaced.
  // Typically these values are in the range of [-1, +1] (always for
  // `iamf_tools`-encoded data). Values outside of that range will be clipped in
  // `NormalizedFloatToInt32`.
  std::vector<float> output_pcm_float;
  output_pcm_float.resize(num_samples_per_channel_ * num_channels_);

  // Transform the data and feed it to the decoder.
  std::vector<unsigned char> input_data(encoded_frame.size());
  std::transform(encoded_frame.begin(), encoded_frame.end(), input_data.begin(),
                 [](uint8_t c) { return static_cast<unsigned char>(c); });

  const int num_output_samples = opus_decode_float(
      decoder_, input_data.data(), static_cast<opus_int32>(input_data.size()),
      output_pcm_float.data(),
      /*frame_size=*/num_samples_per_channel_,
      /*decode_fec=*/0);
  output_pcm_float.resize(num_output_samples * num_channels_);
  LOG_FIRST_N(INFO, 3) << "Opus decoded " << num_output_samples
                       << " samples per channel. With " << num_channels_
                       << " channels.";

  if (num_output_samples < 0) {
    // When `num_output_samples` is negative, it is an Opus error code.
    return absl::Status(
        OpusErrorCodeToAbslStatusCode(num_output_samples),
        absl::StrCat("Failed to initialize Opus decoder: num_output_samples= ",
                     num_output_samples));
  }

  // Convert data to channels arranged in (time, channel) axes. There can only
  // be one or two channels.
  decoded_frames.reserve(output_pcm_float.size() / num_channels_);
  for (int i = 0; i < output_pcm_float.size(); i += num_channels_) {
    std::vector<int32_t> time_sample(num_channels_, 0);
    // Grab samples in all channels associated with this time instant.
    for (int j = 0; j < num_channels_; ++j) {
      RETURN_IF_NOT_OK(
          NormalizedFloatToInt32(output_pcm_float[i + j], time_sample[j]));
    }
    decoded_frames.push_back(time_sample);
  }

  return absl::OkStatus();
}

absl::Status OpusEncoder::SetNumberOfSamplesToDelayAtStart() {
  opus_int32 lookahead;
  opus_encoder_ctl(encoder_, OPUS_GET_LOOKAHEAD(&lookahead));
  LOG(INFO) << "Opus lookahead=" << lookahead;
  // Opus calls the number of samples that should be trimmed/pre-skipped
  // `lookahead`.
  required_samples_to_delay_at_start_ = static_cast<uint32_t>(lookahead);

  // Validate this matches the data from the associated Codec Config OBU.
  if (static_cast<uint32_t>(decoder_config_.pre_skip_) !=
      required_samples_to_delay_at_start_) {
    LOG(ERROR) << "Opus required `pre_skip` to be: "
               << required_samples_to_delay_at_start_
               << " but it was configured to: " << decoder_config_.pre_skip_;
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

absl::Status OpusEncoder::InitializeEncoder() {
  RETURN_IF_NOT_OK(ValidateDecoderConfig(decoder_config_));

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
      LOG(ERROR) << "Unrecognized application: "
                 << encoder_metadata_.application();
      return absl::UnknownError("");
  }

  int opus_error_code;
  encoder_ = opus_encoder_create(input_sample_rate_, num_channels_, application,
                                 &opus_error_code);

  const auto status_code = OpusErrorCodeToAbslStatusCode(opus_error_code);
  if (status_code != absl::StatusCode::kOk) {
    return absl::Status(
        status_code,
        absl::StrCat("Failed to initialize Opus encoder: opus_error_code= ",
                     opus_error_code));
  }

  // `OPUS_SET_BITRATE` treats this as the bit-rate for the entire substream.
  // Configure `libopus` so coupled substreams and mono substreams have the same
  // effective bit-rate per channel.
  const auto substream_bitrate = static_cast<opus_int32>(
      encoder_metadata_.target_bitrate_per_channel() * num_channels_);
  opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(substream_bitrate));

  return absl::OkStatus();
}

OpusEncoder::~OpusEncoder() { opus_encoder_destroy(encoder_); }

absl::Status OpusEncoder::EncodeAudioFrame(
    int /*input_bit_depth*/, const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  const int num_samples_per_channel = static_cast<int>(num_samples_per_frame_);

  // Opus output could take up to 4 bytes per sample. Reserve an output vector
  // of the maximum possible size.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  audio_frame.resize(num_samples_per_channel * num_channels_ * 4, 0);

  const auto encoded_length_bytes =
      encoder_metadata_.use_float_api()
          ? EncodeFloat(samples, num_samples_per_channel, num_channels_,
                        encoder_, audio_frame)
          : EncodeInt16(samples, num_samples_per_channel, num_channels_,
                        encoder_, audio_frame);

  if (!encoded_length_bytes.ok()) {
    return encoded_length_bytes.status();
  }

  if (*encoded_length_bytes < 0) {
    // When `encoded_length_bytes` is negative, it is an Opus error code.
    return absl::Status(
        OpusErrorCodeToAbslStatusCode(*encoded_length_bytes),
        absl::StrCat(
            "Failed to initialize Opus decoder: encoded_length_bytes= ",
            *encoded_length_bytes));
  }

  // Shrink output vector to actual size.
  audio_frame.resize(*encoded_length_bytes);

  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  return absl::OkStatus();
}

}  // namespace iamf_tools
