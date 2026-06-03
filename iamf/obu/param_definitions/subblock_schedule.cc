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
#include "iamf/obu/param_definitions/subblock_schedule.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

// TODO(b/345799072): Determine how `GetDuration`,
//     `GetConstantSubblockDuration`, and `GetSubblockDurations` should behave
//     when the schedule represents unset states, or when special/optional
//     fields are not present.

SubblockSchedule::SubblockSchedule(
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration,
    DecodedUleb128 num_subblocks,
    absl::Span<const DecodedUleb128> subblock_durations)
    : duration_(duration),
      constant_subblock_duration_(constant_subblock_duration),
      num_subblocks_(num_subblocks),
      subblock_durations_(subblock_durations.begin(),
                          subblock_durations.end()) {}

absl::StatusOr<SubblockSchedule> SubblockSchedule::CreateFromBuffer(
    ReadBitBuffer& rb) {
  DecodedUleb128 duration;
  DecodedUleb128 constant_subblock_duration;
  RETURN_IF_NOT_OK(rb.ReadULeb128(duration));
  RETURN_IF_NOT_OK(rb.ReadULeb128(constant_subblock_duration));

  if (constant_subblock_duration != 0) {
    return CreateWithConstantSubblockDuration(duration,
                                              constant_subblock_duration);
  }

  DecodedUleb128 num_subblocks;
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_subblocks));
  if (num_subblocks > SubblockSchedule::kMaxNumSubblocks) {
    return absl::InvalidArgumentError(
        absl::StrCat("num_subblocks= ", num_subblocks, " exceeds maximum."));
  }
  std::vector<DecodedUleb128> subblock_durations;
  subblock_durations.reserve(num_subblocks);
  for (int i = 0; i < num_subblocks; ++i) {
    DecodedUleb128 subblock_duration;
    RETURN_IF_NOT_OK(rb.ReadULeb128(subblock_duration));
    subblock_durations.push_back(subblock_duration);
  }
  return CreateWithVariableSubblockDuration(subblock_durations);
}

absl::Status SubblockSchedule::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(duration_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(constant_subblock_duration_));
  if (constant_subblock_duration_ != 0) {
    return absl::OkStatus();
  }

  RETURN_IF_NOT_OK(wb.WriteUleb128(num_subblocks_));
  for (int i = 0; i < num_subblocks_; ++i) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(subblock_durations_[i]));
  }
  return absl::OkStatus();
}

absl::StatusOr<SubblockSchedule>
SubblockSchedule::CreateWithConstantSubblockDuration(
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration) {
  if (duration == 0) {
    return absl::InvalidArgumentError("Duration should not be zero.");
  }
  if (constant_subblock_duration == 0) {
    return absl::InvalidArgumentError(
        "Constant subblock duration should not be zero.");
  }
  if (constant_subblock_duration > duration) {
    return absl::InvalidArgumentError(
        "Constant subblock duration should not be greater than duration.");
  }

  // Get the implicit value of `num_subblocks` using `ceil(duration /
  // constant_subblock_duration)`. Intentionally use integer division and ceil
  // based on the remainder to avoid floating point math.
  DecodedUleb128 num_subblocks = duration / constant_subblock_duration;
  if (duration % constant_subblock_duration != 0) {
    num_subblocks += 1;
  }

  return SubblockSchedule(duration, constant_subblock_duration, num_subblocks,
                          {});
}

absl::StatusOr<SubblockSchedule>
SubblockSchedule::CreateWithVariableSubblockDuration(
    absl::Span<const DecodedUleb128> subblock_durations) {
  const DecodedUleb128 num_subblocks = subblock_durations.size();
  RETURN_IF_NOT_OK(ValidateInRange(
      num_subblocks, {DecodedUleb128{1}, kMaxNumSubblocks}, "num_subblocks"));

  uint32_t total_subblock_durations = 0;
  for (DecodedUleb128 i = 0; i < num_subblocks; i++) {
    if (subblock_durations[i] == 0) {
      // No individual subblock duration can be zero, and because there must be
      // at least one subblock, the total duration is non-zero and.
      return absl::InvalidArgumentError(
          absl::StrCat("Illegal zero duration for subblock[", i, "]."));
    }

    RETURN_IF_NOT_OK(AddUint32CheckOverflow(total_subblock_durations,
                                            subblock_durations[i],
                                            total_subblock_durations));
  }

  return SubblockSchedule(total_subblock_durations, 0, num_subblocks,
                          subblock_durations);
}

void SubblockSchedule::Print() const {
  ABSL_LOG(INFO) << "  duration= " << duration_;
  ABSL_LOG(INFO) << "  constant_subblock_duration= "
                 << constant_subblock_duration_;
  ABSL_LOG(INFO) << "  num_subblocks= " << num_subblocks_;

  if (constant_subblock_duration_ == 0) {
    for (size_t i = 0; i < num_subblocks_; i++) {
      ABSL_LOG(INFO) << "  subblock_durations[" << i
                     << "]= " << subblock_durations_[i];
    }
  }
}

}  // namespace iamf_tools
