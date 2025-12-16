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
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr int32_t kDefaultBufferSize = 64;
constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 64;

void PopulateParameterDefinitionMode0(ParamDefinition& param_definition) {
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = kParameterRate;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = kDuration;
  param_definition.constant_subblock_duration_ = kDuration;
  param_definition.reserved_ = 0;
}

void InitSubblockDurations(
    ParamDefinition& param_definition,
    absl::Span<const DecodedUleb128> subblock_durations) {
  param_definition.InitializeSubblockDurations(
      static_cast<DecodedUleb128>(subblock_durations.size()));
  for (int i = 0; i < subblock_durations.size(); ++i) {
    EXPECT_THAT(param_definition.SetSubblockDuration(i, subblock_durations[i]),
                IsOk());
  }
}

TEST(DemixingParamDefinition, CopyConstructible) {
  DemixingParamDefinition demixing_param_definition;
  PopulateParameterDefinitionMode0(demixing_param_definition);
  demixing_param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode1;
  demixing_param_definition.default_demixing_info_parameter_data_.reserved = 0;
  demixing_param_definition.default_demixing_info_parameter_data_.default_w = 0;
  demixing_param_definition.default_demixing_info_parameter_data_
      .reserved_for_future_use = 0;

  const auto other = demixing_param_definition;

  EXPECT_EQ(demixing_param_definition, other);
}

DemixingParamDefinition CreateDemixingParamDefinition() {
  DemixingParamDefinition demixing_param_definition;
  PopulateParameterDefinitionMode0(demixing_param_definition);
  demixing_param_definition.parameter_id_ = 0;
  demixing_param_definition.parameter_rate_ = 1;
  demixing_param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode1;
  demixing_param_definition.default_demixing_info_parameter_data_.reserved = 0;
  demixing_param_definition.default_demixing_info_parameter_data_.default_w = 0;
  demixing_param_definition.default_demixing_info_parameter_data_
      .reserved_for_future_use = 0;
  return demixing_param_definition;
}

TEST(DemixingParamDefinition, GetTypeHasCorrectValue) {
  auto demixing_param_definition = CreateDemixingParamDefinition();

  EXPECT_TRUE(demixing_param_definition.GetType().has_value());
  EXPECT_EQ(*demixing_param_definition.GetType(),
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
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.parameter_id_ = 1;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(demixing_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(
      wb, {// Parameter ID.
           0x01,
           // Same as default.
           0x01, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0});
}

TEST(DemixingParamDefinitionValidateAndWrite, WritesParameterRate) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.parameter_rate_ = 2;
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
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.duration_ = 32;
  demixing_param_definition.constant_subblock_duration_ = 32;
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
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.duration_ = 64;
  demixing_param_definition.constant_subblock_duration_ = 63;

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
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.parameter_id_ = 0;
  demixing_param_definition.parameter_rate_ = 1;
  demixing_param_definition.duration_ = 64;
  demixing_param_definition.constant_subblock_duration_ = 64;
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

TEST(DemixingParamDefinitionValidate,
     InvalidWhenConstantSubblockDurationIsZero) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.duration_ = 64;
  demixing_param_definition.constant_subblock_duration_ = 0;
  InitSubblockDurations(demixing_param_definition, {32, 32});

  EXPECT_THAT(demixing_param_definition.Validate(), Not(IsOk()));
}

TEST(DemixingParamDefinitionValidate, InvalidWhenImpliedNumSubblocksIsNotOne) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.duration_ = 64;
  demixing_param_definition.constant_subblock_duration_ = 32;

  EXPECT_THAT(demixing_param_definition.Validate(), Not(IsOk()));
}

TEST(DemixingParamDefinitionValidate, InvalidWhenParamDefinitionModeIsOne) {
  auto demixing_param_definition = CreateDemixingParamDefinition();
  demixing_param_definition.param_definition_mode_ = true;

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
  DemixingParamDefinition param_definition = DemixingParamDefinition();
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
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
  DemixingParamDefinition param_definition = DemixingParamDefinition();
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDemixing);
  EXPECT_EQ(param_definition.default_demixing_info_parameter_data_.dmixp_mode,
            DemixingInfoParameterData::kDMixPMode1);
  EXPECT_EQ(param_definition.default_demixing_info_parameter_data_.default_w,
            1);
}

}  // namespace
}  // namespace iamf_tools
