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
#include "iamf/obu/polar_position_data.h"

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
// CreateFromBuffer - Single Point Tests
// ============================================================================

TEST(CreateFromBuffer, ReadsSinglePointStepAnimation) {
  // Expected values:
  //   `azimuth`   (9-bit signed)   =  1 (binary 0'0000'0001)
  //   `elevation` (8-bit signed)   = -2 (binary 1111'1110)
  //   `distance`  (7-bit unsigned) = 127 (binary 111'1111)
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: azimuth_start[8:1] (00000000)
      0b00000000,
      // Byte 2: azimuth_start[0] (1) | elevation_start[7:1] (1111111)
      0b11111111,
      // Byte 3: elevation_start[0] (0) | distance_start[6:0] (1111111)
      0b01111111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_position().azimuth.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().elevation.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().distance.start_point_value(), 127);
}

TEST(CreateFromBuffer, ReadsSinglePointLinearAnimation) {
  // Expected values:
  //   `azimuth`   (9-bit signed):   start = 1,  end = 2
  //   `elevation` (8-bit signed):   start = -2, end = -3
  //   `distance`  (7-bit unsigned): start = 127, end = 0
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (1 = kLinear)
      0b00000001,
      // Byte 1: azimuth_start[8:1] (00000000)
      0b00000000,
      // Byte 2: azimuth_start[0] (1) | azimuth_end[8:2] (0000000)
      0b10000000,
      // Byte 3: azimuth_end[1:0] (10) | elevation_start[7:2] (111111)
      0b10111111,
      // Byte 4: elevation_start[1:0] (10) | elevation_end[7:2] (111111)
      0b10111111,
      // Byte 5: elevation_end[1:0] (01) | distance_start[6:1] (111111)
      0b01111111,
      // Byte 6: distance_start[0] (1) | distance_end[6:0] (0000000)
      0b10000000,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_position().azimuth.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().azimuth.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().elevation.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().elevation.end_point_value(), -3);
  EXPECT_EQ(*data->first_position().distance.start_point_value(), 127);
  EXPECT_EQ(*data->first_position().distance.end_point_value(), 0);
}

TEST(CreateFromBuffer, ClipsSinglePointAzimuthAndElevation) {
  // Input values in bitstream:
  //   azimuth = 181 (clipped to 180)
  //   elevation = 91 (clipped to 90)
  //   distance = 4
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: azimuth_start[8:1] (181 = 0b0'1011'0101)
      0b01011010,
      // Byte 2: azimuth_start[0] (1) | elevation_start[7:1] (91 = 0b0101'1011)
      0b10101101,
      // Byte 3: elevation_start[0] (1) | distance_start[6:0] (4 = 0b000'0100)
      0b10000100,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/false);

  ASSERT_THAT(data, IsOk());
  EXPECT_FALSE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_position().azimuth.start_point_value(), 180);
  EXPECT_EQ(*data->first_position().elevation.start_point_value(), 90);
  EXPECT_EQ(*data->first_position().distance.start_point_value(), 4);
}

TEST(CreateFromBuffer, ReturnsErrorWhenNotEnoughBytesForSinglePoint) {
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: azimuth_start[8:1]
      0b00000000,
      // Byte 2: azimuth_start[0] | elevation_start[7:1]
      0b11111111,
      // Missing Byte 3
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/false);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorForInvalidAnimationTypeInSinglePoint) {
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (invalid = 5)
      0b00000101,
      // Dummy payload bytes
      0b00000000,
      0b11111111,
      0b01111111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/false);

  EXPECT_THAT(data, Not(IsOk()));
}

// ============================================================================
// CreateFromBuffer - Dual Points Tests
// ============================================================================

