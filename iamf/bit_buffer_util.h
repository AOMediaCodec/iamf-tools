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
#ifndef BIT_BUFFER_UTIL_H_
#define BIT_BUFFER_UTIL_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace iamf_tools {

/*!\brief Confirms that `num_bits` can be written to `bit_buffer`.
 *
 * \param allow_resizing Whether the buffer can be resized if need be.
 * \param num_bits Number of bits we'd like to write.
 * \param bit_offset Bit index representing where we'd like to start writing
 *     within `bit_buffer`.
 * \param bit_buffer Buffer to write to.
 *
 * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
 * the `bit_buffer` does not have space to write `num_bites` and
 * `allow_resizing` is false.
 */
absl::Status CanWriteBits(bool allow_resizing, int num_bits, int64_t bit_offset,
                          std::vector<uint8_t>& bit_buffer);

/*!\brief Confirms that `num_bytes` can be written to `bit_buffer`.
 *
 * \param allow_resizing Whether the buffer can be resized if need be.
 * \param num_bytes Number of bytes we'd like to write.
 * \param bit_offset Bit index representing where we'd like to start writing
 *     within `bit_buffer`.
 * \param bit_buffer Buffer to write to.
 *
 * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
 * the `bit_buffer` does not have space to write `num_bytes` and
 * `allow_resizing` is false.
 */
absl::Status CanWriteBytes(bool allow_resizing, int num_bytes,
                           int64_t bit_offset,
                           std::vector<uint8_t>& bit_buffer);

/*!\brief Write `bit` to `bit_buffer` at position `bit_offset`.
 *
 * \param bit Bit to write.
 * \param bit_offset Index within `bit_buffer` where `bit` should be written to.
 * \param bit_buffer Buffer to write to.
 *
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 * `bit_offset` is negative.
 */
absl::Status WriteBit(int bit, int64_t& bit_offset,
                      std::vector<uint8_t>& bit_buffer);

}  // namespace iamf_tools

#endif  // BIT_BUFFER_UTIL_H_
