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
#include "iamf/obu/codec_config.h"

#include <cstdint>
#include <utility>
#include <variant>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status ValidateNumSamplesPerFrame(uint32_t num_samples_per_frame) {
  if (num_samples_per_frame == 0) {
    return absl::InvalidArgumentError(
        "Number of samples per frame must be non-zero.");
  }
  return absl::OkStatus();
}

absl::Status OverrideAudioRollDistance(CodecConfig::CodecId codec_id,
                                       uint32_t num_samples_per_frame,
                                       int16_t& output_audio_roll_distance) {
  switch (codec_id) {
    using enum CodecConfig::CodecId;
    case CodecConfig::kCodecIdOpus: {
      auto audio_roll_distance =
          OpusDecoderConfig::GetRequiredAudioRollDistance(
              num_samples_per_frame);
      if (!audio_roll_distance.ok()) {
        return audio_roll_distance.status();
      }
      output_audio_roll_distance = *audio_roll_distance;
      return absl::OkStatus();
    }
    case kCodecIdLpcm:
      output_audio_roll_distance =
          LpcmDecoderConfig::GetRequiredAudioRollDistance();
      return absl::OkStatus();
    case kCodecIdFlac:
      output_audio_roll_distance =
          FlacDecoderConfig::GetRequiredAudioRollDistance();
      return absl::OkStatus();
    case kCodecIdAacLc:
      output_audio_roll_distance =
          AacDecoderConfig::GetRequiredAudioRollDistance();
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_id));
  }
}

absl::Status SetSampleRatesAndBitDepths(
    uint32_t codec_id, const DecoderConfig& decoder_config,
    uint32_t& output_sample_rate, uint32_t& input_sample_rate,
    uint8_t& bit_depth_to_measure_loudness) {
  switch (codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus: {
      const auto& opus_decoder_config =
          std::get<OpusDecoderConfig>(decoder_config);
      output_sample_rate = opus_decoder_config.GetOutputSampleRate();
      input_sample_rate = opus_decoder_config.GetInputSampleRate();
      bit_depth_to_measure_loudness =
          OpusDecoderConfig::GetBitDepthToMeasureLoudness();
      return absl::OkStatus();
    }
    case kCodecIdLpcm: {
      const auto& lpcm_decoder_config =
          std::get<LpcmDecoderConfig>(decoder_config);
      RETURN_IF_NOT_OK(
          lpcm_decoder_config.GetOutputSampleRate(output_sample_rate));
      input_sample_rate = output_sample_rate;
      RETURN_IF_NOT_OK(lpcm_decoder_config.GetBitDepthToMeasureLoudness(
          bit_depth_to_measure_loudness));
      return absl::OkStatus();
    }
    case kCodecIdAacLc:
      RETURN_IF_NOT_OK(std::get<AacDecoderConfig>(decoder_config)
                           .GetOutputSampleRate(output_sample_rate));
      input_sample_rate = output_sample_rate;
      bit_depth_to_measure_loudness =
          AacDecoderConfig::GetBitDepthToMeasureLoudness();
      return absl::OkStatus();
    case kCodecIdFlac: {
      const auto& flac_decoder_config =
          std::get<FlacDecoderConfig>(decoder_config);
      RETURN_IF_NOT_OK(
          flac_decoder_config.GetOutputSampleRate(output_sample_rate));
      input_sample_rate = output_sample_rate;
      RETURN_IF_NOT_OK(flac_decoder_config.GetBitDepthToMeasureLoudness(
          bit_depth_to_measure_loudness));

      return absl::OkStatus();
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_id));
  }
}

}  // namespace

CodecConfigObu::CodecConfigObu(const ObuHeader& header,
                               const DecodedUleb128 codec_config_id,
                               const CodecConfig& codec_config)
    : ObuBase(header, kObuIaCodecConfig),
      codec_config_id_(codec_config_id),
      codec_config_(std::move(codec_config)) {}

