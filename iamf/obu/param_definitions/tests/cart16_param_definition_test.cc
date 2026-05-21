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
#include "iamf/obu/param_definitions/cart16_param_definition.h"

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

ParamDefinition::BaseArgs GetCart16ParamDefinitionArgs() {
  constexpr DecodedUleb128 kParameterId = 1;
  constexpr DecodedUleb128 kParameterRate = 1;
  constexpr DecodedUleb128 kDuration = 10;
  return MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                                kDuration);
}

TEST(Cart16ParamDefinitionTest, GetType) {
  Cart16ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionCart16);
}

TEST(Cart16ParamDefinitionTest, ReadAndValidateSucceeds) {
  Cart16ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_x = 1 (16 bits)
                               // default_y = 2 (16 bits)
                               // default_z = 3 (16 bits)
                               0x00, 0x01, 0x00, 0x02, 0x00, 0x03};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.GetParameterId(), 1);
  EXPECT_EQ(param_definition.GetParameterRate(), 1);
  EXPECT_EQ(param_definition.GetParamDefinitionMode(),
            ParamDefinition::kModeScheduleInParamDefinition);
  EXPECT_EQ(param_definition.GetDuration(), 10);
  EXPECT_EQ(param_definition.GetConstantSubblockDuration(), 10);
  EXPECT_EQ(param_definition.default_x_, 1);
  EXPECT_EQ(param_definition.default_y_, 2);
  EXPECT_EQ(param_definition.default_z_, 3);
}

TEST(Cart16ParamDefinitionTest, WriteAndValidateSucceeds) {
  Cart16ParamDefinition param_definition(GetCart16ParamDefinitionArgs());
  param_definition.default_x_ = 1;
  param_definition.default_y_ = 2;
  param_definition.default_z_ = 3;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rate
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                             // default_x = 1 (16 bits)
                                        // default_y = 2 (16 bits)
                                        // default_z = 3 (16 bits)
                                        0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(Cart16ParamDefinitionTest, CreateParameterDataReturnsNonNull) {
  Cart16ParamDefinition param_definition(ParamDefinition::BaseArgs{});
  EXPECT_NE(param_definition.CreateParameterData(), nullptr);
}

}  // namespace
}  // namespace iamf_tools
