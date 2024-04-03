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

#ifndef CLI_ADM_TO_USER_METADATA_ADM_FORMAT_INFO_CHUNK_H_
#define CLI_ADM_TO_USER_METADATA_ADM_FORMAT_INFO_CHUNK_H_

#include <cstdint>

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Represents the format info chunk ("fmt ") of a wav audio file. */
struct FormatInfoChunk {
  uint16_t format_tag;
  uint16_t num_channels;
  uint32_t samples_per_sec;
  uint32_t avg_bytes_per_sec;
  uint16_t block_align;
  uint16_t bits_per_sample;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_ADM_FORMAT_INFO_CHUNK_H_
