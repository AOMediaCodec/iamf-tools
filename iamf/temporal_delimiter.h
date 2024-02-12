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
#ifndef TEMPORAL_DELIMITER_H_
#define TEMPORAL_DELIMITER_H_

#include "absl/status/status.h"
#include "iamf/ia.h"
#include "iamf/obu_base.h"
#include "iamf/obu_header.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

class TemporalDelimiterObu : public ObuBase {
 public:
  /*!\brief Constructor. */
  explicit TemporalDelimiterObu(ObuHeader header)
      : ObuBase(header, kObuIaTemporalDelimiter) {}

  /*\!brief Move constructor.*/
  TemporalDelimiterObu(TemporalDelimiterObu&& other) = default;

  /*!\brief Destructor. */
  ~TemporalDelimiterObu() override = default;

  /*\!brief Prints logging information about the OBU.*/
  void PrintObu() const override {
    // There is nothing to print for a Temporal Delimiter OBU.
  };

  // Temporal delimiter has no payload

 private:
  /*\!brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` always.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& /*wb*/) const override {
    // There is nothing to write for a Temporal Delimiter OBU payload.
    return absl::OkStatus();
  }
};
}  // namespace iamf_tools

#endif  // TEMPORAL_DELIMITER_H_
