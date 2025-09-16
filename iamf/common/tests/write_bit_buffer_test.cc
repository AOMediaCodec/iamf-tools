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

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl::StatusCode::kInvalidArgument;
using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;

// The buffer is resizable; the initial capacity does not matter.
constexpr int64_t kInitialCapacity = 0;

TEST(FlushAndWriteToFile, WritesToOutputFile) {
  constexpr std::array<uint8_t, 4> kDataToOutput = {0x00, '\r', '\n', 0x1a};
  const auto file_to_write_to = GetAndCleanupOutputFileName(".bin");
  auto output_stream = std::make_optional<std::fstream>(
      file_to_write_to, std::ios::binary | std::ios::out);
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.WriteUint8Span(absl::MakeConstSpan(kDataToOutput)), IsOk());
  EXPECT_THAT(wb.FlushAndWriteToFile(output_stream), IsOk());
  output_stream->close();

  EXPECT_EQ(std::filesystem::file_size(file_to_write_to), kDataToOutput.size());
}

TEST(FlushAndWriteToFile, SucceedsWithoutOutputFile) {
  std::optional<std::fstream> omit_output_stream = std::nullopt;
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.FlushAndWriteToFile(omit_output_stream), IsOk());
}

TEST(FlushAndWriteToFile, FlushesBuffer) {
  std::optional<std::fstream> omit_output_stream = std::nullopt;
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0x01, 8), IsOk());

  EXPECT_THAT(wb.FlushAndWriteToFile(omit_output_stream), IsOk());

  EXPECT_TRUE(wb.bit_buffer().empty());
}

// Validates a write buffer that may or may not be byte-aligned.
void ValidateMaybeNotAlignedWriteBuffer(
    const WriteBitBuffer& wb, int64_t expected_num_bits,
    absl::Span<const uint8_t> expected_data) {
  // Verify exact number of expected bits were written.
  EXPECT_EQ(wb.bit_offset(), expected_num_bits);

  const unsigned int ceil_num_bytes =
      expected_num_bits / 8 + (expected_num_bits % 8 == 0 ? 0 : 1);

  ASSERT_LE(expected_data.size(), ceil_num_bytes);

  // Compare rounded up to the nearest byte with expected result.
  EXPECT_EQ(wb.bit_buffer(), expected_data);
}

TEST(WriteUnsignedLiteral, WritesZeroBits) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0x00, 0), IsOk());

  ValidateWriteResults(wb, {});
}

TEST(WriteUnsignedLiteral, OneByteZero) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0x00, 8), IsOk());

  ValidateWriteResults(wb, {0x00});
}

TEST(WriteUnsignedLiteral, OneByteNonZero) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0xab, 8), IsOk());

  ValidateWriteResults(wb, {0xab});
}

TEST(WriteUnsignedLiteral, TwoBytes) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0xffee, 16), IsOk());

  ValidateWriteResults(wb, {0xff, 0xee});
}

TEST(WriteUnsignedLiteral, FourBytes) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0xffeeddcc, 32), IsOk());

  ValidateWriteResults(wb, {0xff, 0xee, 0xdd, 0xcc});
}

// This test is not byte aligned. So all expected result bits required to
// round up to the nearest byte are set to zero.
TEST(WriteUnsignedLiteral, NotByteAligned) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0b11, 2), IsOk());

  ValidateMaybeNotAlignedWriteBuffer(wb, 2, {0b1100'0000});
}

TEST(WriteUnsignedLiteral, MixedAlignedAndNotAligned) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0, 1), IsOk());
  EXPECT_THAT(wb.WriteUnsignedLiteral(0xff, 8), IsOk());
  EXPECT_THAT(wb.WriteUnsignedLiteral(0, 7), IsOk());

  ValidateWriteResults(wb, {0x7f, 0x80});
}

