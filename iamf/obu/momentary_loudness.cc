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

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<MomentaryLoudness> MomentaryLoudness::CreateFromBuffer(
    ReadBitBuffer& rb) {
  MomentaryLoudnessParamDefinition momentary_loudness_param_def(
      ParamDefinition::BaseArgs{});
  RETURN_IF_NOT_OK(momentary_loudness_param_def.ReadAndValidate(rb));

  uint8_t num_bin_pairs_minus_one;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, num_bin_pairs_minus_one));
  uint8_t bin_width_minus_one;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, bin_width_minus_one));
  uint8_t first_bin_center;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, first_bin_center));
  uint8_t reserved_momentary_loudness;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(1, reserved_momentary_loudness));

  const size_t num_bins =
      (static_cast<size_t>(num_bin_pairs_minus_one) + 1) * 2;
  std::vector<DecodedUleb128> count(num_bins);
  for (size_t i = 0; i < num_bins; ++i) {
    RETURN_IF_NOT_OK(rb.ReadULeb128(count[i]));
  }

  return Create(momentary_loudness_param_def, bin_width_minus_one,
                first_bin_center, reserved_momentary_loudness, count);
}

absl::StatusOr<MomentaryLoudness> MomentaryLoudness::Create(
    const MomentaryLoudnessParamDefinition& momentary_loudness_param_def,
    uint8_t bin_width_minus_one, uint8_t first_bin_center,
    uint8_t reserved_momentary_loudness,
    absl::Span<const DecodedUleb128> count) {
  RETURN_IF_NOT_OK(momentary_loudness_param_def.Validate());

  constexpr uint8_t kMaxBinWidthMinusOne = 0b111111;
  constexpr uint8_t kMaxFirstBinCenter = 0b111111;
  constexpr uint8_t kMaxReservedMomentaryLoudness = 0b1;
  constexpr size_t kMinCountSize = 2;
  constexpr size_t kMaxCountSize = 16;

  RETURN_IF_NOT_OK(ValidateInRange(bin_width_minus_one,
                                   {uint8_t{0}, kMaxBinWidthMinusOne},
                                   "bin_width_minus_one"));
  RETURN_IF_NOT_OK(ValidateInRange(
      first_bin_center, {uint8_t{0}, kMaxFirstBinCenter}, "first_bin_center"));
  RETURN_IF_NOT_OK(ValidateInRange(reserved_momentary_loudness,
                                   {uint8_t{0}, kMaxReservedMomentaryLoudness},
                                   "reserved_momentary_loudness"));
  RETURN_IF_NOT_OK(ValidateInRange(count.size(), {kMinCountSize, kMaxCountSize},
                                   "count.size()"));
  RETURN_IF_NOT_OK(
      ValidateEqual(count.size() % 2, size_t{0}, "count.size() % 2"));

  return MomentaryLoudness(
      momentary_loudness_param_def, bin_width_minus_one, first_bin_center,
      reserved_momentary_loudness,
      std::vector<DecodedUleb128>(count.begin(), count.end()));
}

absl::Status MomentaryLoudness::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(momentary_loudness_param_def_.ValidateAndWrite(wb));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(num_bin_pairs_minus_one(), 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(bin_width_minus_one_, 6));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(first_bin_center_, 6));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_momentary_loudness_, 1));
  for (const DecodedUleb128 count_value : count_) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(count_value));
  }
  return absl::OkStatus();
}

std::string MomentaryLoudness::ToString() const {
  std::string result = absl::StrCat(
      momentary_loudness_param_def_, "\n",
      "num_bin_pairs_minus_one= ", num_bin_pairs_minus_one(), "\n",
      "bin_width_minus_one= ", bin_width_minus_one_, "\n",
      "first_bin_center= ", first_bin_center_, "\n",
      "reserved_momentary_loudness= ", reserved_momentary_loudness_);
  for (size_t i = 0; i < count_.size(); ++i) {
    absl::StrAppend(&result, "\ncount[", i, "]= ", count_[i]);
  }
  return result;
}

}  // namespace iamf_tools
