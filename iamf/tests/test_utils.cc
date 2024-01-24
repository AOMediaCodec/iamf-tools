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
#include "iamf/tests/test_utils.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

void ValidateWriteResults(const WriteBitBuffer& wb,
                          const std::vector<uint8_t>& expected_data) {
  // Check that sizes and amount of data written are all consistent.
  EXPECT_EQ(static_cast<int64_t>(expected_data.size()) * 8, wb.bit_offset());

  // Check the data matches expected.
  EXPECT_EQ(wb.bit_buffer(), expected_data);
}

void ValidateObuWriteResults(const WriteBitBuffer& wb,
                             const std::vector<uint8_t>& header,
                             const std::vector<uint8_t>& payload) {
  // Concatenate the header and payload the call `ValidateWriteResults()`.
  std::vector<uint8_t> concatenated;
  concatenated.reserve(header.size() + payload.size());
  concatenated.insert(concatenated.end(), header.begin(), header.end());
  concatenated.insert(concatenated.end(), payload.begin(), payload.end());
  ValidateWriteResults(wb, concatenated);
}

}  // namespace iamf_tools
