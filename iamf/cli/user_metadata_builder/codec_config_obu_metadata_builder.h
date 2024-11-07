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

#ifndef CLI_USER_METADATA_BUILDER_CODEC_CONFIG_OBU_BUILDER_H_
#define CLI_USER_METADATA_BUILDER_CODEC_CONFIG_OBU_BUILDER_H_

#include <cstdint>

#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*\brief Static functions to get `CodecConfigObuMetadata`s.
 *
 * Useful for generating `CodecConfigObuMetadata` with reasonable defaults.
 */
class CodecConfigObuMetadataBuilder {
 public:
  /*!\brief Gets a `CodecConfigObuMetadata` for LPCM.
   *
   * \param codec_config_id Codec config id.
   * \param num_samples_per_frame Number of samples per frame.
   * \param sample_size Sample size.
   * \param sample_rate Sample rate.
   * \return Codec config metadata.
   */
  static iamf_tools_cli_proto::CodecConfigObuMetadata
  GetLpcmCodecConfigObuMetadata(DecodedUleb128 codec_config_id,
                                uint32_t num_samples_per_frame,
                                uint8_t sample_size, uint32_t sample_rate);

  /*!\brief Gets a `CodecConfigObuMetadata` for Opus.
   *
   * \param codec_config_id Codec config id.
   * \param num_samples_per_frame Number of samples per frame.
   * \return Codec config metadata.
   */
  static iamf_tools_cli_proto::CodecConfigObuMetadata
  GetOpusCodecConfigObuMetadata(DecodedUleb128 codec_config_id,
                                uint32_t num_samples_per_frame);
};

}  // namespace iamf_tools

#endif  // CLI_USER_METADATA_BUILDER_CODEC_CONFIG_OBU_BUILDER_H_
