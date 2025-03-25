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
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/mix_gain_parameter_data.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using absl_testing::IsOkAndHolds;
using MixGainParameterData::kAnimateBezier;
using MixGainParameterData::kAnimateLinear;
using MixGainParameterData::kAnimateStep;

constexpr uint32_t kAudioElementId = 0;

TEST(AnimationStepInt16, ReadAndValidate) {
  std::vector<uint8_t> source_data = {
      // Start point value.
      0x02,
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  AnimationStepInt16 step_animation;
  EXPECT_THAT(step_animation.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(step_animation.start_point_value, 0x0201);
}

TEST(AnimationLinearInt16, ReadAndValidate) {
  std::vector<uint8_t> source_data = {
      // Start point value.
      0x04,
      0x03,
      // End point value.
      0x02,
      0x01,
  };
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  AnimationLinearInt16 linear_animation;
  EXPECT_THAT(linear_animation.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(linear_animation.start_point_value, 0x0403);
  EXPECT_EQ(linear_animation.end_point_value, 0x0201);
}

TEST(AnimationBezierInt16, ReadAndValidate) {
  std::vector<uint8_t> source_data = {// Start point value.
                                      0x07, 0x06,
                                      // End point value.
                                      0x05, 0x04,
                                      // Control point value.
                                      0x03, 0x02,
                                      // Control point relative time.
                                      0x01};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  AnimationBezierInt16 bezier_animation;
  EXPECT_THAT(bezier_animation.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(bezier_animation.start_point_value, 0x0706);
  EXPECT_EQ(bezier_animation.end_point_value, 0x0504);
  EXPECT_EQ(bezier_animation.control_point_value, 0x0302);
  EXPECT_EQ(bezier_animation.control_point_relative_time, 0x01);
}

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
  EXPECT_EQ(mix_gain_parameter_data.animation_type, kAnimateStep);
  EXPECT_TRUE(std::holds_alternative<AnimationStepInt16>(
      mix_gain_parameter_data.param_data));
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
  EXPECT_EQ(mix_gain_parameter_data.animation_type, kAnimateLinear);
  EXPECT_TRUE(std::holds_alternative<AnimationLinearInt16>(
      mix_gain_parameter_data.param_data));
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
  EXPECT_EQ(mix_gain_parameter_data.animation_type, kAnimateBezier);
  EXPECT_TRUE(std::holds_alternative<AnimationBezierInt16>(
      mix_gain_parameter_data.param_data));
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
  EXPECT_FALSE(mix_gain_parameter_data.ReadAndValidate(*buffer).ok());
}

}  // namespace
}  // namespace iamf_tools
