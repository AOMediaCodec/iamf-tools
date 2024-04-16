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
#include "iamf/common/read_bit_buffer.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/leb128.h"

namespace iamf_tools {
using absl::StatusCode::kInvalidArgument;
using absl::StatusCode::kResourceExhausted;
using testing::ElementsAreArray;
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
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(source_data_));
}

TEST_F(ReadBitBufferTest, LoadBitsNotByteAligned) {
  source_data_ = {0b10100001};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(3, false).ok());
  // Only read the first 3 bits (101) - the rest of the bits in the byte are
  // zeroed out.
  std::vector<uint8_t> expected = {0b10100000};
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 3);
  // Load bits again. This will clear the buffer while still reading from the
  // updated source offset.
  EXPECT_TRUE(rb_->LoadBits(5, false).ok());
  expected = {
      0b00001000};  // {00001} these bits are loaded from the 5 remaining bits
                    // in the buffer - the rest of the bits are zeroed out.
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest, LoadBitsNotByteAlignedMultipleBytes) {
  source_data_ = {0b10100001, 0b00110011};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(3, false).ok());
  // Only read the first 3 bits (101) - the rest of the bits in the byte are
  // zeroed out.
  std::vector<uint8_t> expected = {0b10100000};
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 3);
  EXPECT_EQ(rb_->buffer_size(), 3);
  // Load bits again. This will clear the buffer while still reading from the
  // updated source offset.
  EXPECT_TRUE(rb_->LoadBits(12, false).ok());
  expected = {
      0b00001001,
      0b10010000};  // {00001} these bits are loaded from the 5 remaining bits
                    // in the first byte - {0011001} comes from the first 7 bits
                    // of the second byte of the source_data. The rest of the
                    // second byte in the buffer is zeroed out.
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 15);
  EXPECT_EQ(rb_->buffer_size(), 12);
}

TEST_F(ReadBitBufferTest, LoadBitsAsManyAsPossibleLimitedSource) {
  source_data_ = {0b10100001, 0b00110011, 0b10001110};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(3, true).ok());
  // Even though we only requested 3 bits, by default we will fill the buffer as
  // much as possible.
  std::vector<uint8_t> expected = {0b10100001, 0b00110011, 0b10001110};
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 24);
  EXPECT_EQ(rb_->buffer_size(), 24);
}

TEST_F(ReadBitBufferTest, LoadBitsAsManyAsPossibleLimitedBufferCapacity) {
  source_data_ = {0b10100001, 0b00110011, 0b10001110};
  // Capacity is specified in bytes.
  rb_capacity_ = 2;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(3, true).ok());
  // Even though we only requested 3 bits, by default we will fill the buffer as
  // much as possible.
  std::vector<uint8_t> expected = {0b10100001, 0b00110011};
  EXPECT_THAT(rb_->bit_buffer(), ElementsAreArray(expected));
  EXPECT_EQ(rb_->source_bit_offset(), 16);
  EXPECT_EQ(rb_->buffer_size(), 16);
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
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(24, output_literal).ok());
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
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(24, output_literal).ok());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(rb_->buffer_bit_offset(), 24);

  // Second read to same output integer - will be overwritten.
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(8, output_literal).ok());
  EXPECT_EQ(output_literal, 0xff);
  EXPECT_EQ(rb_->buffer_bit_offset(), 32);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedNotEnoughBitsInBuffer) {
  source_data_ = {0xab, 0xcd, 0xef, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(24, false).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 3);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  // We request more bits than there are in the buffer. ReadUnsignedLiteral will
  // load more bits from source into the buffer & then return those bits.
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(32, output_literal).ok());
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
  EXPECT_EQ(rb_->ReadUnsignedLiteral(32, output_literal).code(),
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
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(6, output_literal).ok());
  EXPECT_EQ(output_literal, 0b110001);
  EXPECT_EQ(rb_->buffer_bit_offset(), 6);

  EXPECT_TRUE(rb_->ReadUnsignedLiteral(10, output_literal).ok());
  EXPECT_EQ(output_literal, 0b0110000010);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest,
       ReadUnsignedLiteralNotByteAlignedMultipleReadsNoPriorLoad) {
  source_data_ = {0b11000101, 0b10000011, 0b00000110};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint64_t output_literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(6, output_literal).ok());
  EXPECT_EQ(output_literal, 0b110001);
  EXPECT_EQ(rb_->buffer_bit_offset(), 6);
  // By default,` ReadUnsignedLiteral` will call `LoadBits` with
  // `fill_to_capacity` set to true, meaning that we will load as much data as
  // we can from the source into the buffer.
  EXPECT_EQ(rb_->buffer_size(), 24);

  EXPECT_TRUE(rb_->ReadUnsignedLiteral(10, output_literal).ok());
  EXPECT_EQ(output_literal, 0b0110000011);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
  EXPECT_EQ(rb_->buffer_size(), 24);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralBufferBitOffsetNotByteAligned) {
  source_data_ = {0b11000101, 0b10000010};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t output_literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(2, output_literal).ok());
  EXPECT_EQ(output_literal, 0b11);
  EXPECT_EQ(rb_->buffer_bit_offset(), 2);

  // Checks that bitwise reading is used when the num_bits requested is
  // byte-aligned but the buffer_bit_offset is not byte-aligned.
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(8, output_literal).ok());
  EXPECT_EQ(output_literal, 0b00010110);
  EXPECT_EQ(rb_->buffer_bit_offset(), 10);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralRequestTooLarge) {
  source_data_ = {0b00000101, 0b00000010, 0b00000110};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint64_t output_literal = 0;
  EXPECT_EQ(rb_->ReadUnsignedLiteral(65, output_literal).code(),
            kInvalidArgument);
}

