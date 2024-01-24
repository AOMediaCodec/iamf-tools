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
#include "iamf/obu_header.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"
#include "iamf/tests/test_utils.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {
namespace {

// Max value of a decoded ULEB128.
constexpr uint32_t kMaxUlebDecoded = UINT32_MAX;

class ObuHeaderTest : public testing::Test {
 public:
  ObuHeaderTest()
      : obu_type_(kObuIaTemporalDelimiter),
        obu_header_(),
        payload_serialized_size_(0),
        expected_data_({}) {}

  void TestGenerateAndWrite(
      absl::StatusCode expected_status_code = absl::StatusCode::kOk) {
    // Usually OBU Headers are small. The internal buffer will resize if this is
    // not large enough.
    ASSERT_NE(leb_generator_, nullptr);
    WriteBitBuffer wb(1024, *leb_generator_);

    ASSERT_NE(leb_generator_, nullptr);
    EXPECT_EQ(
        obu_header_.ValidateAndWrite(obu_type_, payload_serialized_size_, wb)
            .code(),
        expected_status_code);
    if (expected_status_code == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_data_);
    };
  }

  std::unique_ptr<LebGenerator> leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kMinimum);

  ObuType obu_type_;
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
  obu_type_ = kObuIaSequenceHeader;
  payload_serialized_size_ = 6;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaSequenceHeader << kObuTypeBitShift,
                    // `obu_size`.
                    6};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ExplicitAudioFrame) {
  obu_type_ = kObuIaAudioFrame;
  payload_serialized_size_ = 64;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaAudioFrame << kObuTypeBitShift,
                    // `obu_size`.
                    64};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, ImplicitAudioFrameId17) {
  obu_type_ = kObuIaAudioFrameId17;
  payload_serialized_size_ = 64;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaAudioFrameId17 << kObuTypeBitShift,
                    // `obu_size`.
                    64};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, RedundantCopy) {
  obu_type_ = kObuIaSequenceHeader;
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
  obu_type_ = kObuIaTemporalDelimiter;
  obu_header_.obu_redundant_copy = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, IllegalRedundantCopyFlagParameterBlock) {
  // Parameter blocks cannot be redundant in simple or base profile.
  obu_type_ = kObuIaParameterBlock;
  obu_header_.obu_redundant_copy = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, IllegalRedundantCopyFlagAudioFrame) {
  obu_type_ = kObuIaAudioFrame;
  obu_header_.obu_redundant_copy = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, UpperEdgeObuSizeOneByteLeb128) {
  obu_type_ = kObuIaCodecConfig;
  payload_serialized_size_ = 0x7f;
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0x7f};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, LowerEdgeObuSizeTwoByteLeb128) {
  obu_type_ = kObuIaCodecConfig;
  payload_serialized_size_ = (1 << 7);
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0x80, 0x01};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, UpperEdgeObuSizeFourByteLeb128) {
  obu_type_ = kObuIaCodecConfig;
  payload_serialized_size_ = ((1 << 28) - 1);
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0xff, 0xff, 0xff, 0x7f};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, LowerEdgeObuSizeFiveByteLeb128) {
  obu_type_ = kObuIaCodecConfig;
  payload_serialized_size_ = (1 << 28);

  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0x80, 0x80, 0x80, 0x80, 0x01};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, MaxObuSizeFullPayload) {
  obu_type_ = kObuIaCodecConfig;
  payload_serialized_size_ = std::numeric_limits<uint32_t>::max();
  expected_data_ = {// `obu_type`, `obu_redundant_copy`,
                    // `obu_trimming_status_flag, `obu_extension_flag`.
                    kObuIaCodecConfig << kObuTypeBitShift,
                    // `obu_size`.
                    0xff, 0xff, 0xff, 0xff, 0x0f};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, InvalidArgumentOver32Bits) {
  payload_serialized_size_ =
      static_cast<int64_t>(std::numeric_limits<uint32_t>::max()) + 1;
  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, MaxObuSizeWithMinimalTrim) {
  obu_type_ = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;
  payload_serialized_size_ = std::numeric_limits<uint32_t>::max() - 2;

  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      0xff, 0xff, 0xff, 0xff, 0x0f,
      // `num_samples_to_trim_at_end`.
      0x00,
      // `num_samples_to_trim_at_start`.
      0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, PayloadSizeOverflow) {
  obu_type_ = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  payload_serialized_size_ = std::numeric_limits<uint32_t>::max() - 1;

  // `obu_size` includes the 2 bytes of trim flags and the payload. The sum
  // surpasses the maximum value of a ULEB128.
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest,
       MaxObuSizeWithTrimUsingGenerationModeFixedSizeWithEightBytes) {
  obu_type_ = kObuIaAudioFrameId0;
  obu_header_.obu_trimming_status_flag = true;
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);
  obu_header_.num_samples_to_trim_at_end = 0;
  obu_header_.num_samples_to_trim_at_start = 0;

  // Obu size includes the trim fields. This reduce the maximum payload.
  payload_serialized_size_ = std::numeric_limits<uint32_t>::max() - 16;

  expected_data_ = {
      // `obu_type`, `obu_redundant_copy`,
      // `obu_trimming_status_flag, `obu_extension_flag`.
      kObuIaAudioFrameId0 << kObuTypeBitShift | kObuTrimFlagBitMask,
      // `obu_size`.
      0xff, 0xff, 0xff, 0xff, 0x8f, 0x80, 0x80, 0x00,
      // `num_samples_to_trim_at_end`.
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
      // `num_samples_to_trim_at_start`.
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  TestGenerateAndWrite();
}

TEST_F(ObuHeaderTest, IllegalTrimmingStatusFlagIaSequenceHeader) {
  obu_type_ = kObuIaSequenceHeader;
  obu_header_.obu_trimming_status_flag = true;

  TestGenerateAndWrite(absl::StatusCode::kInvalidArgument);
}

TEST_F(ObuHeaderTest, TrimmingStatusFlagZeroTrim) {
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_type_ = kObuIaAudioFrameId0;
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
  obu_header_.obu_extension_flag = true;
  obu_type_ = kObuIaSequenceHeader;
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
  obu_type_ = kObuIaAudioFrameId1;
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

}  // namespace
}  // namespace iamf_tools
