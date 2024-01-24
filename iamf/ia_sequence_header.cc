/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/ia_sequence_header.h"

#include <cstdint>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/ia.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

absl::Status ValidateProfileVersion(ProfileVersion profile_version) {
  switch (profile_version) {
    case ProfileVersion::kIamfSimpleProfile:
    case ProfileVersion::kIamfBaseProfile:
      return absl::OkStatus();
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unexpected profile_version=", profile_version));
  }
}

absl::Status ValidateIaCode(uint32_t ia_code) {
  // TODO(b/269708630): A different validation logic is needed for decoding.
  // IA Code must equal "iamf" encoded as 4 bytes. If it is any other value then
  // the data may not actually be an IA Sequence, or it may mean the data is
  // corrupt / misaligned.
  if (ia_code != IASequenceHeaderObu::kIaCode) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unexpected ia_code=0x", absl::Hex(ia_code)));
  }

  return absl::OkStatus();
}

void IASequenceHeaderObu::PrintObu() const {
  LOG(INFO) << "IA Sequence Header OBU:";
  LOG(INFO) << "  ia_code= " << ia_code_;
  LOG(INFO) << "  primary_profile= " << static_cast<int>(primary_profile_);
  LOG(INFO) << "  additional_profile= "
            << static_cast<int>(additional_profile_);
}

absl::Status IASequenceHeaderObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateIaCode(ia_code_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(ia_code_, 32));

  // The spec notes that `primary_profile` can be used to determine if the
  // bitstream is backwards compatible with an IA Decoder.
  RETURN_IF_NOT_OK(ValidateProfileVersion(primary_profile_));

  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(static_cast<uint32_t>(primary_profile_), 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(static_cast<uint32_t>(additional_profile_), 8));

  return absl::OkStatus();
}

}  // namespace iamf_tools
