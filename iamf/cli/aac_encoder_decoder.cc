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
#include "iamf/cli/aac_encoder_decoder.h"

#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"

// This symbol conflicts with `aacenc_lib.h` and `aacdecoder_lib.h`.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/aac_decoder_config.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/decoder_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"
#include "iamf/write_bit_buffer.h"
#include "libAACdec/include/aacdecoder_lib.h"
#include "libAACenc/include/aacenc_lib.h"
#include "libSYS/include/FDK_audio.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

// IAMF requires raw AAC frames with no ADTS header.
const auto kAacTranportType = TT_MP4_RAW;

// `libfdk_aac` has the bytes per sample fixed at compile time.
const size_t kFdkAacBytesPerSample = sizeof(INT_PCM);
const size_t kFdkAacBitDepth = kFdkAacBytesPerSample * 8;

namespace {

absl::Status AacEncErrorToAbslStatus(AACENC_ERROR aac_error_code,
                                     const std::string& error_message) {
  absl::StatusCode status_code;
  switch (aac_error_code) {
    case AACENC_OK:
      return absl::OkStatus();
    case AACENC_INVALID_HANDLE:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AACENC_MEMORY_ERROR:
      status_code = absl::StatusCode::kResourceExhausted;
      break;
    case AACENC_UNSUPPORTED_PARAMETER:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AACENC_INVALID_CONFIG:
      status_code = absl::StatusCode::kFailedPrecondition;
      break;
    case AACENC_INIT_ERROR:
    case AACENC_INIT_AAC_ERROR:
    case AACENC_INIT_SBR_ERROR:
    case AACENC_INIT_TP_ERROR:
    case AACENC_INIT_META_ERROR:
    case AACENC_INIT_MPS_ERROR:
      status_code = absl::StatusCode::kInternal;
      break;
    case AACENC_ENCODE_EOF:
      status_code = absl::StatusCode::kOutOfRange;
      break;
    case AACENC_ENCODE_ERROR:
    default:
      status_code = absl::StatusCode::kUnknown;
      break;
  }

  return absl::Status(
      status_code,
      absl::StrCat(error_message, " AACENC_ERROR= ", aac_error_code));
}

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

absl::Status ConfigureAacEncoder(
    const iamf_tools_cli_proto::AacEncoderMetadata& encoder_metadata,
    int num_channels, uint32_t num_samples_per_frame,
    uint32_t output_sample_rate, AACENCODER* const encoder) {
  // IAMF requires metadata is not embedded in the stream.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_METADATA_MODE, 0),
      "Failed to configure encoder metadata mode."));

  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_TRANSMUX, kAacTranportType),
      "Failed to configure encoder transport type."));

  // IAMF only supports AAC-LC.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_AOT, AOT_AAC_LC),
      "Failed to configure encoder audio object type."));

  // Configure values based on the associated Codec Config OBU.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_SAMPLERATE,
                          static_cast<UINT>(output_sample_rate)),
      "Failed to configure encoder sample rate."));

  CHANNEL_MODE aac_channel_mode;
  switch (num_channels) {
    case 1:
      aac_channel_mode = MODE_1;

      break;
    case 2:
      aac_channel_mode = MODE_2;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("IAMF requires AAC to be used with 1 or 2 channels. Got "
                       "num_channels= ",
                       num_channels));
  }
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_CHANNELMODE, aac_channel_mode),
      absl::StrCat("Failed to configure encoder channel mode= ",
                   aac_channel_mode)));

  // Set bitrate based on the equation recommended by the documentation.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(
          encoder, AACENC_BITRATE,
          3 * num_channels * num_samples_per_frame * output_sample_rate / 2),
      "Failed to configure encoder bitrate."));

  // Set some arguments configured by the user-provided `encoder_metadata_`.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_AFTERBURNER,
                          encoder_metadata.enable_afterburner() ? 1 : 0),
      absl::StrCat(
          "Failed to configure encoder afterburner enable_afterburner= ",
          encoder_metadata.enable_afterburner())));

  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_BITRATEMODE,
                          encoder_metadata.bitrate_mode()),
      absl::StrCat("Failed to configure encoder bitrate mode= ",
                   encoder_metadata.bitrate_mode())));

  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncoder_SetParam(encoder, AACENC_SIGNALING_MODE,
                          encoder_metadata.signaling_mode()),
      absl::StrCat("Failed to configure encoder signaling mode= ",
                   encoder_metadata.signaling_mode())));

  return absl::OkStatus();
}

