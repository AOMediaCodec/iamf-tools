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
#include "iamf/obu/animated_parameter_data.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using absl_testing::StatusIs;
using ::testing::Not;
using enum AnimationType;

auto ReadInt16 = [](ReadBitBuffer& rb, int16_t& val) {
  return rb.ReadSigned16(val);
};

TEST(MakeStep, SetsFieldsCorrectly) {
  constexpr int16_t kStartPointValue = 0x0201;

  const auto data = AnimatedParameterData<int16_t>::MakeStep(kStartPointValue);

  EXPECT_EQ(data.animation_type(), kStep);
  EXPECT_EQ(data.start_point_value(), kStartPointValue);
  EXPECT_FALSE(data.end_point_value().has_value());
  EXPECT_FALSE(data.control_point_value().has_value());
  EXPECT_FALSE(data.control_point_relative_time().has_value());
}

TEST(MakeLinear, SetsFieldsCorrectly) {
  constexpr int16_t kStartPointValue = 0x0201;
  constexpr int16_t kEndPointValue = 0x0403;

  const auto data = AnimatedParameterData<int16_t>::MakeLinear(kStartPointValue,
                                                               kEndPointValue);

  EXPECT_EQ(data.animation_type(), kLinear);
  EXPECT_EQ(data.start_point_value(), kStartPointValue);
  EXPECT_EQ(data.end_point_value(), kEndPointValue);
  EXPECT_FALSE(data.control_point_value().has_value());
  EXPECT_FALSE(data.control_point_relative_time().has_value());
}

TEST(MakeBezier, SetsFieldsCorrectly) {
  constexpr int16_t kStartPointValue = 0x0201;
  constexpr int16_t kEndPointValue = 0x0403;
  constexpr int16_t kControlPointValue = 0x0605;
  constexpr uint8_t kControlPointRelativeTime = 0x80;

  const auto data = AnimatedParameterData<int16_t>::MakeBezier(
      kStartPointValue, kEndPointValue, kControlPointValue,
      kControlPointRelativeTime);

  EXPECT_EQ(data.animation_type(), kBezier);
  EXPECT_EQ(data.start_point_value(), kStartPointValue);
  EXPECT_EQ(data.end_point_value(), kEndPointValue);
  EXPECT_EQ(data.control_point_value(), kControlPointValue);
  EXPECT_EQ(data.control_point_relative_time(), kControlPointRelativeTime);
}

TEST(MakeInterLinear, SetsFieldsCorrectly) {
  constexpr int16_t kEndPointValue = 0x0403;

  const auto data =
      AnimatedParameterData<int16_t>::MakeInterLinear(kEndPointValue);

  EXPECT_EQ(data.animation_type(), kInterLinear);
  EXPECT_FALSE(data.start_point_value().has_value());
  EXPECT_EQ(data.end_point_value(), kEndPointValue);
  EXPECT_FALSE(data.control_point_value().has_value());
  EXPECT_FALSE(data.control_point_relative_time().has_value());
}

TEST(MakeInterBezier, SetsFieldsCorrectly) {
  constexpr int16_t kEndPointValue = 0x0403;
  constexpr int16_t kControlPointValue = 0x0605;
  constexpr uint8_t kControlPointRelativeTime = 0x80;

  const auto data = AnimatedParameterData<int16_t>::MakeInterBezier(
      kEndPointValue, kControlPointValue, kControlPointRelativeTime);

  EXPECT_EQ(data.animation_type(), kInterBezier);
  EXPECT_FALSE(data.start_point_value().has_value());
  EXPECT_EQ(data.end_point_value(), kEndPointValue);
  EXPECT_EQ(data.control_point_value(), kControlPointValue);
  EXPECT_EQ(data.control_point_relative_time(), kControlPointRelativeTime);
}

