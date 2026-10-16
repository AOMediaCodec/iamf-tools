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
#include "validation/iamf_loudness_comparator/iamf_loudness_comparator.h"

ABSL_FLAG(std::string, file1, "", "First IAMF file");
ABSL_FLAG(std::string, file2, "", "Second IAMF file");
ABSL_FLAG(double, tolerance, 0.1, "Loudness tolerance in LUFS");
ABSL_FLAG(std::string, report_file, "iamf_loudness_report.txt",
          "Path to write the output report");

namespace iamf_loudness_comparator {

// A tool to compare loudness metadata of two IAMF files.
// It parses both files, extracts mix presentations, and compares the loudness
// values (integrated and peak) for corresponding layouts.
// Returns 0 if all comparisons are within tolerance, 1 otherwise.
int Main(int argc, char* argv[]) {
  std::string file1_path = absl::GetFlag(FLAGS_file1);
  std::string file2_path = absl::GetFlag(FLAGS_file2);
  double tolerance = absl::GetFlag(FLAGS_tolerance);
  std::string report_file_path = absl::GetFlag(FLAGS_report_file);

  if (file1_path.empty() || file2_path.empty()) {
    ABSL_LOG(FATAL) << "Both --file1 and --file2 must be specified.";
    return 1;
  }

  std::ofstream file_stream(report_file_path);
  if (!file_stream.is_open()) {
    ABSL_LOG(FATAL) << "Failed to open report file: " << report_file_path;
    return 1;
  }
  std::ostream* out = &file_stream;

  auto obus1 = ParseIamfFile(file1_path);
  if (!obus1.ok()) {
    ABSL_LOG(FATAL) << "Error parsing file1: " << obus1.status();
    return 1;
  }

  auto obus2 = ParseIamfFile(file2_path);
  if (!obus2.ok()) {
    ABSL_LOG(FATAL) << "Error parsing file2: " << obus2.status();
    return 1;
  }

  *out << "File 1: " << file1_path << "\n";
  *out << "File 2: " << file2_path << "\n";
  *out << "Tolerance Threshold: " << tolerance << " LUFS\n\n";

  auto result = CompareLoudness(*obus1, *obus2, tolerance);
  *out << result.report;

  if (result.all_match) {
    *out << "\nOverall Result: Within Tolerance\n";
  } else {
    *out << "\nOverall Result: Out of Tolerance\n";
  }

  // Log a summary to the terminal output.
  if (result.all_match) {
    ABSL_LOG(INFO) << "Overall Result: Within Tolerance";
  } else {
    ABSL_LOG(INFO) << "Overall Result: Out of Tolerance";
  }
  ABSL_LOG(INFO) << "Detailed report saved to: " << report_file_path;

  return result.all_match ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace iamf_loudness_comparator

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(argv[0]);
  absl::ParseCommandLine(argc, argv);
  return iamf_loudness_comparator::Main(argc, argv);
}
