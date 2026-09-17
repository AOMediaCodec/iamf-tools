/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/cartesian_position_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr int32_t kBufferSize = 256;

ParamDefinition::BaseArgs GetDualCart16ParamDefinitionArgs() {
  constexpr DecodedUleb128 kParameterId = 1;
  constexpr DecodedUleb128 kParameterRate = 1;
  constexpr DecodedUleb128 kDuration = 10;
  return MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                                kDuration);
}

TEST(DualCart16ParamDefinitionTest, GetType) {
  DualCart16ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDualCart16);
}

TEST(DualCart16ParamDefinitionTest, ReadAndValidateSucceeds) {
  DualCart16ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_first_x = 1 (16 bits)
                               // default_first_y = 2 (16 bits)
                               // default_first_z = 3 (16 bits)
                               // default_second_x = 4 (16 bits)
                               // default_second_y = 5 (16 bits)
                               // default_second_z = 6 (16 bits)
                               0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04,
                               0x00, 0x05, 0x00, 0x06};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.GetParameterId(), 1);
  EXPECT_EQ(param_definition.GetParameterRate(), 1);
  EXPECT_EQ(param_definition.GetParamDefinitionMode(),
            ParamDefinition::kModeScheduleInParamDefinition);
  EXPECT_EQ(param_definition.GetDuration(), 10);
  EXPECT_EQ(param_definition.GetConstantSubblockDuration(), 10);
  EXPECT_EQ(param_definition.default_first_x_, 1);
  EXPECT_EQ(param_definition.default_first_y_, 2);
  EXPECT_EQ(param_definition.default_first_z_, 3);
  EXPECT_EQ(param_definition.default_second_x_, 4);
  EXPECT_EQ(param_definition.default_second_y_, 5);
  EXPECT_EQ(param_definition.default_second_z_, 6);
}

TEST(DualCart16ParamDefinitionTest, WriteAndValidateSucceeds) {
  DualCart16ParamDefinition param_definition(
      GetDualCart16ParamDefinitionArgs());
  param_definition.default_first_x_ = 1;
  param_definition.default_first_y_ = 2;
  param_definition.default_first_z_ = 3;
  param_definition.default_second_x_ = 4;
  param_definition.default_second_y_ = 5;
  param_definition.default_second_z_ = 6;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rates
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                        // default_first_x = 1 (16 bits)
                                        // default_first_y = 2 (16 bits)
                                        // default_first_z = 3 (16 bits)
                                        // default_second_x = 4 (16 bits)
                                        // default_second_y = 5 (16 bits)
                                        // default_second_z = 6 (16 bits)
                                        0x00, 0x01, 0x00, 0x02, 0x00, 0x03,
                                        0x00, 0x04, 0x00, 0x05, 0x00, 0x06};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(DualCart16ParamDefinitionTest, CreateParameterDataFromBufferSucceeds) {
  DualCart16ParamDefinition param_definition(
      GetDualCart16ParamDefinitionArgs());
  std::vector<uint8_t> payload = {
      // `animation_type` (0 = kStep)
      0x00,
      // `first_x` start_value = 1
      0x00,
      0x01,
      // `first_y` start_value = 2
      0x00,
      0x02,
      // `first_z` start_value = 3
      0x00,
      0x03,
      // `second_x` start_value = 4
      0x00,
      0x04,
      // `second_y` start_value = 5
      0x00,
      0x05,
      // `second_z` start_value = 6
      0x00,
      0x06,
  };
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(payload);
  auto parameter_data = param_definition.CreateParameterDataFromBuffer(*rb);
  ASSERT_THAT(parameter_data, IsOk());
  auto* cart_data = dynamic_cast<CartesianPositionData*>(parameter_data->get());
  ASSERT_NE(cart_data, nullptr);
  EXPECT_TRUE(cart_data->is_dual());
  EXPECT_EQ(cart_data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(cart_data->bit_depth(), CartesianBitDepth::k16Bit);
  EXPECT_EQ(*cart_data->first_position().x.start_point_value(), 1);
  EXPECT_EQ(*cart_data->first_position().y.start_point_value(), 2);
  EXPECT_EQ(*cart_data->first_position().z.start_point_value(), 3);
  ASSERT_TRUE(cart_data->second_position().has_value());
  EXPECT_EQ(*cart_data->second_position()->x.start_point_value(), 4);
  EXPECT_EQ(*cart_data->second_position()->y.start_point_value(), 5);
  EXPECT_EQ(*cart_data->second_position()->z.start_point_value(), 6);
}

TEST(DualCart16ParamDefinitionTest, AbslStringifyFormatsCorrectly) {
  const DualCart16ParamDefinition param_definition(
      GetDualCart16ParamDefinitionArgs());

  const std::string formatted = absl::StrCat(param_definition);

  EXPECT_EQ(formatted,
            "DualCart16ParamDefinition:\n"
            "  parameter_type= 8\n"
            "  parameter_id= 1\n"
            "  parameter_rate= 1\n"
            "  param_definition_mode= 0\n"
            "  reserved= 0\n"
            "  duration= 10\n"
            "  constant_subblock_duration= 10\n"
            "  num_subblocks= 1\n"
            "  default_first_x: 0\n"
            "  default_first_y: 0\n"
            "  default_first_z: 0\n"
            "  default_second_x: 0\n"
            "  default_second_y: 0\n"
            "  default_second_z: 0");
}

}  // namespace
}  // namespace iamf_tools
