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
#include "validation/iamf_loudness_comparator/iamf_loudness_comparator.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_loudness_comparator {

using iamf_tools::DecodedUleb128;
using iamf_tools::DescriptorObuParser;
using iamf_tools::FileBasedReadBitBuffer;
using iamf_tools::MixPresentationObu;

namespace {

absl::StatusOr<std::vector<MixPresentationObu>> ExtractMixPresentations(
    iamf_tools::ReadBitBuffer& rb, const std::string& buffer_name) {
  std::vector<MixPresentationObu> mix_presentations;
  while (rb.IsDataAvailable()) {
    iamf_tools::ObuHeader header;
    int64_t payload_size = 0;
    auto status = header.ReadAndValidate(rb, payload_size);
    if (!status.ok()) {
      return absl::InternalError("Failed to read header from " + buffer_name +
                                 ": " + std::string(status.message()));
    }
    if (header.obu_type == iamf_tools::kObuIaMixPresentation) {
      auto obu = iamf_tools::MixPresentationObu::CreateFromBuffer(
          header, payload_size, rb);
      if (!obu.ok()) {
        return absl::InternalError(
            "Failed to parse Mix Presentation OBU from " + buffer_name + ": " +
            std::string(obu.status().message()));
      }
      mix_presentations.push_back(*std::move(obu));
    } else {
      auto status = rb.IgnoreBytes(payload_size);
      if (!status.ok()) {
        return absl::InternalError("Failed to ignore bytes in " + buffer_name +
                                   ": " + std::string(status.message()));
      }
    }
  }
  return mix_presentations;
}

}  // namespace

absl::StatusOr<std::vector<uint8_t>> ParseIamfFile(
    const std::string& file_path) {
  constexpr int64_t kBufferCapacity = 1024 * 1024;  // 1 MB buffer.
  auto read_bit_buffer =
      FileBasedReadBitBuffer::CreateFromFilePath(kBufferCapacity, file_path);

  if (read_bit_buffer == nullptr) {
    return absl::InternalError("Failed to create FileBasedReadBitBuffer");
  }

  bool insufficient_data = false;
  auto parsed_obus = DescriptorObuParser::ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/false, *read_bit_buffer, insufficient_data);

  if (!parsed_obus.ok()) {
    return parsed_obus.status();
  }

  if (insufficient_data) {
    return absl::InvalidArgumentError(
        "Insufficient data to parse descriptor OBUs");
  }

  // Serialize only the Mix Presentation OBUs to a buffer.
  iamf_tools::WriteBitBuffer wb(kBufferCapacity);
  for (const auto& obu : parsed_obus->mix_presentation_obus) {
    auto status = obu.ValidateAndWriteObu(wb);
    if (!status.ok()) {
      return status;
    }
  }

  return wb.bit_buffer();
}