TEST(CreateFromBuffer, ReadsDualPointsStepAnimation) {
  // Expected values:
  //   first_azimuth = 1, first_elevation = -2, first_distance = 127
  //   second_azimuth = -1, second_elevation = 2, second_distance = 64
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: first_azimuth_start[8:1] (00000000)
      0b00000000,
      // Byte 2: first_azimuth_start[0] (1) | first_elevation_start[7:1]
      // (1111111)
      0b11111111,
      // Byte 3: first_elevation_start[0] (0) | first_distance_start[6:0]
      // (1111111)
      0b01111111,
      // Byte 4: second_azimuth_start[8:1] (11111111)
      0b11111111,
      // Byte 5: second_azimuth_start[0] (1) | second_elevation_start[7:1]
      // (0000001)
      0b10000001,
      // Byte 6: second_elevation_start[0] (0) | second_distance_start[6:0]
      // (1000000)
      0b01000000,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_position().azimuth.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().elevation.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().distance.start_point_value(), 127);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->azimuth.start_point_value(), -1);
  EXPECT_EQ(*data->second_position()->elevation.start_point_value(), 2);
  EXPECT_EQ(*data->second_position()->distance.start_point_value(), 64);
}

TEST(CreateFromBuffer, ReadsDualPointsLinearAnimation) {
  // Expected values:
  //   first_azimuth: start = 1, end = 2
  //   first_elevation: start = -2, end = -3
  //   first_distance: start = 127, end = 0
  //   second_azimuth: start = -1, end = -2
  //   second_elevation: start = 2, end = 3
  //   second_distance: start = 64, end = 32
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (1 = kLinear)
      0b00000001,
      // Bytes 1..6: first polar coordinates
      0b00000000,
      0b10000000,
      0b10111111,
      0b10111111,
      0b01111111,
      0b10000000,
      // Bytes 7..12: second polar coordinates
      0b11111111,
      0b11111111,
      0b10000000,
      0b10000000,
      0b11100000,
      0b00100000,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_position().azimuth.start_point_value(), 1);
  EXPECT_EQ(*data->first_position().azimuth.end_point_value(), 2);
  EXPECT_EQ(*data->first_position().elevation.start_point_value(), -2);
  EXPECT_EQ(*data->first_position().elevation.end_point_value(), -3);
  EXPECT_EQ(*data->first_position().distance.start_point_value(), 127);
  EXPECT_EQ(*data->first_position().distance.end_point_value(), 0);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->azimuth.start_point_value(), -1);
  EXPECT_EQ(*data->second_position()->azimuth.end_point_value(), -2);
  EXPECT_EQ(*data->second_position()->elevation.start_point_value(), 2);
  EXPECT_EQ(*data->second_position()->elevation.end_point_value(), 3);
  EXPECT_EQ(*data->second_position()->distance.start_point_value(), 64);
  EXPECT_EQ(*data->second_position()->distance.end_point_value(), 32);
}

TEST(CreateFromBuffer, ClipsDualPointsAzimuthAndElevation) {
  // Input values in bitstream:
  //   first_azimuth = 181 (clipped to 180), first_elevation = 91 (clipped to
  //   90), first_distance = 4 second_azimuth = -181 (clipped to -180),
  //   second_elevation = -91 (clipped to -90), second_distance = 7
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: first_azimuth_start[8:1] (181 = 0b0'1011'0101)
      0b01011010,
      // Byte 2: first_azimuth_start[0] (1) | first_elevation_start[7:1] (91 =
      // 0b0101'1011)
      0b10101101,
      // Byte 3: first_elevation_start[0] (1) | first_distance_start[6:0] (4 =
      // 0b000'0100)
      0b10000100,
      // Byte 4: second_azimuth_start[8:1] (-181 = 0b1'0100'1011)
      0b10100101,
      // Byte 5: second_azimuth_start[0] (1) | second_elevation_start[7:1] (-91
      // = 0b1010'0101)
      0b11010010,
      // Byte 6: second_elevation_start[0] (1) | second_distance_start[6:0] (7 =
      // 0b000'0111)
      0b10000111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/true);

  ASSERT_THAT(data, IsOk());
  EXPECT_TRUE(data->is_dual());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_position().azimuth.start_point_value(), 180);
  EXPECT_EQ(*data->first_position().elevation.start_point_value(), 90);
  EXPECT_EQ(*data->first_position().distance.start_point_value(), 4);
  ASSERT_TRUE(data->second_position().has_value());
  EXPECT_EQ(*data->second_position()->azimuth.start_point_value(), -180);
  EXPECT_EQ(*data->second_position()->elevation.start_point_value(), -90);
  EXPECT_EQ(*data->second_position()->distance.start_point_value(), 7);
}

