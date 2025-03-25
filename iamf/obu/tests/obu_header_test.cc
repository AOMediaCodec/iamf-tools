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
#include "iamf/obu/obu_header.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl::StatusCode::kResourceExhausted;
using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::Not;

// Max value of a decoded ULEB128.
constexpr uint32_t kMaxUlebDecoded = UINT32_MAX;

// The spec serializes several fields into the first byte of the OBU header.
// `obu type` (5 bits), `obu_redundant_copy` (1 bit), `obu_trimming_status_flag`
// (1 bit), `obu_extension_flag` (1 bit).
constexpr uint8_t kUnimportantFirstByte = 0x00;
constexpr uint8_t kAudioFrameId0WithTrim = 0b00110010;

class ObuHeaderTest : public testing::Test {
 public:
  ObuHeaderTest()
      : obu_header_({.obu_type = kObuIaTemporalDelimiter}),
        payload_serialized_size_(0),
        expected_data_({}) {}

  void TestGenerateAndWrite(
      absl::StatusCode expected_status_code = absl::StatusCode::kOk) {
    // Usually OBU Headers are small. The internal buffer will resize if this is
    // not large enough.
    ASSERT_NE(leb_generator_, nullptr);
    WriteBitBuffer wb(1024, *leb_generator_);

    ASSERT_NE(leb_generator_, nullptr);
    EXPECT_EQ(obu_header_.ValidateAndWrite(payload_serialized_size_, wb).code(),
              expected_status_code);
    if (expected_status_code == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_data_);
    };
  }

  std::unique_ptr<LebGenerator> leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kMinimum);

  ObuHeader obu_header_;
  int64_t payload_serialized_size_;

  std::vector<uint8_t> expected_data_;
};

const int kObuTypeBitShift = 3;
const int kObuRedundantCopyBitMask = 4;
const int kObuTrimFlagBitMask = 2;
const int kObuExtensionFlagBitMask = 1;

TEST_F(ObuHeaderTest, DefaultTemporalDelimiter) {
  expected_data_ = {kObuIaTemporalDelimiter << kObuTypeBitShift, 0};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ObuTypeAndPayloadSizeIaSequenceHeader) {
  obu_header_.obu_type = kObuIaSequenceHeader;
  payload_serialized_size_ = 6;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaSequenceHeader << kObuTypeBitShift,
                    // `obu_size`.
                    6};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ExplicitAudioFrame) {
  obu_header_.obu_type = kObuIaAudioFrame;
  payload_serialized_size_ = 64;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaAudioFrame << kObuTypeBitShift,
                    // `obu_size`.
                    64};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ImplicitAudioFrameId17) {
  obu_header_.obu_type = kObuIaAudioFrameId17;
  payload_serialized_size_ = 64;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaAudioFrameId17 << kObuTypeBitShift,
                    // `obu_size`.
                    64};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, RedundantCopy) {
  obu_header_.obu_type = kObuIaSequenceHeader;
  obu_header_.obu_redundant_copy = true;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaSequenceHeader << kObuTypeBitShift | kObuRedundantCopyBitMask,
      // `obu_size`.
      0};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, IllegalRedundantCopyFlagIaSequenceHeader) {
  obu_header_.obu_type = kObuIaTemporalDelimiter;
  obu_header_.obu_redundant_copy = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, IllegalRedundantCopyFlagParameterBlock) {
  // Parameter blocks cannot be redundant in simple or base profile.
  obu_header_.obu_type = kObuIaParameterBlock;
  obu_header_.obu_redundant_copy = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, IllegalRedundantCopyFlagAudioFrame) {
  obu_header_.obu_type = kObuIaAudioFrame;
  obu_header_.obu_redundant_copy = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, UpperEdgeObuSizeOneByteLeb128) {
  obu_header_.obu_type = kObuIaCodecConfig;
  payload_serialized_size_ = 0x7f;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0x7f};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, LowerEdgeObuSizeTwoByteLeb128) {
  obu_header_.obu_type = kObuIaCodecConfig;
  payload_serialized_size_ = (1 << 7);
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0x80, 0x01};
  TestGenerateAndWrite();
}

