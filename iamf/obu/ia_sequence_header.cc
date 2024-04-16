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
#include "iamf/obu/ia_sequence_header.h"

#include <cstdint>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

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

absl::Status IASequenceHeaderObu::Validate() const {
  // If the IA Code is any other value then the data may not actually be an IA
  // Sequence, or it may mean the data is corrupt / misaligned.
  RETURN_IF_NOT_OK(
      ValidateEqual(ia_code_, IASequenceHeaderObu::kIaCode, "ia_code"));
  RETURN_IF_NOT_OK(ValidateProfileVersion(primary_profile_));
  return absl::OkStatus();
}

absl::StatusOr<IASequenceHeaderObu> IASequenceHeaderObu::CreateFromBuffer(
    const ObuHeader& header, ReadBitBuffer& rb) {
  IASequenceHeaderObu ia_sequence_header_obu(header);
  RETURN_IF_NOT_OK(ia_sequence_header_obu.ValidateAndReadPayload(rb));
  return ia_sequence_header_obu;
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
  RETURN_IF_NOT_OK(Validate());
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(ia_code_, 32));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(static_cast<uint32_t>(primary_profile_), 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(static_cast<uint32_t>(additional_profile_), 8));

  return absl::OkStatus();
}

absl::Status IASequenceHeaderObu::ValidateAndReadPayload(ReadBitBuffer& rb) {
  return absl::UnimplementedError(
      "IASequenceHeaderObu ValidateAndReadPayload not yet implemented.");
}

}  // namespace iamf_tools