// ---- ReadULeb128 Tests -----

// Successful Uleb128 reads.
TEST_F(ReadBitBufferTest, ReadUleb128Read5Bytes) {
  source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(40).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  EXPECT_TRUE(rb_->ReadULeb128(output_leb).ok());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  // Expect to read 40 bits.
  EXPECT_EQ(rb_->buffer_bit_offset(), 40);
}

TEST_F(ReadBitBufferTest, ReadUleb128Read5BytesAndStoreSize) {
  source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(40).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  int8_t encoded_leb_size = 0;
  EXPECT_TRUE(rb_->ReadULeb128(output_leb, encoded_leb_size).ok());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  EXPECT_EQ(encoded_leb_size, 5);
  // Expect to read 40 bits.
  EXPECT_EQ(rb_->buffer_bit_offset(), 40);
}

TEST_F(ReadBitBufferTest, ReadUleb128NotEnoughDataInBuffer) {
  source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(32, false).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  // Buffer has a one in the most significant position of each byte, which tells
  // us to continue reading to the next byte. The 4th byte tells us to read the
  // next byte, but there is no 5th byte in the buffer - however, there is in
  // the source, so we load the 5th byte from source into the buffer, which is
  // then output to the DecodedLeb128.
  EXPECT_TRUE(rb_->ReadULeb128(output_leb).ok());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  // Expect that the buffer_bit_offset was reset to 0 when LoadBits() was called
  // a second time; it is then incremented by 8 as we read the 5th byte.
  EXPECT_EQ(rb_->buffer_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest, ReadUleb128TwoBytes) {
  source_data_ = {0x81, 0x03, 0x81, 0x83, 0x0f};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(40).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  EXPECT_TRUE(rb_->ReadULeb128(output_leb).ok());
  // Expect the buffer to read only the first two bytes, since 0x03 does not
  // have a one in the most significant spot of the byte.
  EXPECT_EQ(output_leb, 0b00000110000001);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, ReadUleb128ExtraZeroes) {
  source_data_ = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(64).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  EXPECT_TRUE(rb_->ReadULeb128(output_leb).ok());
  // Expect the buffer to read every byte.
  EXPECT_EQ(output_leb, 0b1);
  EXPECT_EQ(rb_->buffer_bit_offset(), 64);
}

TEST_F(ReadBitBufferTest, ReadUleb128ExtraZeroesAndStoreSize) {
  source_data_ = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(64).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  int8_t encoded_leb_size = 0;
  EXPECT_TRUE(rb_->ReadULeb128(output_leb, encoded_leb_size).ok());
  // Expect the buffer to read every byte.
  EXPECT_EQ(output_leb, 0b1);
  EXPECT_EQ(encoded_leb_size, 8);
  EXPECT_EQ(rb_->buffer_bit_offset(), 64);
}

// Uleb128 read errors.
TEST_F(ReadBitBufferTest, ReadUleb128Overflow) {
  source_data_ = {0x80, 0x80, 0x80, 0x80, 0x10};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(40).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  EXPECT_EQ(rb_->ReadULeb128(output_leb).code(), kInvalidArgument);
}

TEST_F(ReadBitBufferTest, ReadUleb128TooManyBytes) {
  source_data_ = {0x80, 0x83, 0x81, 0x83, 0x80, 0x80, 0x80, 0x80};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(64).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  EXPECT_EQ(rb_->ReadULeb128(output_leb).code(), kInvalidArgument);
}

TEST_F(ReadBitBufferTest, ReadUleb128NotEnoughDataInBufferOrSource) {
  source_data_ = {0x80, 0x80, 0x80, 0x80};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(32).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  DecodedUleb128 output_leb = 0;
  // Buffer has a one in the most significant position of each byte, which tells
  // us to continue reading to the next byte. The 4th byte tells us to read the
  // next byte, but there is no 5th byte in neither the buffer nor the source.
  EXPECT_EQ(rb_->ReadULeb128(output_leb).code(), kResourceExhausted);
  // Expect to buffer_bit_offset to be reset if there is not enough data in
  // the buffer.
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

// --- ReadUint8Vector tests ---

// Successful ReadUint8Vector reads
TEST_F(ReadBitBufferTest, ReadUint8VectorRead5Bytes) {
  source_data_ = {0b10000001, 0b10000011, 0b10000001, 0b10000011, 0b00001111};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(40).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  std::vector<uint8_t> output = {};
  EXPECT_TRUE(rb_->ReadUint8Vector(5, output).ok());
  for (int i = 0; i < output.size(); ++i) {
    EXPECT_EQ(output[i], source_data_[i]);
  }
  // Expect to read 40 bits.
  EXPECT_EQ(rb_->buffer_bit_offset(), 40);
}

TEST_F(ReadBitBufferTest, ReadUint8VectorReadBytesMisalignedBuffer) {
  source_data_ = {0b10000001, 0b10000011, 0b10000001, 0b10000011, 0b00001111};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(40).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(2, literal).ok());
  // Bit buffer offset is now misaligned, but ReadUint8Vector should still work.
  EXPECT_EQ(rb_->buffer_bit_offset(), 2);
  std::vector<uint8_t> output = {};
  EXPECT_TRUE(rb_->ReadUint8Vector(4, output).ok());
  // Expected output starts reading at bit 2 instead of at 0.
  std::vector<uint8_t> expected_output = {0b00000110, 0b00001110, 0b00000110,
                                          0b00001100};
  for (int i = 0; i < output.size(); ++i) {
    EXPECT_EQ(output[i], expected_output[i]);
  }
  // Expect to read 32 bits (5 bytes) + the 2 we initially read.
  EXPECT_EQ(rb_->buffer_bit_offset(), 34);
}

// ReadUint8Vector Errors
TEST_F(ReadBitBufferTest, ReadUint8VectorNotEnoughDataInBufferOrSource) {
  source_data_ = {0x80, 0x80, 0x80, 0x80};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(32).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  std::vector<uint8_t> output = {};
  EXPECT_EQ(rb_->ReadUint8Vector(5, output).code(), kResourceExhausted);
  // Expect to buffer_bit_offset to be reset if there is not enough data in
  // the buffer.
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

// --- ReadBoolean tests ---

// Successful ReadBoolean reads
TEST_F(ReadBitBufferTest, ReadBoolean8Bits) {
  source_data_ = {0b10011001};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(8).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  bool output = false;
  std::vector<bool> expected_output = {true, false, false, true,
                                       true, false, false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(rb_->ReadBoolean(output).ok());
    EXPECT_EQ(output, expected_output[i]);
  }
  // Expect to read 8 bits.
  EXPECT_EQ(rb_->buffer_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest, ReadBooleanMisalignedBuffer) {
  source_data_ = {0b10000001, 0b01000000};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  uint64_t literal = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(2, literal).ok());
  // Bit buffer offset is now misaligned, but ReadBoolean should still work.
  EXPECT_EQ(rb_->buffer_bit_offset(), 2);
  bool output = false;
  // Expected output starts reading at bit 2 instead of at 0.
  std::vector<bool> expected_output = {false, false, false, false,
                                       false, true,  false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(rb_->ReadBoolean(output).ok());
    EXPECT_EQ(output, expected_output[i]);
  }
  // Expect to read 8 bits + the 2 we initially read.
  EXPECT_EQ(rb_->buffer_bit_offset(), 10);
}

// ReadBoolean Error
TEST_F(ReadBitBufferTest, ReadBooleanNotEnoughDataInBufferOrSource) {
  source_data_ = {0b10011001};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(8).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  bool output = false;
  std::vector<bool> expected_output = {true, false, false, true,
                                       true, false, false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_TRUE(rb_->ReadBoolean(output).ok());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_EQ(rb_->ReadBoolean(output).code(), kResourceExhausted);
  // Expect offset to be reset after failed read.
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

// --- ReadSigned16 tests ---

TEST_F(ReadBitBufferTest, Signed16Zero) {
  source_data_ = {0x00, 0x00};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  int16_t output = 0;
  EXPECT_TRUE(rb_->ReadSigned16(output).ok());
  EXPECT_EQ(output, 0);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MaxPositive) {
  source_data_ = {0x7f, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  int16_t output = 0;
  EXPECT_TRUE(rb_->ReadSigned16(output).ok());
  EXPECT_EQ(output, 32767);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MinPositive) {
  source_data_ = {0x00, 0x01};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  int16_t output = 0;
  EXPECT_TRUE(rb_->ReadSigned16(output).ok());
  EXPECT_EQ(output, 1);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MinNegative) {
  source_data_ = {0x80, 0x00};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  int16_t output = 0;
  EXPECT_TRUE(rb_->ReadSigned16(output).ok());
  EXPECT_EQ(output, -32768);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, Signed16MaxNegative) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->LoadBits(16).ok());
  EXPECT_EQ(rb_->bit_buffer().size(), 2);
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
  int16_t output = 0;
  EXPECT_TRUE(rb_->ReadSigned16(output).ok());
  EXPECT_EQ(output, -1);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, IsDataAvailable) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_TRUE(rb_->IsDataAvailable());
  uint64_t output = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(16, output).ok());
  EXPECT_FALSE(rb_->IsDataAvailable());
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralMax32) {
  source_data_ = {0xff, 0xff, 0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint32_t output = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(32, output).ok());
  EXPECT_EQ(output, 4294967295);
  EXPECT_EQ(rb_->buffer_bit_offset(), 32);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteral32Overflow) {
  source_data_ = {0xff, 0xff, 0xff, 0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint32_t output = 0;
  EXPECT_FALSE(rb_->ReadUnsignedLiteral(40, output).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralMax16) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint16_t output = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(16, output).ok());
  EXPECT_EQ(output, 65535);
  EXPECT_EQ(rb_->buffer_bit_offset(), 16);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteral16Overflow) {
  source_data_ = {0xff, 0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint16_t output = 0;
  EXPECT_FALSE(rb_->ReadUnsignedLiteral(24, output).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteralMax8) {
  source_data_ = {0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint8_t output = 0;
  EXPECT_TRUE(rb_->ReadUnsignedLiteral(8, output).ok());
  EXPECT_EQ(output, 255);
  EXPECT_EQ(rb_->buffer_bit_offset(), 8);
}

TEST_F(ReadBitBufferTest, ReadUnsignedLiteral8Overflow) {
  source_data_ = {0xff, 0xff};
  rb_capacity_ = 1024;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  uint8_t output = 0;
  EXPECT_FALSE(rb_->ReadUnsignedLiteral(9, output).ok());
  EXPECT_EQ(rb_->buffer_bit_offset(), 0);
}

}  // namespace
}  // namespace iamf_tools
