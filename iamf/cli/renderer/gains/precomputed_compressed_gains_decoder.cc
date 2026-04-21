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

#include "iamf/cli/renderer/gains/precomputed_compressed_gains_decoder.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/renderer/gains/compressed_gains_codebook.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains.h"

namespace iamf_tools {
namespace {

int GetChannelCount(const std::string& layout_name) {
  static const auto* kLayoutMap = new absl::flat_hash_map<std::string, int>{
      {"0+1+0", 1},    {"0+2+0", 2},  {"0+5+0", 6},  {"2+5+0", 8},
      {"4+5+0", 10},   {"0+7+0", 8},  {"4+7+0", 12}, {"9+10+3", 24},
      {"7.1.5.4", 17}, {"3.1.2", 6},  {"7.1.2", 10}, {"9.1.6", 16},
      {"4+5+1", 11},   {"3+7+0", 12}, {"4+9+0", 14}, {"A0", 1},
      {"A1", 4},       {"A2", 9},     {"A3", 16},    {"A4", 25},
  };

  auto layout = kLayoutMap->find(layout_name);
  if (layout != kLayoutMap->end()) {
    return layout->second;
  }
  return 0;
}

}  // namespace

absl::StatusOr<std::vector<std::vector<double>>> DecompressMatrix(
    const std::string& input_layout_name, const std::string& output_layout_name,
    const CompressedMatrix& compressed) {
  int num_input_channels = GetChannelCount(input_layout_name);
  int num_output_channels = GetChannelCount(output_layout_name);

  if (num_input_channels == 0 || num_output_channels == 0) {
    return absl::InvalidArgumentError("Invalid layout name.");
  }

  std::vector<std::vector<double>> decompressed(
      num_input_channels, std::vector<double>(num_output_channels, 0.0));

  if (!compressed.dense_data.empty()) {
    // Dense decompression. Pull float values from flattened vector, cast to
    // doubles and place in matrix in the correct position.
    int idx = 0;
    for (int i = 0; i < num_input_channels; ++i) {
      for (int j = 0; j < num_output_channels; ++j) {
        if (idx < compressed.dense_data.size()) {
          decompressed[i][j] =
              static_cast<double>(compressed.dense_data[idx++]);
        }
      }
    }
  } else if (!compressed.row_ptr.empty()) {
    // Sparse decompression. Pull uint8 values from CSR format, convert them
    // to doubles using codebook, and place in matrix in the correct position.
    for (int i = 0; i < num_input_channels; ++i) {
      if (i + 1 >= compressed.row_ptr.size()) break;
      int start_idx = compressed.row_ptr[i];
      int end_idx = compressed.row_ptr[i + 1];

      for (int k = start_idx; k < end_idx; ++k) {
        if (k >= compressed.col_indices.size() ||
            k >= compressed.sparse_data.size()) {
          break;
        }
        int col = compressed.col_indices[k];
        uint8_t val_idx = compressed.sparse_data[k];

        const std::vector<double>& codebook = iamf_tools::GetCodebook();
        if (col < num_output_channels && val_idx < codebook.size()) {
          decompressed[i][col] = codebook[val_idx];
        }
      }
    }
  }

  return decompressed;
}

}  // namespace iamf_tools
