/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/recon_gain_info_parameter_data.h"

#include <optional>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::Status ReconGainInfoParameterData::ReadAndValidate(ReadBitBuffer& rb) {
  recon_gain_elements.resize(recon_gain_is_present_flags.size());
  for (int i = 0; i < recon_gain_is_present_flags.size(); i++) {
    auto& recon_gain_element = recon_gain_elements[i];

    // Each layer depends on the `recon_gain_is_present_flags` within the
    // associated Audio Element OBU. The size of `recon_gain_is_present_flags`
    // is equal to the number of layers.
    if (!recon_gain_is_present_flags[i]) {
      recon_gain_element = std::nullopt;
      continue;
    }
    recon_gain_element = ReconGainElement();
    RETURN_IF_NOT_OK(rb.ReadULeb128(recon_gain_element->recon_gain_flag));

    const DecodedUleb128 recon_gain_flag = recon_gain_element->recon_gain_flag;
    DecodedUleb128 mask = 1;

    // Apply bitmask to examine each bit in the flag. Only read elements with
    // the flag implying they should be read.
    for (int j = 0; j < recon_gain_element->recon_gain.size(); j++) {
      if (recon_gain_flag & mask) {
        RETURN_IF_NOT_OK(
            rb.ReadUnsignedLiteral(8, recon_gain_element->recon_gain[j]));
      } else {
        recon_gain_element->recon_gain[j] = 0;
      }
      mask <<= 1;
    }
  }
  RETURN_IF_NOT_OK(ValidateEqual(recon_gain_elements.size(),
                                 recon_gain_is_present_flags.size(),
                                 "size of `recon_gain_elements`"));

  return absl::OkStatus();
}

absl::Status ReconGainInfoParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateEqual(recon_gain_elements.size(),
                                 recon_gain_is_present_flags.size(),
                                 "size of `recon_gain_elements`"));

  for (int i = 0; i < recon_gain_is_present_flags.size(); i++) {
    // Each layer depends on the `recon_gain_is_present_flags` within the
    // associated Audio Element OBU.
    if (!recon_gain_is_present_flags[i]) {
      continue;
    }
    RETURN_IF_NOT_OK(ValidateHasValue(
        recon_gain_elements[i], absl::StrCat("recon_gain_elements[", i, "]")));
    const auto& recon_gain_element = *recon_gain_elements[i];
    RETURN_IF_NOT_OK(wb.WriteUleb128(recon_gain_element.recon_gain_flag));

    const DecodedUleb128 recon_gain_flag = recon_gain_element.recon_gain_flag;
    DecodedUleb128 mask = 1;

    // Apply bitmask to examine each bit in the flag. Only write elements with
    // the flag implying they should be written.
    for (const auto& recon_gain : recon_gain_element.recon_gain) {
      if (recon_gain_flag & mask) {
        RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(recon_gain, 8));
      }
      mask <<= 1;
    }
  }

  return absl::OkStatus();
}

void ReconGainInfoParameterData::Print() const {
  LOG(INFO) << "  ReconGainInfoParameterData:";
  for (int l = 0; l < recon_gain_elements.size(); l++) {
    const auto& recon_gain_element = recon_gain_elements[l];
    LOG(INFO) << "    recon_gain_elements[" << l << "]:";
    if (!recon_gain_element.has_value()) {
      LOG(INFO) << "      NONE";
      continue;
    }
    LOG(INFO) << "      recon_gain_flag= "
              << recon_gain_element->recon_gain_flag;
    for (int b = 0; b < recon_gain_element->recon_gain.size(); b++) {
      LOG(INFO) << "      recon_gain[" << b
                << "]= " << absl::StrCat(recon_gain_element->recon_gain[b]);
    }
  }

  LOG(INFO) << "    // recon_gain_is_present_flags: ";
  for (const auto& flag : recon_gain_is_present_flags) {
    LOG(INFO) << "    //   " << absl::StrCat(flag);
  }
}

}  // namespace iamf_tools
