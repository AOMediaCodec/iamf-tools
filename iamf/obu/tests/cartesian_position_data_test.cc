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

#include "iamf/obu/cartesian_position_data.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using ::testing::Not;

// ============================================================================
// CreateFromBuffer - Single Point 8-Bit Tests
// ============================================================================

TEST(CreateFromBuffer, ReadsSinglePointStepAnimation8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (0 = kStep)
      0x00,
      // `x` start_value
      0x01,
      // `y` start_value (-2)
      0xfe,
      // `z` start_value (127)
      0x7f,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k8Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 127);
}

TEST(CreateFromBuffer, ReadsSinglePointLinearAnimation8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (1 = kLinear)
      0x01,
      // `x` start_value, end_value
      0x01,
      0x02,
      // `y` start_value (-2), end_value (-3)
      0xfe,
      0xfd,
      // `z` start_value (127), end_value (0)
      0x7f,
      0x00,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k8Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().y.end_point_value(), -3);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 127);
  EXPECT_EQ(*data->first_position().z.end_point_value(), 0);
}

TEST(CreateFromBuffer, ReadsSinglePointBezierAnimation8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (2 = kBezier)
      0x02,
      // `x` start, end, control, rel_time
      0x01,
      0x02,
      0x03,
      0x05,
      // `y` start (-2), end (-3), control (-4), rel_time
      0xfe,
      0xfd,
      0xfc,
      0x0a,
      // `z` start (127), end (0), control (50), rel_time
      0x7f,
      0x00,
      0x32,
      0x0f,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kBezier);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k8Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().x.control_point_value(), 3);
  EXPECT_EQ(*data->first_position().x.control_point_relative_time(), 5);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().y.end_point_value(), -3);
  EXPECT_EQ(*data->first_position().y.control_point_value(), -4);
  EXPECT_EQ(*data->first_position().y.control_point_relative_time(), 10);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 127);
  EXPECT_EQ(*data->first_position().z.end_point_value(), 0);
  EXPECT_EQ(*data->first_position().z.control_point_value(), 50);
  EXPECT_EQ(*data->first_position().z.control_point_relative_time(), 15);
}

// ============================================================================
// CreateFromBuffer - Single Point 16-Bit Tests
// ============================================================================

TEST(CreateFromBuffer, ReadsSinglePointStepAnimation16Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (0 = kStep)
      0x00,
      // `x` start_value (1)
      0x00,
      0x01,
      // `y` start_value (-2)
      0xff,
      0xfe,
      // `z` start_value (32767)
      0x7f,
      0xff,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k16Bit, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 32767);
}

TEST(CreateFromBuffer, ReadsSinglePointLinearAnimation16Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (1 = kLinear)
      0x01,
      // `x` start_value (1), end_value (2)
      0x00,
      0x01,
      0x00,
      0x02,
      // `y` start_value (-2), end_value (-3)
      0xff,
      0xfe,
      0xff,
      0xfd,
      // `z` start_value (32767), end_value (0)
      0x7f,
      0xff,
      0x00,
      0x00,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k16Bit, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().y.end_point_value(), -3);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 32767);
  EXPECT_EQ(*data->first_position().z.end_point_value(), 0);
}

TEST(CreateFromBuffer, ReadsSinglePointBezierAnimation16Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (2 = kBezier)
      0x02,
      // `x` start (1), end (2), control (3), rel_time (5)
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
      0x05,
      // `y` start (-2), end (-3), control (-4), rel_time (10)
      0xff,
      0xfe,
      0xff,
      0xfd,
      0xff,
      0xfc,
      0x0a,
      // `z` start (32767), end (0), control (50), rel_time (15)
      0x7f,
      0xff,
      0x00,
      0x00,
      0x00,
      0x32,
      0x0f,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k16Bit, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kBezier);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().x.control_point_value(), 3);
  EXPECT_EQ(*data->first_position().x.control_point_relative_time(), 5);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().y.end_point_value(), -3);
  EXPECT_EQ(*data->first_position().y.control_point_value(), -4);
  EXPECT_EQ(*data->first_position().y.control_point_relative_time(), 10);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 32767);
  EXPECT_EQ(*data->first_position().z.end_point_value(), 0);
  EXPECT_EQ(*data->first_position().z.control_point_value(), 50);
  EXPECT_EQ(*data->first_position().z.control_point_relative_time(), 15);
}

