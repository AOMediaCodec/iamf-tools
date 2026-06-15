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
#include "validation/opus_hoa/opus_hoa.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/common/read_bit_buffer.h"

namespace iamf_tools {
namespace opus_hoa {
namespace {

absl::StatusOr<DescriptorObus> ParseIamfFile(const std::string& file_path) {
  constexpr int64_t kBufferCapacity = 1024 * 1024;  // 1 MB buffer capacity.
  auto read_bit_buffer =
      FileBasedReadBitBuffer::CreateFromFilePath(kBufferCapacity, file_path);

  if (read_bit_buffer == nullptr) {
    return absl::InternalError(
        "Failed to open FileBasedReadBitBuffer for path: " + file_path);
  }

  bool insufficient_data = false;
  auto parsed_obus = DescriptorObuParser::ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/false, *read_bit_buffer, insufficient_data);

  if (!parsed_obus.ok()) {
    return parsed_obus.status();
  }

  if (insufficient_data) {
    return absl::InvalidArgumentError(
        "Insufficient bitstream data when parsing descriptor OBUs in file: " +
        file_path);
  }

  return *std::move(parsed_obus);
}

absl::StatusOr<std::vector<AudioElementVerificationResult>> VerifyAudioElements(
    const DescriptorObus& obus) {
  return std::vector<AudioElementVerificationResult>{};
}

}  // namespace

absl::StatusOr<std::vector<AudioElementVerificationResult>>
VerifyOpusAmbisonics(const std::string& file_path) {
  auto obus = ParseIamfFile(file_path);
  if (!obus.ok()) {
    return obus.status();
  }
  return VerifyAudioElements(*obus);
}

VerificationReport GenerateVerificationReport(
    const std::vector<AudioElementVerificationResult>& results) {
  VerificationReport report;
  report.all_canonical = true;
  std::stringstream ss;

  for (const auto& result : results) {
    if (result.status != VerificationStatus::kCanonical) {
      report.all_canonical = false;
    }

    ss << "[Audio Element ID: " << result.audio_element_id << "] Status: ";
    switch (result.status) {
      case VerificationStatus::kCanonical:
        ss << "CANONICAL (" << result.order << "OA)\n";
        break;
      case VerificationStatus::kCustom:
        ss << "CUSTOM (" << result.order << "OA)\n"
           << "  Rationale: " << result.custom_rationale << "\n";
        break;
      case VerificationStatus::kInvalidOrNonOpus:
        ss << "INVALID OR NON-OPUS (skipping)\n"
           << "  Details: " << result.custom_rationale << "\n";
        break;
    }
  }

  if (results.empty()) {
    report.overall_summary = "Result: No Opus Ambisonics Audio Elements found.";
  } else if (report.all_canonical) {
    report.overall_summary = "Result: All Canonical";
  } else {
    report.overall_summary = "Result: Non-Canonical Elements Detected";
  }

  ss << "\n" << report.overall_summary << "\n";
  report.report = ss.str();
  return report;
}

}  // namespace opus_hoa
}  // namespace iamf_tools
