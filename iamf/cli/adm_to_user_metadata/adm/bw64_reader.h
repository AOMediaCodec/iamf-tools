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

#ifndef CLI_ADM_TO_USER_METADATA_ADM_BW64_READER_H_
#define CLI_ADM_TO_USER_METADATA_ADM_BW64_READER_H_

#include <cstdint>
#include <istream>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

// This class is used for reading and validating RIFF WAV files, extracting
// chunk information, and storing format details. The class performs RIFF
// header validation, and create maps with chunk names as keys and associated
// offsets (name, size, data). It also creates an object of the FormatInfoChunk
// class to store attributes of the 'fmt' chunk in the WAV file. It also
// provides a method to retrieve the axml (ADM XML) data associated with the WAV
// file.
class Bw64Reader {
 public:
  struct ChunkInfo {
    int32_t size, offset;
  };
  typedef absl::flat_hash_map<std::string, ChunkInfo> ChunksOffsetMap;

  // Each chunk has a 4 character code (e.g. "RIFF", "WAVE", "fmt ", "axml",
  // etc.).
  static constexpr int32_t kChunkNameSize = 4;
  // Each chunk has a 4 byte length.
  static constexpr int32_t kChunkLengthSize = 4;
  // Offset of the chunk name and size.
  static constexpr int32_t kChunkHeaderOffset =
      kChunkNameSize + kChunkLengthSize;

  /*!\brief Builds a `Bw64Reader` from a stream.
   *
   * \param importance_threshold Threshold below which the audio objects will be
   *     ignored.
   * \param buffer Stream to consume. which represents a valid RIFF WAV file
   *     with an `axml` chunk.
   * \return Initialized `Bw64Reader` or a specific error code on failure.
   */
  static absl::StatusOr<Bw64Reader> BuildFromStream(
      int32_t importance_threshold, std::istream& buffer);

  /*!\brief Returns information about a chunk.
   *
   * \chunk_name Chunk name to retrieve.
   * \return Chunk info or a `absl::FailedPreconditionError` error code if the
   *     chunk name is not present.
   */
  absl::StatusOr<ChunkInfo> GetChunkInfo(absl::string_view chunk_name) const;

  /*!\brief Returns the total number of samples per channel.
   *
   * \return Total number of samples or a specific error code if it cannot be
   *     calculated.
   */
  absl::StatusOr<int64_t> GetTotalSamplesPerChannel() const;

  const ADM adm_;
  const FormatInfoChunk format_info_;

 private:
  Bw64Reader(const ADM& adm, const FormatInfoChunk& format_info,
             const ChunksOffsetMap& chunks_offset_map)
      : adm_(adm),
        format_info_(format_info),
        chunks_offset_map_(chunks_offset_map) {};

  const ChunksOffsetMap chunks_offset_map_;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_ADM_BW64_READER_H_
