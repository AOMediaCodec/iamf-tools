/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/param_definitions/extended_param_definition.h"

#include <cstdint>
#include <memory>
#include <optional>
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

constexpr int32_t kDefaultBufferSize = 64;
constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;

ParamDefinition::BaseArgs GetExtendedParamDefinitionMode1Args() {
  return MakeScheduleInParameterBlockBaseArgs(kParameterId, kParameterRate);
}

TEST(ExtendedParamDefinition, CopyConstructible) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedStart,
      MakeScheduleInParameterBlockBaseArgs(kParameterId, kParameterRate));
  extended_param_definition.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};

  const auto other = extended_param_definition;

  EXPECT_EQ(extended_param_definition, other);
}

TEST(ExtendedParamDefinition, GetTypeHasCorrectValue) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd,
      GetExtendedParamDefinitionMode1Args());

  EXPECT_EQ(extended_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionReservedEnd);
}

TEST(ExtendedParamDefinitionValidateAndWrite, SizeMayBeZero) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd,
      GetExtendedParamDefinitionMode1Args());
  extended_param_definition.param_definition_bytes_ = {};
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(extended_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Param Definition Size.
                            0x00});
}

TEST(ExtendedParamDefinitionValidateAndWrite,
     WritesOnlySizeAndParamDefinitionBytes) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd,
      GetExtendedParamDefinitionMode1Args());
  extended_param_definition.param_definition_bytes_ = {0x01, 0x02, 0x03, 0x04};
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(extended_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Param Definition Size.
                            0x04,
                            // Param Definition Bytes.
                            0x01, 0x02, 0x03, 0x04});
}

constexpr auto kExtensiontype =
    ParamDefinition::kParameterDefinitionReservedStart;
TEST(ExtendedParamDefinition, ReadAndValidateWithZeroSize) {
  std::vector<uint8_t> bitstream = {// param_definition_size.
                                    0x00};

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ExtendedParamDefinition param_definition =
      ExtendedParamDefinition(kExtensiontype, ParamDefinition::BaseArgs{});
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());

  EXPECT_EQ(param_definition.GetType(), kExtensiontype);
  EXPECT_TRUE(param_definition.param_definition_bytes_.empty());
}

TEST(ExtendedParamDefinition, ReadAndValidateWithNonZeroSize) {
  const std::vector<uint8_t> kExpectedParamDefinitionBytes = {'e', 'x', 't',
                                                              'r', 'a'};
  std::vector<uint8_t> bitstream = {// param_definition_size.
                                    0x05,
                                    // param_definition_bytes.
                                    'e', 'x', 't', 'r', 'a'};

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ExtendedParamDefinition param_definition =
      ExtendedParamDefinition(kExtensiontype, ParamDefinition::BaseArgs{});
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());

  EXPECT_EQ(param_definition.GetType(), kExtensiontype);
  EXPECT_EQ(param_definition.param_definition_bytes_,
            kExpectedParamDefinitionBytes);
}

TEST(ExtendedParamDefinitionEqualityOperator, Equals) {
  ExtendedParamDefinition lhs(
      ParamDefinition::kParameterDefinitionReservedStart,
      ParamDefinition::BaseArgs{});
  lhs.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};
  ExtendedParamDefinition rhs(
      ParamDefinition::kParameterDefinitionReservedStart,
      ParamDefinition::BaseArgs{});
  rhs.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};

  EXPECT_EQ(lhs, rhs);
}

TEST(ExtendedParamDefinitionEqualityOperator, NotEqualsWhenTypeIsDifferent) {
  const ExtendedParamDefinition lhs(
      ParamDefinition::kParameterDefinitionReservedStart,
      ParamDefinition::BaseArgs{});
  const ExtendedParamDefinition rhs(
      ParamDefinition::kParameterDefinitionReservedEnd,
      ParamDefinition::BaseArgs{});

  EXPECT_NE(lhs, rhs);
}

TEST(ExtendedParamDefinitionEqualityOperator, NotEqualsWhenPayloadIsDifferent) {
  ExtendedParamDefinition lhs(
      ParamDefinition::kParameterDefinitionReservedStart,
      ParamDefinition::BaseArgs{});
  lhs.param_definition_bytes_ = {'e', 'x', 't'};
  ExtendedParamDefinition rhs(
      ParamDefinition::kParameterDefinitionReservedStart,
      ParamDefinition::BaseArgs{});
  rhs.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};

  EXPECT_NE(lhs, rhs);
}

}  // namespace
}  // namespace iamf_tools