constexpr uint32_t kMaxObuSizeIamfV1_1_0WithMinimalLeb = 2097148;
constexpr uint32_t kMaxObuSizeIamfV1_1_0WithFixedSizeLebEight = 2097143;

TEST_F(ObuHeaderTest, TwoMegaByteObuWithMinimalLebIamfV1_1_0) {
  obu_header_.obu_type = kObuIaCodecConfig;
  payload_serialized_size_ = kMaxObuSizeIamfV1_1_0WithMinimalLeb;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0xfc, 0xff, 0x7f};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, InvalidOverTwoMegaByteObuWithMinimalLebIamfV1_1_0) {
  obu_header_.obu_type = kObuIaCodecConfig;
  payload_serialized_size_ = kMaxObuSizeIamfV1_1_0WithMinimalLeb + 1;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, TwoMegaByteObuWithFixedSizeLeb8IamfV1_1_0) {
  obu_header_.obu_type = kObuIaCodecConfig;
  payload_serialized_size_ = kMaxObuSizeIamfV1_1_0WithFixedSizeLebEight;
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);

  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0xf7, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80, 0x00};

  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, InvalidOverTwoMegaByteObuWithFixedSizeLeb8IamfV1_1_0) {
  obu_header_.obu_type = kObuIaCodecConfig;
  payload_serialized_size_ = kMaxObuSizeIamfV1_1_0WithFixedSizeLebEight + 1;
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, MaxObuSizeWithMinimalTrim) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;
  payload_serialized_size_ = kMaxObuSizeIamfV1_1_0WithMinimalLeb - 2;

  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      0xfc, 0xff, 0x7f,
      // `num_samples_to_trim_at_end`.
      0x00,
      // `num_samples_to_trim_at_start`.
      0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest,
       MaxObuSizeWithTrimUsingGenerationModeFixedSizeWithEightBytes) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;

  // Obu size includes the trim fields. This reduce the maximum payload.
  payload_serialized_size_ = kMaxObuSizeIamfV1_1_0WithFixedSizeLebEight - 16;

  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      0xf7, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80, 0x00,
      // `num_samples_to_trim_at_end`.
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
      // `num_samples_to_trim_at_start`.
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, InvalidArgumentOver32Bits) {
  payload_serialized_size_ =
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1;
  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, PayloadSizeOverflow) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  payload_serialized_size_ = std::numeric_limits<uint32_t>::max() - 1;

  // `obu_size` includes the 2 bytes of trim flags and the payload. The sum
  // surpasses the maximum value of a ULEB128.
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest,
       ValidateAndWriteFailsWhenTrimmingIsSetForIaSequenceHeader) {
  ObuHeader header(
      {.obu_type = kObuIaSequenceHeader, .obu_trimming_status_flag = true});
  WriteBitBuffer unused_wb(0);

  EXPECT_FALSE(
      header.ValidateAndWrite(payload_serialized_size_, unused_wb).ok());
}

