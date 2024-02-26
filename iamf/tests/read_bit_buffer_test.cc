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
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
using absl::StatusCode::kInvalidArgument;
using absl::StatusCode::kResourceExhausted;
using testing::UnorderedElementsAreArray;
namespace {

class ReadBitBufferTest : public ::testing::Test {
 public:
  std::vector<uint8_t> source_data_;
  int64_t rb_capacity_;
  std::unique_ptr<ReadBitBuffer> CreateReadBitBuffer() {
    return std::make_unique<ReadBitBuffer>(rb_capacity_, &source_data_);
  }
};

TEST_F(ReadBitBufferTest, ReadBitBufferConstructor) {
  source_data_ = {};
  rb_capacity_ = 0;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_NE(rb_, nullptr);
}

// ---- Load Bits Tests -----
TEST_F(ReadBitBufferTest, LoadBitsByteAligned) {
  source_data_ = {0x09, 0x02, 0xab};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24).ok());
  EXPECT_THAT(rb_->bit_buffer(), UnorderedElementsAreArray(source_data_));
}

TEST_F(ReadBitBufferTest, LoadBitsNotByteAligned) {
  source_data_ = {0b10100001};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(3).ok());
  // Only read the first 3 bits (101) - the rest of the bits in the byte are
  // zeroed out.
  std::vector<uint8_t> expected = {0b10100000};
  EXPECT_THAT(rb_->bit_buffer(), UnorderedElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 3);
  // Load bits again. This will clear the buffer while still reading from the
  // updated source offset.
  EXPECT_TRUE(rb_->LoadBits(5).ok());
  expected = {
      0b00001000};  // {00001} these bits are loaded from the 5 remaining bits
                    // in the buffer - the rest of the bits are zeroed out.
  EXPECT_THAT(rb_->bit_buffer(), UnorderedElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest, LoadBitsNotEnoughSourceBits) {
  source_data_ = {0x09, 0x02, 0xab};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_EQ(rb_->LoadBits(32).code(), kResourceExhausted);
  EXPECT_EQ(rb_->bit_buffer().size(), 0);
}

// ---- ReadUnsignedLiteral Tests -----
TEST_F(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedAllBits) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 3);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(24, &output_literal).ok());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(rb_->buffer_bit_offset(), 24);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedMultipleReads) {
  source_data_ = {0xab, 0xcd, 0xef, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(32).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 4);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(24, &output_literal).ok());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(rb_->buffer_bit_offset(), 24);

  // Second read to same output integer - will be overwritten.
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(8, &output_literal).ok());
  EXPECT_EQ(output_literal, 0xff);
  EXPECT_EQ(rb_->buffer_bit_offset(), 32);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedNotEnoughBitsInBuffer) {
  source_data_ = {0xab, 0xcd, 0xef, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 3);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  // We request more bits than there are in the buffer. ReadUnsignedLiteral will
  // load more bits from source into the buffer & then return those bits.
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(32, &output_literal).ok());
  // Output value should be the same as if we had called loadbits(32) &
  // readunsignedliteral(32).
  EXPECT_EQ(output_literal, 0xabcdefff);
  // offset is not the same, however - since we called load bits, we reset the
  // offset to 0 and then increment it by the extra 8 bits we read.
  EXPECT_EQ(rb_->buffer_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest,
       ReadUnsignedLiteralByteAlignedNotEnoughBitsInBufferOrSource) {
  source_data_ = {0xab, 0xcd, 0xef};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 3);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  // We request more bits than there are in the buffer. ReadUnsignedLiteral will
  // attempt to load more bits from source into the buffer, but that will fail,
  // since there aren't enough bits in the source either.
  EXPECT_EQ(rb_->ReadUnsignedLiteral(32, &output_literal).code(),
            kResourceExhausted);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralNotByteAlignedMultipleReads) {
  source_data_ = {0b11000101, 0b10000010, 0b00000110};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 3);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(6, &output_literal).ok());
  EXPECT_EQ(output_literal, 0b110001);
  EXPECT_EQ(rb_->buffer_bit_offset(), 6);

  EXPECT_TRUE(rb_->ReadUnsignedLiteral(10, &output_literal).ok());
  EXPECT_EQ(output_literal, 0b0110000010);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralBufferBitOffsetNotByteAligned) {
  source_data_ = {0b11000101, 0b10000010};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(2, &output_literal).ok());
  EXPECT_EQ(output_literal, 0b11);
  EXPECT_EQ(rb_->buffer_bit_offset(), 2);

  // Checks that bitwise reading is used when the num_bits requested is
  // byte-aligned but the buffer_bit_offset is not byte-aligned.
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(8, &output_literal).ok());
  EXPECT_EQ(output_literal, 0b00010110);
  EXPECT_EQ(rb_->buffer_bit_offset(), 10);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralRequestTooLarge) {
  source_data_ = {0b00000101, 0b00000010, 0b00000110};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint64_t output_literal = 0;
  EXPECT_EQ(rb_->ReadUnsignedLiteral(65, &output_literal).code(),
            kInvalidArgument);
}

}  // namespace
}  // namespace iamf_tools
