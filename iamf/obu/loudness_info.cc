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
#include "iamf/obu/loudness_info.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

absl::StatusOr<LoudnessInfo> LoudnessInfo::CreateFromBuffer(
    ReadBitBuffer& /*rb*/) {
  return absl::UnimplementedError(
      "LoudnessInfo::CreateFromBuffer is not implemented.");
}

absl::Status LoudnessInfo::ValidateAndWrite(WriteBitBuffer& /*wb*/) const {
  return absl::UnimplementedError(
      "LoudnessInfo::ValidateAndWrite is not implemented.");
}

std::string LoudnessInfo::ToString() const { return ""; }

}  // namespace iamf_tools