TEST(WriteUnsignedLiteral, NotByteAlignedLarge) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0b0001'0010'0011'0100'0101'0110'0111, 28),
              IsOk());

  ValidateMaybeNotAlignedWriteBuffer(
      wb, 28, {0b0001'0010, 0b0011'0100, 0b0101'0110, 0b0111'0000});
}

TEST(WriteUnsignedLiteral, InvalidOverflowOverRequestedNumBits) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(16, 4), StatusIs(kInvalidArgument));
}

TEST(WriteUnsignedLiteral, InvalidOverNumBitsOver32) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0, /*num_bits=*/33),
              StatusIs(kInvalidArgument));
}

TEST(WriteUnsignedLiteral, ZeroNumBits) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0, /*num_bits=*/0), IsOk());
  EXPECT_THAT(wb.bit_offset(), 0);
}

TEST(WriteUnsignedLiteral, InvalidNegativeNumBits) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0, /*num_bits=*/-1),
              StatusIs(kInvalidArgument));
}

TEST(WriteUnsignedLiteral64, OneByteZero) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(0x00, 8), IsOk());

  ValidateWriteResults(wb, {0x00});
}

TEST(WriteUnsignedLiteral64, FiveBytes) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(0xffffffffff, 40), IsOk());

  ValidateWriteResults(wb, {0xff, 0xff, 0xff, 0xff, 0xff});
}

TEST(WriteUnsignedLiteral64, EightBytes) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(0xfedcba9876543210l, 64), IsOk());

  ValidateWriteResults(wb, {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10});
}

// These tests are not byte aligned. So all expected result bits required to
// round up to the nearest byte are set to zero.
TEST(WriteUnsignedLiteral64, NotByteAlignedSmall) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(0b101, 3), IsOk());

  ValidateMaybeNotAlignedWriteBuffer(wb, 3, {0b1010'0000});
}

TEST(WriteUnsignedLiteral64, NotByteAlignedLarge) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(0x7fffffffffffffff, 63), IsOk());

  ValidateMaybeNotAlignedWriteBuffer(
      wb, 63, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe});
}

TEST(WriteUnsignedLiteral64, InvalidOverflowOverRequestedNumBits) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(uint64_t{1} << 34, 34),
              StatusIs(kInvalidArgument));
}

TEST(WriteUnsignedLiteral64, InvalidNumBitsOver64) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral64(0, /*num_bits=*/65),
              StatusIs(kInvalidArgument));
}

TEST(WriteSigned16, Zero) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteSigned16(0x00), IsOk());

  ValidateWriteResults(wb, {0x00, 0x00});
}

TEST(WriteSigned16, MaxPositive) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteSigned16(32767), IsOk());

  ValidateWriteResults(wb, {0x7f, 0xff});
}

TEST(WriteSigned16, MinPositive) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteSigned16(1), IsOk());

  ValidateWriteResults(wb, {0x00, 0x01});
}

TEST(WriteSigned16, MinNegative) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteSigned16(-32768), IsOk());

  ValidateWriteResults(wb, {0x80, 0x00});
}

TEST(WriteSigned16, MaxNegative) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteSigned16(-1), IsOk());

  ValidateWriteResults(wb, {0xff, 0xff});
}