absl::Status ValidateEncoderInfo(int num_channels,
                                 uint32_t num_samples_per_frame,
                                 AACENCODER* const encoder) {
  // Validate the configuration is consistent with the associated Codec Config
  // OBU.
  AACENC_InfoStruct enc_info;
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(aacEncInfo(encoder, &enc_info),
                                           "Failed to get encoder info."));

  if (enc_info.inputChannels != num_channels) {
    LOG(ERROR) << "Incorrect number of input channels: "
               << enc_info.inputChannels << " vs " << num_channels;
    return absl::UnknownError("");
  }

  if (enc_info.frameLength != num_samples_per_frame) {
    LOG(ERROR) << "Incorrect frame length: " << enc_info.frameLength << " vs "
               << num_samples_per_frame;
    return absl::UnknownError("");
  }

  return absl::OkStatus();
}

}  // namespace

AacDecoder::AacDecoder(const CodecConfigObu& codec_config_obu, int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      aac_decoder_config_(std::get<AacDecoderConfig>(
          codec_config_obu.codec_config_.decoder_config)) {}

AacDecoder::~AacDecoder() {
  if (decoder_ != nullptr) {
    aacDecoder_Close(decoder_);
  }
}

absl::Status AacDecoder::Initialize() {
  // Initialize the decoder.
  decoder_ = aacDecoder_Open(kAacTranportType, /*nrOfLayers=*/1);

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
    std::vector<std::vector<int32_t>>& decoded_frames) {
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
  decoded_frames.reserve(output_pcm.size() / num_channels_);
  for (int i = 0; i < output_pcm.size(); i += num_channels_) {
    // Grab samples in all channels associated with this time instant and store
    // it in the upper bytes.
    std::vector<int32_t> time_sample(num_channels_, 0);
    for (int j = 0; j < num_channels_; ++j) {
      time_sample[j] = static_cast<int32_t>(output_pcm[i + j])
                       << (32 - kFdkAacBitDepth);
    }
    decoded_frames.push_back(time_sample);
  }

  return absl::OkStatus();
}

absl::Status AacEncoder::InitializeEncoder() {
  if (encoder_) {
    LOG(ERROR) << "Expected `encoder_` to not be initialized yet.";
    return absl::UnknownError("");
  }

  // Open the encoder.
  RETURN_IF_NOT_OK(
      AacEncErrorToAbslStatus(aacEncOpen(&encoder_, 0, num_channels_),
                              "Failed to initialize AAC encoder."));

  // Configure the encoder.
  RETURN_IF_NOT_OK(ConfigureAacEncoder(encoder_metadata_, num_channels_,
                                       num_samples_per_frame_,
                                       output_sample_rate_, encoder_));

  // Call `aacEncEncode` with `nullptr` arguments to initialize the encoder.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncEncode(encoder_, nullptr, nullptr, nullptr, nullptr),
      "Failed on call to `aacEncEncode`."));

  // Validate the configuration matches expected results.
  RETURN_IF_NOT_OK(
      ValidateEncoderInfo(num_channels_, num_samples_per_frame_, encoder_));

  return absl::OkStatus();
}

AacEncoder::~AacEncoder() { aacEncClose(&encoder_); }

