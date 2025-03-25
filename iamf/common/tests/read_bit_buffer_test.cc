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

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/utils/bit_buffer_util.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {
using absl::StatusCode::kInvalidArgument;
using absl::StatusCode::kResourceExhausted;

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;

constexpr int kBitsPerByte = 8;
constexpr int kMaxUint32 = std::numeric_limits<uint32_t>::max();

constexpr std::array<uint8_t, 3> kThreeBytes = {0x01, 0x23, 0x45};

TEST(FileBasedReadBitBufferTest, CreateFromFilePathFailsWithNegativeCapacity) {
  const auto file_path = GetAndCleanupOutputFileName(".iamf");
  EXPECT_THAT(FileBasedReadBitBuffer::CreateFromFilePath(-1, file_path),
              ::testing::IsNull());
}

TEST(StreamBasedReadBitBufferTest, CreateFromStreamFailsWithNegativeCapacity) {
  EXPECT_THAT(StreamBasedReadBitBuffer::Create(-1), ::testing::IsNull());
}

TEST(StreamBasedReadBitBufferTest, PushBytesFailsWithTooManyBytes) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  const std::vector<uint8_t> source_data(
      (kEntireObuSizeMaxTwoMegabytes * 2) + 1, 0);
  EXPECT_FALSE(rb->PushBytes(absl::MakeConstSpan(source_data)).ok());
}

TEST(StreamBasedReadBitBufferTest, PushBytesSucceedsWithTwoMaxSizedObus) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  const std::vector<uint8_t> source_data(kEntireObuSizeMaxTwoMegabytes, 0);
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(source_data)), IsOk());
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(source_data)), IsOk());
  const std::vector<uint8_t> one_byte(1, 0);
  EXPECT_FALSE(rb->PushBytes(absl::MakeConstSpan(one_byte)).ok());
}

template <typename BufferReaderType>
std::unique_ptr<BufferReaderType> CreateConcreteReadBitBuffer(
    int64_t capacity, absl::Span<const uint8_t> source_data);

template <>
std::unique_ptr<MemoryBasedReadBitBuffer> CreateConcreteReadBitBuffer(
    int64_t capacity, absl::Span<const uint8_t> source_data) {
  return MemoryBasedReadBitBuffer::CreateFromSpan(source_data);
}

template <>
std::unique_ptr<FileBasedReadBitBuffer> CreateConcreteReadBitBuffer(
    int64_t capacity, absl::Span<const uint8_t> source_data) {
  // First write the content of `source_data` into a temporary file.
  const auto output_filename = GetAndCleanupOutputFileName(".iamf");
  std::ofstream ofs(output_filename, std::ios::binary | std::ios::out);
  ofs.write(reinterpret_cast<const char*>(source_data.data()),
            source_data.size());
  ofs.close();

  // Then create a `FileBasedReadBitBuffer` from the temporary file.
  return FileBasedReadBitBuffer::CreateFromFilePath(capacity, output_filename);
}

template <>
std::unique_ptr<StreamBasedReadBitBuffer> CreateConcreteReadBitBuffer(
    int64_t capacity, absl::Span<const uint8_t> source_data) {
  auto rb = StreamBasedReadBitBuffer::Create(capacity);
  EXPECT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(source_data), IsOk());
  return rb;
}

template <typename BufferReaderType>
class ReadBitBufferTest : public ::testing::Test {
 protected:
  void CreateReadBitBuffer() {
    rb_ = CreateConcreteReadBitBuffer<BufferReaderType>(
        this->rb_capacity_, absl::MakeConstSpan(source_data_));
    EXPECT_NE(this->rb_, nullptr);
    EXPECT_EQ(this->rb_->Tell(), 0);
  }

  std::vector<uint8_t> source_data_;

  int64_t rb_capacity_ = 0;
  std::unique_ptr<ReadBitBuffer> rb_;
};

using BufferReaderTypes =
    ::testing::Types<MemoryBasedReadBitBuffer, FileBasedReadBitBuffer,
                     StreamBasedReadBitBuffer>;
TYPED_TEST_SUITE(ReadBitBufferTest, BufferReaderTypes);