// ============================================================================
// CreateFromBuffer - Dual Point Tests
// ============================================================================

TEST(CreateFromBuffer, ReadsDualPointStepAnimation8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (0 = kStep)
      0x00,
      // `first_x`, `first_y`, `first_z`
      0x01,
      0x02,
      0x03,
      // `second_x`, `second_y`, `second_z`
      0x04,
      0x05,
      0x06,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k8Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().y.start_point_value(), 2);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 3);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->x.start_point_value(), 4);
  EXPECT_EQ(*data->second_position()->y.start_point_value(), 5);
  EXPECT_EQ(*data->second_position()->z.start_point_value(), 6);
}

TEST(CreateFromBuffer, ReadsDualPointLinearAnimation8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (1 = kLinear)
      0x01,
      // `first_x` start, end
      0x01,
      0x02,
      // `first_y` start, end
      0x03,
      0x04,
      // `first_z` start, end
      0x05,
      0x06,
      // `second_x` start, end
      0x07,
      0x08,
      // `second_y` start, end
      0x09,
      0x0a,
      // `second_z` start, end
      0x0b,
      0x0c,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().y.start_point_value(), 3);
  EXPECT_EQ(*data->first_position().y.end_point_value(), 4);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 5);
  EXPECT_EQ(*data->first_position().z.end_point_value(), 6);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->x.start_point_value(), 7);
  EXPECT_EQ(*data->second_position()->x.end_point_value(), 8);
  EXPECT_EQ(*data->second_position()->y.start_point_value(), 9);
  EXPECT_EQ(*data->second_position()->y.end_point_value(), 10);
  EXPECT_EQ(*data->second_position()->z.start_point_value(), 11);
  EXPECT_EQ(*data->second_position()->z.end_point_value(), 12);
}

TEST(CreateFromBuffer, ReadsDualPointStepAnimation16Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (0 = kStep)
      0x00,
      // `first_x`, `first_y`, `first_z`
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
      // `second_x`, `second_y`, `second_z`
      0x00,
      0x04,
      0x00,
      0x05,
      0x00,
      0x06,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k16Bit, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().y.start_point_value(), 2);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 3);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->x.start_point_value(), 4);
  EXPECT_EQ(*data->second_position()->y.start_point_value(), 5);
  EXPECT_EQ(*data->second_position()->z.start_point_value(), 6);
}

