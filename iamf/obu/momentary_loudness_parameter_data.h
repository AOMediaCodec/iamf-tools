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

#ifndef OBU_MOMENTARY_LOUDNESS_PARAMETER_DATA_H_
#define OBU_MOMENTARY_LOUDNESS_PARAMETER_DATA_H_

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

/*!\brief Parameter data for momentary loudness (IAMF Spec §3.9.4). */
class MomentaryLoudnessParameterData : public ParameterData {
 public:
  /*!\brief Overridden destructor. */
  ~MomentaryLoudnessParameterData() override = default;

  /*!\brief Creates `MomentaryLoudnessParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `MomentaryLoudnessParameterData` or error.
   */
  static absl::StatusOr<MomentaryLoudnessParameterData> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Creates `MomentaryLoudnessParameterData` from values.
   *
   * \param momentary_loudness 6-bit magnitude of the momentary loudness in
   *        LKFS (actual value is `-1 * momentary_loudness`).
   * \param reserved 2-bit reserved field.
   * \return `MomentaryLoudnessParameterData` or error if fields are out of
   *         bounds.
   */
  static absl::StatusOr<MomentaryLoudnessParameterData> Create(
      uint8_t momentary_loudness, uint8_t reserved);

  bool friend operator==(const MomentaryLoudnessParameterData& lhs,
                         const MomentaryLoudnessParameterData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. Specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Returns string representation of parameter data.
   *
   * \return String representation of parameter data.
   */
  std::string ToString() const override;

  uint8_t momentary_loudness() const { return momentary_loudness_; }
  uint8_t reserved() const { return reserved_; }

 private:
  MomentaryLoudnessParameterData(uint8_t momentary_loudness, uint8_t reserved)
      : momentary_loudness_(momentary_loudness), reserved_(reserved) {}

  uint8_t momentary_loudness_ = 0;  // 6 bits.
  uint8_t reserved_ = 0;            // 2 bits.
};

}  // namespace iamf_tools

#endif  // OBU_MOMENTARY_LOUDNESS_PARAMETER_DATA_H_
