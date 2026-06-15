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
#ifndef OPUS_HOA_OPUS_HOA_H_
#define OPUS_HOA_OPUS_HOA_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "iamf/obu/ambisonics_config.h"

namespace iamf_tools {
namespace opus_hoa {

/*!\brief Status of verifying an Audio Element against recommended practices. */
enum class VerificationStatus {
  kCanonical,         // Follows recommended order/mode/matrix.
  kCustom,            // Diverges from standard mode or matrix recommendations.
  kInvalidOrNonOpus,  // Corrupt or not an Opus Ambisonics element.
};

/*!\brief Detailed verification result for an Audio Element. */
struct AudioElementVerificationResult {
  uint32_t audio_element_id;
  VerificationStatus status;
  int order;  // Ambisonics order N (0, 1, 2, 3, 4), or -1 if invalid.
  AmbisonicsConfig::AmbisonicsMode ambisonics_mode;
  std::string custom_rationale;  // Explanation if status != kCanonical.
};

/*!\brief Ingests an IAMF file and verifies all Opus Ambisonics Audio Elements
 * against recommended practices.
 *
 * \param file_path Path to the standalone `.iamf` file.
 * \return A list of verification results for each Opus Ambisonics Audio
 * Element, or a specific absl::Status on failure.
 */
absl::StatusOr<std::vector<AudioElementVerificationResult>>
VerifyOpusAmbisonics(const std::string& file_path);

struct VerificationReport {
  bool all_canonical;
  std::string report;
  std::string overall_summary;
};

VerificationReport GenerateVerificationReport(
    const std::vector<AudioElementVerificationResult>& results);

}  // namespace opus_hoa
}  // namespace iamf_tools

#endif  // OPUS_HOA_OPUS_HOA_H_