TEST(CreateFromBuffer, ReadsDualPointLinearAnimation16Bit) {
  std::vector<uint8_t> source = {
      // `animation_type` (1 = kLinear)
      0x01,
      // `first_x` start (1), end (2)
      0x00,
      0x01,
      0x00,
      0x02,
      // `first_y` start (3), end (4)
      0x00,
      0x03,
      0x00,
      0x04,
      // `first_z` start (5), end (6)
      0x00,
      0x05,
      0x00,
      0x06,
      // `second_x` start (7), end (8)
      0x00,
      0x07,
      0x00,
      0x08,
      // `second_y` start (9), end (10)
      0x00,
      0x09,
      0x00,
      0x0a,
      // `second_z` start (11), end (12)
      0x00,
      0x0b,
      0x00,
      0x0c,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = CartesianPositionData::CreateFromBuffer(
      *buffer, CartesianBitDepth::k16Bit, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->x.start_point_value(), 7);
  EXPECT_EQ(*data->second_position()->x.end_point_value(), 8);
}

// ============================================================================
// CreateFromBuffer - Failure Tests
// ============================================================================

TEST(CreateFromBuffer, ReturnsErrorWhenNotEnoughBytesSingle8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type`
      0x00,
      // `x` start_value
      0x01,
      // `y` start_value
      0xfe,
      // `z` missing
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(CartesianPositionData::CreateFromBuffer(
                  *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/false),
              Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorWhenNotEnoughBytesSingle16Bit) {
  std::vector<uint8_t> source = {
      // `animation_type`
      0x00,
      // `x` start_value
      0x00,
      0x01,
      // `y` start_value
      0x00,
      0x02,
      // `z` only 1 byte
      0x00,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(CartesianPositionData::CreateFromBuffer(
                  *buffer, CartesianBitDepth::k16Bit, /*is_dual=*/false),
              Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorWhenNotEnoughBytesDual8Bit) {
  std::vector<uint8_t> source = {
      // `animation_type`
      0x00,
      // first position
      0x01,
      0x02,
      0x03,
      // second position incomplete
      0x04,
      0x05,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(CartesianPositionData::CreateFromBuffer(
                  *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/true),
              Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorForInvalidAnimationType) {
  std::vector<uint8_t> source = {
      // invalid animation type
      0x05,
      0x01,
      0x01,
      0x01,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(CartesianPositionData::CreateFromBuffer(
                  *buffer, CartesianBitDepth::k8Bit, /*is_dual=*/false),
              Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorForInvalidBitDepth) {
  std::vector<uint8_t> source = {0x00, 0x01, 0x02, 0x03};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(CartesianPositionData::CreateFromBuffer(
                  *buffer, static_cast<CartesianBitDepth>(24),
                  /*is_dual=*/false),
              Not(IsOk()));
}

// ============================================================================
// Create (Single Point) Tests
// ============================================================================

TEST(Create, CreatesSinglePointStepAnimation8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(5),
      .y = AnimatedParameterData<int16_t>::MakeStep(-10),
      .z = AnimatedParameterData<int16_t>::MakeStep(100),
  };

  auto data = CartesianPositionData::Create(AnimationType::kStep,
                                            CartesianBitDepth::k8Bit, pos);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k8Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 5);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -10);
  EXPECT_EQ(*data->first_position().z.start_point_value(), 100);
}

TEST(Create, CreatesSinglePointLinearAnimation8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .y = AnimatedParameterData<int16_t>::MakeLinear(-2, -3),
      .z = AnimatedParameterData<int16_t>::MakeLinear(127, 0),
  };

  auto data = CartesianPositionData::Create(AnimationType::kLinear,
                                            CartesianBitDepth::k8Bit, pos);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
}

TEST(Create, CreatesSinglePointBezierAnimation8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeBezier(1, 2, 3, 5),
      .y = AnimatedParameterData<int16_t>::MakeBezier(-2, -3, -4, 10),
      .z = AnimatedParameterData<int16_t>::MakeBezier(127, 0, 50, 15),
  };

  auto data = CartesianPositionData::Create(AnimationType::kBezier,
                                            CartesianBitDepth::k8Bit, pos);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kBezier);
  EXPECT_EQ(*data->first_position().x.control_point_value(), 3);
}

TEST(Create, CreatesSinglePointInterLinearAnimation8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeInterLinear(2),
      .y = AnimatedParameterData<int16_t>::MakeInterLinear(-3),
      .z = AnimatedParameterData<int16_t>::MakeInterLinear(0),
  };

  auto data = CartesianPositionData::Create(AnimationType::kInterLinear,
                                            CartesianBitDepth::k8Bit, pos);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kInterLinear);
  EXPECT_EQ(*data->first_position().x.end_point_value(), 2);
}

TEST(Create, CreatesSinglePointStepAnimation16Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(32767),
      .y = AnimatedParameterData<int16_t>::MakeStep(-32768),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  auto data = CartesianPositionData::Create(AnimationType::kStep,
                                            CartesianBitDepth::k16Bit, pos);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 32767);
  EXPECT_EQ(*data->first_position().y.start_point_value(), -32768);
}

TEST(Create, ReturnsErrorWhen8BitCoordinateExceedsMaximum) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(128),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kStep,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhen8BitCoordinateBelowMinimum) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(-129),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kStep,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhen8BitEndPointExceedsMaximum) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeLinear(0, 128),
      .y = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      .z = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kLinear,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhen8BitControlPointExceedsMaximum) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeBezier(0, 0, 128, 5),
      .y = AnimatedParameterData<int16_t>::MakeBezier(0, 0, 0, 5),
      .z = AnimatedParameterData<int16_t>::MakeBezier(0, 0, 0, 5),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kBezier,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenAnimationTypeMismatchesX) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      .z = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kLinear,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenAnimationTypeMismatchesY) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kStep,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenAnimationTypeMismatchesZ) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
  };

  EXPECT_THAT(CartesianPositionData::Create(AnimationType::kStep,
                                            CartesianBitDepth::k8Bit, pos),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorForInvalidBitDepth) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(
      CartesianPositionData::Create(AnimationType::kStep,
                                    static_cast<CartesianBitDepth>(24), pos),
      Not(IsOk()));
}

// ============================================================================
// CreateDual (Dual Points) Tests
// ============================================================================

TEST(CreateDual, CreatesDualPointStepAnimation8Bit) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(1),
      .y = AnimatedParameterData<int16_t>::MakeStep(2),
      .z = AnimatedParameterData<int16_t>::MakeStep(3),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(4),
      .y = AnimatedParameterData<int16_t>::MakeStep(5),
      .z = AnimatedParameterData<int16_t>::MakeStep(6),
  };

  auto data = CartesianPositionData::CreateDual(
      AnimationType::kStep, CartesianBitDepth::k8Bit, pos1, pos2);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k8Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->x.start_point_value(), 4);
}

TEST(CreateDual, CreatesDualPointLinearAnimation16Bit) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeLinear(1000, 2000),
      .y = AnimatedParameterData<int16_t>::MakeLinear(3000, 4000),
      .z = AnimatedParameterData<int16_t>::MakeLinear(5000, 6000),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeLinear(7000, 8000),
      .y = AnimatedParameterData<int16_t>::MakeLinear(9000, 10000),
      .z = AnimatedParameterData<int16_t>::MakeLinear(11000, 12000),
  };

  auto data = CartesianPositionData::CreateDual(
      AnimationType::kLinear, CartesianBitDepth::k16Bit, pos1, pos2);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*data->first_position().x.start_point_value(), 1000);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->x.start_point_value(), 7000);
}

TEST(CreateDual, ReturnsErrorWhenFirstPositionCoordinateOutOfBounds) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(128),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(CartesianPositionData::CreateDual(
                  AnimationType::kStep, CartesianBitDepth::k8Bit, pos1, pos2),
              Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorWhenSecondPositionCoordinateOutOfBounds) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(-129),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(CartesianPositionData::CreateDual(
                  AnimationType::kStep, CartesianBitDepth::k8Bit, pos1, pos2),
              Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorWhenSecondPositionAnimationTypeMismatches) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      .y = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      .z = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
  };

  EXPECT_THAT(CartesianPositionData::CreateDual(
                  AnimationType::kStep, CartesianBitDepth::k8Bit, pos1, pos2),
              Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForInvalidBitDepth) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(0),
      .y = AnimatedParameterData<int16_t>::MakeStep(0),
      .z = AnimatedParameterData<int16_t>::MakeStep(0),
  };

  EXPECT_THAT(
      CartesianPositionData::CreateDual(
          AnimationType::kStep, static_cast<CartesianBitDepth>(24), pos1, pos2),
      Not(IsOk()));
}

// ============================================================================
// Write - Single Point Tests
// ============================================================================

TEST(Write, StepAnimationWritesCorrectly8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(5),
      .y = AnimatedParameterData<int16_t>::MakeStep(-10),
      .z = AnimatedParameterData<int16_t>::MakeStep(100),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kStep, CartesianBitDepth::k8Bit, pos);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(4);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`
                                 0x00,
                                 // `x`
                                 5,
                                 // `y` (-10 = 246)
                                 246,
                                 // `z`
                                 100,
                             }));
}

