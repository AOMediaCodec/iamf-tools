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
#include "iamf/obu/dual_polar_parameter_data.h"

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
  //   first_azimuth = 1
  //   first_elevation = -2
  //   first_distance = 127
  //   second_azimuth = -1
  //   second_elevation = 2
  //   second_distance = 64
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

  auto data = DualPolarParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_azimuth().start_point_value(), 1);
  EXPECT_EQ(*data->first_elevation().start_point_value(), -2);
  EXPECT_EQ(*data->first_distance().start_point_value(), 127);
  EXPECT_EQ(*data->second_azimuth().start_point_value(), -1);
  EXPECT_EQ(*data->second_elevation().start_point_value(), 2);
  EXPECT_EQ(*data->second_distance().start_point_value(), 64);
}

TEST(CreateFromBuffer, SuccessLinear) {
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

  auto data = DualPolarParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_azimuth().start_point_value(), 1);
  EXPECT_EQ(*data->first_azimuth().end_point_value(), 2);
  EXPECT_EQ(*data->first_elevation().start_point_value(), -2);
  EXPECT_EQ(*data->first_elevation().end_point_value(), -3);
  EXPECT_EQ(*data->first_distance().start_point_value(), 127);
  EXPECT_EQ(*data->first_distance().end_point_value(), 0);
  EXPECT_EQ(*data->second_azimuth().start_point_value(), -1);
  EXPECT_EQ(*data->second_azimuth().end_point_value(), -2);
  EXPECT_EQ(*data->second_elevation().start_point_value(), 2);
  EXPECT_EQ(*data->second_elevation().end_point_value(), 3);
  EXPECT_EQ(*data->second_distance().start_point_value(), 64);
  EXPECT_EQ(*data->second_distance().end_point_value(), 32);
}

TEST(CreateFromBuffer, ClipsAzimuthAndElevation) {
  // Input values in bitstream:
  //   first_azimuth = 181 (clipped to 180)
  //   first_elevation = 91 (clipped to 90)
  //   first_distance = 4
  //   second_azimuth = -181 (clipped to -180)
  //   second_elevation = -91 (clipped to -90)
  //   second_distance = 7
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

  auto data = DualPolarParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_azimuth().start_point_value(), 180);
  EXPECT_EQ(*data->first_elevation().start_point_value(), 90);
  EXPECT_EQ(*data->first_distance().start_point_value(), 4);
  EXPECT_EQ(*data->second_azimuth().start_point_value(), -180);
  EXPECT_EQ(*data->second_elevation().start_point_value(), -90);
  EXPECT_EQ(*data->second_distance().start_point_value(), 7);
}

TEST(CreateFromBuffer, FailsWhenNotEnoughBytes) {
  std::vector<uint8_t> source = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Incomplete payload bytes
      0b00000000,
      0b11111111,
      0b01111111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = DualPolarParameterData::CreateFromBuffer(*buffer);

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
      0b00000000,
      0b11111111,
      0b01111111,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = DualPolarParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstAzimuthMaxOutOfRange) {
  // first_azimuth is out of range [-180, 180].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(181),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstAzimuthMinOutOfRange) {
  // first_azimuth is out of range [-180, 180].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(-181),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstElevationMaxOutOfRange) {
  // first_elevation is out of range [-90, 90].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(91),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstElevationMinOutOfRange) {
  // first_elevation is out of range [-90, 90].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(-91),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstAzimuthEndPointOutOfRange) {
  // first_azimuth end_point_value is out of range [-180, 180].
  auto data = DualPolarParameterData::Create(
      AnimationType::kLinear,
      AnimatedParameterData<int16_t>::MakeLinear(0, 181),
      AnimatedParameterData<int8_t>::MakeLinear(0, 0),
      AnimatedParameterData<uint8_t>::MakeLinear(0, 0),
      AnimatedParameterData<int16_t>::MakeLinear(0, 0),
      AnimatedParameterData<int8_t>::MakeLinear(0, 0),
      AnimatedParameterData<uint8_t>::MakeLinear(0, 0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstAzimuthControlPointOutOfRange) {
  // first_azimuth control_point_value is out of range [-180, 180].
  auto data = DualPolarParameterData::Create(
      AnimationType::kBezier,
      AnimatedParameterData<int16_t>::MakeBezier(0, 0, 181, 128),
      AnimatedParameterData<int8_t>::MakeBezier(0, 0, 0, 128),
      AnimatedParameterData<uint8_t>::MakeBezier(0, 0, 0, 128),
      AnimatedParameterData<int16_t>::MakeBezier(0, 0, 0, 128),
      AnimatedParameterData<int8_t>::MakeBezier(0, 0, 0, 128),
      AnimatedParameterData<uint8_t>::MakeBezier(0, 0, 0, 128));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForFirstDistanceOutOfRange) {
  // first_distance is out of range [0, 127].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(150),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForSecondAzimuthMaxOutOfRange) {
  // second_azimuth is out of range [-180, 180].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(181),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForSecondAzimuthMinOutOfRange) {
  // second_azimuth is out of range [-180, 180].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(-181),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForSecondElevationMaxOutOfRange) {
  // second_elevation is out of range [-90, 90].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(91),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForSecondElevationMinOutOfRange) {
  // second_elevation is out of range [-90, 90].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(-91),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForSecondDistanceOutOfRange) {
  // second_distance is out of range [0, 127].
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(150));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForMismatchingFirstAzimuthAnimationType) {
  // animation_type is kStep, but first_azimuth is kLinear
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeLinear(0, 10),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForMismatchingFirstElevationAnimationType) {
  // animation_type is kStep, but first_elevation is kLinear
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeLinear(0, 10),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Create, FailsForMismatchingSecondAzimuthAnimationType) {
  // animation_type is kStep, but second_azimuth is kLinear
  auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(0),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0),
      AnimatedParameterData<int16_t>::MakeLinear(0, 10),
      AnimatedParameterData<int8_t>::MakeStep(0),
      AnimatedParameterData<uint8_t>::MakeStep(0));

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Write, StepAnimationWritesCorrectly) {
  const auto data = DualPolarParameterData::Create(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(1),
      AnimatedParameterData<int8_t>::MakeStep(-2),
      AnimatedParameterData<uint8_t>::MakeStep(127),
      AnimatedParameterData<int16_t>::MakeStep(-1),
      AnimatedParameterData<int8_t>::MakeStep(2),
      AnimatedParameterData<uint8_t>::MakeStep(64));

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

TEST(Write, LinearAnimationWritesCorrectly) {
  const auto data = DualPolarParameterData::Create(
      AnimationType::kLinear, AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      AnimatedParameterData<int8_t>::MakeLinear(-2, -3),
      AnimatedParameterData<uint8_t>::MakeLinear(127, 0),
      AnimatedParameterData<int16_t>::MakeLinear(-1, -2),
      AnimatedParameterData<int8_t>::MakeLinear(2, 3),
      AnimatedParameterData<uint8_t>::MakeLinear(64, 32));

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

}  // namespace
}  // namespace iamf_tools
