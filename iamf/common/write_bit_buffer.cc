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
#include "iamf/common/write_bit_buffer.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/bit_buffer_util.h"
#include "iamf/common/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

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

// Write one bit to the buffer using an AND or OR mask. All unwritten bits are
// unchanged. This function is designed to work even if the buffer has
// uninitialized data.
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

// A helper function to write out n = `num_bits` bits to the buffer. These
// are the lower n bits of uint64_t `data`. n must be <= 64.
absl::Status InternalWriteUnsigned(int max_bits, uint64_t data, int num_bits,
                                   int64_t& bit_offset,
                                   std::vector<uint8_t>& bit_buffer) {
  // The `uint64` input limits this function to only write 64 bits at a time.
  if (max_bits > 64) {
    return absl::InvalidArgumentError("max_bits cannot be greater than 64.");
  }

  // Check the calling function's limitation to guard against unexpected
  // behavior.
  if (num_bits > max_bits) {
    return absl::InvalidArgumentError(
        absl::StrCat("num_bits= ", num_bits,
                     " cannot be greater than max_bits= ", max_bits, "."));
  }

  // Check if there would be any non-zero bits left after writing. Avoid
  // shifting by 64 which results in undefined behavior. This is not possible
  // when writing out 64 bits.
  if (num_bits != 64 && (data >> num_bits) != 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("There is more bits of data in the provided uint64 "
                     "than requested for writing.  num_bits= ",
                     num_bits, " data= ", data));
  }

  // Expand the buffer and pad the input data with zeroes.
  RETURN_IF_NOT_OK(CanWriteBits(true, num_bits, bit_offset, bit_buffer));

  if (bit_offset % 8 == 0 && num_bits % 8 == 0) {
    // Short-circuit the common case of writing a byte-aligned input to a
    // byte-aligned output. Copy one byte at a time.
    for (int byte = (num_bits / 8) - 1; byte >= 0; byte--) {
      bit_buffer[bit_offset / 8] = (data >> (byte * 8)) & 0xff;
      bit_offset += 8;
    }
  } else {
    // The input and/or output are not byte-aligned. Write one bit at
    // a time.
    for (int bit = num_bits - 1; bit >= 0; bit--) {
      RETURN_IF_NOT_OK(WriteBit((data >> bit) & 1, bit_offset, bit_buffer));
    }
  }

  return absl::OkStatus();
}

absl::Status WriteBufferToFile(const std::vector<uint8_t>& buffer,
                               std::fstream& output_file) {
  if (!output_file.is_open()) {
    return absl::UnknownError("Expected file to be opened.");
  }
  output_file.write(reinterpret_cast<const char*>(buffer.data()),
                    buffer.size());

  if (output_file.bad()) {
    return absl::UnknownError("Writing to file failed.");
  }

  return absl::OkStatus();
}

}  // namespace

WriteBitBuffer::WriteBitBuffer(int64_t initial_capacity,
                               const LebGenerator& leb_generator)
    : leb_generator_(leb_generator), bit_buffer_(), bit_offset_(0) {
  bit_buffer_.reserve(initial_capacity);
}

// Writes n = `num_bits` bits to the buffer. These are the lower n bits of
// uint32_t `data`. n must be <= 32.
absl::Status WriteBitBuffer::WriteUnsignedLiteral(uint32_t data, int num_bits) {
  return InternalWriteUnsigned(32, static_cast<uint64_t>(data), num_bits,
                               bit_offset_, bit_buffer_);
}

// Writes n = `num_bits` bits to the buffer. These are the lower n bits of
// uint64_t `data`. n must be <= 64.
absl::Status WriteBitBuffer::WriteUnsignedLiteral64(uint64_t data,
                                                    int num_bits) {
  return InternalWriteUnsigned(64, data, num_bits, bit_offset_, bit_buffer_);
}

// Writes a standard int8_t in two's complement form to the write buffer. No
// special conversion needed as the raw value is in the correct format.
absl::Status WriteBitBuffer::WriteSigned8(int8_t data) {
  return WriteUnsignedLiteral((static_cast<uint32_t>(data)) & 0xff, 8);
}