TEST(WriteBoolean, WritesTrue) {
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.WriteBoolean(true), IsOk());

  ValidateMaybeNotAlignedWriteBuffer(wb, 1, {0b1000'0000});
}

TEST(WriteBoolean, WritesFalse) {
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.WriteBoolean(false), IsOk());

  ValidateMaybeNotAlignedWriteBuffer(wb, 1, {0b0000'0000});
}

TEST(WriteBoolean, WritesMultipleBooleanValues) {
  WriteBitBuffer wb(0);

  EXPECT_THAT(wb.WriteBoolean(false), IsOk());
  EXPECT_THAT(wb.WriteBoolean(true), IsOk());
  EXPECT_THAT(wb.WriteBoolean(true), IsOk());
  EXPECT_THAT(wb.WriteBoolean(false), IsOk());
  EXPECT_THAT(wb.WriteBoolean(true), IsOk());

  constexpr auto kExpectedBitsWritten = 5;
  ValidateMaybeNotAlignedWriteBuffer(wb, kExpectedBitsWritten, {0b0110'1000});
}

TEST(WriteString, InvalidInternalNullTerminator) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::string kInternalNull("a\0b", 3);

  EXPECT_THAT(wb.WriteString(kInternalNull), StatusIs(kInvalidArgument));
  EXPECT_EQ(wb.bit_offset(), 0);
}

TEST(WriteString, EmptyLiteralString) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::string kEmptyString = "";

  EXPECT_THAT(wb.WriteString(kEmptyString), IsOk());

  ValidateWriteResults(wb, {'\0'});
}

TEST(WriteString, OnlyNullCharacter) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::string kEmptyString = "\0";

  EXPECT_THAT(wb.WriteString(kEmptyString), IsOk());

  ValidateWriteResults(wb, {'\0'});
}

TEST(WriteString, Ascii) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::string kAsciiInput = "ABC\0";

  EXPECT_THAT(wb.WriteString(kAsciiInput), IsOk());

  ValidateWriteResults(wb, {'A', 'B', 'C', '\0'});
}

TEST(WriteString, Utf8) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::string kUtf8Input(
      "\xc3\xb3"          // A 1-byte UTF-8 character.
      "\xf0\x9d\x85\x9f"  // A 4-byte UTF-8 character.
      "\0");

  EXPECT_THAT(wb.WriteString(kUtf8Input), IsOk());

  ValidateWriteResults(wb,
                       {0xc3, 0xb3,              // A 1-byte UTF-8 character.
                        0xf0, 0x9d, 0x85, 0x9f,  // A 4-byte UTF-8 character.
                        '\0'});
}

TEST(WriteString, MaxLength) {
  WriteBitBuffer wb(kInitialCapacity);

  // Make a string and expected output with 127 non-NULL characters, followed by
  // a NULL character.
  const std::string kMaxLengthString =
      absl::StrCat(std::string(kIamfMaxStringSize - 1, 'a'), "\0");
  std::vector<uint8_t> expected_result(kIamfMaxStringSize, 'a');
  expected_result.back() = '\0';

  EXPECT_THAT(wb.WriteString(kMaxLengthString), IsOk());

  ValidateWriteResults(wb, expected_result);
}

TEST(WriteString, InvalidTooLong) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::string kMaxLengthString(kIamfMaxStringSize, 'a');

  EXPECT_THAT(wb.WriteString(kMaxLengthString), StatusIs(kInvalidArgument));
  EXPECT_EQ(wb.bit_offset(), 0);
}

TEST(WriteUint8Span, WorksForEmptySpan) {
  WriteBitBuffer wb(kInitialCapacity);
  constexpr absl::Span<const uint8_t> kEmptySpan = {};

  EXPECT_THAT(wb.WriteUint8Span(kEmptySpan), IsOk());

  ValidateWriteResults(wb, {});
}

TEST(WriteUint8Span, WorksWhenBufferIsByteAligned) {
  WriteBitBuffer wb(kInitialCapacity);
  const std::vector<uint8_t> kFivebytes = {0, 10, 20, 30, 255};

  EXPECT_THAT(wb.WriteUint8Span(absl::MakeConstSpan(kFivebytes)), IsOk());

  ValidateWriteResults(wb, kFivebytes);
}

TEST(WriteUint8Span, WorksWhenBufferIsNotByteAligned) {
  WriteBitBuffer wb(kInitialCapacity);

  // Force the buffer to be mis-aligned.
  EXPECT_THAT(wb.WriteUnsignedLiteral(0, 1), IsOk());
  // It is OK to write a span, even when the underlying buffer is mis-aligned.
  EXPECT_THAT(wb.WriteUint8Span({0xff}), IsOk());
  EXPECT_THAT(wb.WriteUnsignedLiteral(0, 7), IsOk());

  ValidateWriteResults(wb,
                       {
                           0b0111'1111,  // The first mis-aligned bit, then
                                         // the first 7-bits of the span.
                           0b1000'0000   // The final bit of the span, then the
                                         // final 7 mis-aligned bits.
                       });
}

