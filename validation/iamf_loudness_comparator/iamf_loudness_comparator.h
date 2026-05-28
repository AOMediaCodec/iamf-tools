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
#ifndef IAMF_LOUDNESS_COMPARATOR_IAMF_LOUDNESS_COMPARATOR_H_
#define IAMF_LOUDNESS_COMPARATOR_IAMF_LOUDNESS_COMPARATOR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace iamf_loudness_comparator {

struct ComparisonResult {
  bool all_match;
  std::string report;
};

/*!\brief Parses an IAMF file and extracts serialized Mix Presentation OBUs.
 *
 * \param file_path Path to the IAMF file.
 * \return A StatusOr containing the serialized OBUs or an error status.
 */
absl::StatusOr<std::vector<uint8_t>> ParseIamfFile(
    const std::string& file_path);

/*!\brief Compares the integrated and peak loudness metadata of two buffers
 * containing serialized Mix Presentation OBUs.
 *
 * All other loudness types are ignored.
 *
 * \param buffer1 Serialized Mix Presentation OBUs from the first file.
 * \param buffer2 Serialized Mix Presentation OBUs from the second file.
 * \param tolerance_lufs Loudness tolerance in LUFS.
 * \return A ComparisonResult containing the match status and detailed report.
 */
ComparisonResult CompareLoudness(absl::Span<const uint8_t> buffer1,
                                 absl::Span<const uint8_t> buffer2,
                                 double tolerance_lufs = 0.1);

}  // namespace iamf_loudness_comparator

#endif  // IAMF_LOUDNESS_COMPARATOR_IAMF_LOUDNESS_COMPARATOR_H_
