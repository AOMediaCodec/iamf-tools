/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef OBU_TESTS_OBU_TEST_UTILS_H_
#define OBU_TESTS_OBU_TEST_UTILS_H_

#include <cstdint>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

/*!\brief A mock OBU. */
class MockObu : public ObuBase {
 public:
  /*!\brief Constructor.
   *
   * \param header OBU header.
   * \param obu_type OBU type.
   */
  MockObu(const ObuHeader& header, ObuType obu_type)
      : ObuBase(header, obu_type) {}

  MOCK_METHOD(void, PrintObu, (), (const, override));

  MOCK_METHOD(absl::Status, ValidateAndWritePayload, (WriteBitBuffer & wb),
              (const, override));

  MOCK_METHOD(absl::Status, ReadAndValidatePayloadDerived,
              (int64_t payload_size, ReadBitBuffer& rb), (override));
};

}  // namespace iamf_tools
#endif  // OBU_TESTS_OBU_TEST_UTILS_H_