TEST_F(ObuHeaderTest, TrimmingStatusFlagZeroTrim) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      2,
      // `num_samples_to_trim_at_end`.
      0x00,
      // `num_samples_to_trim_at_start`.
      0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, TrimmingStatusFlagNonZeroTrimAtEnd) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 1;
  obu_header_.num_samples_to_trim_at_start = 0;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      2,
      // `num_samples_to_trim_at_end`.
      0x01,
      // `num_samples_to_trim_at_start`.
      0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, TrimmingStatusFlagNonZeroTrimAtStart) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 2;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      2,
      // `num_samples_to_trim_at_end`.
      0x00,
      // `num_samples_to_trim_at_start`.
      0x02};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, TrimmingStatusFlagNonZeroBothTrims) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 1;
  obu_header_.num_samples_to_trim_at_start = 2;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      2,
      // `num_samples_to_trim_at_end`.
      0x01,
      // `num_samples_to_trim_at_start`.
      0x02};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.obu_extension_flag = true;
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);

  obu_header_.num_samples_to_trim_at_end = 1;
  obu_header_.num_samples_to_trim_at_start = 0;

  obu_header_.extension_header_size = 2;
  obu_header_.extension_header_bytes = {100, 101};

  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaAudioFrameId0 << kObuTypeBitShift |
                        kObuTrimFlagBitMask | kObuExtensionFlagBitMask,
                    // `obu_size`.
                    0x80 | 26, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
                    // `num_samples_to_trim_at_end`.
                    0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
                    // `num_samples_to_trim_at_start`.
                    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
                    // `extension_header_size_`.
                    0x82, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
                    // `extension_header_bytes_`.
                    100, 101};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, UpperEdgeOneByteLeb128ObuSizeIncludesPayloadSizeAndTrim) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 1;
  obu_header_.num_samples_to_trim_at_start = 0;
  payload_serialized_size_ = 125;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      0x7f,
      // `num_samples_to_trim_at_end`.
      0x01,
      // `num_samples_to_trim_at_start`.
      0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, LowerEdgeOneByteLeb128ObuSizeIncludesPayloadSizeAndTrim) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 1;
  obu_header_.num_samples_to_trim_at_start = 0;
  payload_serialized_size_ = 126;
  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      0x80, 0x01,
      // `num_samples_to_trim_at_end`.
      0x01,
      // `num_samples_to_trim_at_start`.
      0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, SerializedSizeTooBig) {
  obu_header_.obu_type = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);

  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;
  payload_serialized_size_ = kMaxUlebDecoded - 15;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, ExtensionHeaderSizeZero) {
  obu_header_.extension_header_size = 0;
  obu_header_.obu_extension_flag = true;
  expected_data_ = {
      kObuIaTemporalDelimiter << kObuTypeBitShift | kObuExtensionFlagBitMask, 1,
      0};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ExtensionHeaderSizeNonzero) {
  obu_header_.obu_extension_flag = true;
  obu_header_.extension_header_size = 3;
  obu_header_.extension_header_bytes = {100, 101, 102};
  expected_data_ = {
      kObuIaTemporalDelimiter << kObuTypeBitShift | kObuExtensionFlagBitMask,
      // `obu_size`.
      4,
      // `extension_header_size`.
      3,
      // `extension_header_bytes`.
      100, 101, 102};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, InconsistentExtensionHeader) {
  obu_header_.obu_extension_flag = false;
  obu_header_.extension_header_size = 1;
  obu_header_.extension_header_bytes = {100};

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, ExtensionHeaderIaSequenceHeader) {
  obu_header_.obu_type = kObuIaSequenceHeader;
  obu_header_.obu_extension_flag = true;
  obu_header_.extension_header_size = 3;
  obu_header_.extension_header_bytes = {100, 101, 102};
  payload_serialized_size_ = 6;
  expected_data_ = {
      kObuIaSequenceHeader << kObuTypeBitShift | kObuExtensionFlagBitMask,
      // `obu_size`.
      10,
      // `extension_header_size`.
      3,
      // `extension_header_bytes`.
      100, 101, 102};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ObuSizeIncludesAllConditionalFields) {
  obu_header_.obu_type = kObuIaAudioFrameId1;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.obu_extension_flag = true;
  obu_header_.num_samples_to_trim_at_end = 128;
  obu_header_.num_samples_to_trim_at_start = 128;
  obu_header_.extension_header_size = 3;
  obu_header_.extension_header_bytes = {100, 101, 102};
  payload_serialized_size_ = 1016;

  expected_data_ = {kObuIaAudioFrameId1 << kObuTypeBitShift |
                        kObuTrimFlagBitMask | kObuExtensionFlagBitMask,
                    // `obu_size == 1024`.
                    0x80, 0x08,
                    // `num_samples_to_trim_at_end`.
                    0x80, 0x01,
                    // `num_samples_to_trim_at_start`.
                    0x80, 0x01,
                    // `extension_header_size`.
                    3,
                    // `extension_header_bytes`.
                    100, 101, 102};
  TestGenerateAndWrite();
}

