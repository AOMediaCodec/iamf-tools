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

#include "iamf/read_bit_buffer.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/leb_generator.h"

namespace iamf_tools {

ReadBitBuffer::ReadBitBuffer(int64_t capacity, std::vector<uint8_t>* source,
                             const LebGenerator& leb_generator)
    : leb_generator_(leb_generator), source_(source) {
  bit_buffer_.reserve(capacity);
}

// Reads n = `num_bits` bits from the buffer. These are the lower n bits of
// bit_buffer_. n must be <= 64. The read data is consumed, meaning
// bit_buffer_offset_ is incremented by n as a side effect of this fxn.
absl::Status ReadBitBuffer::ReadUnsignedLiteral(uint64_t* data, int num_bits) {
  return absl::UnimplementedError("Not yet implemented.");
}

// Loads enough bits from source such that there are at least n =
// `required_num_bits` in bit_buffer_ after completion. Returns an error if
// there are not enough bits in source_ to fulfill this request. If source_
// contains enough data, this function will fill the read buffer completely.
absl::Status ReadBitBuffer::LoadBits(const int32_t required_num_bits) {
  return absl::UnimplementedError("Not yet implemented.");
}

absl::Status ReadBitBuffer::DiscardAllBits() {
  bit_buffer_offset_ = 0;
  bit_buffer_.clear();
  return absl::OkStatus();
}

}  // namespace iamf_tools