TEST(CreateFromBuffer, ReturnsErrorWhenNotEnoughBytesForDualPoints) {
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Only first point payload bytes
      0b00000000,
      0b11111111,
      0b01111111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/true);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorForInvalidAnimationTypeInDualPoints) {
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (invalid = 5)
      0b00000101,
      // Dummy payload bytes
      0b00000000,
      0b11111111,
      0b01111111,
      0b00000000,
      0b11111111,
      0b01111111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarPositionData::CreateFromBuffer(*buffer, /*is_dual=*/true);

  EXPECT_THAT(data, Not(IsOk()));
}

// ============================================================================
// Create - Single Point Tests
// ============================================================================

TEST(Create, ReturnsErrorForAzimuthMaxOutOfRange) {
  // azimuth is out of range [-180, 180].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(181),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForAzimuthMinOutOfRange) {
  // azimuth is out of range [-180, 180].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(-181),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForElevationMaxOutOfRange) {
  // elevation is out of range [-90, 90].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(91),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForElevationMinOutOfRange) {
  // elevation is out of range [-90, 90].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-91),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForAzimuthEndPointOutOfRange) {
  // azimuth end_point_value is out of range [-180, 180].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(0, 181),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(0, 0),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(0, 0),
  };

  auto data = PolarPositionData::Create(AnimationType::kLinear, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForAzimuthControlPointOutOfRange) {
  // azimuth control_point_value is out of range [-180, 180].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeBezier(0, 0, 181, 128),
      .elevation = AnimatedParameterData<int8_t>::MakeBezier(0, 0, 0, 128),
      .distance = AnimatedParameterData<uint8_t>::MakeBezier(0, 0, 0, 128),
  };

  auto data = PolarPositionData::Create(AnimationType::kBezier, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForDistanceOutOfRange) {
  // distance is out of range [0, 127].
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(150),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForMismatchingAzimuthAnimationType) {
  // animation_type is kStep, but azimuth is kLinear
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(0, 10),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForMismatchingElevationAnimationType) {
  // animation_type is kStep, but elevation is kLinear
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(0, 10),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, ReturnsErrorForMismatchingDistanceAnimationType) {
  // animation_type is kStep, but distance is kLinear
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(0, 10),
  };

  auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  EXPECT_THAT(data, Not(IsOk()));
}

// ============================================================================
// CreateDual - Dual Points Tests
// ============================================================================

TEST(CreateDual, ReturnsErrorForFirstAzimuthMaxOutOfRange) {
  // first_azimuth is out of range [-180, 180].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(181),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForFirstAzimuthMinOutOfRange) {
  // first_azimuth is out of range [-180, 180].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(-181),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForFirstElevationMaxOutOfRange) {
  // first_elevation is out of range [-90, 90].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(91),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForFirstElevationMinOutOfRange) {
  // first_elevation is out of range [-90, 90].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-91),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForFirstAzimuthEndPointOutOfRange) {
  // first_azimuth end_point_value is out of range [-180, 180].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(0, 181),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(0, 0),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(0, 0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(0, 0),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(0, 0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kLinear, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForFirstAzimuthControlPointOutOfRange) {
  // first_azimuth control_point_value is out of range [-180, 180].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeBezier(0, 0, 181, 128),
      .elevation = AnimatedParameterData<int8_t>::MakeBezier(0, 0, 0, 128),
      .distance = AnimatedParameterData<uint8_t>::MakeBezier(0, 0, 0, 128),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeBezier(0, 0, 0, 128),
      .elevation = AnimatedParameterData<int8_t>::MakeBezier(0, 0, 0, 128),
      .distance = AnimatedParameterData<uint8_t>::MakeBezier(0, 0, 0, 128),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kBezier, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForFirstDistanceOutOfRange) {
  // first_distance is out of range [0, 127].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(150),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForSecondAzimuthMaxOutOfRange) {
  // second_azimuth is out of range [-180, 180].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(181),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForSecondAzimuthMinOutOfRange) {
  // second_azimuth is out of range [-180, 180].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(-181),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForSecondElevationMaxOutOfRange) {
  // second_elevation is out of range [-90, 90].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(91),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForSecondElevationMinOutOfRange) {
  // second_elevation is out of range [-90, 90].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-91),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForSecondDistanceOutOfRange) {
  // second_distance is out of range [0, 127].
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(150),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForMismatchingFirstAzimuthAnimationType) {
  // animation_type is kStep, but first_azimuth is kLinear
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(0, 10),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForMismatchingFirstElevationAnimationType) {
  // animation_type is kStep, but first_elevation is kLinear
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(0, 10),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateDual, ReturnsErrorForMismatchingSecondAzimuthAnimationType) {
  // animation_type is kStep, but second_azimuth is kLinear
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(0),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(0, 10),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(0),
  };

  auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  EXPECT_THAT(data, Not(IsOk()));
}