TEST(Write, LinearAnimationWritesCorrectly8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .y = AnimatedParameterData<int16_t>::MakeLinear(-2, -3),
      .z = AnimatedParameterData<int16_t>::MakeLinear(127, 0),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kLinear, CartesianBitDepth::k8Bit, pos);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(7);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`
                                 0x01,
                                 // `x` start, end
                                 0x01,
                                 0x02,
                                 // `y` start (-2 = 254), end (-3 = 253)
                                 254,
                                 253,
                                 // `z` start, end
                                 127,
                                 0x00,
                             }));
}

TEST(Write, BezierAnimationWritesCorrectly8Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeBezier(1, 2, 3, 5),
      .y = AnimatedParameterData<int16_t>::MakeBezier(-2, -3, -4, 10),
      .z = AnimatedParameterData<int16_t>::MakeBezier(127, 0, 50, 15),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kBezier, CartesianBitDepth::k8Bit, pos);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(13);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`
                                 0x02,
                                 // `x` start, end, control, rel_time
                                 0x01,
                                 0x02,
                                 0x03,
                                 0x05,
                                 // `y` start, end, control, rel_time
                                 254,
                                 253,
                                 252,
                                 0x0a,
                                 // `z` start, end, control, rel_time
                                 127,
                                 0x00,
                                 50,
                                 0x0f,
                             }));
}

