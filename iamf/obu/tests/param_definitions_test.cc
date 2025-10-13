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
#include "iamf/obu/param_definitions.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr int32_t kDefaultBufferSize = 64;
constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 64;

void PopulateParameterDefinitionMode1(ParamDefinition& param_definition) {
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  param_definition.reserved_ = 0;
}

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

TEST(MixGainParamDefinition, ConstructorSetsDefaultMixGainToZero) {
  MixGainParamDefinition mix_gain_param_definition;

  EXPECT_EQ(mix_gain_param_definition.default_mix_gain_, 0);
}

TEST(MixGainParamDefinition, CopyConstructible) {
  MixGainParamDefinition mix_gain_param_definition;
  PopulateParameterDefinitionMode1(mix_gain_param_definition);
  mix_gain_param_definition.default_mix_gain_ = -16;

  const auto other = mix_gain_param_definition;

  EXPECT_EQ(mix_gain_param_definition, other);
}

TEST(MixGainParamDefinition, GetTypeHasCorrectValue) {
  MixGainParamDefinition mix_gain_param_definition;

  EXPECT_EQ(mix_gain_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionMixGain);
}

TEST(MixGainParamDefinitionValidateAndWrite, DefaultParamDefinitionMode1) {
  MixGainParamDefinition mix_gain_param_definition;
  PopulateParameterDefinitionMode1(mix_gain_param_definition);
  mix_gain_param_definition.default_mix_gain_ = 0;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(mix_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            1,
                            // Param Definition Mode (upper bit).
                            0x80,
                            // Default Mix Gain.
                            0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, WritesParameterId) {
  MixGainParamDefinition mix_gain_param_definition;
  PopulateParameterDefinitionMode1(mix_gain_param_definition);
  mix_gain_param_definition.parameter_id_ = 1;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(mix_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x01,
                            // Same as default.
                            1, 0x80, 0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, NonMinimalLeb) {
  MixGainParamDefinition mix_gain_param_definition;
  PopulateParameterDefinitionMode1(mix_gain_param_definition);
  mix_gain_param_definition.parameter_id_ = 1;
  mix_gain_param_definition.parameter_rate_ = 5;
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  ASSERT_NE(leb_generator.get(), nullptr);
  WriteBitBuffer wb(kDefaultBufferSize, *leb_generator);

  EXPECT_THAT(mix_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x81, 0x00,
                            // Parameter Rate.
                            0x85, 0x00,
                            // Same as default.
                            0x80, 0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, WritesParameterRate) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.parameter_rate_ = 64;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00,
                            // Parameter Rate.
                            64,
                            // Same as default.
                            0x80, 0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, WritesDefaultMixGain) {
  MixGainParamDefinition param_definition;
  param_definition.default_mix_gain_ = 3;
  PopulateParameterDefinitionMode1(param_definition);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 1, 0x80,
                            // Default Mix Gain.
                            0, 3});
}

TEST(MixGainParamDefinitionValidate, ParameterRateMustNotBeZero) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.parameter_rate_ = 0;

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(MixGainParamDefinitionValidateAndWrite,
     ParamDefinitionMode0WithConstantSubblockDurationNonZero) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.param_definition_mode_ = false;
  param_definition.duration_ = 3;
  param_definition.constant_subblock_duration_ = 3;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            1,
                            // Param Definition Mode (upper bit).
                            0,
                            // Duration.
                            3,
                            // Constant Subblock Duration.
                            3,
                            // Default Mix Gain.
                            0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite,
     ParamDefinitionMode0WithConstantSubblockDurationZeroIncludesDurations) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.param_definition_mode_ = false;
  param_definition.duration_ = 10;
  param_definition.constant_subblock_duration_ = 0;
  InitSubblockDurations(param_definition, {1, 2, 3, 4});
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            1,
                            // Parameter Definition Mode (upper bit).
                            0,
                            // Duration.
                            10,
                            // Constant Subblock Duration.
                            0,
                            // Num subblocks.
                            4,
                            // Subblock 0.
                            1,
                            // Subblock 1.
                            2,
                            // Subblock 2.
                            3,
                            // Subblock 3.
                            4,
                            // Default Mix Gain.
                            0, 0});
}

TEST(MixGainParamDefinitionValidate,
     InvalidWhenExplicitSubblockDurationsDoNotSumToDuration) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.param_definition_mode_ = false;
  param_definition.duration_ = 100;
  param_definition.constant_subblock_duration_ = 0;
  InitSubblockDurations(param_definition, {1, 2, 3, 4});  // Does not sum 100.

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(MixGainParamDefinitionValidate, InvalidWhenDurationIsZero) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.param_definition_mode_ = false;
  param_definition.duration_ = 0;
  param_definition.constant_subblock_duration_ = 0;
  InitSubblockDurations(param_definition, {0});

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(MixGainParamDefinitionValidate, InvalidWhenSubblockDurationIsZero) {
  MixGainParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.param_definition_mode_ = false;
  param_definition.duration_ = 10;
  param_definition.constant_subblock_duration_ = 0;
  InitSubblockDurations(param_definition, {5, 0, 5});

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
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
  demixing_param_definition.constant_subblock_duration_ = 65;

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

ReconGainParamDefinition CreateReconGainParamDefinition() {
  ReconGainParamDefinition recon_gain_param_definition(0);
  recon_gain_param_definition.parameter_id_ = 0;
  recon_gain_param_definition.parameter_rate_ = 1;
  recon_gain_param_definition.param_definition_mode_ = false;
  recon_gain_param_definition.duration_ = 64;
  recon_gain_param_definition.constant_subblock_duration_ = 64;
  return recon_gain_param_definition;
}

TEST(ReconGainParamDefinition, CopyConstructible) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();

  const auto other = recon_gain_param_definition;

  EXPECT_EQ(recon_gain_param_definition, other);
}

