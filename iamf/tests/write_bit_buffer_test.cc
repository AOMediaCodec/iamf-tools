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
#include "iamf/write_bit_buffer.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"
#include "iamf/tests/test_utils.h"

namespace iamf_tools {
namespace {

class WriteBitBufferTest : public ::testing::Test {
 protected:
  // Validates a write buffer that may or may not be byte-aligned.
  void ValidateMaybeNotAlignedWriteBuffer(
      int64_t num_bits, const std::vector<uint8_t>& expected_data) {
    // Verify exact number of expected bits were written.
    EXPECT_EQ(wb_->bit_offset(), num_bits);

    const unsigned int ceil_num_bytes =
        num_bits / 8 + (num_bits % 8 == 0 ? 0 : 1);

    ASSERT_LE(expected_data.size(), ceil_num_bytes);

    // Compare rounded up to the nearest byte with expected result.
    EXPECT_EQ(wb_->bit_buffer(), expected_data);
  }

  // The buffer is resizable; the initial capacity does not matter.
  std::unique_ptr<WriteBitBuffer> wb_ = std::make_unique<WriteBitBuffer>(0);
};

TEST_F(WriteBitBufferTest, UnsignedLiteralNumBitsEqualsZero) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0x00, 0).ok());
  ValidateWriteResults(*wb_, {});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralOneByteZero) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0x00, 8).ok());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralOneByteNonZero) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0xab, 8).ok());
  ValidateWriteResults(*wb_, {0xab});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralTwoBytes) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0xffee, 16).ok());
  ValidateWriteResults(*wb_, {0xff, 0xee});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralFourBytes) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0xffeeddcc, 32).ok());
  ValidateWriteResults(*wb_, {0xff, 0xee, 0xdd, 0xcc});
}

// This test is not byte aligned. So all expected result bits required to
// round up to the nearest byte are set to zero.
TEST_F(WriteBitBufferTest, UnsignedLiteralNotByteAligned) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0b11, 2).ok());
  ValidateMaybeNotAlignedWriteBuffer(2, {0b1100'0000});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralMixedAlignedAndNotAligned) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0, 1).ok());
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0xff, 8).ok());
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0, 7).ok());
  ValidateWriteResults(*wb_, {0x7f, 0x80});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralNotByteAlignedLarge) {
  EXPECT_TRUE(
      wb_->WriteUnsignedLiteral(0b0001'0010'0011'0100'0101'0110'0111, 28).ok());
  ValidateMaybeNotAlignedWriteBuffer(
      28, {0b0001'0010, 0b0011'0100, 0b0101'0110, 0b0111'0000});
}

TEST_F(WriteBitBufferTest, InvalidUnsignedLiteralOverflowOverRequestedNumBits) {
  EXPECT_EQ(wb_->WriteUnsignedLiteral(16, 4).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(WriteBitBufferTest, InvalidUnsignedLiteralOverNumBitsOver32) {
  EXPECT_EQ(wb_->WriteUnsignedLiteral(0, /*num_bits=*/33).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64OneByteZero) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral64(0x00, 8).ok());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64FiveBytes) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral64(0xffffffffff, 40).ok());
  ValidateWriteResults(*wb_, {0xff, 0xff, 0xff, 0xff, 0xff});
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64EightBytes) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral64(0xfedcba9876543210l, 64).ok());
  ValidateWriteResults(*wb_, {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10});
}

// These tests are not byte aligned. So all expected result bits required to
// round up to the nearest byte are set to zero.
TEST_F(WriteBitBufferTest, UnsignedLiteral64NotByteAlignedSmall) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral64(0b101, 3).ok());
  ValidateMaybeNotAlignedWriteBuffer(3, {0b1010'0000});
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64NotByteAlignedLarge) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral64(0x7fffffffffffffff, 63).ok());
  ValidateMaybeNotAlignedWriteBuffer(
      63, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe});
}