absl::StatusOr<CodecConfigObu> CodecConfigObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size, ReadBitBuffer& rb) {
  CodecConfigObu codec_config_obu(header);
  RETURN_IF_NOT_OK(codec_config_obu.ReadAndValidatePayload(payload_size, rb));
  RETURN_IF_NOT_OK(codec_config_obu.Initialize());
  return codec_config_obu;
}

absl::Status CodecConfigObu::ValidateAndWriteDecoderConfig(
    WriteBitBuffer& wb) const {
  if (!init_status_.ok()) {
    return init_status_;
  }

  // Write the `decoder_config` struct portion. This is codec specific.
  const int16_t audio_roll_distance = codec_config_.audio_roll_distance;
  const uint32_t num_samples_per_frame = codec_config_.num_samples_per_frame;
  switch (codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus:
      return std::get<OpusDecoderConfig>(codec_config_.decoder_config)
          .ValidateAndWrite(num_samples_per_frame, audio_roll_distance, wb);
    case kCodecIdLpcm:
      return std::get<LpcmDecoderConfig>(codec_config_.decoder_config)
          .ValidateAndWrite(audio_roll_distance, wb);
    case kCodecIdAacLc:
      return std::get<AacDecoderConfig>(codec_config_.decoder_config)
          .ValidateAndWrite(audio_roll_distance, wb);
    case kCodecIdFlac:
      return std::get<FlacDecoderConfig>(codec_config_.decoder_config)
          .ValidateAndWrite(num_samples_per_frame, audio_roll_distance, wb);
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_config_.codec_id));
  }
}

