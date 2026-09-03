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
#include "iamf/obu/param_definitions/polar_param_definition.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/polar_parameter_data.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr int32_t kBufferSize = 256;

ParamDefinition::BaseArgs GetPolarParamDefinitionArgs() {
  constexpr DecodedUleb128 kParameterId = 1;
  constexpr DecodedUleb128 kParameterRate = 1;
  constexpr DecodedUleb128 kDuration = 10;
  return MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                                kDuration);
}

TEST(PolarParamDefinitionTest, GetType) {
  PolarParamDefinition param_definition(ParamDefinition::BaseArgs{});
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionPolar);
}

TEST(PolarParamDefinitionTest, CompareDefaultConstructed) {
  PolarParamDefinition param_definition1(ParamDefinition::BaseArgs{});
  PolarParamDefinition param_definition2(ParamDefinition::BaseArgs{});
  EXPECT_EQ(param_definition1, param_definition2);
}

TEST(PolarParamDefinitionTest, ReadAndValidateSucceeds) {
  PolarParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_azimuth = 2 (9 bits)
                               // default_elevation = 3 (8 bits)
                               // default_distance = 4 (7 bits)
                               // 00000001 00000001 10000100
                               0x01, 0x01, 0x84};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.GetParameterId(), 1);
  EXPECT_EQ(param_definition.GetParameterRate(), 1);
  EXPECT_EQ(param_definition.GetParamDefinitionMode(),
            ParamDefinition::kModeScheduleInParamDefinition);
  EXPECT_EQ(param_definition.GetDuration(), 10);
  EXPECT_EQ(param_definition.GetConstantSubblockDuration(), 10);
  EXPECT_EQ(param_definition.default_azimuth_, 2);
  EXPECT_EQ(param_definition.default_elevation_, 3);
  EXPECT_EQ(param_definition.default_distance_, 4);
}

TEST(PolarParamDefinitionTest, ReadAndValidateClipsAzimuth) {
  PolarParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_azimuth = 181 (9 bits)
                               // default_elevation = 3 (8 bits)
                               // default_distance = 4 (7 bits)
                               0b0101'1010, 0b1000'0001, 0b1000'0100};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.GetParameterId(), 1);
  EXPECT_EQ(param_definition.GetParameterRate(), 1);
  EXPECT_EQ(param_definition.GetParamDefinitionMode(),
            ParamDefinition::kModeScheduleInParamDefinition);
  EXPECT_EQ(param_definition.GetDuration(), 10);
  EXPECT_EQ(param_definition.GetConstantSubblockDuration(), 10);
  EXPECT_EQ(param_definition.default_azimuth_, 180);
  EXPECT_EQ(param_definition.default_elevation_, 3);
  EXPECT_EQ(param_definition.default_distance_, 4);
}

TEST(PolarParamDefinitionTest, ReadAndValidateClipsElevation) {
  PolarParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_azimuth = 2 (9 bits)
                               // default_elevation = 91 (8 bits)
                               // default_distance = 4 (7 bits)
                               0b0000'0001, 0b0010'1101, 0b1000'0100};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.GetParameterId(), 1);
  EXPECT_EQ(param_definition.GetParameterRate(), 1);
  EXPECT_EQ(param_definition.GetParamDefinitionMode(),
            ParamDefinition::kModeScheduleInParamDefinition);
  EXPECT_EQ(param_definition.GetDuration(), 10);
  EXPECT_EQ(param_definition.GetConstantSubblockDuration(), 10);
  EXPECT_EQ(param_definition.default_azimuth_, 2);
  EXPECT_EQ(param_definition.default_elevation_, 90);
  EXPECT_EQ(param_definition.default_distance_, 4);
}

TEST(PolarParamDefinitionTest, WriteAndValidateSucceeds) {
  PolarParamDefinition param_definition(GetPolarParamDefinitionArgs());
  param_definition.default_azimuth_ = 2;
  param_definition.default_elevation_ = 3;
  param_definition.default_distance_ = 4;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rate
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                        0x01, 0x01, 0x84};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(PolarParamDefinitionTest,
     WriteAndValidateSucceedsAzimuthAndElevationClipped) {
  PolarParamDefinition param_definition(GetPolarParamDefinitionArgs());
  param_definition.default_azimuth_ = 181;
  param_definition.default_elevation_ = 91;
  param_definition.default_distance_ = 4;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rate
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                        // default_azimuth = 180 (9 bits)
                                        // default_elevation = 90 (8 bits)
                                        // default_distance = 4 (7 bits)
                                        0b0101'1010, 0b0010'1101, 0b0000'0100};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(PolarParamDefinitionTest, CreateParameterDataFromBufferSucceeds) {
  PolarParamDefinition param_definition(GetPolarParamDefinitionArgs());
  // Expected values:
  //   azimuth = 1
  //   elevation = -2
  //   distance = 127
  std::vector<uint8_t> payload = {
      // Byte 0: animation_type (0 = kStep)
      0b00000000,
      // Byte 1: azimuth_start[8:1] (00000000)
      0b00000000,
      // Byte 2: azimuth_start[0] (1) | elevation_start[7:1] (1111111)
      0b11111111,
      // Byte 3: elevation_start[0] (0) | distance_start[6:0] (1111111)
      0b01111111,
  };
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(payload);
  auto parameter_data = param_definition.CreateParameterDataFromBuffer(*rb);
  ASSERT_THAT(parameter_data, IsOk());
  auto* polar_data = dynamic_cast<PolarParameterData*>(parameter_data->get());
  ASSERT_NE(polar_data, nullptr);
  EXPECT_EQ(polar_data->animation_type(), AnimationType::kStep);
  EXPECT_EQ(*polar_data->azimuth().start_point_value(), 1);
  EXPECT_EQ(*polar_data->elevation().start_point_value(), -2);
  EXPECT_EQ(*polar_data->distance().start_point_value(), 127);
}

}  // namespace
}  // namespace iamf_tools
