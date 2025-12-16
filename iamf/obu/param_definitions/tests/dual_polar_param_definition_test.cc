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
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/param_definition_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr int32_t kBufferSize = 256;

void PopulateParamDefinition(ParamDefinition* param_definition) {
  param_definition->parameter_id_ = 1;
  param_definition->parameter_rate_ = 1;
  param_definition->param_definition_mode_ = 0;
  param_definition->duration_ = 10;
  param_definition->constant_subblock_duration_ = 10;
  param_definition->reserved_ = 0;
}

TEST(DualPolarParamDefinitionTest, GetType) {
  DualPolarParamDefinition param_definition;
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDualPolar);
}

TEST(DualPolarParamDefinitionTest, ReadAndValidateSucceeds) {
  DualPolarParamDefinition param_definition;
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_first_azimuth = 2 (9 bits)
                               // default_first_elevation = 3 (8 bits)
                               // default_first_distance = 4 (7 bits)
                               // 00000001 00000001 10000100
                               0x01, 0x01, 0x84,
                               // default_second_azimuth = 5 (9 bits)
                               // default_second_elevation = 6 (8 bits)
                               // default_second_distance = 7 (7 bits)
                               0b0000'0010, 0b1000'0011, 0b0000'0111};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_EQ(param_definition.param_definition_mode_, 0);
  EXPECT_EQ(param_definition.duration_, 10);
  EXPECT_EQ(param_definition.constant_subblock_duration_, 10);
  EXPECT_EQ(param_definition.default_first_azimuth_, 2);
  EXPECT_EQ(param_definition.default_first_elevation_, 3);
  EXPECT_EQ(param_definition.default_first_distance_, 4);
  EXPECT_EQ(param_definition.default_second_azimuth_, 5);
  EXPECT_EQ(param_definition.default_second_elevation_, 6);
  EXPECT_EQ(param_definition.default_second_distance_, 7);
}

TEST(DualPolarParamDefinitionTest, ReadAndValidateClipsAzimuth) {
  DualPolarParamDefinition param_definition;
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_first_azimuth = 181 (9 bits)
                               // default_first_elevation = 3 (8 bits)
                               // default_first_distance = 4 (7 bits)
                               0b0101'1010, 0b1000'0001, 0b1000'0100,
                               // default_second_azimuth = 5 (9 bits)
                               // default_second_elevation = 6 (8 bits)
                               // default_second_distance = 7 (7 bits)
                               0b0000'0010, 0b1000'0011, 0b0000'0111};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_EQ(param_definition.param_definition_mode_, 0);
  EXPECT_EQ(param_definition.duration_, 10);
  EXPECT_EQ(param_definition.constant_subblock_duration_, 10);
  EXPECT_EQ(param_definition.default_first_azimuth_, 180);
  EXPECT_EQ(param_definition.default_first_elevation_, 3);
  EXPECT_EQ(param_definition.default_first_distance_, 4);
  EXPECT_EQ(param_definition.default_second_azimuth_, 5);
  EXPECT_EQ(param_definition.default_second_elevation_, 6);
  EXPECT_EQ(param_definition.default_second_distance_, 7);
}

TEST(DualPolarParamDefinitionTest, ReadAndValidateClipsElevation) {
  DualPolarParamDefinition param_definition;
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_first_azimuth = 2 (9 bits)
                               // default_first_elevation = 91 (8 bits)
                               // default_first_distance = 4 (7 bits)
                               0b0000'0001, 0b0010'1101, 0b1000'0100,
                               // default_second_azimuth = 2 (9 bits)
                               // default_second_elevation = 91 (8 bits)
                               // default_second_distance = 4 (7 bits)
                               0b0000'0001, 0b0010'1101, 0b1000'0100};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_EQ(param_definition.param_definition_mode_, 0);
  EXPECT_EQ(param_definition.duration_, 10);
  EXPECT_EQ(param_definition.constant_subblock_duration_, 10);
  EXPECT_EQ(param_definition.default_first_azimuth_, 2);
  EXPECT_EQ(param_definition.default_first_elevation_, 90);
  EXPECT_EQ(param_definition.default_first_distance_, 4);
  EXPECT_EQ(param_definition.default_second_azimuth_, 2);
  EXPECT_EQ(param_definition.default_second_elevation_, 90);
  EXPECT_EQ(param_definition.default_second_distance_, 4);
}

TEST(DualPolarParamDefinitionTest, WriteAndValidateSucceeds) {
  DualPolarParamDefinition param_definition;
  PopulateParamDefinition(&param_definition);
  param_definition.default_first_azimuth_ = 2;
  param_definition.default_first_elevation_ = 3;
  param_definition.default_first_distance_ = 4;
  param_definition.default_second_azimuth_ = 5;
  param_definition.default_second_elevation_ = 6;
  param_definition.default_second_distance_ = 7;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rate
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                        // default_first_azimuth = 2 (9 bits)
                                        // default_first_elevation = 3 (8 bits)
                                        // default_first_distance = 4 (7 bits)
                                        0x01, 0x01, 0x84,
                                        // default_second_azimuth = 5 (9 bits)
                                        // default_second_elevation = 6 (8 bits)
                                        // default_second_distance = 7 (7 bits)
                                        0b0000'0010, 0b1000'0011, 0b0000'0111};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(DualPolarParamDefinitionTest,
     WriteAndValidateSucceedsAzimuthAndElevationClipped) {
  DualPolarParamDefinition param_definition;
  PopulateParamDefinition(&param_definition);
  param_definition.default_first_azimuth_ = 181;
  param_definition.default_first_elevation_ = 91;
  param_definition.default_first_distance_ = 4;
  param_definition.default_second_azimuth_ = 190;
  param_definition.default_second_elevation_ = 91;
  param_definition.default_second_distance_ = 7;

  std::vector<uint8_t> expected_data = {
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_azimuth = 180 (9 bits)
      // default_first_elevation = 90 (8 bits)
      // default_first_distance = 4 (7 bits)
      0b0101'1010, 0b0010'1101, 0b0000'0100,
      // default_second_azimuth = 180 (9 bits)
      // default_second_elevation = 90 (8 bits)
      // default_second_distance = 7 (7 bits)
      0b0101'1010, 0b0010'1101, 0b0000'0111};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(DualPolarParamDefinitionTest, CreateParameterDataReturnsNonNull) {
  DualPolarParamDefinition param_definition;
  EXPECT_NE(param_definition.CreateParameterData(), nullptr);
}

}  // namespace
}  // namespace iamf_tools
