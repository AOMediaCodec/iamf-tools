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
#include "iamf/obu/param_definitions/demixing_param_definition.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr int32_t kDefaultBufferSize = 64;
constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 64;

ParamDefinition::BaseArgs GetDefaultDemixingBaseArgs() {
  return MakeOneSubblockParamDefinitionBaseArgs(
      kParameterId, /*parameter_rate=*/1, kDuration);
}

DemixingParamDefinition CreateDemixingParamDefinition(
    const ParamDefinition::BaseArgs& args = GetDefaultDemixingBaseArgs()) {
  DemixingParamDefinition demixing_param_definition(args);
  demixing_param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode1;
  demixing_param_definition.default_demixing_info_parameter_data_.reserved = 0;
  demixing_param_definition.default_demixing_info_parameter_data_.default_w = 0;
  demixing_param_definition.default_demixing_info_parameter_data_
      .reserved_for_future_use = 0;
  return demixing_param_definition;
}

TEST(DemixingParamDefinition, CopyConstructible) {
  DemixingParamDefinition demixing_param_definition(
      MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                             kDuration));
  demixing_param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode1;
  demixing_param_definition.default_demixing_info_parameter_data_.reserved = 0;
  demixing_param_definition.default_demixing_info_parameter_data_.default_w = 0;
  demixing_param_definition.default_demixing_info_parameter_data_
      .reserved_for_future_use = 0;

  const auto other = demixing_param_definition;

  EXPECT_EQ(demixing_param_definition, other);
}

TEST(DemixingParamDefinition, GetTypeHasCorrectValue) {
  auto demixing_param_definition = CreateDemixingParamDefinition();

  EXPECT_EQ(demixing_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDemixing);
}

TEST(DemixingParamDefinitionValidateAndWrite, DefaultParamDefinitionMode0) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            0x01,
                            // Parameter Definition Mode (upper bit).
                            0x00,
                            // Duration.
                            64,
                            // Constant Subblock Duration.
                            64,
                            // Default Demixing Info Parameter Data.
                            DemixingInfoParameterData::kDMixPMode1 << 5, 0});
}

TEST(DemixingParamDefinitionValidateAndWrite, WritesParameterId) {
  auto args = GetDefaultDemixingBaseArgs();
  args.parameter_id = 1;
  auto demixing_param_definition = CreateDemixingParamDefinition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(
      wb, {// Parameter ID.
           0x01,
           // Same as default.
           0x01, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0});
}

TEST(DemixingParamDefinitionValidateAndWrite, WritesParameterRate) {
  auto args = GetDefaultDemixingBaseArgs();
  args.parameter_rate = 2;
  auto demixing_param_definition = CreateDemixingParamDefinition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(
      wb, {0x00,
           // Parameter Rate.
           0x02,
           // Same as default.
           0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0});
}

TEST(DemixingParamDefinitionValidateAndWrite,
     EqualDurationAndConstantSubblockDuration) {
  auto args = MakeOneSubblockParamDefinitionBaseArgs(
      kParameterId, /*parameter_rate=*/1, /*duration=*/32);
  auto demixing_param_definition = CreateDemixingParamDefinition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 0x01, 0x00,
                            // Duration.
                            32,
                            // Constant Subblock Duration.
                            32, 0, 0});
}

TEST(DemixingParamDefinitionValidate,
     InvalidWhenDurationDoesNotEqualConstantSubblockDuration) {
  const DecodedUleb128 kMisMatchingSubblockDuration = kDuration / 2;
  auto args = MakeConstantSubblocksParamDefinitionBaseArgs(
      kParameterId, kParameterRate, kDuration, kMisMatchingSubblockDuration);
  auto demixing_param_definition = CreateDemixingParamDefinition(args);

  EXPECT_THAT(demixing_param_definition.Validate(), Not(IsOk()));
}

TEST(DemixingParamDefinitionValidateAndWrite, WritesDefaultDmixPMode) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode2;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 0x01, 0x00, 64, 64,
                            // `dmixp_mode`.
                            DemixingInfoParameterData::kDMixPMode2 << 5,
                            // `default_w`.
                            0});
}

