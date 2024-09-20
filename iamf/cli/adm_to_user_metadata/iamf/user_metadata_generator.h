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

#ifndef CLI_ADM_TO_USER_METADATA_IAMF_USER_METADATA_GENERATOR_H_
#define CLI_ADM_TO_USER_METADATA_IAMF_USER_METADATA_GENERATOR_H_

#include <cstdint>
#include <filesystem>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

class UserMetadataGenerator {
 public:
  /*!\brief Writes a `UserMetadata` as a text proto to a file.
   *
   * \param write_binary_proto `true` to write a binary proto, `false` to write
   *     a text proto.
   * \param path Path to write the data to.
   * \param user_metadata User metadata to write. The filename is determined by
   *      the inner `file_name_prefix` field with a suffix of `.binpb` for
   *      binary protos or `.textproto` for text protos.
   * \return `absl::OkStatus()` if the write was successful a specific error
   *      otherwise.
   */
  static absl::Status WriteUserMetadataToFile(
      bool write_binary_proto, const std::filesystem::path& path,
      const iamf_tools_cli_proto::UserMetadata& user_metadata);

  /*!\brief Constructor.
   *
   * \param adm ADM to use.
   * \param format_info Format info chunk to use.
   * \param max_frame_duration_ms Maximum frame duration in milliseconds. The
   *     actual frame duration may be shorter due to rounding.
   */
  UserMetadataGenerator(const ADM& adm, const FormatInfoChunk& format_info,
                        int32_t max_frame_duration)
      : adm_(adm),
        format_info_(format_info),
        max_frame_duration_(max_frame_duration) {};

  /*!\brief Generates a `UserMetadata` proto.
   *
   * \param file_prefix File prefix to use when naming output wav files.
   * \return Proto based on the constructor arguments or a specific error code
   *     on failure.
   */
  absl::StatusOr<iamf_tools_cli_proto::UserMetadata> GenerateUserMetadata(
      absl::string_view file_prefix) const;

 private:
  const ADM& adm_;
  const FormatInfoChunk& format_info_;
  const int32_t max_frame_duration_;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_IAMF_USER_METADATA_GENERATOR_H_
