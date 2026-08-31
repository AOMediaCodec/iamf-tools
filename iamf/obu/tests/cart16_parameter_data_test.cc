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
#include "iamf/obu/cart16_parameter_data.h"

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
  std::vector<uint8_t> source = {// `animation_type`.
                                 0x00,
                                 // `x` start_value.
                                 0x00, 0x01,
                                 // `y` start_value.
                                 0xff, 0xfe,
                                 // `z` start_value.
                                 0x7f, 0xff};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*data->x().start_point_value(), 1);
  EXPECT_EQ(*data->y().start_point_value(), -2);
  EXPECT_EQ(*data->z().start_point_value(), 32767);
}

TEST(CreateFromBuffer, SuccessLinear) {
  std::vector<uint8_t> source = {// `animation_type`.
                                 0x01,
                                 // `x` start_value, end_value.
                                 0x00, 0x01, 0x00, 0x02,
                                 // `y` start_value, end_value.
                                 0xff, 0xfe, 0xff, 0xfd,
                                 // `z` start_value, end_value.
                                 0x7f, 0xff, 0x00, 0x00};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kLinear);
  EXPECT_EQ(*data->x().start_point_value(), 1);
  EXPECT_EQ(*data->x().end_point_value(), 2);
  EXPECT_EQ(*data->y().start_point_value(), -2);
  EXPECT_EQ(*data->y().end_point_value(), -3);
  EXPECT_EQ(*data->z().start_point_value(), 32767);
  EXPECT_EQ(*data->z().end_point_value(), 0);
}

TEST(CreateFromBuffer, SuccessBezier) {
  std::vector<uint8_t> source = {
      // `animation_type`.
      0x02,
      // `x` start_val, end_val, control_val, rel_time.
      0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x05,
      // `y` start_val, end_val, control_val, rel_time.
      0xff, 0xfe, 0xff, 0xfd, 0xff, 0xfc, 0x0a,
      // `z` start_val, end_val, control_val, rel_time.
      0x7f, 0xff, 0x00, 0x00, 0x00, 0x32, 0x0f};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kBezier);
  EXPECT_EQ(*data->x().start_point_value(), 1);
  EXPECT_EQ(*data->x().end_point_value(), 2);
  EXPECT_EQ(*data->x().control_point_value(), 3);
  EXPECT_EQ(*data->x().control_point_relative_time(), 5);
  EXPECT_EQ(*data->y().start_point_value(), -2);
  EXPECT_EQ(*data->y().end_point_value(), -3);
  EXPECT_EQ(*data->y().control_point_value(), -4);
  EXPECT_EQ(*data->y().control_point_relative_time(), 10);
  EXPECT_EQ(*data->z().start_point_value(), 32767);
  EXPECT_EQ(*data->z().end_point_value(), 0);
  EXPECT_EQ(*data->z().control_point_value(), 50);
  EXPECT_EQ(*data->z().control_point_relative_time(), 15);
}

TEST(CreateFromBuffer, SuccessInterLinear) {
  std::vector<uint8_t> source = {// `animation_type`.
                                 0x03,
                                 // `x` end_value.
                                 0x00, 0x02,
                                 // `y` end_value.
                                 0xff, 0xfd,
                                 // `z` end_value.
                                 0x00, 0x00};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kInterLinear);
  EXPECT_EQ(*data->x().end_point_value(), 2);
  EXPECT_EQ(*data->y().end_point_value(), -3);
  EXPECT_EQ(*data->z().end_point_value(), 0);
}

TEST(CreateFromBuffer, SuccessInterBezier) {
  std::vector<uint8_t> source = {// `animation_type`.
                                 0x04,
                                 // `x` end, control, rel_time.
                                 0x00, 0x02, 0x00, 0x03, 0x05,
                                 // `y` end, control, rel_time.
                                 0xff, 0xfd, 0xff, 0xfc, 0x0a,
                                 // `z` end, control, rel_time.
                                 0x00, 0x00, 0x00, 0x32, 0x0f};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->animation_type(), AnimationType::kInterBezier);
  EXPECT_EQ(*data->x().end_point_value(), 2);
  EXPECT_EQ(*data->x().control_point_value(), 3);
  EXPECT_EQ(*data->x().control_point_relative_time(), 5);
  EXPECT_EQ(*data->y().end_point_value(), -3);
  EXPECT_EQ(*data->y().control_point_value(), -4);
  EXPECT_EQ(*data->y().control_point_relative_time(), 10);
  EXPECT_EQ(*data->z().end_point_value(), 0);
  EXPECT_EQ(*data->z().control_point_value(), 50);
  EXPECT_EQ(*data->z().control_point_relative_time(), 15);
}

