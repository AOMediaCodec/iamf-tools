/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_PARAM_DEFINITIONS_SUBBLOCK_SCHEDULE_H_
#define OBU_PARAM_DEFINITIONS_SUBBLOCK_SCHEDULE_H_

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A subblock schedule for IAMF parameters.
 *
 * Encodes the durations of subblocks in a parameter definition. The subblock
 * schedule has multiple modes which affect how durations are specified.
 *
 * Class invariant:
 *   - Maintains the invariants for the timing information in section 3.6.1 of
 *       the IAMF spec.
 *     - Various getters return the serialized value in the spec when called,
 *       or zero or empty contains when not set.
 *     - `subblock_durations` is empty if and only if
 *       `constant_subblock_duration` is non-zero.
 *     - When `constant_subblock_duration` is less than or equal to `duration`.
 *     - The total duration is non-zero and consistent with the sum of the
 *     subblock durations (where applicable).
 *     - All subblocks have a non-zero duration.
 */
class SubblockSchedule {
 public:
  /*!\brief Static limit on num_subblocks prevents OOMs from implausible values.
   *
   * The maximum sample rate is 192000 Hz and maximum duration is 1 second.
   * Therefore the theoretical maximum number of subblocks is 192000.
   */
  static constexpr DecodedUleb128 kMaxNumSubblocks = 192000;

  friend bool operator==(const SubblockSchedule& lhs,
                         const SubblockSchedule& rhs) = default;

  /*!\brief Factory function to create a validated SubblockSchedule with
   * constant subblock duration.
   *
   * \param duration Total duration of the schedule.
   * \param constant_subblock_duration Constant subblock duration. Must not be
   * 0.
   * \return Validated SubblockSchedule or error status.
   */
  static absl::StatusOr<SubblockSchedule> CreateWithConstantSubblockDuration(
      DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration);

  /*!\brief Factory function to create a validated SubblockSchedule with
   * variable subblock durations.
   *
   * \param subblock_durations Vector of subblock durations.
   * \return Validated SubblockSchedule or error status.
   */
  static absl::StatusOr<SubblockSchedule> CreateWithVariableSubblockDuration(
      absl::Span<const DecodedUleb128> subblock_durations);

  /*!\brief Factory function to create a validated SubblockSchedule from a
   * buffer.
   *
   * \param rb Buffer to read from.
   * \return Validated SubblockSchedule or error status.
   */
  static absl::StatusOr<SubblockSchedule> CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Writes the SubblockSchedule to a buffer.
   *
   * \param wb Buffer to write to.
   * \return Status of the write operation.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Returns the total duration of the schedule.
   *
   * \return Total duration of the schedule.
   */
  DecodedUleb128 GetDuration() const { return duration_; }

  /*!\brief Returns the constant subblock duration of the schedule.
   *
   * \return Constant subblock duration of the schedule, or 0 if the schedule
   *     has variable subblock durations.
   */
  DecodedUleb128 GetConstantSubblockDuration() const {
    return constant_subblock_duration_;
  }

  /*!\brief Returns the number of subblocks in the schedule.
   *
   * \return Number of subblocks in the schedule.
   */
  DecodedUleb128 GetNumSubblocks() const { return num_subblocks_; }

  /*!\brief Returns the vector of subblock durations.
   *
   * \return Vector of subblock durations.
   */
  absl::Span<const DecodedUleb128> GetSubblockDurations() const {
    return absl::MakeConstSpan(subblock_durations_);
  }

  /*!\brief Prints the subblock schedule information. */
  void Print() const;

 private:
  /*!\brief Private constructor.
   *
   * Factory functions that call this should ensure the invariants are
   * maintained.
   *
   * \param duration Total duration of the schedule.
   * \param constant_subblock_duration Constant subblock duration.
   * \param num_subblocks Number of subblocks.
   * \param subblock_durations Vector of subblock durations.
   */
  SubblockSchedule(DecodedUleb128 duration,
                   DecodedUleb128 constant_subblock_duration,
                   DecodedUleb128 num_subblocks,
                   absl::Span<const DecodedUleb128> subblock_durations);

  DecodedUleb128 duration_;
  DecodedUleb128 constant_subblock_duration_;
  DecodedUleb128 num_subblocks_;
  std::vector<DecodedUleb128> subblock_durations_;
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_SUBBLOCK_SCHEDULE_H_
