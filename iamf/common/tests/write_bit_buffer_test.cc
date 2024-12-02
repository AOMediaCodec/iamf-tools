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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/bit_buffer_util.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(FlushAndWriteToStream, WritesToOutputStream) {
  const std::vector<uint8_t> kDataToOutput = {0x00, '\r', '\n', 0x1a};
  const auto file_to_write_to = GetAndCleanupOutputFileName(".bin");
  auto output_stream = std::make_optional<std::fstream>(
      file_to_write_to, std::ios::binary | std::ios::out);

  WriteBitBuffer wb(0);
  EXPECT_THAT(wb.WriteUint8Vector(kDataToOutput), IsOk());
  EXPECT_THAT(wb.FlushAndWriteToFile(output_stream), IsOk());
  output_stream->close();

  EXPECT_EQ(std::filesystem::file_size(file_to_write_to), kDataToOutput.size());
}

TEST(FlushAndWriteToStream, SucceedsWithoutOutputStream) {
  std::optional<std::fstream> omit_output_stream = std::nullopt;
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.FlushAndWriteToFile(omit_output_stream), IsOk());
}

TEST(FlushAndWriteToStream, FlushesBuffer) {
  std::optional<std::fstream> omit_output_stream = std::nullopt;
  WriteBitBuffer wb(0);
  EXPECT_THAT(wb.WriteUnsignedLiteral(0x01, 8), IsOk());

  EXPECT_THAT(wb.FlushAndWriteToFile(omit_output_stream), IsOk());

  EXPECT_TRUE(wb.bit_buffer().empty());
}

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
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0x00, 0), IsOk());
  ValidateWriteResults(*wb_, {});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralOneByteZero) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0x00, 8), IsOk());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralOneByteNonZero) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0xab, 8), IsOk());
  ValidateWriteResults(*wb_, {0xab});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralTwoBytes) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0xffee, 16), IsOk());
  ValidateWriteResults(*wb_, {0xff, 0xee});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralFourBytes) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0xffeeddcc, 32), IsOk());
  ValidateWriteResults(*wb_, {0xff, 0xee, 0xdd, 0xcc});
}

// This test is not byte aligned. So all expected result bits required to
// round up to the nearest byte are set to zero.
TEST_F(WriteBitBufferTest, UnsignedLiteralNotByteAligned) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0b11, 2), IsOk());
  ValidateMaybeNotAlignedWriteBuffer(2, {0b1100'0000});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralMixedAlignedAndNotAligned) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0, 1), IsOk());
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0xff, 8), IsOk());
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0, 7), IsOk());
  ValidateWriteResults(*wb_, {0x7f, 0x80});
}

TEST_F(WriteBitBufferTest, UnsignedLiteralNotByteAlignedLarge) {
  EXPECT_THAT(
      wb_->WriteUnsignedLiteral(0b0001'0010'0011'0100'0101'0110'0111, 28),
      IsOk());
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

TEST_F(WriteBitBufferTest, UnsignedLiteralZeroNumBits) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0, /*num_bits=*/0), IsOk());
  EXPECT_THAT(wb_->bit_offset(), 0);
}

TEST_F(WriteBitBufferTest, InvalidUnsignedLiteralNegativeNumBits) {
  EXPECT_EQ(wb_->WriteUnsignedLiteral(0, /*num_bits=*/-1).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64OneByteZero) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral64(0x00, 8), IsOk());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64FiveBytes) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral64(0xffffffffff, 40), IsOk());
  ValidateWriteResults(*wb_, {0xff, 0xff, 0xff, 0xff, 0xff});
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64EightBytes) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral64(0xfedcba9876543210l, 64), IsOk());
  ValidateWriteResults(*wb_, {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10});
}

// These tests are not byte aligned. So all expected result bits required to
// round up to the nearest byte are set to zero.
TEST_F(WriteBitBufferTest, UnsignedLiteral64NotByteAlignedSmall) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral64(0b101, 3), IsOk());
  ValidateMaybeNotAlignedWriteBuffer(3, {0b1010'0000});
}

TEST_F(WriteBitBufferTest, UnsignedLiteral64NotByteAlignedLarge) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral64(0x7fffffffffffffff, 63), IsOk());
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
  EXPECT_THAT(wb_->WriteSigned8(0x00), IsOk());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, Signed8MaxPositive) {
  EXPECT_THAT(wb_->WriteSigned8(127), IsOk());
  ValidateWriteResults(*wb_, {0x7f});
}

TEST_F(WriteBitBufferTest, Signed8MinPositive) {
  EXPECT_THAT(wb_->WriteSigned8(1), IsOk());
  ValidateWriteResults(*wb_, {0x01});
}

TEST_F(WriteBitBufferTest, Signed8MinNegative) {
  EXPECT_THAT(wb_->WriteSigned8(-128), IsOk());
  ValidateWriteResults(*wb_, {0x80});
}