TYPED_TEST(ReadBitBufferTest, CreateReadBitBufferSucceeds) {
  this->source_data_ = {};
  this->rb_capacity_ = 0;
  this->CreateReadBitBuffer();
  EXPECT_NE(this->rb_, nullptr);
}

// ---- Seek and Tell Tests -----
TYPED_TEST(ReadBitBufferTest, SeekAndTellMatch) {
  this->source_data_ = {0xab, 0xcd, 0xef};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  // Start at position 0.
  EXPECT_EQ(this->rb_->Tell(), 0);

  // Move to various positions and expect that the positions are updated.
  EXPECT_THAT(this->rb_->Seek(3), IsOk());
  EXPECT_EQ(this->rb_->Tell(), 3);

  EXPECT_THAT(this->rb_->Seek(17), IsOk());
  EXPECT_EQ(this->rb_->Tell(), 17);

  EXPECT_THAT(this->rb_->Seek(23), IsOk());
  EXPECT_EQ(this->rb_->Tell(), 23);

  EXPECT_THAT(this->rb_->Seek(10), IsOk());
  EXPECT_EQ(this->rb_->Tell(), 10);
}

TYPED_TEST(ReadBitBufferTest, SeekFailsWithNegativePosition) {
  this->source_data_ = {0xab, 0xcd, 0xef};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  EXPECT_THAT(this->rb_->Seek(-1), StatusIs(kInvalidArgument));
}

TYPED_TEST(ReadBitBufferTest, SeekFailsWithPositionTooLarge) {
  this->source_data_ = {0xab, 0xcd, 0xef};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  EXPECT_THAT(this->rb_->Seek(24), StatusIs(kResourceExhausted));
}

// ---- ReadUnsignedLiteral Tests -----
TYPED_TEST(ReadBitBufferTest, ReadZeroBitsFromEmptySourceSucceeds) {
  this->source_data_ = {};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(0, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0);
  EXPECT_EQ(this->rb_->Tell(), 0);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedAllBits) {
  this->source_data_ = {0xab, 0xcd, 0xef};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(24, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(this->rb_->Tell(), 24);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralByteAlignedMultipleReads) {
  this->source_data_ = {0xab, 0xcd, 0xef, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(24, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0xabcdef);
  EXPECT_EQ(this->rb_->Tell(), 24);

  // Second read to same output integer - will be overwritten.
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(8, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0xff);
  EXPECT_EQ(this->rb_->Tell(), 32);
}

TYPED_TEST(ReadBitBufferTest,
           ReadUnsignedLiteralByteAlignedNotEnoughBitsInBufferOrSource) {
  this->source_data_ = {0xab, 0xcd, 0xef};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint64_t output_literal = 0;
  // We request more bits than there are in the buffer. ReadUnsignedLiteral will
  // attempt to load more bits from source into the buffer, but that will fail,
  // since there aren't enough bits in the source either.
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(32, output_literal),
              StatusIs(kResourceExhausted));
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralNotByteAlignedOneByte) {
  this->source_data_ = {0b10100001};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint8_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(3, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b101);
  EXPECT_EQ(this->rb_->Tell(), 3);

  // Read 5 bits more and expect the position is at 8 bits.
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(5, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b00001);
  EXPECT_EQ(this->rb_->Tell(), 8);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralNotByteAlignedMultipleReads) {
  this->source_data_ = {0b11000101, 0b10000010, 0b00000110};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint64_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(6, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b110001);
  EXPECT_EQ(this->rb_->Tell(), 6);

  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(10, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b0110000010);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralRequestTooLarge) {
  this->source_data_ = {0b00000101, 0b00000010, 0b00000110};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint64_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(65, output_literal),
              StatusIs(kInvalidArgument));
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralAfterSeek) {
  this->source_data_ = {0b00000111, 0b10000000};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  // Move the position to the 6-th bit, which points to the first "1".
  EXPECT_THAT(this->rb_->Seek(5), IsOk());
  EXPECT_EQ(this->rb_->Tell(), 5);

  // Read in 4 bits, which are all "1"s.
  uint64_t output_literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(4, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b1111);

  // Read in another 7 bits, which are all "0"s.
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(7, output_literal), IsOk());
  EXPECT_EQ(output_literal, 0b0000000);
}