ComparisonResult CompareLoudness(absl::Span<const uint8_t> buffer1,
                                 absl::Span<const uint8_t> buffer2,
                                 double tolerance_lufs) {
  std::stringstream ss;
  bool all_match = true;

  if (buffer1.empty() && buffer2.empty()) {
    return {true, "Both buffers are empty\n"};
  }
  if (buffer1.empty() || buffer2.empty()) {
    return {false, "One buffer is empty and the other is not\n"};
  }

  auto rb1 = iamf_tools::MemoryBasedReadBitBuffer::CreateFromSpan(buffer1);
  auto rb2 = iamf_tools::MemoryBasedReadBitBuffer::CreateFromSpan(buffer2);

  auto mix_presentations1 = ExtractMixPresentations(*rb1, "buffer1");
  if (!mix_presentations1.ok()) {
    return {false, std::string(mix_presentations1.status().message())};
  }

  auto mix_presentations2 = ExtractMixPresentations(*rb2, "buffer2");
  if (!mix_presentations2.ok()) {
    return {false, std::string(mix_presentations2.status().message())};
  }

  // Build a map of Mix Presentations in file 2 for efficient lookup by ID.
  absl::flat_hash_map<DecodedUleb128, const MixPresentationObu*> mix_map2;
  for (const auto& obu : *mix_presentations2) {
    mix_map2[obu.GetMixPresentationId()] = &obu;
  }

  // Compare Mix Presentations from file 1 against file 2.
  for (const auto& obu1 : *mix_presentations1) {
    DecodedUleb128 id = obu1.GetMixPresentationId();
    ss << "Comparing Mix Presentation ID: " << id << "\n";

    auto it = mix_map2.find(id);
    if (it == mix_map2.end()) {
      ss << "  Result: Not found in File 2\n";
      all_match = false;
      continue;
    }

    const auto* obu2 = it->second;

    if (obu1.sub_mixes_.size() != obu2->sub_mixes_.size()) {
      ss << "  Result: Sub-mix count mismatch (" << obu1.sub_mixes_.size()
         << " vs " << obu2->sub_mixes_.size() << ")\n";
      all_match = false;
      continue;
    }

    for (size_t i = 0; i < obu1.sub_mixes_.size(); ++i) {
      const auto& sub_mix1 = obu1.sub_mixes_[i];
      const auto& sub_mix2 = obu2->sub_mixes_[i];

      if (sub_mix1.layouts.size() != sub_mix2.layouts.size()) {
        ss << "  Sub-mix " << i << " Result: Layout count mismatch ("
           << sub_mix1.layouts.size() << " vs " << sub_mix2.layouts.size()
           << ")\n";
        all_match = false;
        continue;
      }

      // Search for a layout in sub_mix2 that matches the loudness_layout of
      // the current layout in sub_mix1, as the order of layouts may differ.
      for (size_t j = 0; j < sub_mix1.layouts.size(); ++j) {
        const auto& layout1 = sub_mix1.layouts[j];
        const iamf_tools::MixPresentationLayout* matching_layout2 = nullptr;
        for (const auto& l2 : sub_mix2.layouts) {
          if (layout1.loudness_layout == l2.loudness_layout) {
            matching_layout2 = &l2;
            break;
          }
        }

        if (matching_layout2 == nullptr) {
          ss << "  Sub-mix " << i << " Layout " << j
             << " Result: Not found in File 2\n";
          all_match = false;
          continue;
        }
        const auto& layout2 = *matching_layout2;

        ss << "  Sub-mix " << i << " Layout " << j << " Result: ";

        double integrated1 = iamf_tools::QFormatOrFloatingPoint::MakeFromQ7_8(
                                 layout1.loudness.integrated_loudness)
                                 .GetFloatingPoint();
        double integrated2 = iamf_tools::QFormatOrFloatingPoint::MakeFromQ7_8(
                                 layout2.loudness.integrated_loudness)
                                 .GetFloatingPoint();
        double peak1 = iamf_tools::QFormatOrFloatingPoint::MakeFromQ7_8(
                           layout1.loudness.digital_peak)
                           .GetFloatingPoint();
        double peak2 = iamf_tools::QFormatOrFloatingPoint::MakeFromQ7_8(
                           layout2.loudness.digital_peak)
                           .GetFloatingPoint();

        double diff_integrated = std::abs(integrated1 - integrated2);
        double diff_peak = std::abs(peak1 - peak2);

        if (diff_integrated > tolerance_lufs || diff_peak > tolerance_lufs) {
          all_match = false;
          ss << "Out of Tolerance\n";
        } else {
          ss << "Within Tolerance\n";
        }
        ss << "    Integrated: " << integrated1 << " vs " << integrated2
           << " (diff: " << diff_integrated << ")\n";
        ss << "    Peak: " << peak1 << " vs " << peak2
           << " (diff: " << diff_peak << ")\n";
      }
    }
  }

  // Check for Mix Presentations in file 2 that are missing in file 1.
  absl::flat_hash_set<DecodedUleb128> ids1;
  for (const auto& obu : *mix_presentations1) {
    ids1.insert(obu.GetMixPresentationId());
  }

  for (const auto& obu : *mix_presentations2) {
    if (!ids1.contains(obu.GetMixPresentationId())) {
      ss << "Comparing Mix Presentation ID: " << obu.GetMixPresentationId()
         << "\n";
      ss << "  Result: Not found in File 1\n";
      all_match = false;
    }
  }

  return {all_match, ss.str()};
}

}  // namespace iamf_loudness_comparator
