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

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <istream>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/cli/adm_to_user_metadata/adm/wav_file_splicer.h"
#include "iamf/cli/adm_to_user_metadata/iamf/user_metadata_generator.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
absl::StatusOr<iamf_tools_cli_proto::UserMetadata>
GenerateUserMetadataAndSpliceWavFiles(absl::string_view file_prefix,
                                      int32_t frame_duration_ms,
                                      int32_t input_importance_threshold,
                                      const std::filesystem::path& output_path,
                                      std::istream& adm_file) {
  // Parse the input ADM BWF file.
  const auto& reader =
      iamf_tools::adm_to_user_metadata::Bw64Reader::BuildFromStream(
          std::clamp(input_importance_threshold, 0, 10), adm_file);
  if (!reader.ok()) {
    return reader.status();
  }

  // Write output ".wav" file(s).
  if (const auto status =
          iamf_tools::adm_to_user_metadata::SpliceWavFilesFromAdm(
              output_path, file_prefix, *reader, adm_file);
      !status.ok()) {
    return status;
  }

  // Generate the user metadata.
  const auto& user_metadata_generator =
      iamf_tools::adm_to_user_metadata::UserMetadataGenerator(
          reader->adm_, reader->format_info_, frame_duration_ms);

  return user_metadata_generator.GenerateUserMetadata(file_prefix);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
