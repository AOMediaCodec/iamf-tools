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
#ifndef ARBITRARY_OBU_H_
#define ARBITRARY_OBU_H_

#include <cstdint>
#include <list>
#include <vector>

#include "absl/status/status.h"
#include "iamf/ia.h"
#include "iamf/obu_base.h"
#include "iamf/obu_header.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief An arbitrary OBU. */
class ArbitraryObu : public ObuBase {
 public:
  /*!\brief A hook describing how the OBU will be put into the bitstream. */
  enum InsertionHook {
    kInsertionHookBeforeDescriptors,
    kInsertionHookAfterDescriptors,
    kInsertionHookAfterIaSequenceHeader,
  };

  /*!\brief Constructor. */
  ArbitraryObu(ObuType obu_type, const ObuHeader& header,
               const std::vector<uint8_t>& payload,
               InsertionHook insertion_hook)
      : ObuBase(header, obu_type),
        payload_(payload),
        insertion_hook_(insertion_hook) {}

  /*\!brief Move constructor.*/
  ArbitraryObu(ArbitraryObu&& other) = default;

  /*!\brief Destructor. */
  ~ArbitraryObu() override = default;

  friend bool operator==(const ArbitraryObu& lhs,
                         const ArbitraryObu& rhs) = default;

  /*\!brief Writes arbitrary OBUs with the specified hook.
   *
   * \param insertion_hook Hook of OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \param wb Write buffer to write to.
   * \return `absl::OkStatus()` on success. A specific status if
   *     writing any of the OBUs fail.
   */
  static absl::Status WriteObusWithHook(
      InsertionHook insertion_hook,
      const std::list<ArbitraryObu>& arbitrary_obus, WriteBitBuffer& wb);

  /*\!brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  const std::vector<uint8_t> payload_;

  // Metadata.
  const InsertionHook insertion_hook_;

 private:
  /*\!brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;
};

}  // namespace iamf_tools

#endif  // ARBITRARY_OBU_H_