// ---- ReadULeb128 Tests -----

// Successful Uleb128 reads.
TYPED_TEST(ReadBitBufferTest, ReadUleb128Read5Bytes) {
  this->source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb), IsOk());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  EXPECT_EQ(this->rb_->Tell(), 40);
}

TYPED_TEST(ReadBitBufferTest, ReadUleb128Read5BytesAndStoreSize) {
  this->source_data_ = {0x81, 0x83, 0x81, 0x83, 0x0f};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  int8_t encoded_leb_size = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb, encoded_leb_size), IsOk());
  EXPECT_EQ(output_leb, 0b11110000011000000100000110000001);
  EXPECT_EQ(encoded_leb_size, 5);
  EXPECT_EQ(this->rb_->Tell(), 40);
}

TYPED_TEST(ReadBitBufferTest, ReadUleb128TwoBytes) {
  this->source_data_ = {0x81, 0x03, 0x81, 0x83, 0x0f};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb), IsOk());

  // Expect the buffer to read only the first two bytes, since 0x03 does not
  // have a one in the most significant spot of the byte.
  EXPECT_EQ(output_leb, 0b00000110000001);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, ReadUleb128ExtraZeroes) {
  this->source_data_ = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb), IsOk());
  EXPECT_EQ(output_leb, 0b1);
  EXPECT_EQ(this->rb_->Tell(), 64);
}

TYPED_TEST(ReadBitBufferTest, ReadUleb128ExtraZeroesAndStoreSize) {
  this->source_data_ = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  int8_t encoded_leb_size = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb, encoded_leb_size), IsOk());
  EXPECT_EQ(output_leb, 0b1);
  EXPECT_EQ(encoded_leb_size, 8);
  EXPECT_EQ(this->rb_->Tell(), 64);
}

// Uleb128 read errors.
TYPED_TEST(ReadBitBufferTest, ReadUleb128Overflow) {
  this->source_data_ = {0x80, 0x80, 0x80, 0x80, 0x10};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb), StatusIs(kInvalidArgument));
}

TYPED_TEST(ReadBitBufferTest, ReadUleb128TooManyBytes) {
  this->source_data_ = {0x80, 0x83, 0x81, 0x83, 0x80, 0x80, 0x80, 0x80};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb), StatusIs(kInvalidArgument));
}

TYPED_TEST(ReadBitBufferTest, ReadUleb128NotEnoughDataInBufferOrSource) {
  this->source_data_ = {0x80, 0x80, 0x80, 0x80};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  DecodedUleb128 output_leb = 0;

  // Buffer has a one in the most significant position of each byte, which tells
  // us to continue reading to the next byte. The 4th byte tells us to read the
  // next byte, but there is no 5th byte in neither the buffer nor the source.
  EXPECT_THAT(this->rb_->ReadULeb128(output_leb), StatusIs(kResourceExhausted));
}

// ---- ReadIso14496_1Expanded Tests -----
// This is to emulate type-parameterized tests as value-parameterized tests
// (since there is no native support for both in one test suite). The enum
// is used to create different concrete types of buffer readers. Then we
// augment the tests by taking the cartesian product of {test values} and
// {types of buffer readers} (using `testing::Combine`).
enum BufferReaderType { kMemoryBased, kFileBased, kStreamBased };
struct SourceAndSize {
  std::vector<uint8_t> source_data;
  uint32_t expected_size_of_instance;
};

using ReadIso14496_1ExpandedTest =
    ::testing::TestWithParam<std::tuple<SourceAndSize, BufferReaderType>>;