TEST(Write, StepAnimationWritesCorrectly16Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(1),
      .y = AnimatedParameterData<int16_t>::MakeStep(-2),
      .z = AnimatedParameterData<int16_t>::MakeStep(32767),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kStep, CartesianBitDepth::k16Bit, pos);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(7);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`
                                 0x00,
                                 // `x` start_value (1)
                                 0x00,
                                 0x01,
                                 // `y` start_value (-2)
                                 0xff,
                                 0xfe,
                                 // `z` start_value (32767)
                                 0x7f,
                                 0xff,
                             }));
}

TEST(Write, LinearAnimationWritesCorrectly16Bit) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .y = AnimatedParameterData<int16_t>::MakeLinear(-2, -3),
      .z = AnimatedParameterData<int16_t>::MakeLinear(32767, 0),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kLinear, CartesianBitDepth::k16Bit, pos);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(13);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`
                                 0x01,
                                 // `x` start, end
                                 0x00,
                                 0x01,
                                 0x00,
                                 0x02,
                                 // `y` start, end
                                 0xff,
                                 0xfe,
                                 0xff,
                                 0xfd,
                                 // `z` start, end
                                 0x7f,
                                 0xff,
                                 0x00,
                                 0x00,
                             }));
}

// ============================================================================
// Write - Dual Point Tests
// ============================================================================

TEST(Write, StepAnimationWritesCorrectlyForDualPoints8Bit) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(1),
      .y = AnimatedParameterData<int16_t>::MakeStep(2),
      .z = AnimatedParameterData<int16_t>::MakeStep(3),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(4),
      .y = AnimatedParameterData<int16_t>::MakeStep(5),
      .z = AnimatedParameterData<int16_t>::MakeStep(6),
  };
  const auto data = CartesianPositionData::CreateDual(
      AnimationType::kStep, CartesianBitDepth::k8Bit, pos1, pos2);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(7);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 0x00,
                                 0x01,
                                 0x02,
                                 0x03,
                                 0x04,
                                 0x05,
                                 0x06,
                             }));
}

