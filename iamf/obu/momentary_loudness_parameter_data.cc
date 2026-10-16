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

#include "iamf/obu/momentary_loudness_parameter_data.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

absl::StatusOr<MomentaryLoudnessParameterData>
MomentaryLoudnessParameterData::CreateFromBuffer(ReadBitBuffer& /*rb*/) {
  return absl::UnimplementedError(
      "MomentaryLoudnessParameterData::CreateFromBuffer is not implemented.");
}

absl::StatusOr<MomentaryLoudnessParameterData>
MomentaryLoudnessParameterData::Create(uint8_t /*momentary_loudness*/,
                                       uint8_t /*reserved*/) {
  return absl::UnimplementedError(
      "MomentaryLoudnessParameterData::Create is not implemented.");
}

absl::Status MomentaryLoudnessParameterData::Write(
    WriteBitBuffer& /*wb*/) const {
  return absl::UnimplementedError(
      "MomentaryLoudnessParameterData::Write is not implemented.");
}

std::string MomentaryLoudnessParameterData::ToString() const { return ""; }

}  // namespace iamf_tools
