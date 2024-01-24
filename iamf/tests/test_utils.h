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
#ifndef TESTS_TEST_UTILS_H_
#define TESTS_TEST_UTILS_H_

#include <cstdint>
#include <vector>

#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief Validates the byte-aligned buffer matches the expected data.
 *
 * \param wb Buffer to validate.
 * \param expected_data Expected data that was written to the underlying buffer.
 */
void ValidateWriteResults(const WriteBitBuffer& wb,
                          const std::vector<uint8_t>& expected_data);

/*!\brief Validates the buffer matches the expected OBU header and payload.
 *
 * \param wb Buffer to validate.
 * \param expected_header Expected OBU header that was written to the underlying
 *     buffer.
 * \param expected_payload Expected OBU payload data that was written to the
 *     underlying buffer.
 */
void ValidateObuWriteResults(const WriteBitBuffer& wb,
                             const std::vector<uint8_t>& expected_header,
                             const std::vector<uint8_t>& expected_payload);

}  // namespace iamf_tools

#endif  // TESTS_TEST_UTILS_H_
