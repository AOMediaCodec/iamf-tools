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
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/encoder_main_lib.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "src/google/protobuf/text_format.h"

ABSL_FLAG(std::string, user_metadata_filename, "",
          "Filename of the text proto containing user metadata");
ABSL_FLAG(std::string, input_wav_directory, "",
          "Directory containing the input wav files");
ABSL_FLAG(std::string, output_wav_directory, "",
          "Output directory for wav files");
ABSL_FLAG(std::string, output_iamf_directory, "",
          "Output directory for iamf files");

namespace {

bool ReadInputUserMetadata(const std::filesystem::path& user_metadata_filename,
                           iamf_tools_cli_proto::UserMetadata& user_metadata) {
  if (user_metadata_filename.empty()) {
    LOG(ERROR) << "Flag --user_metadata_filename not set.";
    return false;
  }

  std::ifstream user_metadata_file(user_metadata_filename.string());
  if (!user_metadata_file) {
    LOG(ERROR) << "Error loading user_metadata_filename="
               << user_metadata_filename;
    return false;
  }

  std::ostringstream user_metadata_stream;
  user_metadata_stream << user_metadata_file.rdbuf();
  if (!google::protobuf::TextFormat::ParseFromString(user_metadata_stream.str(),
                                                     &user_metadata)) {
    LOG(ERROR) << "Error parsing user_metadata_filename="
               << user_metadata_filename;
    return false;
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  // Read user input from a text proto file.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  if (!::ReadInputUserMetadata(
          std::filesystem::path(absl::GetFlag(FLAGS_user_metadata_filename)),
          user_metadata)) {
    return -1;
  }
  LOG(INFO) << user_metadata;

  // Get the directory of the input wav files.
  const auto& input_wav_directory =
      absl::GetFlag(FLAGS_input_wav_directory).empty()
          ? std::filesystem::path("iamf/cli/testdata/")
          : std::filesystem::path(absl::GetFlag(FLAGS_input_wav_directory));

  // Get the directory for the output wav files.
  const auto& output_wav_directory =
      absl::GetFlag(FLAGS_output_wav_directory).empty()
          ? std::filesystem::temp_directory_path()
          : std::filesystem::path(absl::GetFlag(FLAGS_output_wav_directory));

  const auto& output_iamf_directory =
      absl::GetFlag(FLAGS_output_iamf_directory).empty()
          ? std::filesystem::temp_directory_path()
          : std::filesystem::path(absl::GetFlag(FLAGS_output_iamf_directory));

  absl::Status status =
      iamf_tools::TestMain(user_metadata, input_wav_directory,
                           output_wav_directory, output_iamf_directory);

  // Log success or failure. Success is defined as a valid test vector returning
  // `absl::OkStatus()` or an invalid test vector returning a different status.
  const bool test_vector_is_valid =
      user_metadata.test_vector_metadata().is_valid();
  std::stringstream ss;
  ss << "Test case expected to " << (test_vector_is_valid ? "pass" : "fail")
     << ".\nstatus= " << status;
  if (test_vector_is_valid == status.ok()) {
    LOG(INFO) << "Success. " << ss.str();
  } else {
    LOG(ERROR) << "Failure. " << ss.str();
  }

  return static_cast<int>(status.code());
}