TEST_P(ReadIso14496_1ExpandedTest, ReadIso14496_1Expanded) {
  const auto& [source_and_size, buffer_reader_type] = GetParam();

  // Copy the source data because the `ReadBitBuffer` will consume it.
  std::vector<uint8_t> source_data = source_and_size.source_data;
  std::unique_ptr<ReadBitBuffer> rb;
  if (buffer_reader_type == kMemoryBased) {
    rb = CreateConcreteReadBitBuffer<MemoryBasedReadBitBuffer>(
        source_data.size() * kBitsPerByte, absl::MakeSpan(source_data));
  } else if (buffer_reader_type == kFileBased) {
    rb = CreateConcreteReadBitBuffer<FileBasedReadBitBuffer>(
        source_data.size() * kBitsPerByte, absl::MakeSpan(source_data));
  } else {
    rb = CreateConcreteReadBitBuffer<StreamBasedReadBitBuffer>(
        source_data.size() * kBitsPerByte, absl::MakeSpan(source_data));
  }

  uint32_t output_size_of_instance = 0;
  EXPECT_THAT(rb->ReadIso14496_1Expanded(kMaxUint32, output_size_of_instance),
              IsOk());

  EXPECT_EQ(output_size_of_instance, source_and_size.expected_size_of_instance);
}

INSTANTIATE_TEST_SUITE_P(OneByteInput, ReadIso14496_1ExpandedTest,
                         testing::Combine(testing::ValuesIn<SourceAndSize>({
                                              {{0x00}, 0},
                                              {{0x40}, 64},
                                              {{0x7f}, 127},
                                          }),
                                          testing::Values(kMemoryBased,
                                                          kFileBased)));

INSTANTIATE_TEST_SUITE_P(TwoByteInput, ReadIso14496_1ExpandedTest,
                         testing::Combine(testing::ValuesIn<SourceAndSize>({
                                              {{0x81, 0x00}, 128},
                                              {{0x81, 0x01}, 129},
                                              {{0xff, 0x7e}, 0x3ffe},
                                              {{0xff, 0x7f}, 0x3fff},
                                          }),
                                          testing::Values(kMemoryBased,
                                                          kFileBased)));

INSTANTIATE_TEST_SUITE_P(
    FourByteInput, ReadIso14496_1ExpandedTest,
    testing::Combine(testing::ValuesIn<SourceAndSize>({
                         {{0x81, 0x80, 0x80, 0x00}, 0x0200000},
                         {{0x81, 0x80, 0x80, 0x01}, 0x0200001},
                         {{0xff, 0xff, 0xff, 0x7e}, 0x0ffffffe},
                         {{0xff, 0xff, 0xff, 0x7f}, 0x0fffffff},
                     }),
                     testing::Values(kMemoryBased, kFileBased)));

INSTANTIATE_TEST_SUITE_P(
    FiveByteInput, ReadIso14496_1ExpandedTest,
    testing::Combine(testing::ValuesIn<SourceAndSize>({
                         {{0x81, 0x80, 0x80, 0x80, 0x00}, 0x10000000},
                         {{0x8f, 0x80, 0x80, 0x80, 0x00}, 0xf0000000},
                         {{0x8f, 0xff, 0xff, 0xff, 0x7f}, 0xffffffff},
                     }),
                     testing::Values(kMemoryBased, kFileBased)));

INSTANTIATE_TEST_SUITE_P(
    HandlesLeadingZeroes, ReadIso14496_1ExpandedTest,
    testing::Combine(testing::ValuesIn<SourceAndSize>({
                         {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 1},
                     }),
                     testing::Values(kMemoryBased, kFileBased)));

TYPED_TEST(ReadBitBufferTest,
           ReadIso14496_1ExpandedSucceedsWhenDecodedValueEqualToMaxClassSize) {
  constexpr uint32_t kMaxClassSizeExact = 127;
  this->source_data_ = {0x7f};
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  uint32_t unused_output = 0;
  EXPECT_THAT(
      this->rb_->ReadIso14496_1Expanded(kMaxClassSizeExact, unused_output),
      IsOk());
}

TYPED_TEST(
    ReadBitBufferTest,
    ReadIso14496_1ExpandedFailsWhenDecodedValueIsGreaterThanMaxClassSize) {
  constexpr uint32_t kMaxClassSizeTooLow = 126;
  this->source_data_ = {0x7f};
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  uint32_t unused_output = 0;
  EXPECT_FALSE(
      this->rb_->ReadIso14496_1Expanded(kMaxClassSizeTooLow, unused_output)
          .ok());
}

