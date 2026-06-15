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
#ifndef OPUS_HOA_MAPPING_MATRIX_H_
#define OPUS_HOA_MAPPING_MATRIX_H_

#include <array>
#include <cstdint>

namespace iamf_tools {
namespace opus_hoa {

// External ground-truth Opus Channel Mapping Family 3 demixing reference data
// linked directly from upstream libopus (defined in src/mapping_matrix.c).
// Dimensions include 2 additional channels for non-diegetic stereo:
// - mapping_matrix_toa_demixing_data: Third-Order Ambisonics
//   (3OA, N=3), 18x18 (16 HOA + 2 non-diegetic).
// - mapping_matrix_fourthoa_demixing_data: Fourth-Order Ambisonics
//   (4OA, N=4), 27x27 (25 HOA + 2 non-diegetic).
extern "C" {
extern const int16_t mapping_matrix_toa_demixing_data[324];
extern const int16_t mapping_matrix_fourthoa_demixing_data[729];
}

// Helper function to derive the IAMF Ambisonics projection submatrix (dropping
// non-diegetic stereo head-locked loudspeaker virtual feeds) at runtime.
template <int OpusSize, int IamfSize>
std::array<int16_t, IamfSize * IamfSize> DeriveIamfDemixingMatrix(
    const int16_t* opus_matrix) {
  std::array<int16_t, IamfSize * IamfSize> iamf_matrix = {};
  for (int col = 0; col < IamfSize; ++col) {
    for (int row = 0; row < IamfSize; ++row) {
      iamf_matrix[col * IamfSize + row] = opus_matrix[col * OpusSize + row];
    }
  }
  return iamf_matrix;
}

// IAMF projection reference matrix for 3OA (16 rows x 16 cols).
inline const auto kIamfDemixingMatrix3OA =
    DeriveIamfDemixingMatrix<18, 16>(mapping_matrix_toa_demixing_data);

// IAMF projection reference matrix for 4OA (25 rows x 25 cols).
inline const auto kIamfDemixingMatrix4OA =
    DeriveIamfDemixingMatrix<27, 25>(mapping_matrix_fourthoa_demixing_data);

}  // namespace opus_hoa
}  // namespace iamf_tools

#endif  // OPUS_HOA_MAPPING_MATRIX_H_
