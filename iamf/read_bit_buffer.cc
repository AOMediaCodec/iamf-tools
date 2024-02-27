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

#include "iamf/read_bit_buffer.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/bit_buffer_util.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"

namespace iamf_tools {

namespace {

bool ShouldRead(const int64_t& source_offset,
                const std::vector<uint8_t>& source,
                const int32_t& remaining_bits_to_read) {
  const bool valid_bit_offset = (source_offset / 8) < source.size();
  const bool bits_to_read = remaining_bits_to_read > 0;
  return valid_bit_offset && bits_to_read;
}

bool CanReadByteAligned(const int64_t& buffer_bit_offset,
                        const int32_t& num_bits) {
  const bool buffer_bit_offset_is_aligned = (buffer_bit_offset % 8 == 0);
  const bool num_bits_to_read_is_aligned = (num_bits % 8 == 0);
  return buffer_bit_offset_is_aligned && num_bits_to_read_is_aligned;
}

// Reads one bit from data at position `offset`. Reads in order of most
// significant to least significant - that is, offset = 0 refers to the bit in
// position 2^7, offset = 1 refers to the bit in position 2^6, etc. Caller
// should ensure that offset/8 is < data.size().
uint8_t GetUpperBit(const int64_t& offset, const std::vector<uint8_t>* data) {
  int64_t byteIndex = offset / 8;
  uint8_t bitIndex = 7 - (offset % 8);
  return (data->at(byteIndex) >> bitIndex) & 0x01;
}

// Read unsigned literal bit by bit. Data is read into the lower
// `remaining_bits_to_read` of `data` from the upper `remaining_bits_to_read` of
// bit_offer[buffer_bit_offset].
//
// Ex: Input: bit_buffer = 10000111, buffer_bit_offset = 0,
//        remaining_bits_to_read = 5, data = 0
//     Output: data = {59 leading zeroes} + 10000, buffer_bit_offset = 5,
//        remaining_bits_to_read = 0.
void ReadUnsignedLiteralBits(int64_t& buffer_bit_offset,
                             const std::vector<uint8_t>& bit_buffer,
                             int& remaining_bits_to_read, uint64_t* data) {
  while (((buffer_bit_offset / 8) < bit_buffer.size()) &&
         remaining_bits_to_read > 0) {
    uint8_t upper_bit = GetUpperBit(buffer_bit_offset, &bit_buffer);
    *data |= (uint64_t)(upper_bit) << (remaining_bits_to_read - 1);
    remaining_bits_to_read--;
    buffer_bit_offset++;
  }
}

// Read unsigned literal byte by byte.
void ReadUnsignedLiteralBytes(int64_t& buffer_bit_offset,
                              const std::vector<uint8_t>& bit_buffer,
                              int& remaining_bits_to_read, uint64_t* data) {
  while (((buffer_bit_offset / 8) < bit_buffer.size()) &&
         remaining_bits_to_read > 0) {
    *data = *data << 8;
    *data |= (uint64_t)(bit_buffer.at(buffer_bit_offset / 8));
    remaining_bits_to_read -= 8;
    buffer_bit_offset += 8;
  }
}

}  // namespace

ReadBitBuffer::ReadBitBuffer(int64_t capacity, std::vector<uint8_t>* source,
                             const LebGenerator& leb_generator)
    : leb_generator_(leb_generator), source_(source) {
  bit_buffer_.reserve(capacity);
}

// Reads n = `num_bits` bits from the buffer. These are the upper n bits of
// `bit_buffer_`. n must be <= 64. The read data is consumed, meaning
// `buffer_bit_offset_` is incremented by n as a side effect of this fxn.
absl::Status ReadBitBuffer::ReadUnsignedLiteral(const int num_bits,
                                                uint64_t* data) {
  if (num_bits > 64) {
    return absl::InvalidArgumentError("num_bits must be <= 64.");
  }
  if (buffer_bit_offset_ < 0) {
    return absl::UnknownError("buffer_bit_offset_ must be >= 0.");
  }
  *data = 0;
  int remaining_bits_to_read = num_bits;
  if (CanReadByteAligned(buffer_bit_offset_, num_bits)) {
    ReadUnsignedLiteralBytes(buffer_bit_offset_, bit_buffer_,
                             remaining_bits_to_read, data);
  } else {
    ReadUnsignedLiteralBits(buffer_bit_offset_, bit_buffer_,
                            remaining_bits_to_read, data);
  }
  if (remaining_bits_to_read != 0) {
    RETURN_IF_NOT_OK(LoadBits(remaining_bits_to_read));
    // Guaranteed to have enough bits to read the unsigned literal at this
    // point.
    if (CanReadByteAligned(buffer_bit_offset_, num_bits)) {
      ReadUnsignedLiteralBytes(buffer_bit_offset_, bit_buffer_,
                               remaining_bits_to_read, data);
    } else {
      ReadUnsignedLiteralBits(buffer_bit_offset_, bit_buffer_,
                              remaining_bits_to_read, data);
    }
  }
  return absl::OkStatus();
}

/*!\brief Reads an unsigned leb128 from buffer into `uleb128`.
 *
 *
 * In accordance with the encoder implementation, this function will consume at
 * most `kMaxLeb128Size` bytes of the read buffer.
 *
 * \param uleb128 Decoded unsigned leb128 from buffer will be written here.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 *     the consumed value from the buffer does not fit into the 32 bits of
 * uleb128, or if the buffer is exhausted before the uleb128 is fully read.
 * `absl::UnknownError()` if the `rb->bit_offset` is negative.
 */
absl::Status ReadBitBuffer::ReadULeb128(DecodedUleb128* uleb128) {
  int64_t original_buffer_bit_offset = buffer_bit_offset_;
  uint64_t accumulated_value = 0;
  uint64_t byte = 0;
  bool terminal_block = false;
  for (int i = 0; i < kMaxLeb128Size; ++i) {
    RETURN_IF_NOT_OK(ReadUnsignedLiteral(8, &byte));
    accumulated_value |= (byte & 0x7f) << (7 * i);
    terminal_block = ((byte & 0x80) == 0);
    if ((i == (kMaxLeb128Size - 1)) && !terminal_block) {
      buffer_bit_offset_ = original_buffer_bit_offset;
      return absl::InvalidArgumentError(
          "Have read the max allowable bytes for a uleb128, but bitstream "
          "says to keep reading.");
    }
    if (accumulated_value > UINT32_MAX) {
      buffer_bit_offset_ = original_buffer_bit_offset;
      return absl::InvalidArgumentError(
          "Overflow - data does not fit into a DecodedUleb128, i.e. a "
          "uint32_t");
    }
    if (terminal_block) {
      break;
    }
  }
  // Accumulated value is guaranteed to fit into a uint_32_t at this stage.
  *uleb128 = static_cast<uint64_t>(accumulated_value);
  return absl::OkStatus();
}

// Loads enough bits from source such that there are at least n =
// `required_num_bits` in `bit_buffer_` after completion. Returns an error if
// there are not enough bits in `source_` to fulfill this request. If `source_`
// contains enough data, this function will fill the read buffer completely.
absl::Status ReadBitBuffer::LoadBits(const int32_t required_num_bits) {
  DiscardAllBits();
  int original_source_offset = source_bit_offset_;
  int remaining_bits_to_load = required_num_bits;
  int64_t bit_buffer_write_offset = 0;
  while (ShouldRead(source_bit_offset_, *source_, remaining_bits_to_load) &&
         (bit_buffer_.size() != bit_buffer_.capacity())) {
    if (remaining_bits_to_load < 8 || source_bit_offset_ % 8 != 0) {
      // Load bit by bit
      uint8_t loaded_bit = GetUpperBit(source_bit_offset_, source_);
      RETURN_IF_NOT_OK(
          CanWriteBits(true, 1, bit_buffer_write_offset, bit_buffer_));
      RETURN_IF_NOT_OK(
          WriteBit(loaded_bit, bit_buffer_write_offset, bit_buffer_));
      bit_buffer_write_offset = bit_buffer_write_offset % 8;
      source_bit_offset_++;
      remaining_bits_to_load--;
    } else {
      // Load byte by byte
      bit_buffer_.push_back(source_->at(source_bit_offset_ / 8));
      source_bit_offset_ += 8;
      remaining_bits_to_load -= 8;
    }
  }
  if (remaining_bits_to_load != 0) {
    source_bit_offset_ = original_source_offset;
    DiscardAllBits();
    return absl::ResourceExhaustedError("Not enough bits in source.");
  }
  return absl::OkStatus();
}

void ReadBitBuffer::DiscardAllBits() {
  buffer_bit_offset_ = 0;
  bit_buffer_.clear();
}

}  // namespace iamf_tools