TEST_F(WriteBitBufferTest,
       InvalidUnsignedLiteral64OverflowOverRequestedNumBits) {
  EXPECT_EQ(wb_->WriteUnsignedLiteral64(uint64_t{1} << 34, 34).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(WriteBitBufferTest, InvalidUnsignedLiteral64NumBitsOver64) {
  EXPECT_EQ(wb_->WriteUnsignedLiteral64(0, /*num_bits=*/65).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(WriteBitBufferTest, Signed8Zero) {
  EXPECT_TRUE(wb_->WriteSigned8(0x00).ok());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, Signed8MaxPositive) {
  EXPECT_TRUE(wb_->WriteSigned8(127).ok());
  ValidateWriteResults(*wb_, {0x7f});
}

TEST_F(WriteBitBufferTest, Signed8MinPositive) {
  EXPECT_TRUE(wb_->WriteSigned8(1).ok());
  ValidateWriteResults(*wb_, {0x01});
}

TEST_F(WriteBitBufferTest, Signed8MinNegative) {
  EXPECT_TRUE(wb_->WriteSigned8(-128).ok());
  ValidateWriteResults(*wb_, {0x80});
}

TEST_F(WriteBitBufferTest, Signed8MaxNegative) {
  EXPECT_TRUE(wb_->WriteSigned8(-1).ok());
  ValidateWriteResults(*wb_, {0xff});
}

TEST_F(WriteBitBufferTest, Signed16Zero) {
  EXPECT_TRUE(wb_->WriteSigned16(0x00).ok());
  ValidateWriteResults(*wb_, {0x00, 0x00});
}

TEST_F(WriteBitBufferTest, Signed16MaxPositive) {
  EXPECT_TRUE(wb_->WriteSigned16(32767).ok());
  ValidateWriteResults(*wb_, {0x7f, 0xff});
}

TEST_F(WriteBitBufferTest, Signed16MinPositive) {
  EXPECT_TRUE(wb_->WriteSigned16(1).ok());
  ValidateWriteResults(*wb_, {0x00, 0x01});
}

TEST_F(WriteBitBufferTest, Signed16MinNegative) {
  EXPECT_TRUE(wb_->WriteSigned16(-32768).ok());
  ValidateWriteResults(*wb_, {0x80, 0x00});
}

TEST_F(WriteBitBufferTest, Signed16MaxNegative) {
  EXPECT_TRUE(wb_->WriteSigned16(-1).ok());
  ValidateWriteResults(*wb_, {0xff, 0xff});
}

TEST_F(WriteBitBufferTest, StringOnlyNullCharacter) {
  const std::string kEmptyString = "\0";

  EXPECT_TRUE(wb_->WriteString(kEmptyString).ok());

  ValidateWriteResults(*wb_, {'\0'});
}

TEST_F(WriteBitBufferTest, StringAscii) {
  const std::string kAsciiInput = "ABC\0";

  EXPECT_TRUE(wb_->WriteString(kAsciiInput).ok());

  ValidateWriteResults(*wb_, {'A', 'B', 'C', '\0'});
}

TEST_F(WriteBitBufferTest, StringUtf8) {
  const std::string kUtf8Input(
      "\xc3\xb3"          // A 1-byte UTF-8 character.
      "\xf0\x9d\x85\x9f"  // A 4-byte UTF-8 character.
      "\0");

  EXPECT_TRUE(wb_->WriteString(kUtf8Input).ok());

  ValidateWriteResults(*wb_,
                       {0xc3, 0xb3,              // A 1-byte UTF-8 character.
                        0xf0, 0x9d, 0x85, 0x9f,  // A 4-byte UTF-8 character.
                        '\0'});
}

TEST_F(WriteBitBufferTest, StringMaxLength) {
  // Make a string and expected output with 127 non-NULL characters, followed by
  // a NULL character.
  const std::string kMaxLengthString = absl::StrCat(
      std::string(WriteBitBuffer::kIamfMaxStringSize - 1, 'a'), "\0");
  std::vector<uint8_t> expected_result(WriteBitBuffer::kIamfMaxStringSize, 'a');
  expected_result.back() = '\0';

  EXPECT_TRUE(wb_->WriteString(kMaxLengthString).ok());
  ValidateWriteResults(*wb_, expected_result);
}

TEST_F(WriteBitBufferTest, InvalidStringMissingNullTerminator) {
  const std::string kMaxLengthString(WriteBitBuffer::kIamfMaxStringSize, 'a');

  EXPECT_FALSE(wb_->WriteString(kMaxLengthString).ok());
}

TEST_F(WriteBitBufferTest, Uint8ArrayLengthZero) {
  EXPECT_TRUE(wb_->WriteUint8Vector({}).ok());
  ValidateWriteResults(*wb_, {});
}

TEST_F(WriteBitBufferTest, Uint8ArrayByteAligned) {
  const std::vector<uint8_t> input = {0, 10, 20, 30, 255};

  EXPECT_TRUE(wb_->WriteUint8Vector(input).ok());
  ValidateWriteResults(*wb_, input);
}

TEST_F(WriteBitBufferTest, Uint8ArrayNotByteAligned) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0, 1).ok());
  EXPECT_TRUE(wb_->WriteUint8Vector({0xff}).ok());
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0, 7).ok());
  ValidateWriteResults(*wb_, {0x7f, 0x80});
}

