/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/obu/element_gain_offset_config.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

using ::absl::MakeConstSpan;

const absl::Span<const uint8_t> kEmptyElementGainOffsetBytes = {};

const QFormatOrFloatingPoint kZeroQFormatOrFloatingPoint =
    QFormatOrFloatingPoint::MakeFromQ7_8(0);

TEST(CreateRangeType, InvalidWhenMinIsGreaterThanMax) {
  const auto kDefaultElementGainOffset = kZeroQFormatOrFloatingPoint;
  const auto kMinElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(11);
  const auto kMaxElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(10);
  EXPECT_THAT(ElementGainOffsetConfig::CreateRangeType(
                  kDefaultElementGainOffset, kMinElementGainOffset,
                  kMaxElementGainOffset),
              Not(IsOk()));
}

TEST(CreateRangeType, RangeTypeRejectsDefaultLessThanMin) {
  const auto kDefaultElementGainOffset =
      QFormatOrFloatingPoint::MakeFromQ7_8(-11);
  const auto kMinElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(-10);
  const auto kMaxElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(10);
  EXPECT_THAT(ElementGainOffsetConfig::CreateRangeType(
                  kDefaultElementGainOffset, kMinElementGainOffset,
                  kMaxElementGainOffset),
              Not(IsOk()));
}

TEST(CreateRangeType, RangeTypeRejectsDefaultGreaterThanMax) {
  const auto kDefaultElementGainOffset =
      QFormatOrFloatingPoint::MakeFromQ7_8(11);
  const auto kMinElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(-10);
  const auto kMaxElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(10);
  EXPECT_THAT(ElementGainOffsetConfig::CreateRangeType(
                  kDefaultElementGainOffset, kMinElementGainOffset,
                  kMaxElementGainOffset),
              Not(IsOk()));
}

TEST(CreateExtensionType, RejectsType0) {
  const uint8_t kElementGainOffsetConfigType = 0;
  EXPECT_THAT(ElementGainOffsetConfig::CreateExtensionType(
                  kElementGainOffsetConfigType, kEmptyElementGainOffsetBytes),
              Not(IsOk()));
}

TEST(CreateExtensionType, RejectsType1) {
  const uint8_t kElementGainOffsetConfigType = 1;
  EXPECT_THAT(ElementGainOffsetConfig::CreateExtensionType(
                  kElementGainOffsetConfigType, kEmptyElementGainOffsetBytes),
              Not(IsOk()));
}

TEST(CreateFromBuffer, FailsWithTruncatedExtension) {
  const std::vector<uint8_t> kBuffer = {3};
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(kBuffer));
  ASSERT_NE(rb, nullptr);
  EXPECT_THAT(ElementGainOffsetConfig::CreateFromBuffer(*rb), Not(IsOk()));
}

TEST(Write, ValueTypeMatchesExpected) {
  const auto config = ElementGainOffsetConfig::MakeValueType(
      /*element_gain_offset=*/QFormatOrFloatingPoint::MakeFromQ7_8(256));
  const size_t kInitialBufferSize = 3;
  WriteBitBuffer wb(kInitialBufferSize);
  const std::vector<uint8_t> kExpectedBytes = {
      // `element_gain_offset_config_type`.
      0,
      // `element_gain_offset`.
      1, 0};

  ASSERT_THAT(config.Write(wb), IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBytes);
}

TEST(Write, RangeTypeMatchesExpected) {
  const auto kDefaultElementGainOffset = kZeroQFormatOrFloatingPoint;
  const auto kMinElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(-10);
  const auto kMaxElementGainOffset = QFormatOrFloatingPoint::MakeFromQ7_8(10);
  auto config = ElementGainOffsetConfig::CreateRangeType(
      kDefaultElementGainOffset, kMinElementGainOffset, kMaxElementGainOffset);
  ASSERT_THAT(config, IsOk());
  const size_t kInitialBufferSize = 7;
  WriteBitBuffer wb(kInitialBufferSize);
  const std::vector<uint8_t> kExpectedBytes = {
      // `element_gain_offset_config_type`.
      1,
      // `default_element_gain_offset`.
      0, 0,
      // `min_element_gain_offset`.
      255, 246,
      // `max_element_gain_offset`.
      0, 10};

  ASSERT_THAT(config->Write(wb), IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBytes);
}

TEST(Write, ExtensionTypeMatchesExpected) {
  const uint8_t kElementGainOffsetConfigType = 2;
  const std::array<uint8_t, 4> kElementGainOffsetBytes = {1, 2, 3, 4};
  const auto config = ElementGainOffsetConfig::CreateExtensionType(
      kElementGainOffsetConfigType, kElementGainOffsetBytes);
  ASSERT_THAT(config, IsOk());
  const size_t kInitialBufferSize = 6;
  WriteBitBuffer wb(kInitialBufferSize);
  const std::vector<uint8_t> kExpectedBytes = {
      // `element_gain_offset_config_type`.
      2,
      // `element_gain_offset_size`.
      4,
      // `element_gain_offset_bytes`.
      1, 2, 3, 4};

  ASSERT_THAT(config->Write(wb), IsOk());

  EXPECT_EQ(wb.bit_buffer(), kExpectedBytes);
}

using ElementGainOffsetConfigSymmetricTest =
    ::testing::TestWithParam<ElementGainOffsetConfig>;

TEST_P(ElementGainOffsetConfigSymmetricTest,
       WriteAndCreateFromBufferIsSymmetric) {
  const ElementGainOffsetConfig& config = GetParam();
  const size_t kInitialBufferSize = 64;
  WriteBitBuffer wb(kInitialBufferSize);
  ASSERT_THAT(config.Write(wb), IsOk());
  ASSERT_TRUE(wb.IsByteAligned());
  const auto& buffer = wb.bit_buffer();

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(buffer));
  ASSERT_NE(rb, nullptr);
  auto config_from_buffer = ElementGainOffsetConfig::CreateFromBuffer(*rb);
  ASSERT_THAT(config_from_buffer, IsOk());
  EXPECT_EQ(*config_from_buffer, config);
}

INSTANTIATE_TEST_SUITE_P(Value, ElementGainOffsetConfigSymmetricTest,
                         testing::Values(ElementGainOffsetConfig::MakeValueType(
                             kZeroQFormatOrFloatingPoint)));

INSTANTIATE_TEST_SUITE_P(
    Range, ElementGainOffsetConfigSymmetricTest,
    testing::Values(
        ElementGainOffsetConfig::CreateRangeType(
            /*default_element_gain_offset=*/kZeroQFormatOrFloatingPoint,
            /*min_element_gain_offset=*/kZeroQFormatOrFloatingPoint,
            /*max_element_gain_offset=*/kZeroQFormatOrFloatingPoint)
            .value()));

INSTANTIATE_TEST_SUITE_P(
    Extension, ElementGainOffsetConfigSymmetricTest,
    testing::Values(ElementGainOffsetConfig::CreateExtensionType(
                        /*element_gain_offset_config_type=*/2,
                        /*element_gain_offset_bytes=*/{1, 2, 3, 4})
                        .value()));

}  // namespace
}  // namespace iamf_tools
