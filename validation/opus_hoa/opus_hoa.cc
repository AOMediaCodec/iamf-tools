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
#include "absl/types/span.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "validation/opus_hoa/mapping_matrix.h"

namespace iamf_tools {
namespace opus_hoa {
namespace {

// IAMF Profiles restrict Higher-Order Ambisonics to a maximum order of 4.
constexpr int kMaxAmbisonicsOrder = 4;
// Recommendation is to use MONO mode (0) for orders <= 2.
constexpr int kMaxMonoAmbisonicsOrder = 2;

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
  std::vector<AudioElementVerificationResult> results;

  for (const auto& [id, audio_element_with_data] : obus.audio_elements) {
    const auto& obu = audio_element_with_data.obu;
    if (obu.GetAudioElementType() != AudioElementObu::kAudioElementSceneBased) {
      continue;  // Skip non-Ambisonics elements.
    }

    const auto& ambisonics_config = std::get<AmbisonicsConfig>(obu.config_);
    AudioElementVerificationResult result = {
        .audio_element_id = obu.GetAudioElementId(),
        .status = VerificationStatus::kCanonical,
        .order = -1,
        .ambisonics_mode = ambisonics_config.GetAmbisonicsMode(),
    };

    uint32_t codec_config_id = obu.GetCodecConfigId();
    auto it = obus.codec_config_obus.find(codec_config_id);

    if (it == obus.codec_config_obus.end() ||
        it->second.GetCodecConfig().codec_id != CodecConfig::kCodecIdOpus) {
      result.status = VerificationStatus::kInvalidOrNonOpus;
      result.custom_rationale = (it == obus.codec_config_obus.end())
                                    ? "Missing Codec Config OBU for ID: " +
                                          std::to_string(codec_config_id)
                                    : "Not an Opus Codec Config";
      results.push_back(result);
      continue;
    }

    uint8_t channel_count = ambisonics_config.GetOutputChannelCount();
    for (int n = 0; n <= kMaxAmbisonicsOrder; ++n) {
      if (channel_count == (n + 1) * (n + 1)) {
        result.order = n;
        break;
      }
    }

    if (result.order == -1) {
      result.status = VerificationStatus::kCustom;
      result.custom_rationale =
          "Unsupported output_channel_count: " + std::to_string(channel_count) +
          " (Not 0OA to 4OA)";
      results.push_back(result);
      continue;
    }

    auto recommended_mode = (result.order <= kMaxMonoAmbisonicsOrder)
                                ? AmbisonicsConfig::kAmbisonicsModeMono
                                : AmbisonicsConfig::kAmbisonicsModeProjection;

    if (result.ambisonics_mode != recommended_mode) {
      result.status = VerificationStatus::kCustom;
      std::string mode_str =
          (recommended_mode == AmbisonicsConfig::kAmbisonicsModeMono)
              ? "MONO (0)"
              : "PROJECTION (1)";
      result.custom_rationale =
          "Order " + std::to_string(result.order) +
          " recommended practice is " + mode_str + " mode, but found mode: " +
          std::to_string(static_cast<uint32_t>(result.ambisonics_mode));
    } else if (result.ambisonics_mode ==
               AmbisonicsConfig::kAmbisonicsModeProjection) {
      absl::Span<const int16_t> target_matrix =
          (result.order == 3) ? absl::MakeConstSpan(kIamfDemixingMatrix3OA)
                              : absl::MakeConstSpan(kIamfDemixingMatrix4OA);
      if (ambisonics_config.GetDemixingMatrix() != target_matrix) {
        result.status = VerificationStatus::kCustom;
        result.custom_rationale =
            "Demixing matrix coefficients diverge from Opus Channel Mapping "
            "Family 3 reference.";
      }
    }

    results.push_back(result);
  }

  return results;
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