// ============================================================================
// Write - Single Point Tests
// ============================================================================

TEST(Write, StepAnimationWritesCorrectlyForSinglePoint) {
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(1),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-2),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
  };
  const auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(4);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(
      wb.bit_buffer(),
      (std::vector<uint8_t>{
          // Byte 0: animation_type (0 = kStep)
          0b00000000,
          // Byte 1: azimuth_start[8:1] (00000000)
          0b00000000,
          // Byte 2: azimuth_start[0] (1) | elevation_start[7:1] (1111111)
          0b11111111,
          // Byte 3: elevation_start[0] (0) | distance_start[6:0] (1111111)
          0b01111111,
      }));
}

TEST(Write, LinearAnimationWritesCorrectlyForSinglePoint) {
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(-2, -3),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(127, 0),
  };
  const auto data = PolarPositionData::Create(AnimationType::kLinear, pos);

  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(7);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(
      wb.bit_buffer(),
      (std::vector<uint8_t>{
          // Byte 0: animation_type (1 = kLinear)
          0b00000001,
          // Byte 1: azimuth_start[8:1] (00000000)
          0b00000000,
          // Byte 2: azimuth_start[0] (1) | azimuth_end[8:2] (0000000)
          0b10000000,
          // Byte 3: azimuth_end[1:0] (10) | elevation_start[7:2] (111111)
          0b10111111,
          // Byte 4: elevation_start[1:0] (10) | elevation_end[7:2] (111111)
          0b10111111,
          // Byte 5: elevation_end[1:0] (01) | distance_start[6:1] (111111)
          0b01111111,
          // Byte 6: distance_start[0] (1) | distance_end[6:0] (0000000)
          0b10000000,
      }));
}

// ============================================================================
// Write - Dual Points Tests
// ============================================================================

TEST(Write, StepAnimationWritesCorrectlyForDualPoints) {
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(1),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-2),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(-1),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(2),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(64),
  };
  const auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(7);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // Byte 0: animation_type (0 = kStep)
                                 0b00000000,
                                 // Byte 1: first_azimuth_start[8:1] (00000000)
                                 0b00000000,
                                 // Byte 2: first_azimuth_start[0] (1) |
                                 // first_elevation_start[7:1] (1111111)
                                 0b11111111,
                                 // Byte 3: first_elevation_start[0] (0) |
                                 // first_distance_start[6:0] (1111111)
                                 0b01111111,
                                 // Byte 4: second_azimuth_start[8:1] (11111111)
                                 0b11111111,
                                 // Byte 5: second_azimuth_start[0] (1) |
                                 // second_elevation_start[7:1] (0000001)
                                 0b10000001,
                                 // Byte 6: second_elevation_start[0] (0) |
                                 // second_distance_start[6:0] (1000000)
                                 0b01000000,
                             }));
}

TEST(Write, LinearAnimationWritesCorrectlyForDualPoints) {
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(-2, -3),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(127, 0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(-1, -2),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(2, 3),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(64, 32),
  };
  const auto data =
      PolarPositionData::CreateDual(AnimationType::kLinear, first, second);

  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(13);
  EXPECT_THAT(data->Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // Byte 0: animation_type (1 = kLinear)
                                 0b00000001,
                                 // Bytes 1..6: first polar coordinates
                                 0b00000000,
                                 0b10000000,
                                 0b10111111,
                                 0b10111111,
                                 0b01111111,
                                 0b10000000,
                                 // Bytes 7..12: second polar coordinates
                                 0b11111111,
                                 0b11111111,
                                 0b10000000,
                                 0b10000000,
                                 0b11100000,
                                 0b00100000,
                             }));
}