// --- Begin ReadAndValidate Tests ---
TEST_F(ObuHeaderTest, ReadAndValidateIncludeAllConditionalFields) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b00111011,
      // `obu_size == 1024`
      0x80, 0x08,
      // `num_samples_to_trim_at_end`
      0x80, 0x01,
      // `num_samples_to_trim_at_start`
      0x80, 0x01,
      // `extension_header_size`
      0x03,
      // `extension_header_bytes`
      100, 101, 102};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaAudioFrameId1);

  // 1024 - (2 + 2 + 1 + 3) = 1016.
  EXPECT_EQ(payload_serialized_size_, 1016);

  EXPECT_EQ(obu_header_.obu_redundant_copy, false);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, true);
  EXPECT_EQ(obu_header_.obu_extension_flag, true);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 128);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 128);
  EXPECT_EQ(obu_header_.extension_header_size, 3);
  std::vector<uint8_t> expected_extension_header_bytes = {100, 101, 102};
  for (int i = 0; i < obu_header_.extension_header_bytes.size(); ++i) {
    EXPECT_EQ(obu_header_.extension_header_bytes[i],
              expected_extension_header_bytes[i]);
  }
}

TEST_F(ObuHeaderTest, ReadAndValidateImplicitAudioFrameId17) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b10111000,
      // `obu_size == 1024`
      0x80, 0x08};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaAudioFrameId17);

  // 1024 - (0) = 1024.
  EXPECT_EQ(payload_serialized_size_, 1024);

  EXPECT_EQ(obu_header_.obu_redundant_copy, false);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, false);
  EXPECT_EQ(obu_header_.obu_extension_flag, false);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, ReadAndValidateIaSequenceHeaderNoConditionalFields) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b11111000,
      // `obu_size == 1024`
      0x80, 0x08};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaSequenceHeader);

  // 1024 - (0) = 1024.
  EXPECT_EQ(payload_serialized_size_, 1024);

  EXPECT_EQ(obu_header_.obu_redundant_copy, false);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, false);
  EXPECT_EQ(obu_header_.obu_extension_flag, false);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, ReadAndValidateIaSequenceHeaderRedundantCopy) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b11111100,
      // `obu_size == 1024`
      0x80, 0x08};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaSequenceHeader);

  // 1024 - (0) = 1024.
  EXPECT_EQ(payload_serialized_size_, 1024);

  EXPECT_EQ(obu_header_.obu_redundant_copy, true);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, false);
  EXPECT_EQ(obu_header_.obu_extension_flag, false);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, ReadAndValidateUpperEdgeObuSizeOneByteLeb128) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b00000000,
      // `obu_size == 127`
      0x7f};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaCodecConfig);

  // 127 - (0) = 127.
  EXPECT_EQ(payload_serialized_size_, 127);

  EXPECT_EQ(obu_header_.obu_redundant_copy, false);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, false);
  EXPECT_EQ(obu_header_.obu_extension_flag, false);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, ReadAndValidateLowerEdgeObuSizeTwoByteLeb128) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b00000000,
      // `obu_size == 128`
      0x80, 0x01};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaCodecConfig);

  // 128 - (0) = 128.
  EXPECT_EQ(payload_serialized_size_, 128);

  EXPECT_EQ(obu_header_.obu_redundant_copy, false);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, false);
  EXPECT_EQ(obu_header_.obu_extension_flag, false);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, InvalidWhenObuWouldExceedTwoMegabytes_FourByteObuSize) {
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 268435456 - 1`
                                      0xff, 0xff, 0xff, 0x7f};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_FALSE(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_)
          .ok());
}

TEST_F(ObuHeaderTest, InvalidWhenObuWouldExceedTwoMegabytes_FiveByteObuSize) {
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 268435456`
                                      0x80, 0x80, 0x80, 0x80, 0x01};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_FALSE(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_)
          .ok());
}

TEST_F(ObuHeaderTest, InvalidWhenObuWouldExceedTwoMegabytes_MaxByteObuSize) {
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 4294967295`
                                      0xff, 0xff, 0xff, 0xff, 0x0f};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_FALSE(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_)
          .ok());
}

TEST_F(ObuHeaderTest, MaxObuSizeWithMinimalLeb128) {
  // When the size field is encoded using three bytes, the maximum value it can
  // represent is (2 megabytes - 4 bytes).
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 2 megabytes - 4`
                                      0xfc, 0xff, 0x7f};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.

  EXPECT_EQ(payload_serialized_size_, kMaxObuSizeIamfV1_1_0WithMinimalLeb);
}