TEST(ReconGainParamDefinition, GetTypeHasCorrectValue) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  EXPECT_TRUE(recon_gain_param_definition.GetType().has_value());
  EXPECT_EQ(*recon_gain_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionReconGain);
}

TEST(ReconGainParamDefinitionValidateAndWrite,
     WritesCorrectlyWithDefaultValues) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            0x01,
                            // Parameter Definition Mode (upper bit).
                            0x00,
                            // Duration.
                            64,
                            // Constant Subblock Duration.
                            64});
}

TEST(ReconGainParamDefinitionValidateAndWrite, WritesParameterId) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.parameter_id_ = 1;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x01,
                            // Same as default.
                            0x01, 0x00, 64, 64});
}

TEST(ReconGainParamDefinitionValidateAndWrite, WritesParameterRate) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.parameter_id_ = 1;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x01,
                            // Parameter Rate.
                            0x01,
                            // Same as default.
                            0x00, 64, 64});
}

TEST(ReconGainParamDefinitionValidateAndWrite, WritesDuration) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.duration_ = 32;
  recon_gain_param_definition.constant_subblock_duration_ = 32;
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 0x01, 0x00,
                            // Duration.
                            32,
                            // Constant Subblock Duration.
                            32});
}

TEST(ReconGainParamDefinitionValidateAndWrite, AuxiliaryDataNotWritten) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  WriteBitBuffer wb(kDefaultBufferSize);

  // Fill in some auxililary data.
  recon_gain_param_definition.aux_data_ = {
      {
          .recon_gain_is_present_flag = false,
          .channel_numbers_for_layer = {2, 0, 0},
      },
      {
          .recon_gain_is_present_flag = true,
          .channel_numbers_for_layer = {5, 1, 2},
      },
  };
  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  // Same as the bitstream in the `ReconGainParamDefinitionTest.Default` test
  // above, without the auxiliary data.
  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            0x01,
                            // Parameter Definition Mode (upper bit).
                            0x00,
                            // Duration.
                            64,
                            // Constant Subblock Duration.
                            64});
}

TEST(ReconGainParamDefinitionValidateAndWrite,
     NonMinimalLebGeneratorAffectsAllLeb128s) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.parameter_id_ = 0;
  recon_gain_param_definition.parameter_rate_ = 1;
  recon_gain_param_definition.constant_subblock_duration_ = 64;
  WriteBitBuffer wb(kDefaultBufferSize, *leb_generator);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// `parameter_id`.
                            0x80, 0x00,
                            // `parameter_rate`.
                            0x81, 0x00,
                            // `param_definition_mode` (1), reserved (7).
                            0x00,
                            // `duration`.
                            0x80 | 64, 0x00,
                            // `constant_subblock_duration`.
                            0x80 | 64, 0x00});
}

TEST(ReconGainParamDefinitionValidate,
     InvalidWhenConstantSubblockDurationIsZero) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.duration_ = 64;
  recon_gain_param_definition.constant_subblock_duration_ = 0;
  InitSubblockDurations(recon_gain_param_definition, {32, 32});

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReconGainParamDefinitionValidate, InvalidWhenImpliedNumSubblocksIsNotOne) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.duration_ = 64;
  recon_gain_param_definition.constant_subblock_duration_ = 32;

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReconGainParamDefinitionValidate,
     InvalidWhenDurationDoesNotEqualConstantSubblockDuration) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.duration_ = 64;
  recon_gain_param_definition.constant_subblock_duration_ = 65;

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReconGainParamDefinitionValidate, InvalidWhenParamDefinitionModeIsOne) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  recon_gain_param_definition.param_definition_mode_ = true;

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ExtendedParamDefinition, CopyConstructible) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedStart);
  extended_param_definition.param_definition_mode_ = 1;
  extended_param_definition.parameter_id_ = kParameterId;
  extended_param_definition.parameter_rate_ = kParameterRate;
  extended_param_definition.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};

  const auto other = extended_param_definition;

  EXPECT_EQ(extended_param_definition, other);
}

TEST(ExtendedParamDefinition, GetTypeHasCorrectValue) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd);
  PopulateParameterDefinitionMode1(extended_param_definition);

  EXPECT_TRUE(extended_param_definition.GetType().has_value());
  EXPECT_EQ(*extended_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionReservedEnd);
}

