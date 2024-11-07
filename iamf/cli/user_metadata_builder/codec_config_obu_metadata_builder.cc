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

#include "iamf/cli/user_metadata_builder/codec_config_obu_metadata_builder.h"

#include <cstdint>

#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using ::iamf_tools_cli_proto::CodecConfigObuMetadata;

CodecConfigObuMetadata
CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
    DecodedUleb128 codec_config_id, uint32_t num_samples_per_frame,
    uint8_t sample_size, uint32_t sample_rate) {
  CodecConfigObuMetadata codec_config_obu_metadata;
  codec_config_obu_metadata.set_codec_config_id(codec_config_id);
  auto* codec_config = codec_config_obu_metadata.mutable_codec_config();
  // Set codec id as ipcm.
  codec_config->set_codec_id(iamf_tools_cli_proto::CODEC_ID_LPCM);
  codec_config->set_num_samples_per_frame(num_samples_per_frame);

  auto* decode_config = codec_config->mutable_decoder_config_lpcm();
  decode_config->set_sample_format_flags(
      iamf_tools_cli_proto::LPCM_LITTLE_ENDIAN);
  decode_config->set_sample_size(sample_size);
  decode_config->set_sample_rate(sample_rate);
  return codec_config_obu_metadata;
};

CodecConfigObuMetadata
CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
    DecodedUleb128 codec_config_id, uint32_t num_samples_per_frame) {
  CodecConfigObuMetadata codec_config_obu_metadata;

  constexpr uint8_t kDefaultOpusVersion = 1;
  constexpr uint32_t kDefaultOpusInputSampleRate = 48000;
  constexpr uint32_t kDefaultOpusTargetBitRatePerChannel = 48000;
  constexpr auto kDefaultOpusApplication =
      iamf_tools_cli_proto::APPLICATION_AUDIO;
  constexpr uint32_t kDefaultOpusUseFloatApi = true;

  codec_config_obu_metadata.set_codec_config_id(codec_config_id);
  auto& codec_config = *codec_config_obu_metadata.mutable_codec_config();

  codec_config.set_codec_id(iamf_tools_cli_proto::CODEC_ID_OPUS);
  codec_config.set_num_samples_per_frame(num_samples_per_frame);
  codec_config.set_automatically_override_audio_roll_distance(true);
  codec_config.set_automatically_override_codec_delay(true);
  codec_config.mutable_decoder_config_opus()->set_version(kDefaultOpusVersion);
  codec_config.mutable_decoder_config_opus()->set_input_sample_rate(
      kDefaultOpusInputSampleRate);
  auto& opus_encoder_metadata = *codec_config.mutable_decoder_config_opus()
                                     ->mutable_opus_encoder_metadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(
      kDefaultOpusTargetBitRatePerChannel);
  opus_encoder_metadata.set_application(kDefaultOpusApplication);
  opus_encoder_metadata.set_use_float_api(kDefaultOpusUseFloatApi);
  return codec_config_obu_metadata;
}

}  // namespace iamf_tools