// Writes a standard int16_t in two's complement form to the write buffer. No
// special conversion needed as the raw value is in the correct format.
absl::Status WriteBitBuffer::WriteSigned16(int16_t data) {
  return WriteUnsignedLiteral(static_cast<uint32_t>(data) & 0xffff, 16);
}

// Writes a null terminated C-style string to the buffer - including the null.
absl::Status WriteBitBuffer::WriteString(const std::string& data) {
  // Write up to the first `kIamfMaxStringSize` characters. Exit after writing
  // the null terminator.
  for (int i = 0; i < kIamfMaxStringSize; i++) {
    // Note that some systems have `char` as signed and others unsigned. Write
    // the same raw byte value regardless.
    const uint8_t byte = static_cast<uint8_t>(data[i]);
    RETURN_IF_NOT_OK(WriteUnsignedLiteral(byte, 8));

    // Exit successfully after last byte was written.
    if (data[i] == '\0') {
      return absl::OkStatus();
    }
  }

  // Failed to find the null terminator within `kIamfMaxStringSize` bytes.
  return absl::InvalidArgumentError(
      absl::StrCat("Failed to find the null terminator for data= ", data));
}

absl::Status WriteBitBuffer::WriteUleb128(const DecodedUleb128 data) {
  // Transform data to a temporary buffer. Then write it.
  std::vector<uint8_t> buffer;
  RETURN_IF_NOT_OK(leb_generator_.Uleb128ToUint8Vector(data, buffer));
  RETURN_IF_NOT_OK(WriteUint8Vector(buffer));
  return absl::OkStatus();
}

absl::Status WriteBitBuffer::WriteIso14496_1Expanded(
    uint32_t size_of_instance) {
  constexpr uint8_t kSizeOfInstanceMask = 0x7f;
  constexpr uint8_t kNextByteMask = 0x80;
  std::vector<uint8_t> buffer;

  // Fill the buffer in reverse order. After the loop the most significant bits
  // will be at the start of the buffer.
  do {
    const uint8_t byte =
        (size_of_instance & kSizeOfInstanceMask) | kNextByteMask;
    buffer.insert(buffer.begin(), byte);

    size_of_instance >>= 7;
  } while (size_of_instance > 0);
  // Ensure the last byte signals the end of the data.
  buffer.back() &= kSizeOfInstanceMask;

  return WriteUint8Vector(buffer);
}

absl::Status WriteBitBuffer::WriteUint8Vector(
    const std::vector<uint8_t>& data) {
  if (IsByteAligned()) {
    // In the common case we can just copy all of the data over and update
    // `bit_offset_`.
    bit_buffer_.reserve(bit_buffer_.size() + data.size());
    std::copy(data.begin(), data.end(), std::back_inserter(bit_buffer_));
    bit_offset_ += 8 * data.size();
    return absl::OkStatus();
  }

  // Expand the buffer to fit the data for efficiency when processing large
  // input.
  RETURN_IF_NOT_OK(CanWriteBytes(true, data.size(), bit_offset_, bit_buffer_));

  // The buffer is mis-aligned. Copy it over one byte at a time.
  for (const uint8_t& value : data) {
    RETURN_IF_NOT_OK(WriteUnsignedLiteral(value, 8));
  }
  return absl::OkStatus();
}

absl::Status WriteBitBuffer::FlushAndWriteToFile(std::fstream& output_file) {
  if (!IsByteAligned()) {
    return absl::InvalidArgumentError("Write buffer not byte-aligned");
  }

  bit_buffer_.resize(bit_offset_ / 8);
  RETURN_IF_NOT_OK(WriteBufferToFile(bit_buffer_, output_file));

  LOG(INFO) << "Flushing " << bit_offset_ / 8 << " bytes";
  Reset();
  return absl::OkStatus();
}

absl::Status WriteBitBuffer::MaybeFlushIfCloseToCapacity(
    std::fstream& output_file) {
  // Query if the buffer is close to capacity without letting it resize.
  if (CanWriteBytes(/*allow_resizing=*/false, bit_buffer_.capacity() / 2,
                    bit_offset_, bit_buffer_) != absl::OkStatus()) {
    RETURN_IF_NOT_OK(FlushAndWriteToFile(output_file));
  }

  return absl::OkStatus();
}

void WriteBitBuffer::Reset() {
  bit_offset_ = 0;
  bit_buffer_.clear();
}

}  // namespace iamf_tools
