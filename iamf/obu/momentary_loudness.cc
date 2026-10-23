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
#include "iamf/obu/momentary_loudness.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<MomentaryLoudness> MomentaryLoudness::CreateFromBuffer(
    ReadBitBuffer& /*rb*/) {
  return absl::UnimplementedError(
      "MomentaryLoudness::CreateFromBuffer is not implemented.");
}

absl::StatusOr<MomentaryLoudness> MomentaryLoudness::Create(
    const MomentaryLoudnessParamDefinition& /*momentary_loudness_param_def*/,
    uint8_t /*bin_width_minus_one*/, uint8_t /*first_bin_center*/,
    uint8_t /*reserved_momentary_loudness*/,
    absl::Span<const DecodedUleb128> /*count*/) {
  return absl::UnimplementedError(
      "MomentaryLoudness::Create is not implemented.");
}

absl::Status MomentaryLoudness::Write(WriteBitBuffer& /*wb*/) const {
  return absl::UnimplementedError(
      "MomentaryLoudness::Write is not implemented.");
}

std::string MomentaryLoudness::ToString() const { return ""; }

}  // namespace iamf_tools