TEST_F(ObuHeaderTest, InvalidEdgeOverMaxSizeWithMinimalLeb128) {
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 2 megabytes - 3`
                                      0xfd, 0xff, 0x7f};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  EXPECT_FALSE(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_)
          .ok());
}

TEST_F(ObuHeaderTest, MaxObuSizeWithFixesSizeLebEightBytes) {
  // When the size field is encoded using eight bytes, the maximum value it can
  // represent is (2 megabytes - 9 bytes).
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 2 megabytes - 9`
                                      0xf7, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80,
                                      0x00};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.

  EXPECT_EQ(payload_serialized_size_,
            kMaxObuSizeIamfV1_1_0WithFixedSizeLebEight);
}

TEST_F(ObuHeaderTest, InvalidEdgeOverMaxSizeWithFixedSizeLebEightBytes) {
  std::vector<uint8_t> source_data = {kUnimportantFirstByte,
                                      // `obu_size == 2 megabytes - 8`
                                      0xf8, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80,
                                      0x00};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  EXPECT_FALSE(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_)
          .ok());
}

TEST_F(ObuHeaderTest, ReadAndValidateMaxObuSizeWithMinimalTrim) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b00110010,
      // `obu_size`
      2,
      // `num_samples_to_trim_at_end`.
      0x00,
      // `num_samples_to_trim_at_start`.
      0x00};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  EXPECT_EQ(obu_header_.obu_type, kObuIaAudioFrameId0);

  // The obu header consumes the two bytes of the `obu_size` field.
  EXPECT_EQ(payload_serialized_size_, 0);

  EXPECT_EQ(obu_header_.obu_redundant_copy, false);
  EXPECT_EQ(obu_header_.obu_trimming_status_flag, true);
  EXPECT_EQ(obu_header_.obu_extension_flag, false);

  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest,
       ReadAndValidateIllegalTrimmingStatusFlagIaSequenceHeader) {
  std::vector<uint8_t> source_data = {
      // `obu type`, `obu_redundant_copy`, `obu_trimming_status_flag`,
      // `obu_extension_flag`
      0b11111010,
      // `obu_size`
      2,
      // `num_samples_to_trim_at_end`.
      0x00,
      // `num_samples_to_trim_at_start`.
      0x00};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_FALSE(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_)
          .ok());
}

void ValidateAudioFrameId0WithTrim(const ObuHeader& header) {
  EXPECT_EQ(header.obu_type, kObuIaAudioFrameId0);
  EXPECT_EQ(header.obu_redundant_copy, false);
  EXPECT_EQ(header.obu_trimming_status_flag, true);
  EXPECT_EQ(header.obu_extension_flag, false);
}

TEST_F(ObuHeaderTest, ReadAndValidateTrimmingStatusFlagNonZeroTrimAtEnd) {
  std::vector<uint8_t> source_data = {kAudioFrameId0WithTrim,
                                      // `obu_size`
                                      2,
                                      // `num_samples_to_trim_at_end`.
                                      0x01,
                                      // `num_samples_to_trim_at_start`.
                                      0x00};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  ValidateAudioFrameId0WithTrim(obu_header_);
  EXPECT_EQ(payload_serialized_size_, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 1);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 0);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, ReadAndValidateTrimmingStatusFlagNonZeroTrimAtStart) {
  std::vector<uint8_t> source_data = {kAudioFrameId0WithTrim,
                                      // `obu_size`
                                      2,
                                      // `num_samples_to_trim_at_end`.
                                      0x00,
                                      // `num_samples_to_trim_at_start`.
                                      0x02};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  ValidateAudioFrameId0WithTrim(obu_header_);
  EXPECT_EQ(payload_serialized_size_, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 2);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, ReadAndValidateTrimmingStatusFlagNonZeroBothTrims) {
  std::vector<uint8_t> source_data = {kAudioFrameId0WithTrim,
                                      // `obu_size`
                                      2,
                                      // `num_samples_to_trim_at_end`.
                                      0x01,
                                      // `num_samples_to_trim_at_start`.
                                      0x02};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      IsOk());

  // Validate all OBU Header fields.
  ValidateAudioFrameId0WithTrim(obu_header_);
  EXPECT_EQ(payload_serialized_size_, 0);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_end, 1);
  EXPECT_EQ(obu_header_.num_samples_to_trim_at_start, 2);
  EXPECT_EQ(obu_header_.extension_header_size, 0);
  EXPECT_TRUE(obu_header_.extension_header_bytes.empty());
}

