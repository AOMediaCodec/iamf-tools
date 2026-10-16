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
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"

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
#include "iamf/obu/momentary_loudness_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr int32_t kBufferSize = 256;

ParamDefinition::BaseArgs GetMomentaryLoudnessParamDefinitionArgs() {
  constexpr DecodedUleb128 kParameterId = 1;
  constexpr DecodedUleb128 kParameterRate = 1;
  constexpr DecodedUleb128 kDuration = 10;
  return MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                                kDuration);
}

TEST(ReadAndValidate, SucceedsWithValidData) {
  MomentaryLoudnessParamDefinition param_definition(
      ParamDefinition::BaseArgs{});
  std::vector<uint8_t> data = {
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10   // constant_subblock_duration
  };

  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);
  EXPECT_THAT(param_definition.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(param_definition.GetParameterId(), 1);
  EXPECT_EQ(param_definition.GetParameterRate(), 1);
  EXPECT_EQ(param_definition.GetParamDefinitionMode(),
            ParamDefinition::kModeScheduleInParamDefinition);
  EXPECT_EQ(param_definition.GetDuration(), 10);
  EXPECT_EQ(param_definition.GetConstantSubblockDuration(), 10);
}

TEST(ValidateAndWrite, SucceedsWithValidData) {
  MomentaryLoudnessParamDefinition param_definition(
      GetMomentaryLoudnessParamDefinitionArgs());

  std::vector<uint8_t> expected_data = {
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10   // constant_subblock_duration
  };
  WriteBitBuffer wb(kBufferSize);
  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());
  ValidateWriteResults(wb, expected_data);
}

TEST(CreateParameterDataFromBuffer, SucceedsWithValidPayload) {
  MomentaryLoudnessParamDefinition param_definition(
      GetMomentaryLoudnessParamDefinitionArgs());
  // momentary_loudness (6 bits) = 42 (0b101010)
  // reserved (2 bits)           = 3  (0b11)
  std::vector<uint8_t> payload = {0b101010'11};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(payload);

  auto parameter_data = param_definition.CreateParameterDataFromBuffer(*rb);

  ASSERT_THAT(parameter_data, IsOk());
  auto* loudness_data =
      dynamic_cast<MomentaryLoudnessParameterData*>(parameter_data->get());
  ASSERT_NE(loudness_data, nullptr);
  EXPECT_EQ(loudness_data->momentary_loudness(), 42);
  EXPECT_EQ(loudness_data->reserved(), 3);
}

TEST(CreateParameterDataFromBuffer, FailsWhenBufferIsEmpty) {
  MomentaryLoudnessParamDefinition param_definition(
      GetMomentaryLoudnessParamDefinitionArgs());
  std::vector<uint8_t> payload = {};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(payload);

  EXPECT_THAT(param_definition.CreateParameterDataFromBuffer(*rb), Not(IsOk()));
}

TEST(ToString, FormatsCorrectly) {
  const MomentaryLoudnessParamDefinition param_definition(
      GetMomentaryLoudnessParamDefinitionArgs());

  const std::string formatted = absl::StrCat(param_definition);

  EXPECT_EQ(formatted,
            "MomentaryLoudnessParamDefinition:\n"
            "  parameter_type= 9\n"
            "  parameter_id= 1\n"
            "  parameter_rate= 1\n"
            "  param_definition_mode= 0\n"
            "  reserved= 0\n"
            "  duration= 10\n"
            "  constant_subblock_duration= 10\n"
            "  num_subblocks= 1");
}

}  // namespace
}  // namespace iamf_tools