TEST_F(WriteBitBufferTest, WriteUleb128Min) {
  EXPECT_TRUE(wb_->WriteUleb128(0).ok());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, WriteUleb128Max) {
  EXPECT_TRUE(
      wb_->WriteUleb128(std::numeric_limits<DecodedUleb128>::max()).ok());
  ValidateWriteResults(*wb_, {0xff, 0xff, 0xff, 0xff, 0x0f});
}

TEST_F(WriteBitBufferTest,
       WriteUleb128IsControlledByGeneratorPassedInConstructor) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);
  ASSERT_NE(leb_generator, nullptr);
  wb_ = std::make_unique<WriteBitBuffer>(1, *leb_generator);

  EXPECT_TRUE(wb_->WriteUleb128(0).ok());

  ValidateWriteResults(*wb_, {0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST_F(WriteBitBufferTest, WriteMinUleb128DefaultsToGeneratingMinimalUleb128s) {
  EXPECT_TRUE(wb_->WriteUleb128(129).ok());

  ValidateWriteResults(*wb_, {0x81, 0x01});
}

TEST_F(WriteBitBufferTest, WriteMinUleb128CanFailWithFixedSizeGenerator) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);
  ASSERT_NE(leb_generator, nullptr);
  wb_ = std::make_unique<WriteBitBuffer>(1, *leb_generator);

  EXPECT_FALSE(wb_->WriteUleb128(128).ok());
}

TEST_F(WriteBitBufferTest, CapacityMayBeSmaller) {
  // The buffer may have a small initial capacity and resize as needed.
  wb_ = std::make_unique<WriteBitBuffer>(/*initial_capacity=*/0);
  const std::vector<uint8_t> input = {0, 1, 2, 3, 4, 5};

  EXPECT_TRUE(wb_->WriteUint8Vector(input).ok());
  ValidateWriteResults(*wb_, input);
}

TEST_F(WriteBitBufferTest, CapacityMayBeLarger) {
  // The buffer may have a larger that capacity that necessary.
  wb_ = std::make_unique<WriteBitBuffer>(/*initial_capacity=*/100);
  const std::vector<uint8_t> input = {0, 1, 2, 3, 4, 5};

  EXPECT_TRUE(wb_->WriteUint8Vector(input).ok());
  ValidateWriteResults(*wb_, input);
}

TEST_F(WriteBitBufferTest, ConsecutiveWrites) {
  // The buffer accumulates data from all write calls.
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0x01, 8).ok());
  EXPECT_TRUE(wb_->WriteUnsignedLiteral64(0x0203040506070809, 64).ok());
  EXPECT_TRUE(wb_->WriteUleb128(128).ok());
  ValidateWriteResults(*wb_,
                       {// From `WriteUnsignedLiteral()`.
                        0x01,
                        // From `WriteUnsignedLiteral64()`.
                        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                        // From `WriteLeb128()`.
                        0x80, 0x01});
}

TEST_F(WriteBitBufferTest, UseAfterReset) {
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(0xabcd, 16).ok());
  ValidateWriteResults(*wb_, {0xab, 0xcd});

  // Resetting the buffer clears it.
  wb_->Reset();
  ValidateWriteResults(*wb_, {});

  // The buffer can be used after reset . There is no trace of data before the
  // reset.
  EXPECT_TRUE(wb_->WriteUnsignedLiteral(100, 8).ok());
  ValidateWriteResults(*wb_, {100});
}

}  // namespace
}  // namespace iamf_tools