TEST(ExtendedParamDefinitionValidateAndWrite, SizeMayBeZero) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd);
  PopulateParameterDefinitionMode1(extended_param_definition);
  extended_param_definition.param_definition_bytes_ = {};
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(extended_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Param Definition Size.
                            0x00});
}

TEST(ExtendedParamDefinitionValidateAndWrite,
     WritesOnlySizeAndParamDefinitionBytes) {
  ExtendedParamDefinition extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd);
  PopulateParameterDefinitionMode1(extended_param_definition);
  extended_param_definition.param_definition_bytes_ = {0x01, 0x02, 0x03, 0x04};
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(extended_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Param Definition Size.
                            0x04,
                            // Param Definition Bytes.
                            0x01, 0x02, 0x03, 0x04});
}

TEST(ReadMixGainParamDefinitionTest, DefaultMixGainMode1) {
  MixGainParamDefinition param_definition;
  std::vector<uint8_t> source = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x80,
      // Default Mix Gain.
      0, 4};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
            ParamDefinition::kParameterDefinitionMixGain);
  EXPECT_EQ(param_definition.default_mix_gain_, 4);
}

TEST(ReadMixGainParamDefinitionTest, DefaultMixGainWithSubblockArray) {
  MixGainParamDefinition param_definition;
  std::vector<uint8_t> source = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x00,
      // `duration` (64).
      0xc0, 0x00,
      // `constant_subblock_duration`.
      0x00,
      // `num_subblocks`
      0x02,
      // `subblock_durations`
      // `subblock_duration[0]`
      40,
      // `subblock_duration[1]`
      24,
      // Default Mix Gain.
      0, 3};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
            ParamDefinition::kParameterDefinitionMixGain);
  EXPECT_EQ(param_definition.default_mix_gain_, 3);
}

TEST(ReadReconGainParamDefinitionTest, ReadsCorrectlyWithDefaultValues) {
  std::vector<uint8_t> bitstream = {// Parameter ID.
                                    0x00,
                                    // Parameter Rate.
                                    0x01,
                                    // Parameter Definition Mode (upper bit).
                                    0x00,
                                    // Duration.
                                    64,
                                    // Constant Subblock Duration.
                                    64};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition = ReconGainParamDefinition(0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
            ParamDefinition::kParameterDefinitionReconGain);
}

TEST(ReadReconGainParamDefinitionTest, ReadsMode1) {
  std::vector<uint8_t> bitstream = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x80};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition = ReconGainParamDefinition(0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
}

TEST(ReadReconGainParamDefinitionTest, Mode0NonZeroSubblockDuration) {
  std::vector<uint8_t> bitstream = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x00,
      // `duration`.
      0xc0, 0x00,
      // `constant_subblock_duration`.
      0xc0, 0x00};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition = ReconGainParamDefinition(0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
}

TEST(ReadReconGainParamDefinitionTest, Mode0SubblockArray) {
  std::vector<uint8_t> bitstream = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x00,
      // `duration` (64).
      0xc0, 0x00,
      // `constant_subblock_duration` (64).
      0xc0, 0x00,
      // `num_subblocks`
      0x02,
      // `subblock_durations`
      // `subblock_duration[0]`
      64};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition = ReconGainParamDefinition(0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
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

constexpr auto kExtensiontype =
    ParamDefinition::kParameterDefinitionReservedStart;
TEST(ExtendedParamDefinition, ReadAndValidateWithZeroSize) {
  std::vector<uint8_t> bitstream = {// param_definition_size.
                                    0x00};

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ExtendedParamDefinition param_definition =
      ExtendedParamDefinition(kExtensiontype);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());

  EXPECT_EQ(*param_definition.GetType(), kExtensiontype);
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
      ExtendedParamDefinition(kExtensiontype);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());

  EXPECT_EQ(*param_definition.GetType(), kExtensiontype);
  EXPECT_EQ(param_definition.param_definition_bytes_,
            kExpectedParamDefinitionBytes);
}

TEST(ExtendedParamDefinitionEqualityOperator, Equals) {
  ExtendedParamDefinition lhs(
      ParamDefinition::kParameterDefinitionReservedStart);
  lhs.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};
  ExtendedParamDefinition rhs(
      ParamDefinition::kParameterDefinitionReservedStart);
  rhs.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};

  EXPECT_EQ(lhs, rhs);
}

TEST(ExtendedParamDefinitionEqualityOperator, NotEqualsWhenTypeIsDifferent) {
  const ExtendedParamDefinition lhs(
      ParamDefinition::kParameterDefinitionReservedStart);
  const ExtendedParamDefinition rhs(
      ParamDefinition::kParameterDefinitionReservedEnd);

  EXPECT_NE(lhs, rhs);
}

TEST(ExtendedParamDefinitionEqualityOperator, NotEqualsWhenPayloadIsDifferent) {
  ExtendedParamDefinition lhs(
      ParamDefinition::kParameterDefinitionReservedStart);
  lhs.param_definition_bytes_ = {'e', 'x', 't'};
  ExtendedParamDefinition rhs(
      ParamDefinition::kParameterDefinitionReservedStart);
  rhs.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};

  EXPECT_NE(lhs, rhs);
}

}  // namespace
}  // namespace iamf_tools
