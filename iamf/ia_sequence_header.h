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
#ifndef IA_SEQUENCE_HEADER_H_
#define IA_SEQUENCE_HEADER_H_

#include <cstdint>

#include "absl/status/status.h"
#include "iamf/ia.h"
#include "iamf/obu_base.h"
#include "iamf/obu_header.h"
#include "iamf/read_bit_buffer.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

class IASequenceHeaderObu : public ObuBase {
 public:
  /*!\brief The spec requires the `ia_code` field to be: "iamf".
   *
   * This four-character code (4CC) is used to determine the start of an IA
   * Sequence.
   */
  static constexpr uint32_t kIaCode = 0x69616d66;  // "iamf".

  /*!\brief Constructor. */
  IASequenceHeaderObu(const ObuHeader& header, uint32_t ia_code,
                      ProfileVersion primary_profile,
                      ProfileVersion additional_profile)
      : ObuBase(header, kObuIaSequenceHeader),
        ia_code_(ia_code),
        primary_profile_(primary_profile),
        additional_profile_(additional_profile) {}

  /*\!brief Move constructor.*/
  IASequenceHeaderObu(IASequenceHeaderObu&& other) = default;

  /*!\brief Destructor. */
  ~IASequenceHeaderObu() override = default;

  friend bool operator==(const IASequenceHeaderObu& lhs,
                         const IASequenceHeaderObu& rhs) = default;

  /*\!brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  const uint32_t ia_code_;
  const ProfileVersion primary_profile_;
  const ProfileVersion additional_profile_;

 private:
  /*\!brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if OBU is valid. A specific status on
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*\!brief Reads the OBU payload from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndReadPayload(ReadBitBuffer& rb) override;
};
}  // namespace iamf_tools

#endif  // IA_SEQUENCE_HEADER_H_
