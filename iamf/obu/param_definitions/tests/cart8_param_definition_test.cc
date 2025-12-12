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
#include "iamf/obu/param_definitions/cart8_param_definition.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/cart8_parameter_data.h"
#include "iamf/obu/param_definitions.h"

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

TEST(Cart8ParamDefinitionTest, GetType) {
  Cart8ParamDefinition param_definition;
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionCart8);
}

TEST(Cart8ParamDefinitionTest, ReadAndValidateSucceeds) {
  Cart8ParamDefinition param_definition;
  std::vector<uint8_t> data = {1,   // parameter_id
                               1,   // parameter_rate
                               0,   // mode
                               10,  // duration
                               10,  // constant_subblock_duration
                               // default_x = 1 (8 bits)
                               // default_y = 2 (8 bits)
                               // default_z = 3 (8 bits)
                               0x01, 0x02, 0x03};

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_EQ(param_definition.param_definition_mode_, 0);
  EXPECT_EQ(param_definition.duration_, 10);
  EXPECT_EQ(param_definition.constant_subblock_duration_, 10);
  EXPECT_EQ(param_definition.default_x_, 1);
  EXPECT_EQ(param_definition.default_y_, 2);
  EXPECT_EQ(param_definition.default_z_, 3);
}

TEST(Cart8ParamDefinitionTest, WriteAndValidateSucceeds) {
  Cart8ParamDefinition param_definition;
  PopulateParamDefinition(&param_definition);
  param_definition.default_x_ = 1;
  param_definition.default_y_ = 2;
  param_definition.default_z_ = 3;

  std::vector<uint8_t> expected_data = {1,   // parameter_id
                                        1,   // parameter_rate
                                        0,   // mode
                                        10,  // duration
                                        10,  // constant_subblock_duration
                                        0x01, 0x02, 0x03};
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(Cart8ParamDefinitionTest, CreateParameterDataReturnsNonNull) {
  Cart8ParamDefinition param_definition;
  EXPECT_NE(param_definition.CreateParameterData(), nullptr);
}

}  // namespace
}  // namespace iamf_tools
