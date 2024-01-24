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
#ifndef OBU_HEADER_H_
#define OBU_HEADER_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

struct ObuHeader {
  friend bool operator==(const ObuHeader& lhs, const ObuHeader& rhs) = default;

  /*!\brief Validates and writes an `ObuHeader`.
   *
   * \param obu_type `obu_type` of the output OBU.
   * \param payload_serialized_size `payload_serialized_size` of the output OBU.
   *     The value MUST be able to be cast to `uint32_t` without losing data.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *     if the fields are invalid or inconsistent or if writing the `Leb128`
   *     representation of `obu_size` fails. `absl::InvalidArgumentError()` if
   *     fields are set inconsistent with the IAMF specification or if the
   *     calculated `obu_size_` larger than IAMF limitations. Or a specific
   *     status if the write fails.
   */
  absl::Status ValidateAndWrite(ObuType obu_type,
                                int64_t payload_serialized_size,
                                WriteBitBuffer& wb) const;
  /*!\brief Prints logging information about an `ObuHeader`.
   *
   * \param leb_generator `LebGenerator` to use when calculating `obu_size_`.
   * \param obu_type `obu_type` of the output OBU.
   * \param payload_serialized_size `payload_serialized_size` of the output OBU.
   *     The value MUST be able to be cast to `uint32_t` without losing data.
   */
  void Print(const LebGenerator& leb_generator, ObuType obu_type,
             int64_t payload_serialized_size) const;

  bool obu_redundant_copy = false;
  bool obu_trimming_status_flag = false;
  bool obu_extension_flag = false;
  DecodedUleb128 num_samples_to_trim_at_end = 0;
  DecodedUleb128 num_samples_to_trim_at_start = 0;
  DecodedUleb128 extension_header_size = 0;
  std::vector<uint8_t> extension_header_bytes = {};
};

}  // namespace iamf_tools

#endif  // OBU_HEADER_H_
