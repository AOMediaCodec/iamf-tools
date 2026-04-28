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

#include "iamf/cli/renderer/gains/get_gains.h"

#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains_decoder.h"
#include "iamf/common/utils/map_utils.h"

namespace iamf_tools {

absl::StatusOr<std::vector<std::vector<double>>> GetGainsForLayoutPair(
    absl::string_view input_key, absl::string_view output_key) {
  static const absl::NoDestructor<PrecomputedCompressedGains>
      precomputed_compressed_gains(InitPrecomputedCompressedGains());

  const std::string input_key_debug_message =
      absl::StrCat("Precomputed gains not found for input_key= ", input_key);

  // Search through two layers of maps. We want to find the gains associated
  // with `[input_key][output_key]`.
  auto input_key_it = precomputed_compressed_gains->find(input_key);
  if (input_key_it == precomputed_compressed_gains->end()) [[unlikely]] {
    return absl::NotFoundError(input_key_debug_message);
  }

  auto compressed_matrix =
      LookupInMap(input_key_it->second, std::string(output_key),
                  absl::StrCat(input_key_debug_message, " and output_key"));
  if (!compressed_matrix.ok()) {
    return compressed_matrix.status();
  }

  return DecompressMatrix(std::string(input_key), std::string(output_key),
                          *compressed_matrix);
}

}  // namespace iamf_tools
