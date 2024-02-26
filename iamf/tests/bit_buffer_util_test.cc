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
#include "gtest/gtest.h"

using absl::StatusCode::kResourceExhausted;
namespace iamf_tools {
namespace {

TEST(CanWriteBits, EmptyBuffer) {
  std::vector<uint8_t> bit_buffer;
  EXPECT_TRUE(CanWriteBits(/*allow_resizing=*/true, /*num_bits=*/2,
                           /*bit_offset=*/0, bit_buffer)
                  .ok());
  // Requested to write 2 bits, which fit into one byte.
  EXPECT_EQ(bit_buffer.size(), 1);
}

TEST(CanWriteBits, EmptyBufferNoResize) {
  std::vector<uint8_t> bit_buffer;
  EXPECT_EQ(CanWriteBits(/*allow_resizing=*/false, /*num_bits=*/2,
                         /*bit_offset=*/0, bit_buffer)
                .code(),
            kResourceExhausted);
}

TEST(CanWriteBits, BufferHasSpace) {
  std::vector<uint8_t> bit_buffer;
  // Buffer can hold a byte.
  bit_buffer.resize(1);
  EXPECT_TRUE(CanWriteBits(/*allow_resizing=*/false, /*num_bits=*/2,
                           /*bit_offset=*/0, bit_buffer)
                  .ok());
}

TEST(CanWriteBytes, EmptyBuffer) {
  std::vector<uint8_t> bit_buffer;
  EXPECT_TRUE(CanWriteBytes(/*allow_resizing=*/true, /*num_bytes=*/3,
                            /*bit_offset=*/0, bit_buffer)
                  .ok());
  // Requested to write 3 bytes.
  EXPECT_EQ(bit_buffer.size(), 3);
}

TEST(CanWriteBytes, BufferHasSpace) {
  std::vector<uint8_t> bit_buffer;
  bit_buffer.resize(3);
  EXPECT_TRUE(CanWriteBytes(/*allow_resizing=*/false, /*num_bytes=*/3,
                            /*bit_offset=*/0, bit_buffer)
                  .ok());
}

TEST(WriteBit, WriteSeveralBits) {
  std::vector<uint8_t> bit_buffer;
  int64_t bit_offset = 0;
  // First bit to write.
  EXPECT_TRUE(CanWriteBits(/*allow_resizing=*/true, /*num_bits=*/1, bit_offset,
                           bit_buffer)
                  .ok());
  EXPECT_EQ(bit_buffer.size(), 1);
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_EQ(bit_buffer[0], 128);  // {10000000}.
  EXPECT_EQ(bit_offset, 1);

  // Write several bits: this time request 3 bytes - 1 bit worth of space, for a
  // total of 3 bytes.
  EXPECT_TRUE(CanWriteBits(/*allow_resizing=*/true, /*num_bits=*/23, bit_offset,
                           bit_buffer)
                  .ok());
  EXPECT_EQ(bit_buffer.size(), 3);

  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/0, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());

  // --- End writing first byte.
  EXPECT_EQ(bit_buffer[0], 253);  // {11111101}.
  EXPECT_EQ(bit_offset, 8);

  EXPECT_TRUE(WriteBit(/*bit=*/0, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/0, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/0, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/0, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/0, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());
  EXPECT_TRUE(WriteBit(/*bit=*/1, bit_offset, bit_buffer).ok());

  // --- End writing second byte
  EXPECT_EQ(bit_buffer[1], 19);  // {00010011}.
  EXPECT_EQ(bit_offset, 16);

  EXPECT_EQ(bit_buffer[2], 0);  // {00000000}.
}

}  // namespace
}  // namespace iamf_tools
