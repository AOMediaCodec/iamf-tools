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
#include "iamf/bit_buffer_util.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace iamf_tools {

// Checks to see if `bit_buffer` is writeable. If not, will resize the buffer
// accordingly, so long as `allow_resizing` is set to true.
absl::Status CanWriteBits(const bool allow_resizing, const int num_bits,
                          const int64_t bit_offset,
                          std::vector<uint8_t>& bit_buffer) {
  const int64_t size = static_cast<int64_t>(bit_buffer.size());
  if (bit_offset + num_bits <= size * 8) {
    return absl::OkStatus();
  }

  if (!allow_resizing) {
    return absl::ResourceExhaustedError(
        "The buffer does not have enough capacity to write and cannot be "
        "resized.");
  }

  const int64_t required_bytes = ((bit_offset + num_bits) / 8) +
                                 ((bit_offset + num_bits) % 8 == 0 ? 0 : 1);

  // Adjust the size of the buffer.
  bit_buffer.resize(required_bytes, 0);

  return absl::OkStatus();
}

absl::Status CanWriteBytes(const bool allow_resizing, const int num_bytes,
                           const int64_t bit_offset,
                           std::vector<uint8_t>& bit_buffer) {
  return CanWriteBits(allow_resizing, num_bytes * 8, bit_offset, bit_buffer);
}

// Write one bit `bit` to `bit_buffer` using an AND or OR mask. All unwritten
// bits are unchanged. This function is designed to work even if the buffer has
// uninitialized data - this is done by calling CanWriteBit first. Has the side
// effect of incrementing bit_offset if successful.
absl::Status WriteBit(int bit, int64_t& bit_offset,
                      std::vector<uint8_t>& bit_buffer) {
  if (bit_offset < 0) {
    return absl::InvalidArgumentError("The bit offset should not be negative.");
  }
  const int64_t off = bit_offset;
  const int64_t p = off >> 3;
  const int q = 7 - static_cast<int>(off & 0x7);

  if (bit == 0) {
    // AND mask to set the target bit to 0 and leave others unchanged.
    bit_buffer[p] &= ~(1 << q);
  } else {
    // OR mask to set the target bit to 1 and leave others unchanged.
    bit_buffer[p] |= (1 << q);
  }
  bit_offset = off + 1;

  return absl::OkStatus();
}
}  // namespace iamf_tools
