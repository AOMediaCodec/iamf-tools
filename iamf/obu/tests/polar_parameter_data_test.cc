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
#include "iamf/obu/polar_parameter_data.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
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

TEST(CreateFromBuffer, SuccessStep) {
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

  auto data = PolarParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->azimuth().start_point_value(), 1);
  EXPECT_EQ(*data->elevation().start_point_value(), -2);
  EXPECT_EQ(*data->distance().start_point_value(), 127);
}

TEST(CreateFromBuffer, SuccessLinear) {
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

  auto data = PolarParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->azimuth().start_point_value(), 1);
  EXPECT_EQ(*data->azimuth().end_point_value(), 2);
  EXPECT_EQ(*data->elevation().start_point_value(), -2);
  EXPECT_EQ(*data->elevation().end_point_value(), -3);
  EXPECT_EQ(*data->distance().start_point_value(), 127);
  EXPECT_EQ(*data->distance().end_point_value(), 0);
}

TEST(CreateFromBuffer, FailsWhenNotEnoughBytes) {
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: azimuth_start[8:1]
      0b00000000,
      // Byte 2: azimuth_start[0] | elevation_start[7:1]
      0b11111111,
      // Missing Byte 3 (elevation_start[0] | distance_start[6:0])
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = PolarParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateFromBuffer, FailsForInvalidAnimationType) {
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

  auto data = PolarParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForAzimuthMaxOutOfRange) {
  // azimuth is out of range [-256, 255].
  auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(300),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForAzimuthMinOutOfRange) {
  // azimuth is out of range [-256, 255].
  auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(-300),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForAzimuthEndPointOutOfRange) {
  // azimuth end_point_value is out of range [-256, 255].
  auto data = PolarParameterData::Create(
      AnimationType::kLinear,
      AnimatedParameterData<int16_t>::MakeLinear(0, 300),
      AnimatedParameterData<int8_t>::MakeLinear(0, 0),
      AnimatedParameterData<uint8_t>::MakeLinear(0, 0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForAzimuthControlPointOutOfRange) {
  // azimuth control_point_value is out of range [-256, 255].
  auto data = PolarParameterData::Create(
      AnimationType::kBezier,
      AnimatedParameterData<int16_t>::MakeBezier(0, 0, 300, 128),
      AnimatedParameterData<int8_t>::MakeBezier(0, 0, 0, 128),
      AnimatedParameterData<uint8_t>::MakeBezier(0, 0, 0, 128));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForDistanceOutOfRange) {
  // distance is out of range [0, 127].
  auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(150));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForMismatchingAzimuthAnimationType) {
  // animation_type is kStep, but azimuth is kLinear
  auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeLinear(0, 10),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForMismatchingElevationAnimationType) {
  // animation_type is kStep, but elevation is kLinear
  auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeLinear(0, 10),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForMismatchingDistanceAnimationType) {
  // animation_type is kStep, but distance is kLinear
  auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeLinear(0, 10));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Write, StepAnimationWritesCorrectly) {
  const auto data = PolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(1),
      AnimatedParameterData<int8_t>::MakeStep(-2),
      AnimatedParameterData<uint8_t>::MakeStep(127));

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

TEST(Write, LinearAnimationWritesCorrectly) {
  const auto data = PolarParameterData::Create(
      AnimationType::kLinear, AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      AnimatedParameterData<int8_t>::MakeLinear(-2, -3),
      AnimatedParameterData<uint8_t>::MakeLinear(127, 0));

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

}  // namespace
}  // namespace iamf_tools
