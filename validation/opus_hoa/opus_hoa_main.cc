/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/absl_log.h"
#include "validation/opus_hoa/opus_hoa.h"

ABSL_FLAG(std::string, input, "",
          "Path to the input standalone .iamf bitstream file");
ABSL_FLAG(std::string, report_file, "opus_hoa_report.txt",
          "Path to write the output report");

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);

  std::string input_path = absl::GetFlag(FLAGS_input);
  std::string report_file_path = absl::GetFlag(FLAGS_report_file);

  if (input_path.empty()) {
    ABSL_LOG(FATAL) << "The --input flag is required but missing.";
    return EXIT_FAILURE;
  }

  std::ofstream file_stream(report_file_path);
  if (!file_stream.is_open()) {
    ABSL_LOG(FATAL) << "Failed to open report file: " << report_file_path;
    return EXIT_FAILURE;
  }
  std::ostream* out = &file_stream;

  auto results = iamf_tools::opus_hoa::VerifyOpusAmbisonics(input_path);

  if (!results.ok()) {
    *out << "Failed to verify IAMF bitstream: " << results.status() << "\n";
    ABSL_LOG(FATAL) << "Failed to verify IAMF bitstream: " << results.status();
    return EXIT_FAILURE;
  }

  auto verification_report =
      iamf_tools::opus_hoa::GenerateVerificationReport(*results);
  *out << verification_report.report;

  ABSL_LOG(INFO) << verification_report.overall_summary;
  ABSL_LOG(INFO) << "Detailed report saved to: " << report_file_path;

  return EXIT_SUCCESS;
}