TEST(Write, StepAnimationWritesCorrectlyForDualPoints16Bit) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(1),
      .y = AnimatedParameterData<int16_t>::MakeStep(2),
      .z = AnimatedParameterData<int16_t>::MakeStep(3),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(4),
      .y = AnimatedParameterData<int16_t>::MakeStep(5),
      .z = AnimatedParameterData<int16_t>::MakeStep(6),
  };
  const auto data = CartesianPositionData::CreateDual(
      AnimationType::kStep, CartesianBitDepth::k16Bit, pos1, pos2);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(13);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 0x00,
                                 0x00,
                                 0x01,
                                 0x00,
                                 0x02,
                                 0x00,
                                 0x03,
                                 0x00,
                                 0x04,
                                 0x00,
                                 0x05,
                                 0x00,
                                 0x06,
                             }));
}

// ============================================================================
// AbslStringify / ToString Tests
// ============================================================================

TEST(AbslStringify, FormatsStepAnimationForSinglePoint) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeStep(5),
      .y = AnimatedParameterData<int16_t>::MakeStep(-10),
      .z = AnimatedParameterData<int16_t>::MakeStep(100),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kStep, CartesianBitDepth::k8Bit, pos);
  ASSERT_THAT(data, IsOk());

  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 0\n"
            "    x:\n"
            "     // Step\n"
            "     start_point_value= 5\n"
            "    y:\n"
            "     // Step\n"
            "     start_point_value= -10\n"
            "    z:\n"
            "     // Step\n"
            "     start_point_value= 100");
}

TEST(AbslStringify, FormatsLinearAnimationForSinglePoint) {
  CartesianPosition pos{
      .x = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .y = AnimatedParameterData<int16_t>::MakeLinear(-2, -3),
      .z = AnimatedParameterData<int16_t>::MakeLinear(127, 0),
  };
  const auto data = CartesianPositionData::Create(
      AnimationType::kLinear, CartesianBitDepth::k8Bit, pos);
  ASSERT_THAT(data, IsOk());

  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 1\n"
            "    x:\n"
            "     // Linear\n"
            "     start_point_value= 1\n"
            "     end_point_value= 2\n"
            "    y:\n"
            "     // Linear\n"
            "     start_point_value= -2\n"
            "     end_point_value= -3\n"
            "    z:\n"
            "     // Linear\n"
            "     start_point_value= 127\n"
            "     end_point_value= 0");
}

TEST(AbslStringify, FormatsStepAnimationForDualPoints) {
  CartesianPosition pos1{
      .x = AnimatedParameterData<int16_t>::MakeStep(1),
      .y = AnimatedParameterData<int16_t>::MakeStep(2),
      .z = AnimatedParameterData<int16_t>::MakeStep(3),
  };
  CartesianPosition pos2{
      .x = AnimatedParameterData<int16_t>::MakeStep(4),
      .y = AnimatedParameterData<int16_t>::MakeStep(5),
      .z = AnimatedParameterData<int16_t>::MakeStep(6),
  };
  const auto data = CartesianPositionData::CreateDual(
      AnimationType::kStep, CartesianBitDepth::k8Bit, pos1, pos2);
  ASSERT_THAT(data, IsOk());

  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 0\n"
            "    first_x:\n"
            "     // Step\n"
            "     start_point_value= 1\n"
            "    first_y:\n"
            "     // Step\n"
            "     start_point_value= 2\n"
            "    first_z:\n"
            "     // Step\n"
            "     start_point_value= 3\n"
            "    second_x:\n"
            "     // Step\n"
            "     start_point_value= 4\n"
            "    second_y:\n"
            "     // Step\n"
            "     start_point_value= 5\n"
            "    second_z:\n"
            "     // Step\n"
            "     start_point_value= 6");
}

}  // namespace
}  // namespace iamf_tools