TEST(DemixingParamDefinitionValidateAndWrite, WritesDefaultW) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.default_demixing_info_parameter_data_.default_w = 1;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 0x01, 0x00, 64, 64,
                            DemixingInfoParameterData::kDMixPMode1 << 5,
                            // `default_w`.
                            1 << 4});
}

TEST(DemixingParamDefinitionValidateAndWrite,
     NonMinimalLebGeneratorAffectsAllLeb128s) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  ASSERT_NE(leb_generator, nullptr);
  auto demixing_param_definition =
      CreateDemixingParamDefinition(GetDefaultDemixingBaseArgs());
  WriteBitBuffer wb(kDefaultBufferSize, *leb_generator);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(
      wb, {// `parameter_id`.
           0x80, 0x00,
           // `parameter_rate`.
           0x81, 0x00,
           // `param_definition_mode` (1), reserved (7).
           0x00,
           // `duration`.
           0xc0, 0x00,
           // `constant_subblock_duration`.
           0xc0, 0x00, DemixingInfoParameterData::kDMixPMode1 << 5, 0});
}

TEST(DemixingParamDefinitionValidate, InvalidWithVariableSubblockDurations) {
  auto args = MakeVariableSubblocksParamDefinitionBaseArgs(
      kParameterId, kParameterRate, {kDuration / 2, kDuration / 2});
  auto demixing_param_definition = CreateDemixingParamDefinition(args);

  EXPECT_THAT(demixing_param_definition.Validate(), Not(IsOk()));
}

TEST(DemixingParamDefinitionValidate, InvalidWhenImpliedNumSubblocksIsNotOne) {
  // Two subblocks are implied
  const DecodedUleb128 kConstantSubblockDuration = kDuration / 2;
  auto args = MakeConstantSubblocksParamDefinitionBaseArgs(
      kParameterId, kParameterRate, kDuration, kConstantSubblockDuration);
  auto demixing_param_definition = CreateDemixingParamDefinition(args);

  EXPECT_THAT(demixing_param_definition.Validate(), Not(IsOk()));
}

TEST(DemixingParamDefinitionValidate, InvalidWhenParamDefinitionModeIsOne) {
  auto demixing_param_definition = CreateDemixingParamDefinition(
      MakeScheduleInParameterBlockBaseArgs(kParameterId, kParameterRate));

  EXPECT_THAT(demixing_param_definition.Validate(), Not(IsOk()));
}

TEST(ReadDemixingParamDefinitionTest, ReadsDefaultDmixPMode) {
  std::vector<uint8_t> bitstream = {// Parameter ID.
                                    0x00,
                                    // Parameter Rate.
                                    0x01,
                                    // Parameter Definition Mode (upper bit).
                                    0x00,
                                    // Duration.
                                    64,
                                    // Constant Subblock Duration.
                                    64,
                                    // `dmixp_mode`.
                                    DemixingInfoParameterData::kDMixPMode2 << 5,
                                    // `default_w`.
                                    0};

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  DemixingParamDefinition param_definition =
      DemixingParamDefinition(ParamDefinition::BaseArgs{});
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());

  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDemixing);
  EXPECT_EQ(param_definition.default_demixing_info_parameter_data_.dmixp_mode,
            DemixingInfoParameterData::kDMixPMode2);
}

TEST(ReadDemixingParamDefinitionTest, ReadsDefaultW) {
  std::vector<uint8_t> bitstream = {// Parameter ID.
                                    0x00,
                                    // Parameter Rate.
                                    0x01,
                                    // Parameter Definition Mode (upper bit).
                                    0x00,
                                    // Duration.
                                    64,
                                    // Constant Subblock Duration.
                                    64,
                                    // `dmixp_mode`.
                                    DemixingInfoParameterData::kDMixPMode1 << 5,
                                    // `default_w`.
                                    1 << 4};

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  DemixingParamDefinition param_definition =
      DemixingParamDefinition(ParamDefinition::BaseArgs{});
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDemixing);
  EXPECT_EQ(param_definition.default_demixing_info_parameter_data_.dmixp_mode,
            DemixingInfoParameterData::kDMixPMode1);
  EXPECT_EQ(param_definition.default_demixing_info_parameter_data_.default_w,
            1);
}

}  // namespace
}  // namespace iamf_tools