absl::Status AacEncoder::EncodeAudioFrame(
    int input_bit_depth, const std::vector<std::vector<int32_t>>& samples,
    std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) {
  if (!encoder_) {
    LOG(ERROR) << "Expected `encoder_` to be initialized.";
  }
  RETURN_IF_NOT_OK(ValidateInputSamples(samples));
  const int num_samples_per_channel = static_cast<int>(num_samples_per_frame_);

  AACENC_InfoStruct enc_info;
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(aacEncInfo(encoder_, &enc_info),
                                           "Failed to get encoder info."));

  // Convert input to the array that will be passed to `aacEncEncode`.
  if (input_bit_depth != kFdkAacBitDepth) {
    LOG(ERROR) << "Expected AAC to be " << kFdkAacBitDepth
               << " bits, got "
                  "bit-depth: "
               << input_bit_depth;
    return absl::UnknownError("");
  }

  // `fdk_aac` requires the native system endianness as input.
  const bool big_endian = IsNativeBigEndian();
  std::vector<INT_PCM> encoder_input_pcm(
      num_samples_per_channel * num_channels_, 0);
  int write_position = 0;
  for (int t = 0; t < samples.size(); t++) {
    for (int c = 0; c < samples[0].size(); ++c) {
      // Convert all frames to INT_PCM samples for input for `fdk_aac` (usually
      // 16-bit).
      RETURN_IF_NOT_OK(WritePcmSample(
          static_cast<uint32_t>(samples[t][c]), input_bit_depth, big_endian,
          reinterpret_cast<uint8_t*>(encoder_input_pcm.data()),
          write_position));
    }
  }

  // The `fdk_aac` interface supports multiple input buffers. Although IAMF only
  // uses one buffer without metadata or ancillary data.
  void* in_buffers[1] = {encoder_input_pcm.data()};
  INT in_buffer_identifiers[1] = {IN_AUDIO_DATA};
  INT in_buffer_sizes[1] = {
      static_cast<INT>(encoder_input_pcm.size() * kFdkAacBytesPerSample)};
  INT in_buffer_element_sizes[1] = {kFdkAacBytesPerSample};
  AACENC_BufDesc inBufDesc = {.numBufs = 1,
                              .bufs = in_buffers,
                              .bufferIdentifiers = in_buffer_identifiers,
                              .bufSizes = in_buffer_sizes,
                              .bufElSizes = in_buffer_element_sizes};
  AACENC_InArgs in_args = {
      .numInSamples = num_samples_per_channel * num_channels_,
      .numAncBytes = 0};

  // Resize the output buffer to support the worst case size.
  auto& audio_frame = partial_audio_frame_with_data->obu.audio_frame_;
  audio_frame.resize(enc_info.maxOutBufBytes, 0);

  // The `fdk_aac` interface supports multiple input buffers. Although IAMF only
  // uses one buffer without metadata or ancillary data.
  void* out_bufs[1] = {audio_frame.data()};
  INT out_buffer_identifiers[1] = {OUT_BITSTREAM_DATA};
  INT out_buffer_sizes[1] = {
      static_cast<INT>(audio_frame.size() * sizeof(uint8_t))};
  INT out_buffer_element_sizes[1] = {sizeof(uint8_t)};
  AACENC_BufDesc outBufDesc = {.numBufs = 1,
                               .bufs = out_bufs,
                               .bufferIdentifiers = out_buffer_identifiers,
                               .bufSizes = out_buffer_sizes,
                               .bufElSizes = out_buffer_element_sizes};

  // Encode the frame.
  AACENC_OutArgs out_args;
  // This implementation expects `fdk_aac` to return an entire frame and no
  // error code.
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(
      aacEncEncode(encoder_, &inBufDesc, &outBufDesc, &in_args, &out_args),
      "Failed on call to `aacEncEncode`."));

  if (num_samples_per_channel * num_channels_ != out_args.numInSamples) {
    LOG(ERROR) << "Failed to encode an entire frame.";
    return absl::UnknownError("");
  }

  // Resize the buffer to the actual size and finalize it.
  audio_frame.resize(out_args.numOutBytes);
  finalized_audio_frames_.emplace_back(
      std::move(*partial_audio_frame_with_data));

  LOG_FIRST_N(INFO, 3) << "Encoded " << num_samples_per_channel << " samples * "
                       << num_channels_ << " channels using "
                       << out_args.numOutBytes << " bytes";
  return absl::OkStatus();
}

absl::Status AacEncoder::SetNumberOfSamplesToDelayAtStart() {
  if (!encoder_) {
    LOG(ERROR) << "Expected `encoder_` to be initialized.";
  }

  // Validate the configuration.
  AACENC_InfoStruct enc_info;
  RETURN_IF_NOT_OK(AacEncErrorToAbslStatus(aacEncInfo(encoder_, &enc_info),
                                           "Failed to get encoder info."));

  // Set the number of samples the decoder must ignore. For AAC this appears
  // to be implementation specific. The implementation of AAC-LC in `fdk_aac`
  // seems to usually make this 2048 samples.
  required_samples_to_delay_at_start_ = enc_info.nDelayCore;
  return absl::OkStatus();
}

}  // namespace iamf_tools