TEST(CreateFromBuffer, ParsesStep) {
  constexpr uint8_t kAnimationTypeStep = 0x00;
  constexpr int16_t kStartPointValue = 0x0201;
  const std::vector<uint8_t> source_data = {
      kAnimationTypeStep,
      0x02,
      0x01,  // start_point_value (0x0201)
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  ASSERT_THAT(data.status(), IsOk());
  EXPECT_EQ(data->animation_type(), kStep);
  EXPECT_EQ(data->start_point_value(), kStartPointValue);
}

TEST(CreateFromBuffer, ParsesLinear) {
  constexpr uint8_t kAnimationTypeLinear = 0x01;
  constexpr int16_t kStartPointValue = 0x0201;
  constexpr int16_t kEndPointValue = 0x0403;
  const std::vector<uint8_t> source_data = {
      kAnimationTypeLinear,
      0x02,
      0x01,  // start_point_value (0x0201)
      0x04,
      0x03,  // end_point_value (0x0403)
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  ASSERT_THAT(data.status(), IsOk());
  EXPECT_EQ(data->animation_type(), kLinear);
  EXPECT_EQ(data->start_point_value(), kStartPointValue);
  EXPECT_EQ(data->end_point_value(), kEndPointValue);
}

TEST(CreateFromBuffer, ParsesBezier) {
  constexpr uint8_t kAnimationTypeBezier = 0x02;
  constexpr int16_t kStartPointValue = 0x0201;
  constexpr int16_t kEndPointValue = 0x0403;
  constexpr int16_t kControlPointValue = 0x0605;
  constexpr uint8_t kControlPointRelativeTime = 0x80;
  const std::vector<uint8_t> source_data = {
      kAnimationTypeBezier,
      0x02,
      0x01,  // start_point_value (0x0201)
      0x04,
      0x03,  // end_point_value (0x0403)
      0x06,
      0x05,  // control_point_value (0x0605)
      kControlPointRelativeTime,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  ASSERT_THAT(data.status(), IsOk());
  EXPECT_EQ(data->animation_type(), kBezier);
  EXPECT_EQ(data->start_point_value(), kStartPointValue);
  EXPECT_EQ(data->end_point_value(), kEndPointValue);
  EXPECT_EQ(data->control_point_value(), kControlPointValue);
  EXPECT_EQ(data->control_point_relative_time(), kControlPointRelativeTime);
}

TEST(CreateFromBuffer, ParsesInterLinear) {
  constexpr uint8_t kAnimationTypeInterLinear = 0x03;
  constexpr int16_t kEndPointValue = 0x0403;
  const std::vector<uint8_t> source_data = {
      kAnimationTypeInterLinear,
      0x04,
      0x03,  // end_point_value (0x0403)
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  ASSERT_THAT(data.status(), IsOk());
  EXPECT_EQ(data->animation_type(), kInterLinear);
  EXPECT_FALSE(data->start_point_value().has_value());
  EXPECT_EQ(data->end_point_value(), kEndPointValue);
}

TEST(CreateFromBuffer, ParsesInterBezier) {
  constexpr uint8_t kAnimationTypeInterBezier = 0x04;
  constexpr int16_t kEndPointValue = 0x0403;
  constexpr int16_t kControlPointValue = 0x0605;
  constexpr uint8_t kControlPointRelativeTime = 0x80;
  const std::vector<uint8_t> source_data = {
      kAnimationTypeInterBezier,
      0x04,
      0x03,  // end_point_value (0x0403)
      0x06,
      0x05,  // control_point_value (0x0605)
      kControlPointRelativeTime,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  ASSERT_THAT(data.status(), IsOk());
  EXPECT_EQ(data->animation_type(), kInterBezier);
  EXPECT_FALSE(data->start_point_value().has_value());
  EXPECT_EQ(data->end_point_value(), kEndPointValue);
  EXPECT_EQ(data->control_point_value(), kControlPointValue);
  EXPECT_EQ(data->control_point_relative_time(), kControlPointRelativeTime);
}

TEST(CreateFromBuffer, FailsForInvalidType) {
  constexpr uint8_t kInvalidAnimationType = 0x05;
  const std::vector<uint8_t> source_data = {
      kInvalidAnimationType,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  EXPECT_THAT(data.status(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CreateFromBuffer, FailsWhenValueIsMissing) {
  constexpr uint8_t kAnimationTypeStep = 0x00;
  const std::vector<uint8_t> source_data = {
      kAnimationTypeStep,
      // Missing start_point_value
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data =
      AnimatedParameterData<int16_t>::CreateFromBuffer(*buffer, ReadInt16);

  EXPECT_THAT(data.status(), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
