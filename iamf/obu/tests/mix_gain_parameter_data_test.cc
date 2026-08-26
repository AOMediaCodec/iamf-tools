/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/mix_gain_parameter_data.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using enum AnimationType;
using ::testing::Not;

TEST(MixGainParameterData, ReadAndValidateStep) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x00,
      // Start point value.
      0x02,
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  MixGainParameterData mix_gain_parameter_data;
  EXPECT_THAT(mix_gain_parameter_data.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(mix_gain_parameter_data.GetAnimationType(), kStep);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.start_point_value(), 0x0201);
  EXPECT_EQ(mix_gain_parameter_data.param_data.animation_type(), kStep);
}

TEST(MixGainParameterData, ReadAndValidateLinear) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x01,
      // Start point value.
      0x04,
      0x03,
      // End point value.
      0x02,
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  MixGainParameterData mix_gain_parameter_data;
  EXPECT_THAT(mix_gain_parameter_data.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(mix_gain_parameter_data.GetAnimationType(), kLinear);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.start_point_value(), 0x0403);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.end_point_value(), 0x0201);
  EXPECT_EQ(mix_gain_parameter_data.param_data.animation_type(), kLinear);
}

TEST(MixGainParameterData, ReadAndValidateBezier) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x02,
      // Start point value.
      0x07,
      0x06,
      // End point value.
      0x05,
      0x04,
      // Control point value.
      0x03,
      0x02,
      // Control point relative time.
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  MixGainParameterData mix_gain_parameter_data;
  EXPECT_THAT(mix_gain_parameter_data.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(mix_gain_parameter_data.GetAnimationType(), kBezier);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.start_point_value(), 0x0706);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.end_point_value(), 0x0504);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.control_point_value(), 0x0302);
  EXPECT_EQ(*mix_gain_parameter_data.param_data.control_point_relative_time(),
            0x01);
  EXPECT_EQ(mix_gain_parameter_data.param_data.animation_type(), kBezier);
}

TEST(MixGainParameterData,
     ReadAndValidateReturnsErrorWhenAnimationTypeIsUnknown) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x03,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  MixGainParameterData mix_gain_parameter_data;
  EXPECT_THAT(mix_gain_parameter_data.ReadAndValidate(*buffer), Not(IsOk()));
}
TEST(Make, SucceedsWithStep) {
  const auto anim_data = AnimatedParameterData<int16_t>::MakeStep(0x0201);

  auto data = MixGainParameterData::Make(anim_data);

  EXPECT_EQ(data.GetAnimationType(), kStep);
  EXPECT_EQ(*data.param_data.start_point_value(), 0x0201);
}

TEST(CreateFromBuffer, FailsForInvalidAnimationType) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x05,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data = MixGainParameterData::CreateFromBuffer(*buffer);

  EXPECT_THAT(data, Not(IsOk()));
}

TEST(CreateFromBuffer, ParsesValidStep) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x00,
      // Start point value.
      0x02,
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data = MixGainParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->param_data.animation_type(), kStep);
  EXPECT_EQ(*data->param_data.start_point_value(), 0x0201);
}

TEST(CreateFromBuffer, ParsesValidLinear) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x01,
      // Start point value.
      0x04,
      0x03,
      // End point value.
      0x02,
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data = MixGainParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->param_data.animation_type(), kLinear);
  EXPECT_EQ(*data->param_data.start_point_value(), 0x0403);
  EXPECT_EQ(*data->param_data.end_point_value(), 0x0201);
}

TEST(CreateFromBuffer, ParsesValidBezier) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x02,
      // Start point value.
      0x07,
      0x06,
      // End point value.
      0x05,
      0x04,
      // Control point value.
      0x03,
      0x02,
      // Control point relative time.
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data = MixGainParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->param_data.animation_type(), kBezier);
  EXPECT_EQ(*data->param_data.start_point_value(), 0x0706);
  EXPECT_EQ(*data->param_data.end_point_value(), 0x0504);
  EXPECT_EQ(*data->param_data.control_point_value(), 0x0302);
  EXPECT_EQ(*data->param_data.control_point_relative_time(), 0x01);
}

}  // namespace
}  // namespace iamf_tools