TEST_F(WriteBitBufferTest, Signed8MaxNegative) {
  EXPECT_THAT(wb_->WriteSigned8(-1), IsOk());
  ValidateWriteResults(*wb_, {0xff});
}

TEST_F(WriteBitBufferTest, Signed16Zero) {
  EXPECT_THAT(wb_->WriteSigned16(0x00), IsOk());
  ValidateWriteResults(*wb_, {0x00, 0x00});
}

TEST_F(WriteBitBufferTest, Signed16MaxPositive) {
  EXPECT_THAT(wb_->WriteSigned16(32767), IsOk());
  ValidateWriteResults(*wb_, {0x7f, 0xff});
}

TEST_F(WriteBitBufferTest, Signed16MinPositive) {
  EXPECT_THAT(wb_->WriteSigned16(1), IsOk());
  ValidateWriteResults(*wb_, {0x00, 0x01});
}

TEST_F(WriteBitBufferTest, Signed16MinNegative) {
  EXPECT_THAT(wb_->WriteSigned16(-32768), IsOk());
  ValidateWriteResults(*wb_, {0x80, 0x00});
}

TEST_F(WriteBitBufferTest, Signed16MaxNegative) {
  EXPECT_THAT(wb_->WriteSigned16(-1), IsOk());
  ValidateWriteResults(*wb_, {0xff, 0xff});
}

TEST_F(WriteBitBufferTest, StringOnlyNullCharacter) {
  const std::string kEmptyString = "\0";

  EXPECT_THAT(wb_->WriteString(kEmptyString), IsOk());

  ValidateWriteResults(*wb_, {'\0'});
}

TEST_F(WriteBitBufferTest, StringAscii) {
  const std::string kAsciiInput = "ABC\0";

  EXPECT_THAT(wb_->WriteString(kAsciiInput), IsOk());

  ValidateWriteResults(*wb_, {'A', 'B', 'C', '\0'});
}

TEST_F(WriteBitBufferTest, StringUtf8) {
  const std::string kUtf8Input(
      "\xc3\xb3"          // A 1-byte UTF-8 character.
      "\xf0\x9d\x85\x9f"  // A 4-byte UTF-8 character.
      "\0");

  EXPECT_THAT(wb_->WriteString(kUtf8Input), IsOk());

  ValidateWriteResults(*wb_,
                       {0xc3, 0xb3,              // A 1-byte UTF-8 character.
                        0xf0, 0x9d, 0x85, 0x9f,  // A 4-byte UTF-8 character.
                        '\0'});
}

TEST_F(WriteBitBufferTest, StringMaxLength) {
  // Make a string and expected output with 127 non-NULL characters, followed by
  // a NULL character.
  const std::string kMaxLengthString =
      absl::StrCat(std::string(kIamfMaxStringSize - 1, 'a'), "\0");
  std::vector<uint8_t> expected_result(kIamfMaxStringSize, 'a');
  expected_result.back() = '\0';

  EXPECT_THAT(wb_->WriteString(kMaxLengthString), IsOk());
  ValidateWriteResults(*wb_, expected_result);
}

TEST_F(WriteBitBufferTest, InvalidStringMissingNullTerminator) {
  const std::string kMaxLengthString(kIamfMaxStringSize, 'a');

  EXPECT_FALSE(wb_->WriteString(kMaxLengthString).ok());
}

TEST_F(WriteBitBufferTest, Uint8ArrayLengthZero) {
  EXPECT_THAT(wb_->WriteUint8Vector({}), IsOk());
  ValidateWriteResults(*wb_, {});
}

TEST_F(WriteBitBufferTest, Uint8ArrayByteAligned) {
  const std::vector<uint8_t> input = {0, 10, 20, 30, 255};

  EXPECT_THAT(wb_->WriteUint8Vector(input), IsOk());
  ValidateWriteResults(*wb_, input);
}

TEST_F(WriteBitBufferTest, Uint8ArrayNotByteAligned) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0, 1), IsOk());
  EXPECT_THAT(wb_->WriteUint8Vector({0xff}), IsOk());
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0, 7), IsOk());
  ValidateWriteResults(*wb_, {0x7f, 0x80});
}

TEST_F(WriteBitBufferTest, WriteUleb128Min) {
  EXPECT_THAT(wb_->WriteUleb128(0), IsOk());
  ValidateWriteResults(*wb_, {0x00});
}

TEST_F(WriteBitBufferTest, WriteUleb128Max) {
  EXPECT_THAT(wb_->WriteUleb128(std::numeric_limits<DecodedUleb128>::max()),
              IsOk());
  ValidateWriteResults(*wb_, {0xff, 0xff, 0xff, 0xff, 0x0f});
}