TYPED_TEST(ReadBitBufferTest,
           ReadIso14496_1ExpandedFailsWhenDecodedValueDoesNotFitIntoUint32) {
  this->source_data_ = {0x90, 0x80, 0x80, 0x80, 0x00};
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  uint32_t unused_output = 0;
  EXPECT_FALSE(
      this->rb_->ReadIso14496_1Expanded(kMaxUint32, unused_output).ok());
}

TYPED_TEST(ReadBitBufferTest,
           ReadIso14496_1ExpandedFailsWhenInputDataSignalsMoreThan8Bytes) {
  this->source_data_ = {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01};
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  uint32_t unused_output = 0;
  EXPECT_FALSE(
      this->rb_->ReadIso14496_1Expanded(kMaxUint32, unused_output).ok());
}

// --- `ReadUint8Span` tests ---

// Successful usage of `ReadUint8Span`.
TYPED_TEST(ReadBitBufferTest, ReadUint8SpanSucceedsWithAlignedBuffer) {
  this->source_data_ = {0x01, 0x23, 0x45, 0x68, 0x89};
  const auto source_data_copy = this->source_data_;
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  std::vector<uint8_t> output(source_data_copy.size());
  EXPECT_THAT(this->rb_->ReadUint8Span(absl::MakeSpan(output)), IsOk());
  EXPECT_EQ(output, source_data_copy);
  EXPECT_EQ(this->rb_->Tell(), 40);
}

TYPED_TEST(ReadBitBufferTest, ReadUint8SpanSucceedsWithMisalignedBuffer) {
  // Prepare the buffer with source data, but where partial bytes have been
  // read, so later reads are not on byte boundaries.
  this->source_data_ = {0xab, 0xcd, 0xef, 0x01, 0x23};
  constexpr size_t kOffsetBits = 4;
  constexpr std::array<uint8_t, 4> kExpectedOutput{0xbc, 0xde, 0xf0, 0x12};
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  // Read a 4-bit literal to misalign the buffer.
  uint8_t literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(kOffsetBits, literal), IsOk());
  EXPECT_EQ(this->rb_->Tell(), kOffsetBits);

  std::vector<uint8_t> output(4);
  EXPECT_THAT(this->rb_->ReadUint8Span(absl::MakeSpan(output)), IsOk());
  EXPECT_THAT(output, testing::ElementsAreArray(kExpectedOutput));
  EXPECT_EQ(this->rb_->Tell(), 36);
}

// `ReadUint8Span` errors.
TYPED_TEST(ReadBitBufferTest,
           ReadUint8SpanFailsNotEnoughDataInBufferToFillSpan) {
  constexpr size_t kSourceSize = 4;
  constexpr size_t kOutputSizeTooLarge = 5;

  this->source_data_.resize(kSourceSize);
  this->rb_capacity_ = this->source_data_.size() * kBitsPerByte;
  this->CreateReadBitBuffer();

  std::vector<uint8_t> output(kOutputSizeTooLarge);
  EXPECT_THAT(this->rb_->ReadUint8Span(absl::MakeSpan(output)),
              StatusIs(kResourceExhausted));
}

// --- ReadBoolean tests ---

