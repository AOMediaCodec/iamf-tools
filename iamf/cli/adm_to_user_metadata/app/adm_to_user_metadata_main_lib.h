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
#ifndef CLI_ADM_TO_USER_METADATA_APP_ADM_TO_USER_METADATA_MAIN_LIB_H_
#define CLI_ADM_TO_USER_METADATA_APP_ADM_TO_USER_METADATA_MAIN_LIB_H_

#include <cstdint>
#include <filesystem>
#include <istream>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Generates user metadata and splices wav files from an ADM stream.
 *
 * \param file_prefix File prefix to use when naming output wav files and in the
 *        output textproto.
 * \param max_frame_duration_ms Maximum frame duration in milliseconds. The
 *        actual frame duration may be shorter due to rounding.
 * \param importance_threshold Threshold for to determine which audio objects to
 * \param output_path Directory to output wav files to.
 * \param input_adm_stream Input stream to process.
 * \param profile_version IAMF output specification version to use for
          textproto generation.
 * \return Proto based on the ADM file or a specific error code on failure.
 */
absl::StatusOr<iamf_tools_cli_proto::UserMetadata>
GenerateUserMetadataAndSpliceWavFiles(absl::string_view file_prefix,
                                      int32_t max_frame_duration_ms,
                                      int32_t input_importance_threshold,
                                      const std::filesystem::path& output_path,
                                      std::istream& input_adm_stream,
                                      ProfileVersion profile_version);

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_APP_ADM_TO_USER_METADATA_MAIN_LIB_H_
