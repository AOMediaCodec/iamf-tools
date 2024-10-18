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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "iamf/cli/adm_to_user_metadata/app/adm_to_user_metadata_main_lib.h"
#include "iamf/cli/adm_to_user_metadata/iamf/user_metadata_generator.h"

// Flags to control the input ADM BWF file.
ABSL_FLAG(std::string, adm_filename, "", "Raw input WAV file in ADM format");
// Flags to control the output user metadata.
ABSL_FLAG(int32_t, importance_threshold, 0,
          "Importance value used to skip an audioObject. Clamped to [0, 10]");
ABSL_FLAG(int32_t, frame_duration_ms, 10, "Frame duration in milliseconds");
// Flags to control output files type and location.
ABSL_FLAG(bool, write_binary_proto, true,
          "Whether to write the output as a binary proto or textproto");
ABSL_FLAG(std::string, output_file_path, "",
          "Path to write output spliced wav files and user metadata to");

int main(int32_t argc, char* argv[]) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);

  const std::string adm_filename(absl::GetFlag(FLAGS_adm_filename));
  if (adm_filename.empty() || !std::filesystem::exists(adm_filename)) {
    LOG(ERROR) << "ADM filename was not provided or could not be opened. "
                  "Please provide a valid filename with --adm_filename.";
    return EXIT_FAILURE;
  }

  // Get the user metadata and write the wav files.
  const std::string file_prefix =
      std::filesystem::path(adm_filename).stem().string();
  const std::filesystem::path output_file_path(
      absl::GetFlag(FLAGS_output_file_path));
  std::ifstream adm_file(adm_filename, std::ios::binary | std::ios::in);

  const auto& user_metadata =
      iamf_tools::adm_to_user_metadata::GenerateUserMetadataAndSpliceWavFiles(
          file_prefix, absl::GetFlag(FLAGS_frame_duration_ms),
          absl::GetFlag(FLAGS_importance_threshold), output_file_path,
          adm_file);

  if (!user_metadata.ok()) {
    LOG(ERROR) << user_metadata.status();
    return user_metadata.status().raw_code();
  }

  // Write the user metadata proto file.
  if (const auto& status =
          iamf_tools::adm_to_user_metadata::UserMetadataGenerator::
              WriteUserMetadataToFile(absl::GetFlag(FLAGS_write_binary_proto),
                                      output_file_path, *user_metadata);
      !status.ok()) {
    LOG(ERROR) << status;
    return status.raw_code();
  }

  return EXIT_SUCCESS;
}