// ============================================================================
// AbslStringify - Single Point Tests
// ============================================================================

TEST(AbslStringify, FormatsStepAnimationForSinglePoint) {
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(1),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-2),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
  };
  const auto data = PolarPositionData::Create(AnimationType::kStep, pos);

  ASSERT_THAT(data, IsOk());
  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 0\n"
            "    azimuth:\n"
            "     // Step\n"
            "     start_point_value= 1\n"
            "    elevation:\n"
            "     // Step\n"
            "     start_point_value= -2\n"
            "    distance:\n"
            "     // Step\n"
            "     start_point_value= 127");
}

TEST(AbslStringify, FormatsLinearAnimationForSinglePoint) {
  PolarPosition pos = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(-2, -3),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(127, 0),
  };
  const auto data = PolarPositionData::Create(AnimationType::kLinear, pos);

  ASSERT_THAT(data, IsOk());
  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 1\n"
            "    azimuth:\n"
            "     // Linear\n"
            "     start_point_value= 1\n"
            "     end_point_value= 2\n"
            "    elevation:\n"
            "     // Linear\n"
            "     start_point_value= -2\n"
            "     end_point_value= -3\n"
            "    distance:\n"
            "     // Linear\n"
            "     start_point_value= 127\n"
            "     end_point_value= 0");
}

// ============================================================================
// AbslStringify - Dual Points Tests
// ============================================================================

TEST(AbslStringify, FormatsStepAnimationForDualPoints) {
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(1),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(-2),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeStep(-1),
      .elevation = AnimatedParameterData<int8_t>::MakeStep(2),
      .distance = AnimatedParameterData<uint8_t>::MakeStep(64),
  };
  const auto data =
      PolarPositionData::CreateDual(AnimationType::kStep, first, second);

  ASSERT_THAT(data, IsOk());
  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 0\n"
            "    first_azimuth:\n"
            "     // Step\n"
            "     start_point_value= 1\n"
            "    first_elevation:\n"
            "     // Step\n"
            "     start_point_value= -2\n"
            "    first_distance:\n"
            "     // Step\n"
            "     start_point_value= 127\n"
            "    second_azimuth:\n"
            "     // Step\n"
            "     start_point_value= -1\n"
            "    second_elevation:\n"
            "     // Step\n"
            "     start_point_value= 2\n"
            "    second_distance:\n"
            "     // Step\n"
            "     start_point_value= 64");
}

TEST(AbslStringify, FormatsLinearAnimationForDualPoints) {
  PolarPosition first = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(-2, -3),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(127, 0),
  };
  PolarPosition second = {
      .azimuth = AnimatedParameterData<int16_t>::MakeLinear(-1, -2),
      .elevation = AnimatedParameterData<int8_t>::MakeLinear(2, 3),
      .distance = AnimatedParameterData<uint8_t>::MakeLinear(64, 32),
  };
  const auto data =
      PolarPositionData::CreateDual(AnimationType::kLinear, first, second);

  ASSERT_THAT(data, IsOk());
  const std::string formatted = absl::StrCat(*data);

  EXPECT_EQ(formatted,
            "    animation_type= 1\n"
            "    first_azimuth:\n"
            "     // Linear\n"
            "     start_point_value= 1\n"
            "     end_point_value= 2\n"
            "    first_elevation:\n"
            "     // Linear\n"
            "     start_point_value= -2\n"
            "     end_point_value= -3\n"
            "    first_distance:\n"
            "     // Linear\n"
            "     start_point_value= 127\n"
            "     end_point_value= 0\n"
            "    second_azimuth:\n"
            "     // Linear\n"
            "     start_point_value= -1\n"
            "     end_point_value= -2\n"
            "    second_elevation:\n"
            "     // Linear\n"
            "     start_point_value= 2\n"
            "     end_point_value= 3\n"
            "    second_distance:\n"
            "     // Linear\n"
            "     start_point_value= 64\n"
            "     end_point_value= 32");
}

}  // namespace
}  // namespace iamf_tools
