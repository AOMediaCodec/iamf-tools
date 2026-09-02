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
#include "iamf/obu/dual_cart8_parameter_data.h"

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
  std::vector<uint8_t> source = {
      // `animation_type`.
      0x00,
      // `first_x` start_value.
      0x01,
      // `first_y` start_value.
      0x02,
      // `first_z` start_value.
      0x03,
      // `second_x` start_value.
      0x04,
      // `second_y` start_value.
      0x05,
      // `second_z` start_value.
      0x06,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = DualCart8ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->first_x().start_point_value(), 1);
  EXPECT_EQ(*data->first_y().start_point_value(), 2);
  EXPECT_EQ(*data->first_z().start_point_value(), 3);
  EXPECT_EQ(*data->second_x().start_point_value(), 4);
  EXPECT_EQ(*data->second_y().start_point_value(), 5);
  EXPECT_EQ(*data->second_z().start_point_value(), 6);
}

TEST(CreateFromBuffer, SuccessLinear) {
  std::vector<uint8_t> source = {
      // `animation_type`.
      0x01,
      // `first_x` start_value, end_value.
      0x01,
      0x02,
      // `first_y` start_value, end_value.
      0x03,
      0x04,
      // `first_z` start_value, end_value.
      0x05,
      0x06,
      // `second_x` start_value, end_value.
      0x07,
      0x08,
      // `second_y` start_value, end_value.
      0x09,
      0x0a,
      // `second_z` start_value, end_value.
      0x0b,
      0x0c,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = DualCart8ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->first_x().start_point_value(), 1);
  EXPECT_EQ(*data->first_x().end_point_value(), 2);
  EXPECT_EQ(*data->first_y().start_point_value(), 3);
  EXPECT_EQ(*data->first_y().end_point_value(), 4);
  EXPECT_EQ(*data->first_z().start_point_value(), 5);
  EXPECT_EQ(*data->first_z().end_point_value(), 6);
  EXPECT_EQ(*data->second_x().start_point_value(), 7);
  EXPECT_EQ(*data->second_x().end_point_value(), 8);
  EXPECT_EQ(*data->second_y().start_point_value(), 9);
  EXPECT_EQ(*data->second_y().end_point_value(), 10);
  EXPECT_EQ(*data->second_z().start_point_value(), 11);
  EXPECT_EQ(*data->second_z().end_point_value(), 12);
}

TEST(CreateFromBuffer, FailsWhenNotEnoughBytes) {
  std::vector<uint8_t> source = {
      // `animation_type`.
      0x00,
      // `first_x` start_value.
      0x01,
      // `first_y` start_value.
      0x02,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = DualCart8ParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateFromBuffer, FailsForInvalidAnimationType) {
  std::vector<uint8_t> source = {
      // `animation_type` (invalid).
      0x05,
      // Values.
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = DualCart8ParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Write, StepAnimationWritesCorrectly) {
  const auto data = DualCart8ParameterData::Make(
      AnimationType::kStep, AnimatedParameterData<int8_t>::MakeStep(1),
      AnimatedParameterData<int8_t>::MakeStep(2),
      AnimatedParameterData<int8_t>::MakeStep(3),
      AnimatedParameterData<int8_t>::MakeStep(4),
      AnimatedParameterData<int8_t>::MakeStep(5),
      AnimatedParameterData<int8_t>::MakeStep(6));

  WriteBitBuffer wb(7);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`.
                                 0x00,
                                 // `first_x` start_value.
                                 1,
                                 // `first_y` start_value.
                                 2,
                                 // `first_z` start_value.
                                 3,
                                 // `second_x` start_value.
                                 4,
                                 // `second_y` start_value.
                                 5,
                                 // `second_z` start_value.
                                 6,
                             }));
}

TEST(Write, LinearAnimationWritesCorrectly) {
  const auto data = DualCart8ParameterData::Make(
      AnimationType::kLinear, AnimatedParameterData<int8_t>::MakeLinear(1, 2),
      AnimatedParameterData<int8_t>::MakeLinear(3, 4),
      AnimatedParameterData<int8_t>::MakeLinear(5, 6),
      AnimatedParameterData<int8_t>::MakeLinear(7, 8),
      AnimatedParameterData<int8_t>::MakeLinear(9, 10),
      AnimatedParameterData<int8_t>::MakeLinear(11, 12));

  WriteBitBuffer wb(13);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{
                                 // `animation_type`.
                                 0x01,
                                 // `first_x` start_value, end_value.
                                 1,
                                 2,
                                 // `first_y` start_value, end_value.
                                 3,
                                 4,
                                 // `first_z` start_value, end_value.
                                 5,
                                 6,
                                 // `second_x` start_value, end_value.
                                 7,
                                 8,
                                 // `second_y` start_value, end_value.
                                 9,
                                 10,
                                 // `second_z` start_value, end_value.
                                 11,
                                 12,
                             }));
}

}  // namespace
}  // namespace iamf_tools