TEST(CreateFromBuffer, FailsWhenNotEnoughBytes) {
  std::vector<uint8_t> source = {
      // `animation_type`.
      0x00,
      // `x` start.
      0x00, 0x01,
      // `y` start.
      0xff
      // `z` start missing.
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateFromBuffer, FailsForInvalidAnimationType) {
  std::vector<uint8_t> source = {// `animation_type` (invalid).
                                 0x05,
                                 // Values.
                                 0x00, 0x01, 0x00, 0x01, 0x00, 0x01};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = Cart16ParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(Write, StepAnimationWritesCorrectly) {
  const auto data = Cart16ParameterData::Make(
      AnimationType::kStep, AnimatedParameterData<int16_t>::MakeStep(5),
      AnimatedParameterData<int16_t>::MakeStep(-10),
      AnimatedParameterData<int16_t>::MakeStep(100));

  WriteBitBuffer wb(7);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{// `animation_type`.
                                                   0x00,
                                                   // `x` start.
                                                   0x00, 0x05,
                                                   // `y` start.
                                                   0xff, 0xf6,
                                                   // `z` start.
                                                   0x00, 0x64}));
}

TEST(Write, LinearAnimationWritesCorrectly) {
  const auto data = Cart16ParameterData::Make(
      AnimationType::kLinear, AnimatedParameterData<int16_t>::MakeLinear(1, 2),
      AnimatedParameterData<int16_t>::MakeLinear(-2, -3),
      AnimatedParameterData<int16_t>::MakeLinear(32767, 0));

  WriteBitBuffer wb(13);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{// `animation_type`.
                                                   0x01,
                                                   // `x` start, end.
                                                   0x00, 0x01, 0x00, 0x02,
                                                   // `y` start, end.
                                                   0xff, 0xfe, 0xff, 0xfd,
                                                   // `z` start, end.
                                                   0x7f, 0xff, 0x00, 0x00}));
}

TEST(Write, BezierAnimationWritesCorrectly) {
  const auto data = Cart16ParameterData::Make(
      AnimationType::kBezier,
      AnimatedParameterData<int16_t>::MakeBezier(1, 2, 3, 5),
      AnimatedParameterData<int16_t>::MakeBezier(-2, -3, -4, 10),
      AnimatedParameterData<int16_t>::MakeBezier(32767, 0, 50, 15));

  WriteBitBuffer wb(22);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(),
            (std::vector<uint8_t>{// `animation_type`.
                                  0x02,
                                  // `x` start, end, control, rel_time.
                                  0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x05,
                                  // `y` start, end, control, rel_time.
                                  0xff, 0xfe, 0xff, 0xfd, 0xff, 0xfc, 0x0a,
                                  // `z` start, end, control, rel_time.
                                  0x7f, 0xff, 0x00, 0x00, 0x00, 0x32, 0x0f}));
}

TEST(Write, InterLinearAnimationWritesCorrectly) {
  const auto data = Cart16ParameterData::Make(
      AnimationType::kInterLinear,
      AnimatedParameterData<int16_t>::MakeInterLinear(2),
      AnimatedParameterData<int16_t>::MakeInterLinear(-3),
      AnimatedParameterData<int16_t>::MakeInterLinear(0));

  WriteBitBuffer wb(7);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(), (std::vector<uint8_t>{// `animation_type`.
                                                   0x03,
                                                   // `x` end.
                                                   0x00, 0x02,
                                                   // `y` end.
                                                   0xff, 0xfd,
                                                   // `z` end.
                                                   0x00, 0x00}));
}

TEST(Write, InterBezierAnimationWritesCorrectly) {
  const auto data = Cart16ParameterData::Make(
      AnimationType::kInterBezier,
      AnimatedParameterData<int16_t>::MakeInterBezier(2, 3, 5),
      AnimatedParameterData<int16_t>::MakeInterBezier(-3, -4, 10),
      AnimatedParameterData<int16_t>::MakeInterBezier(0, 50, 15));

  WriteBitBuffer wb(16);
  EXPECT_THAT(data.Write(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer(),
            (std::vector<uint8_t>{// `animation_type`.
                                  0x04,
                                  // `x` end, control, rel_time.
                                  0x00, 0x02, 0x00, 0x03, 0x05,
                                  // `y` end, control, rel_time.
                                  0xff, 0xfd, 0xff, 0xfc, 0x0a,
                                  // `z` end, control, rel_time.
                                  0x00, 0x00, 0x00, 0x32, 0x0f}));
}

}  // namespace
}  // namespace iamf_tools
