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

#include <cstdint>

#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/proto/codec_config.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

constexpr uint32_t kCodecConfigId = 0;

// Sets the required textproto fields for codec_config_metadata.
void GenerateLpcmCodecConfigObuMetadata(
    const FormatInfoChunk& format_info, int64_t num_samples_per_frame,
    iamf_tools_cli_proto::CodecConfigObuMetadata& codec_config_obu_metadata) {
  codec_config_obu_metadata.set_codec_config_id(kCodecConfigId);
  auto* codec_config = codec_config_obu_metadata.mutable_codec_config();
  // Set codec id as ipcm.
  codec_config->set_codec_id(iamf_tools_cli_proto::CODEC_ID_LPCM);
  codec_config->set_num_samples_per_frame(num_samples_per_frame);

  auto* decode_config = codec_config->mutable_decoder_config_lpcm();
  decode_config->set_sample_format_flags(
      iamf_tools_cli_proto::LPCM_LITTLE_ENDIAN);
  decode_config->set_sample_size(format_info.bits_per_sample);
  decode_config->set_sample_rate(format_info.samples_per_sec);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