// Successful ReadBoolean reads
TYPED_TEST(ReadBitBufferTest, ReadBoolean8Bits) {
  this->source_data_ = {0b10011001};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  bool output = false;
  std::vector<bool> expected_output = {true, false, false, true,
                                       true, false, false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_THAT(this->rb_->ReadBoolean(output), IsOk());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_EQ(this->rb_->Tell(), 8);
}

TYPED_TEST(ReadBitBufferTest, ReadBooleanMisalignedBuffer) {
  this->source_data_ = {0b10000001, 0b01000000};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  uint64_t literal = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(2, literal), IsOk());
  EXPECT_EQ(this->rb_->Tell(), 2);

  bool output = false;
  // Expected output starts reading at bit 2 instead of at 0.
  std::vector<bool> expected_output = {false, false, false, false,
                                       false, true,  false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_THAT(this->rb_->ReadBoolean(output), IsOk());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_EQ(this->rb_->Tell(), 10);
}

// ReadBoolean Error
TYPED_TEST(ReadBitBufferTest, ReadBooleanNotEnoughDataInBufferOrSource) {
  this->source_data_ = {0b10011001};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  bool output = false;
  std::vector<bool> expected_output = {true, false, false, true,
                                       true, false, false, true};
  for (int i = 0; i < 8; ++i) {
    EXPECT_THAT(this->rb_->ReadBoolean(output), IsOk());
    EXPECT_EQ(output, expected_output[i]);
  }
  EXPECT_THAT(this->rb_->ReadBoolean(output), StatusIs(kResourceExhausted));
}

// --- ReadSigned16 tests ---

TYPED_TEST(ReadBitBufferTest, Signed16Zero) {
  this->source_data_ = {0x00, 0x00};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(this->rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, 0);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, Signed16MaxPositive) {
  this->source_data_ = {0x7f, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(this->rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, 32767);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, Signed16MinPositive) {
  this->source_data_ = {0x00, 0x01};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(this->rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, 1);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, Signed16MinNegative) {
  this->source_data_ = {0x80, 0x00};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(this->rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, -32768);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, Signed16MaxNegative) {
  this->source_data_ = {0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();

  int16_t output = 0;
  EXPECT_THAT(this->rb_->ReadSigned16(output), IsOk());
  EXPECT_EQ(output, -1);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, IsDataAvailable) {
  this->source_data_ = {0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  EXPECT_TRUE(this->rb_->IsDataAvailable());
  uint64_t output = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(16, output), IsOk());
  EXPECT_FALSE(this->rb_->IsDataAvailable());
}

TYPED_TEST(ReadBitBufferTest, CanReadBytes) {
  this->source_data_ = {0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  EXPECT_TRUE(this->rb_->CanReadBytes(2));
  EXPECT_FALSE(this->rb_->CanReadBytes(3));
  uint64_t output = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(16, output), IsOk());
  EXPECT_FALSE(this->rb_->CanReadBytes(1));
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralMax32) {
  this->source_data_ = {0xff, 0xff, 0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint32_t output = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(32, output), IsOk());
  EXPECT_EQ(output, 4294967295);
  EXPECT_EQ(this->rb_->Tell(), 32);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteral32Overflow) {
  this->source_data_ = {0xff, 0xff, 0xff, 0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint32_t output = 0;
  EXPECT_FALSE(this->rb_->ReadUnsignedLiteral(40, output).ok());
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralMax16) {
  this->source_data_ = {0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint16_t output = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(16, output), IsOk());
  EXPECT_EQ(output, 65535);
  EXPECT_EQ(this->rb_->Tell(), 16);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteral16Overflow) {
  this->source_data_ = {0xff, 0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint16_t output = 0;
  EXPECT_FALSE(this->rb_->ReadUnsignedLiteral(24, output).ok());
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteralMax8) {
  this->source_data_ = {0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint8_t output = 0;
  EXPECT_THAT(this->rb_->ReadUnsignedLiteral(8, output), IsOk());
  EXPECT_EQ(output, 255);
  EXPECT_EQ(this->rb_->Tell(), 8);
}

TYPED_TEST(ReadBitBufferTest, ReadUnsignedLiteral8Overflow) {
  this->source_data_ = {0xff, 0xff};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  uint8_t output = 0;
  EXPECT_FALSE(this->rb_->ReadUnsignedLiteral(9, output).ok());
}

TYPED_TEST(ReadBitBufferTest, StringOnlyNullCharacter) {
  this->source_data_ = {'\0'};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(this->rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "");
}

TYPED_TEST(ReadBitBufferTest, StringAscii) {
  this->source_data_ = {'A', 'B', 'C', '\0'};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(this->rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "ABC");
}

TYPED_TEST(ReadBitBufferTest, StringOverrideOutputParam) {
  this->source_data_ = {'A', 'B', 'C', '\0'};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output = "xyz";
  EXPECT_THAT(this->rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "ABC");
}

TYPED_TEST(ReadBitBufferTest, StringUtf8) {
  this->source_data_ = {0xc3, 0xb3,              // A 1-byte UTF-8 character.
                        0xf0, 0x9d, 0x85, 0x9f,  // A 4-byte UTF-8 character.
                        '\0'};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(this->rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, "\303\263\360\235\205\237");
}

TYPED_TEST(ReadBitBufferTest, StringMaxLength) {
  this->source_data_.assign(kIamfMaxStringSize - 1, 'a');
  this->source_data_.push_back('\0');
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output;
  EXPECT_THAT(this->rb_->ReadString(output), IsOk());
  EXPECT_EQ(output, std::string(kIamfMaxStringSize - 1, 'a'));
}

TYPED_TEST(ReadBitBufferTest, InvalidStringMissingNullTerminator) {
  this->source_data_ = {'a', 'b', 'c'};
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output;
  EXPECT_FALSE(this->rb_->ReadString(output).ok());
}

TYPED_TEST(ReadBitBufferTest, InvalidStringMissingNullTerminatorMaxLength) {
  this->source_data_.assign(kIamfMaxStringSize, 'a');
  this->rb_capacity_ = 1024;
  this->CreateReadBitBuffer();
  std::string output;
  EXPECT_FALSE(this->rb_->ReadString(output).ok());
}

// --- Specific StreamBasedReadBitBuffer tests ---

// --- `Flush` tests ---
TEST(StreamBasedReadBitBufferTest, FlushFailsWhenTryingToFlushTooManyBytes) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  std::vector<uint8_t> output_buffer(kThreeBytes.size());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output_buffer)), IsOk());
  EXPECT_FALSE(rb->Flush(kThreeBytes.size() + 1).ok());
}

TEST(StreamBasedReadBitBufferTest, FlushSuccessfullyEmptiesSource) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  std::vector<uint8_t> output_buffer(kThreeBytes.size());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output_buffer)), IsOk());
  EXPECT_THAT(rb->Flush(kThreeBytes.size()), IsOk());
  EXPECT_FALSE(rb->IsDataAvailable());
}

