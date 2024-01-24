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
#include "iamf/demixing_info_param_data.h"

#include <algorithm>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/ia.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

absl::Status DemixingInfoParameterData::DMixPModeToDownMixingParams(
    const DMixPMode dmixp_mode, const int previous_w_idx,
    const WIdxUpdateRule w_idx_update_rule,
    DownMixingParams& down_mixing_params) {
  static const auto* kDmixPModeToDownMixingParamValues =
      new absl::flat_hash_map<DMixPMode, DownMixingParams>(
          {{kDMixPMode1, {1, 1, 0.707, 0.707, -1, 0}},
           {kDMixPMode2, {0.707, 0.707, 0.707, 0.707, -1, 0}},
           {kDMixPMode3, {1, 0.866, 0.866, 0.866, -1, 0}},
           {kDMixPMode1_n, {1, 1, 0.707, 0.707, 1, 0}},
           {kDMixPMode2_n, {0.707, 0.707, 0.707, 0.707, 1, 0}},
           {kDMixPMode3_n, {1, 0.866, 0.866, 0.866, 1, 0}}});

  static const auto* kWIdxToWValues =
      new absl::flat_hash_map<int, double>({{0, 0},
                                            {1, 0.0179},
                                            {2, 0.0391},
                                            {3, 0.0658},
                                            {4, 0.1038},
                                            {5, 0.25},
                                            {6, 0.3962},
                                            {7, 0.4342},
                                            {8, 0.4609},
                                            {9, 0.4821},
                                            {10, 0.5}});

  const auto down_mixing_params_iter =
      kDmixPModeToDownMixingParamValues->find(dmixp_mode);
  if (down_mixing_params_iter == kDmixPModeToDownMixingParamValues->end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown dmixp_mode: ", dmixp_mode));
  }

  // According to the Spec, normally `wIdx` is updated to be
  // `Clip3(0, 10, wIdx(k - 1) + w_idx_offset(k))`.
  //
  // However, there are two special cases:
  // 1. If it is the first frame, then `wIdx(0) = 0`.
  // 2. If a parameter block is not found, then `default_w` (passed in as
  //    `previous_w_idx`) is used as `wIdx`.
  const int w_idx =
      w_idx_update_rule == DemixingInfoParameterData::kNormal
          ? std::clamp(
                previous_w_idx + down_mixing_params_iter->second.w_idx_offset,
                0, 10)
          : (w_idx_update_rule == DemixingInfoParameterData::kFirstFrame
                 ? 0
                 : previous_w_idx);

  const auto w_idx_iter = kWIdxToWValues->find(w_idx);
  if (w_idx_iter == kWIdxToWValues->end()) {
    return absl::InvalidArgumentError(absl::StrCat("Unknown w_idx: ", w_idx));
  }

  down_mixing_params = down_mixing_params_iter->second;
  down_mixing_params.w = w_idx_iter->second;
  down_mixing_params.w_idx_used = w_idx;
  down_mixing_params.in_bitstream = true;

  return absl::OkStatus();
}

absl::Status DemixingInfoParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(dmixp_mode, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved, 5));

  // Validate that no reserved enums are used.
  switch (dmixp_mode) {
    case kDMixPMode1:
    case kDMixPMode2:
    case kDMixPMode3:
    case kDMixPMode1_n:
    case kDMixPMode2_n:
    case kDMixPMode3_n:
      return absl::OkStatus();
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported dmixp_mode= ", dmixp_mode));
  }
}

void DemixingInfoParameterData::Print() const {
  LOG(INFO) << "  dmixp_mode= " << dmixp_mode;
  LOG(INFO) << "  reserved= " << static_cast<int>(reserved);
}

absl::Status DefaultDemixingInfoParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(DemixingInfoParameterData::Write(wb));

  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(default_w, 4));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved, 4));

  return absl::OkStatus();
}

void DefaultDemixingInfoParameterData::Print() const {
  DemixingInfoParameterData::Print();
  LOG(INFO) << "  default_w= " << static_cast<int>(default_w);
  LOG(INFO) << "  reserved_default= " << static_cast<int>(reserved_default);
}

}  // namespace iamf_tools
