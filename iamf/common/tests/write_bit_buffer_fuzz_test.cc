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
#include <sys/types.h>

#include <cstdint>

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

void WriteSignedLiteral(uint32_t data, int num_bits) {
  WriteBitBuffer wb(0);
  auto status = wb.WriteUnsignedLiteral(data, num_bits);
  if (status.ok()) {
    EXPECT_EQ(wb.bit_offset(), num_bits);
  } else {
    EXPECT_EQ(wb.bit_offset(), 0);
  }
}

FUZZ_TEST(WriteBitBufferFuzzTest, WriteSignedLiteral);

void WriteSignedLiteral64(uint64_t data, int num_bits) {
  WriteBitBuffer wb(0);
  auto status = wb.WriteUnsignedLiteral(data, num_bits);
  if (status.ok()) {
    EXPECT_EQ(wb.bit_offset(), num_bits);
  } else {
    EXPECT_EQ(wb.bit_offset(), 0);
  }
}

FUZZ_TEST(WriteBitBufferFuzzTest, WriteSignedLiteral64);

void WriteSigned8(int8_t data) {
  WriteBitBuffer wb(0);
  auto status = wb.WriteSigned8(data);
  if (status.ok()) {
    EXPECT_EQ(wb.bit_offset(), 8);
  } else {
    EXPECT_EQ(wb.bit_offset(), 0);
  }
}

FUZZ_TEST(WriteBitBufferFuzzTest, WriteSigned8);

void WriteSigned16(int16_t data) {
  WriteBitBuffer wb(0);
  auto status = wb.WriteSigned16(data);
  if (status.ok()) {
    EXPECT_EQ(wb.bit_offset(), 16);
  } else {
    EXPECT_EQ(wb.bit_offset(), 0);
  }
}

FUZZ_TEST(WriteBitBufferFuzzTest, WriteSigned16);

void WriteString(const std::string& data) {
  WriteBitBuffer wb(0);
  auto status = wb.WriteString(data);
}

FUZZ_TEST(WriteBitBufferFuzzTest, WriteString);

void WriteUint8Vector(const std::vector<uint8_t>& data) {
  WriteBitBuffer wb(0);
  auto status = wb.WriteUint8Vector(data);
  if (status.ok()) {
    EXPECT_EQ(wb.bit_offset(), data.size() * 8);
    EXPECT_EQ(wb.bit_buffer(), data);
  } else {
    EXPECT_EQ(wb.bit_offset(), 0);
  }
}

FUZZ_TEST(WriteBitBufferFuzzTest, WriteUint8Vector);

}  // namespace
}  // namespace iamf_tools
