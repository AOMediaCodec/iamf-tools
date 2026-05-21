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
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"

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
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr int32_t kBufferSize = 256;

ParamDefinition::BaseArgs GetDualCart8ParamDefinitionArgs() {
  constexpr DecodedUleb128 kParameterId = 1;
  constexpr DecodedUleb128 kParameterRate = 1;
  constexpr DecodedUleb128 kDuration = 10;
  return MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                                kDuration);
}

TEST(DualCart8ParamDefinitionTest, GetType) {
  DualCart8ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDualCart8);
}

TEST(DualCart8ParamDefinitionTest, ReadAndValidateSucceeds) {
  DualCart8ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_first_x = 1 (8 bits)
                               // default_first_y = 2 (8 bits)
                               // default_first_z = 3 (8 bits)
                               // default_second_x = 4 (8 bits)
                               // default_second_y = 5 (8 bits)
                               // default_second_z = 6 (8 bits)
                               0x01, 0x02, 0x03, 0x04, 0x05, 0x06};

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

TEST(DualCart8ParamDefinitionTest, WriteAndValidateSucceeds) {
  DualCart8ParamDefinition param_definition(GetDualCart8ParamDefinitionArgs());
  param_definition.default_first_x_ = 1;
  param_definition.default_first_y_ = 2;
  param_definition.default_first_z_ = 3;
  param_definition.default_second_x_ = 4;
  param_definition.default_second_y_ = 5;
  param_definition.default_second_z_ = 6;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rate
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                        0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(DualCart8ParamDefinitionTest, CreateParameterDataReturnsNonNull) {
  DualCart8ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  EXPECT_NE(param_definition.CreateParameterData(), nullptr);
}

}  // namespace
}  // namespace iamf_tools