TEST(WriteUleb128, Min) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUleb128(0), IsOk());
  ValidateWriteResults(wb, {0x00});
}

TEST(WriteUleb128, Max) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUleb128(std::numeric_limits<DecodedUleb128>::max()),
              IsOk());
  ValidateWriteResults(wb, {0xff, 0xff, 0xff, 0xff, 0x0f});
}

TEST(WriteUleb128, IsControlledByGeneratorPassedInConstructor) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);
  ASSERT_NE(leb_generator, nullptr);
  WriteBitBuffer wb(1, *leb_generator);

  EXPECT_THAT(wb.WriteUleb128(0), IsOk());

  ValidateWriteResults(wb, {0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST(WriteUleb128, DefaultsToGeneratingMinimalUleb128s) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUleb128(129), IsOk());

  ValidateWriteResults(wb, {0x81, 0x01});
}

TEST(WriteUleb128, CanFailWithFixedSizeGenerator) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);
  ASSERT_NE(leb_generator, nullptr);
  WriteBitBuffer wb(1, *leb_generator);

  EXPECT_FALSE(wb.WriteUleb128(128).ok());
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

TEST(WriteBitBuffer, CapacityMayBeSmaller) {
  // The buffer may have a small initial capacity and resize as needed.
  constexpr int64_t kInitialCapacity = 0;
  WriteBitBuffer wb(kInitialCapacity);

  const std::vector<uint8_t> kSixBytes = {0, 1, 2, 3, 4, 5};

  EXPECT_THAT(wb.WriteUint8Span(absl::MakeConstSpan(kSixBytes)), IsOk());
  ValidateWriteResults(wb, kSixBytes);
}

TEST(WriteBitBuffer, CapacityMayBeLarger) {
  // The buffer may have a larger that capacity that necessary.
  WriteBitBuffer wb(/*initial_capacity=*/100);

  const std::vector<uint8_t> kSixBytes = {0, 1, 2, 3, 4, 5};

  EXPECT_THAT(wb.WriteUint8Span(absl::MakeConstSpan(kSixBytes)), IsOk());
  ValidateWriteResults(wb, kSixBytes);
}

TEST(WriteBitBuffer, ConsecutiveWrites) {
  WriteBitBuffer wb(kInitialCapacity);

  // The buffer accumulates data from all write calls.
  EXPECT_THAT(wb.WriteUnsignedLiteral(0x01, 8), IsOk());
  EXPECT_THAT(wb.WriteUnsignedLiteral64(0x0203040506070809, 64), IsOk());
  EXPECT_THAT(wb.WriteUleb128(128), IsOk());

  ValidateWriteResults(wb,
                       {// From `WriteUnsignedLiteral()`.
                        0x01,
                        // From `WriteUnsignedLiteral64()`.
                        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
                        // From `WriteLeb128()`.
                        0x80, 0x01});
}

TEST(Reset, UseAfterReset) {
  WriteBitBuffer wb(kInitialCapacity);

  EXPECT_THAT(wb.WriteUnsignedLiteral(0xabcd, 16), IsOk());

  ValidateWriteResults(wb, {0xab, 0xcd});

  // Resetting the buffer clears it.
  wb.Reset();
  ValidateWriteResults(wb, {});

  // The buffer can be used after reset . There is no trace of data before the
  // reset.
  EXPECT_THAT(wb.WriteUnsignedLiteral(100, 8), IsOk());
  ValidateWriteResults(wb, {100});
}

}  // namespace
}  // namespace iamf_tools