TEST_F(WriteBitBufferTest,
       WriteUleb128IsControlledByGeneratorPassedInConstructor) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);
  ASSERT_NE(leb_generator, nullptr);
  wb_ = std::make_unique<WriteBitBuffer>(1, *leb_generator);

  EXPECT_THAT(wb_->WriteUleb128(0), IsOk());

  ValidateWriteResults(*wb_, {0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST_F(WriteBitBufferTest, WriteMinUleb128DefaultsToGeneratingMinimalUleb128s) {
  EXPECT_THAT(wb_->WriteUleb128(129), IsOk());

  ValidateWriteResults(*wb_, {0x81, 0x01});
}

TEST_F(WriteBitBufferTest, WriteMinUleb128CanFailWithFixedSizeGenerator) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);
  ASSERT_NE(leb_generator, nullptr);
  wb_ = std::make_unique<WriteBitBuffer>(1, *leb_generator);

  EXPECT_FALSE(wb_->WriteUleb128(128).ok());
}

struct WriteIso14496_1ExpandedTestCase {
  uint32_t size_of_instance;
  const std::vector<uint8_t> expected_source_data;
};

using WriteIso14496_1Expanded =
    ::testing::TestWithParam<WriteIso14496_1ExpandedTestCase>;

TEST_P(WriteIso14496_1Expanded, WriteIso14496_1Expanded) {
  WriteBitBuffer wb(0);
  EXPECT_THAT(wb.WriteIso14496_1Expanded(GetParam().size_of_instance), IsOk());

  EXPECT_EQ(wb.bit_buffer(), GetParam().expected_source_data);
}

INSTANTIATE_TEST_SUITE_P(OneByteOutput, WriteIso14496_1Expanded,
                         testing::ValuesIn<WriteIso14496_1ExpandedTestCase>({
                             {0, {0x00}},
                             {1, {0x01}},
                             {127, {0x7f}},
                         }));

INSTANTIATE_TEST_SUITE_P(TwoByteOutput, WriteIso14496_1Expanded,
                         testing::ValuesIn<WriteIso14496_1ExpandedTestCase>({
                             {128, {0x81, 0x00}},
                             {129, {0x81, 0x01}},
                             {0x3fff, {0xff, 0x7f}},
                         }));

INSTANTIATE_TEST_SUITE_P(FiveByteOutput, WriteIso14496_1Expanded,
                         testing::ValuesIn<WriteIso14496_1ExpandedTestCase>({
                             {0x10000000, {0x81, 0x80, 0x80, 0x80, 0x00}},
                             {0xf0000000, {0x8f, 0x80, 0x80, 0x80, 0x00}},
                         }));

INSTANTIATE_TEST_SUITE_P(MaxOutput, WriteIso14496_1Expanded,
                         testing::ValuesIn<WriteIso14496_1ExpandedTestCase>({
                             {std::numeric_limits<uint32_t>::max(),
                              {0x8f, 0xff, 0xff, 0xff, 0x7f}},
                         }));

TEST_F(WriteBitBufferTest, CapacityMayBeSmaller) {
  // The buffer may have a small initial capacity and resize as needed.
  wb_ = std::make_unique<WriteBitBuffer>(/*initial_capacity=*/0);
  const std::vector<uint8_t> input = {0, 1, 2, 3, 4, 5};

  EXPECT_THAT(wb_->WriteUint8Vector(input), IsOk());
  ValidateWriteResults(*wb_, input);
}

TEST_F(WriteBitBufferTest, CapacityMayBeLarger) {
  // The buffer may have a larger that capacity that necessary.
  wb_ = std::make_unique<WriteBitBuffer>(/*initial_capacity=*/100);
  const std::vector<uint8_t> input = {0, 1, 2, 3, 4, 5};

  EXPECT_THAT(wb_->WriteUint8Vector(input), IsOk());
  ValidateWriteResults(*wb_, input);
}

TEST_F(WriteBitBufferTest, ConsecutiveWrites) {
  // The buffer accumulates data from all write calls.
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0x01, 8), IsOk());
  EXPECT_THAT(wb_->WriteUnsignedLiteral64(0x0203040506070809, 64), IsOk());
  EXPECT_THAT(wb_->WriteUleb128(128), IsOk());
  ValidateWriteResults(*wb_,
                       {// From `WriteUnsignedLiteral()`.
                        0x01,
                        // From `WriteUnsignedLiteral64()`.
                        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                        // From `WriteLeb128()`.
                        0x80, 0x01});
}

TEST_F(WriteBitBufferTest, UseAfterReset) {
  EXPECT_THAT(wb_->WriteUnsignedLiteral(0xabcd, 16), IsOk());
  ValidateWriteResults(*wb_, {0xab, 0xcd});

  // Resetting the buffer clears it.
  wb_->Reset();
  ValidateWriteResults(*wb_, {});

  // The buffer can be used after reset . There is no trace of data before the
  // reset.
  EXPECT_THAT(wb_->WriteUnsignedLiteral(100, 8), IsOk());
  ValidateWriteResults(*wb_, {100});
}

}  // namespace
}  // namespace iamf_tools