TEST_F(ObuHeaderTest, NegativePayloadSizeNotAcceptable) {
  std::vector<uint8_t> source_data = {kAudioFrameId0WithTrim,
                                      // `obu_size`
                                      2,
                                      // `num_samples_to_trim_at_end`.
                                      0x80, 0x01,
                                      // `num_samples_to_trim_at_start`.
                                      0x02};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(
      obu_header_.ReadAndValidate(*read_bit_buffer, payload_serialized_size_),
      Not(IsOk()));
}

TEST(PeekObuTypeAndTotalObuSize, Success) {
  std::vector<uint8_t> source_data = {kAudioFrameId0WithTrim,
                                      // `obu_size`
                                      2};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  auto start_position = read_bit_buffer->Tell();

  auto header_metadata =
      ObuHeader::PeekObuTypeAndTotalObuSize(*read_bit_buffer);

  EXPECT_THAT(header_metadata, IsOk());
  EXPECT_EQ(header_metadata->obu_type, kObuIaAudioFrameId0);
  // obu_size + size_of(obu_size) + 1, 2 + 1 + 1 = 4.
  EXPECT_EQ(header_metadata->total_obu_size, 4);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(PeekObuTypeAndTotalObuSize, SuccessWithMaxSizedObuSize) {
  std::vector<uint8_t> source_data = {kAudioFrameId0WithTrim,
                                      // `obu_size == 2 megabytes - 9`
                                      0xf7, 0xff, 0xff, 0x80, 0x80, 0x80, 0x80,
                                      0x00};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  auto start_position = read_bit_buffer->Tell();

  auto header_metadata =
      ObuHeader::PeekObuTypeAndTotalObuSize(*read_bit_buffer);

  EXPECT_THAT(header_metadata, IsOk());
  EXPECT_EQ(header_metadata->obu_type, kObuIaAudioFrameId0);
  // obu_size + size_of(obu_size) + 1.
  EXPECT_EQ(header_metadata->total_obu_size,
            kMaxObuSizeIamfV1_1_0WithFixedSizeLebEight + 8 + 1);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(PeekObuTypeAndTotalObuSize, EmptyBitBufferResourceExhausted) {
  std::vector<uint8_t> source_data = {};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  auto start_position = read_bit_buffer->Tell();

  auto header_metadata =
      ObuHeader::PeekObuTypeAndTotalObuSize(*read_bit_buffer);

  EXPECT_THAT(header_metadata, Not(IsOk()));
  EXPECT_THAT(header_metadata, StatusIs(kResourceExhausted));
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(PeekObuTypeAndTotalObuSize, NoObuSizeResourceExhausted) {
  std::vector<uint8_t> source_data = {kObuIaAudioFrameId0};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  auto start_position = read_bit_buffer->Tell();
  auto header_metadata =
      ObuHeader::PeekObuTypeAndTotalObuSize(*read_bit_buffer);
  EXPECT_THAT(header_metadata, Not(IsOk()));
  EXPECT_THAT(header_metadata,
              absl_testing::StatusIs(absl::StatusCode::kResourceExhausted));
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(PeekObuTypeAndTotalObuSize, ReturnsResourceExhaustedForPartialObuSize) {
  std::vector<uint8_t> source_data = {kObuIaAudioFrameId0, 0x80};
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  auto start_position = read_bit_buffer->Tell();

  auto header_metadata =
      ObuHeader::PeekObuTypeAndTotalObuSize(*read_bit_buffer);

  EXPECT_THAT(header_metadata, Not(IsOk()));
  EXPECT_THAT(header_metadata,
              absl_testing::StatusIs(absl::StatusCode::kResourceExhausted));
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

}  // namespace
}  // namespace iamf_tools