absl::Status CodecConfigObu::ValidateAndWritePayload(WriteBitBuffer& wb) const {
  if (!init_status_.ok()) {
    return init_status_;
  }

  RETURN_IF_NOT_OK(wb.WriteUleb128(codec_config_id_));

  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(codec_config_.codec_id, 32));
  RETURN_IF_NOT_OK(
      ValidateNumSamplesPerFrame(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(wb.WriteUleb128(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(wb.WriteSigned16(codec_config_.audio_roll_distance));

  // Write the `decoder_config_`. This is codec specific.
  RETURN_IF_NOT_OK(ValidateAndWriteDecoderConfig(wb));

  return absl::OkStatus();
}

absl::Status CodecConfigObu::ReadAndValidateDecoderConfig(ReadBitBuffer& rb) {
  const int16_t audio_roll_distance = codec_config_.audio_roll_distance;
  const uint32_t num_samples_per_frame = codec_config_.num_samples_per_frame;
  // Read the `decoder_config` struct portion. This is codec specific.
  switch (codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdOpus: {
      OpusDecoderConfig opus_decoder_config;
      RETURN_IF_NOT_OK(opus_decoder_config.ReadAndValidate(
          num_samples_per_frame, audio_roll_distance, rb));
      codec_config_.decoder_config = opus_decoder_config;
      return absl::OkStatus();
    }
    case kCodecIdLpcm: {
      LpcmDecoderConfig lpcm_decoder_config;
      RETURN_IF_NOT_OK(
          lpcm_decoder_config.ReadAndValidate(audio_roll_distance, rb));
      codec_config_.decoder_config = lpcm_decoder_config;
      return absl::OkStatus();
    }
    case kCodecIdAacLc: {
      AacDecoderConfig aac_decoder_config;
      RETURN_IF_NOT_OK(
          aac_decoder_config.ReadAndValidate(audio_roll_distance, rb));
      codec_config_.decoder_config = aac_decoder_config;
      return absl::OkStatus();
    }
    case kCodecIdFlac: {
      FlacDecoderConfig flac_decoder_config;
      RETURN_IF_NOT_OK(flac_decoder_config.ReadAndValidate(
          num_samples_per_frame, audio_roll_distance, rb));
      codec_config_.decoder_config = flac_decoder_config;
      return absl::OkStatus();
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown codec_id: ", codec_config_.codec_id));
  }
  return absl::OkStatus();
}

absl::Status CodecConfigObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadULeb128(codec_config_id_));
  uint64_t codec_id;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, codec_id));
  codec_config_.codec_id = static_cast<CodecConfig::CodecId>(codec_id);
  RETURN_IF_NOT_OK(rb.ReadULeb128(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(
      ValidateNumSamplesPerFrame(codec_config_.num_samples_per_frame));
  RETURN_IF_NOT_OK(rb.ReadSigned16(codec_config_.audio_roll_distance));

  // Read the `decoder_config_`. This is codec specific.
  RETURN_IF_NOT_OK(ReadAndValidateDecoderConfig(rb));
  return absl::OkStatus();
}

void CodecConfigObu::PrintObu() const {
  if (!init_status_.ok()) {
    LOG(ERROR) << "This OBU failed to initialize with error= " << init_status_;
  }
  LOG(INFO) << "Codec Config OBU:";
  LOG(INFO) << "  codec_config_id= " << codec_config_id_;
  LOG(INFO) << "  codec_config:";
  LOG(INFO) << "    codec_id= " << codec_config_.codec_id;
  LOG(INFO) << "    num_samples_per_frame= " << GetNumSamplesPerFrame();
  LOG(INFO) << "    audio_roll_distance= " << codec_config_.audio_roll_distance;

  // Print the `decoder_config_`. This is codec specific.
  switch (codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
      std::get<LpcmDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    case kCodecIdOpus:
      std::get<OpusDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    case kCodecIdFlac:
      std::get<FlacDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    case kCodecIdAacLc:
      std::get<AacDecoderConfig>(codec_config_.decoder_config).Print();
      break;
    default:
      LOG(ERROR) << "Unknown codec_id: " << codec_config_.codec_id;
      break;
  }

  LOG(INFO) << "  // input_sample_rate_= " << input_sample_rate_;
  LOG(INFO) << "  // output_sample_rate_= " << output_sample_rate_;
  LOG(INFO) << "  // bit_depth_to_measure_loudness_= "
            << absl::StrCat(bit_depth_to_measure_loudness_);
}

absl::Status CodecConfigObu::Initialize(
    bool automatically_override_roll_distance) {
  init_status_ = SetSampleRatesAndBitDepths(
      codec_config_.codec_id, codec_config_.decoder_config, output_sample_rate_,
      input_sample_rate_, bit_depth_to_measure_loudness_);

  if (automatically_override_roll_distance) {
    init_status_.Update(OverrideAudioRollDistance(
        codec_config_.codec_id, codec_config_.num_samples_per_frame,
        codec_config_.audio_roll_distance));
  }

  if (!init_status_.ok()) {
    PrintObu();
  }
  return init_status_;
}

absl::Status CodecConfigObu::SetCodecDelay(uint16_t codec_delay) {
  switch (codec_config_.codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
    case kCodecIdFlac:
    case kCodecIdAacLc:
      // Ok the `decoder_config` does not have a field for codec delay.
      return absl::OkStatus();
    case kCodecIdOpus: {
      OpusDecoderConfig* opus_decoder_config =
          std::get_if<OpusDecoderConfig>(&codec_config_.decoder_config);
      if (opus_decoder_config == nullptr) {
        return absl::InvalidArgumentError(
            "OpusDecoderConfig is not set in CodecConfig.");
      }
      opus_decoder_config->pre_skip_ = codec_delay;
      return absl::OkStatus();
    }
  }
  LOG(FATAL) << "Unknown codec_id: " << codec_config_.codec_id;
}

bool CodecConfigObu::IsLossless() const {
  using enum CodecConfig::CodecId;
  return codec_config_.codec_id == kCodecIdFlac ||
         codec_config_.codec_id == kCodecIdLpcm;
}

}  // namespace iamf_tools
