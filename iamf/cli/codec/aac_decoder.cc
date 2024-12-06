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

#include "absl/functional/any_invocable.h"
#include "absl/types/span.h"
#include "iamf/common/obu_util.h"

// This symbol conflicts with `aacenc_lib.h` and `aacdecoder_lib.h`.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/codec/aac_utils.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "libAACdec/include/aacdecoder_lib.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

namespace {

// Converts an AAC_DECODER_ERROR to an absl::Status.
absl::Status AacDecoderErrorToAbslStatus(AAC_DECODER_ERROR aac_error_code,
                                         absl::string_view error_message) {
  absl::StatusCode status_code;
  switch (aac_error_code) {
    case AAC_DEC_OK:
      return absl::OkStatus();
    case AAC_DEC_OUT_OF_MEMORY:
      status_code = absl::StatusCode::kResourceExhausted;
      break;
    case AAC_DEC_TRANSPORT_SYNC_ERROR:
    case AAC_DEC_NOT_ENOUGH_BITS:
    case AAC_DEC_INVALID_HANDLE:
    case AAC_DEC_UNSUPPORTED_AOT:
    case AAC_DEC_UNSUPPORTED_FORMAT:
    case AAC_DEC_UNSUPPORTED_ER_FORMAT:
    case AAC_DEC_UNSUPPORTED_EPCONFIG:
    case AAC_DEC_UNSUPPORTED_MULTILAYER:
    case AAC_DEC_UNSUPPORTED_CHANNELCONFIG:
    case AAC_DEC_UNSUPPORTED_SAMPLINGRATE:
    case AAC_DEC_INVALID_SBR_CONFIG:
    case AAC_DEC_SET_PARAM_FAIL:
    case AAC_DEC_OUTPUT_BUFFER_TOO_SMALL:
    case AAC_DEC_UNSUPPORTED_EXTENSION_PAYLOAD:
    case AAC_DEC_UNSUPPORTED_SBA:
    case AAC_DEC_ANC_DATA_ERROR:
    case AAC_DEC_TOO_SMALL_ANC_BUFFER:
    case AAC_DEC_TOO_MANY_ANC_ELEMENTS:
      status_code = absl::StatusCode::kInvalidArgument;
      break;
    case AAC_DEC_NEED_TO_RESTART:
      status_code = absl::StatusCode::kFailedPrecondition;
      break;
    // Several error codes usually imply that the bitstream is corrupt.
    case AAC_DEC_TRANSPORT_ERROR:
    case AAC_DEC_PARSE_ERROR:
    case AAC_DEC_DECODE_FRAME_ERROR:
    case AAC_DEC_INVALID_CODE_BOOK:
    case AAC_DEC_UNSUPPORTED_PREDICTION:
    case AAC_DEC_UNSUPPORTED_CCE:
    case AAC_DEC_UNSUPPORTED_LFE:
    case AAC_DEC_UNSUPPORTED_GAIN_CONTROL_DATA:
    case AAC_DEC_CRC_ERROR:
    case AAC_DEC_RVLC_ERROR:
    case AAC_DEC_TNS_READ_ERROR:
      status_code = absl::StatusCode::kDataLoss;
      break;
    default:
      status_code = absl::StatusCode::kUnknown;
      break;
  }

  return absl::Status(
      status_code,
      absl::StrCat(error_message, " AAC_DECODER_ERROR= ", aac_error_code));
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
    return absl::UnknownError("Failed to initialize AAC decoder.");
  }

  RETURN_IF_NOT_OK(
      ConfigureAacDecoder(aac_decoder_config_, num_channels_, decoder_));

  const auto* stream_info = aacDecoder_GetStreamInfo(decoder_);
  LOG_FIRST_N(INFO, 1) << "Created an AAC encoder with "
                       << stream_info->numChannels << " channels.";

  return absl::OkStatus();
}

absl::Status AacDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame) {
  num_valid_ticks_ = 0;

  // Transform the data and feed it to the decoder.
  std::vector<UCHAR> input_data(encoded_frame.size());
  std::transform(encoded_frame.begin(), encoded_frame.end(), input_data.begin(),
                 [](uint8_t c) { return static_cast<UCHAR>(c); });

  UCHAR* in_buffer[] = {input_data.data()};
  const UINT buffer_size[] = {static_cast<UINT>(encoded_frame.size())};
  UINT bytes_valid = static_cast<UINT>(encoded_frame.size());
  RETURN_IF_NOT_OK(AacDecoderErrorToAbslStatus(
      aacDecoder_Fill(decoder_, in_buffer, buffer_size, &bytes_valid),
      "Failed on `aacDecoder_Fill`: "));
  if (bytes_valid != 0) {
    return absl::InvalidArgumentError(
        "The input frame failed to decode. It may not have been a "
        "complete AAC frame.");
  }

  // Retrieve the decoded frame. `fdk_aac` decodes to INT_PCM (usually 16-bits)
  // samples with channels interlaced.
  std::vector<INT_PCM> output_pcm(num_samples_per_channel_ * num_channels_);
  RETURN_IF_NOT_OK(AacDecoderErrorToAbslStatus(
      aacDecoder_DecodeFrame(decoder_, output_pcm.data(), output_pcm.size(),
                             /*flags=*/0),
      "Failed on `aacDecoder_DecodeFrame`: "));

  // Arrange the interleaved data in (time, channel) axes with samples stored in
  // the upper bytes of an `int32_t`.
  const absl::AnyInvocable<absl::Status(INT_PCM, int32_t&) const>
      kAacInternalTypeToInt32 = [](INT_PCM input, int32_t& output) {
        output = static_cast<int32_t>(input) << (32 - GetFdkAacBitDepth());
        return absl::OkStatus();
      };
  return ConvertInterleavedToTimeChannel(absl::MakeConstSpan(output_pcm),
                                         num_channels_, kAacInternalTypeToInt32,
                                         decoded_samples_, num_valid_ticks_);
}

}  // namespace iamf_tools