TEST(StreamBasedReadBitBufferTest,
     FlushPartiallyEmptiesSourceButSubsequentReadsSucceed) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  std::vector<uint8_t> output = {0};
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output)), IsOk());
  EXPECT_THAT(rb->Flush(output.size()), IsOk());
  EXPECT_TRUE(rb->IsDataAvailable());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output)), IsOk());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output)), IsOk());
}

TEST(StreamBasedReadBitBufferTest, FlushAndPushingMoreDataSucceeds) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  std::vector<uint8_t> output_buffer(kThreeBytes.size());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output_buffer)), IsOk());
  EXPECT_THAT(rb->Flush(output_buffer.size()), IsOk());
  EXPECT_FALSE(rb->IsDataAvailable());
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  EXPECT_TRUE(rb->IsDataAvailable());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output_buffer)), IsOk());
}

TEST(StreamBasedReadBitBufferTest, TellFlushAndSeek) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  EXPECT_EQ(rb->Tell(), 0);
  EXPECT_THAT(rb->Flush(kThreeBytes.size()), IsOk());
  // Seeking is disabled after Flush().
  EXPECT_FALSE(rb->Seek(0).ok());
}

TEST(StreamBasedReadBitBufferTest, PushBytesCanReadBytesSucceeds) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);
  EXPECT_FALSE(rb->CanReadBytes(1));
  EXPECT_THAT(rb->PushBytes(absl::MakeConstSpan(kThreeBytes)), IsOk());
  EXPECT_TRUE(rb->CanReadBytes(3));
  std::vector<uint8_t> output_buffer(kThreeBytes.size());
  EXPECT_THAT(rb->ReadUint8Span(absl::MakeSpan(output_buffer)), IsOk());
  EXPECT_FALSE(rb->CanReadBytes(1));
  EXPECT_THAT(rb->Flush(kThreeBytes.size()), IsOk());
  EXPECT_FALSE(rb->CanReadBytes(1));
  EXPECT_THAT(rb->PushBytes(kThreeBytes), IsOk());
  EXPECT_TRUE(rb->CanReadBytes(3));
}

TEST(StreamBasedReadBitBufferTest, PushBytesFailsOnNegativeNumBytes) {
  auto rb = StreamBasedReadBitBuffer::Create(1024);
  EXPECT_NE(rb, nullptr);

  EXPECT_DEATH(rb->CanReadBytes(-1), "");
}
}  // namespace
}  // namespace iamf_tools
