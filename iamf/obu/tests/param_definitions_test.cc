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
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

void PopulateParameterDefinition(ParamDefinition& param_definition) {
  param_definition.parameter_id_ = 0;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  param_definition.reserved_ = 0;
}

class ParamDefinitionTestBase : public testing::Test {
 public:
  ParamDefinitionTestBase() = default;

  void Init() {
    // Initialize the `subblock_durations_` vector then loop to populate it.
    param_definition_->InitializeSubblockDurations(
        static_cast<DecodedUleb128>(subblock_durations_.size()));
    for (int i = 0; i < subblock_durations_.size(); ++i) {
      EXPECT_THAT(
          param_definition_->SetSubblockDuration(i, subblock_durations_[i]),
          IsOk());
    }
  }

  void TestWrite(std::vector<uint8_t> expected_data) {
    ASSERT_NE(leb_generator_, nullptr);
    WriteBitBuffer wb(expected_data.size(), *leb_generator_);
    EXPECT_EQ(param_definition_->ValidateAndWrite(wb).code(),
              expected_status_code_);
    if (expected_status_code_ == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_data);
    }
  }

 protected:
  std::unique_ptr<ParamDefinition> param_definition_;
  std::vector<DecodedUleb128> subblock_durations_ = {};
  absl::StatusCode expected_status_code_ = absl::StatusCode::kOk;

  std::unique_ptr<LebGenerator> leb_generator_ = LebGenerator::Create();
};

TEST(ParamDefinitionTest, GetTypeHasNoValueWithDefaultConstructor) {
  std::unique_ptr<ParamDefinition> param_definition =
      std::make_unique<ParamDefinition>();
  EXPECT_FALSE(param_definition->GetType().has_value());
}

class MixGainParamDefinitionTest : public ParamDefinitionTestBase {
 public:
  MixGainParamDefinitionTest() {
    auto mix_gain = std::make_unique<MixGainParamDefinition>();
    mix_gain_ = mix_gain.get();
    PopulateParameterDefinition(*mix_gain_);
    mix_gain->default_mix_gain_ = 0;
    param_definition_ = std::move(mix_gain);
  }

 protected:
  // Alias for accessing the sub-class data.
  MixGainParamDefinition* mix_gain_;
};

TEST_F(MixGainParamDefinitionTest, GetTypeHasCorrectValue) {
  ASSERT_TRUE(mix_gain_->GetType().has_value());
  EXPECT_EQ(mix_gain_->GetType().value(),
            ParamDefinition::kParameterDefinitionMixGain);
}

TEST_F(MixGainParamDefinitionTest, DefaultParamDefinitionMode1) {
  Init();

  TestWrite({// Parameter ID.
             0x00,
             // Parameter Rate.
             1,
             // Param Definition Mode (upper bit).
             0x80,
             // Default Mix Gain.
             0, 0});
}

TEST_F(MixGainParamDefinitionTest, ParameterId) {
  param_definition_->parameter_id_ = 1;
  Init();

  TestWrite({// Parameter ID.
             0x01,
             // Same as default.
             1, 0x80, 0, 0});
}

TEST_F(MixGainParamDefinitionTest, NonMinimalLeb) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  param_definition_->parameter_id_ = 1;
  param_definition_->parameter_rate_ = 5;
  Init();

  TestWrite({// Parameter ID.
             0x81, 0x00,
             // Parameter Rate.
             0x85, 0x00,
             // Same as default.
             0x80, 0, 0});
}

TEST_F(MixGainParamDefinitionTest, ParameterRate) {
  param_definition_->parameter_rate_ = 64;
  Init();

  TestWrite({0x00,
             // Parameter Rate.
             64,
             // Same as default.
             0x80, 0, 0});
}

TEST_F(MixGainParamDefinitionTest, DefaultMixGain) {
  mix_gain_->default_mix_gain_ = 3;
  Init();

  TestWrite({0x00, 1, 0x80,
             // Default Mix Gain.
             0, 3});
}

TEST_F(MixGainParamDefinitionTest, ParameterRateMustNotBeZero) {
  param_definition_->parameter_rate_ = 0;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(MixGainParamDefinitionTest,
       ParamDefinitionMode0WithConstantSubblockDurationNonZero) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 3;
  param_definition_->constant_subblock_duration_ = 3;
  Init();

  TestWrite({// Parameter ID.
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

TEST_F(MixGainParamDefinitionTest,
       ParamDefinitionMode0WithConstantSubblockDurationZeroIncludesDurations) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 10;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {1, 2, 3, 4};
  Init();

  TestWrite({// Parameter ID.
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

TEST_F(MixGainParamDefinitionTest,
       InvalidWhenExplicitSubblockDurationsDoNotSumToDuration) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 100;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {1, 2, 3, 4};  // Does not sum 100.
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(MixGainParamDefinitionTest, InvalidWhenDurationIsZero) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 0;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {0};
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(MixGainParamDefinitionTest, InvalidWhenSubblockDurationIsZero) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 10;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {5, 0, 5};

  EXPECT_FALSE(param_definition_->Validate().ok());
}

class DemixingParamDefinitionTest : public ParamDefinitionTestBase {
 public:
  DemixingParamDefinitionTest() {
    auto demixing = std::make_unique<DemixingParamDefinition>();
    demixing_ = demixing.get();
    PopulateParameterDefinition(*demixing_);
    demixing->param_definition_mode_ = 0;
    demixing->duration_ = 64;
    demixing->constant_subblock_duration_ = 64;
    demixing->default_demixing_info_parameter_data_.dmixp_mode =
        DemixingInfoParameterData::kDMixPMode1;
    demixing->default_demixing_info_parameter_data_.reserved = 0;
    demixing->default_demixing_info_parameter_data_.default_w = 0;
    demixing->default_demixing_info_parameter_data_.reserved_default = 0;
    param_definition_ = std::move(demixing);
  }

 protected:
  // Alias for accessing the sub-class data.
  DemixingParamDefinition* demixing_;
};

TEST_F(DemixingParamDefinitionTest, GetTypeHasCorrectValue) {
  EXPECT_TRUE(demixing_->GetType().has_value());
  EXPECT_EQ(demixing_->GetType().value(),
            ParamDefinition::kParameterDefinitionDemixing);
}

TEST_F(DemixingParamDefinitionTest, DefaultParamDefinitionMode0) {
  Init();

  TestWrite({// Parameter ID.
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

TEST_F(DemixingParamDefinitionTest, ParameterId) {
  param_definition_->parameter_id_ = 1;
  Init();

  TestWrite({// Parameter ID.
             0x01,
             // Same as default.
             0x01, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5,
             0});
}

TEST_F(DemixingParamDefinitionTest, ParameterRate) {
  param_definition_->parameter_rate_ = 2;
  Init();

  TestWrite({0x00,
             // Parameter Rate.
             0x02,
             // Same as default.
             0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0});
}

TEST_F(DemixingParamDefinitionTest, EqualDurationAndConstantSubblockDuration) {
  param_definition_->duration_ = 32;
  param_definition_->constant_subblock_duration_ = 32;
  Init();

  TestWrite({0x00, 0x01, 0x00,
             // Duration.
             32,
             // Constant Subblock Duration.
             32, 0, 0});
}

TEST_F(DemixingParamDefinitionTest,
       InvalidWhenDurationDoesNotEqualConstantSubblockDuration) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 65;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(DemixingParamDefinitionTest, DefaultDmixPMode) {
  demixing_->default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode2;
  Init();

  TestWrite({0x00, 0x01, 0x00, 64, 64,
             // `dmixp_mode`.
             DemixingInfoParameterData::kDMixPMode2 << 5,
             // `default_w`.
             0});
}

TEST_F(DemixingParamDefinitionTest, DefaultW) {
  demixing_->default_demixing_info_parameter_data_.default_w = 1;
  Init();

  TestWrite({0x00, 0x01, 0x00, 64, 64,
             DemixingInfoParameterData::kDMixPMode1 << 5,
             // `default_w`.
             1 << 4});
}

TEST_F(DemixingParamDefinitionTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  param_definition_->parameter_id_ = 0;
  param_definition_->parameter_rate_ = 1;
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 64;
  Init();

  TestWrite({// `parameter_id`.
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

TEST_F(DemixingParamDefinitionTest, InvalidWhenConstantSubblockDurationIsZero) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {32, 32};
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(DemixingParamDefinitionTest, InvalidWhenImpliedNumSubblocksIsNotOne) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 32;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(DemixingParamDefinitionTest, InvalidWhenParamDefinitionModeIsOne) {
  param_definition_->param_definition_mode_ = true;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

class ReconGainParamDefinitionTest : public ParamDefinitionTestBase {
 public:
  ReconGainParamDefinitionTest() {
    auto recon_gain = std::make_unique<ReconGainParamDefinition>(0);
    PopulateParameterDefinition(*recon_gain);
    recon_gain->param_definition_mode_ = 0;
    recon_gain->reserved_ = 0;
    recon_gain->duration_ = 64;
    recon_gain->constant_subblock_duration_ = 64;
    param_definition_ = std::move(recon_gain);
  }
};

TEST_F(ReconGainParamDefinitionTest, GetTypeHasCorrectValue) {
  EXPECT_TRUE(param_definition_->GetType().has_value());
  EXPECT_EQ(param_definition_->GetType().value(),
            ParamDefinition::kParameterDefinitionReconGain);
}

TEST_F(ReconGainParamDefinitionTest, Default) {
  Init();

  TestWrite({// Parameter ID.
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

TEST_F(ReconGainParamDefinitionTest, ParameterId) {
  param_definition_->parameter_id_ = 1;
  Init();

  TestWrite({// Parameter ID.
             0x01,
             // Same as default.
             0x01, 0x00, 64, 64});
}

TEST_F(ReconGainParamDefinitionTest, ParameterRate) {
  param_definition_->parameter_id_ = 1;
  Init();

  TestWrite({0x01,
             // Parameter Rate.
             0x01,
             // Same as default.
             0x00, 64, 64});
}

TEST_F(ReconGainParamDefinitionTest, Duration) {
  param_definition_->duration_ = 32;
  param_definition_->constant_subblock_duration_ = 32;
  Init();

  TestWrite({0x00, 0x01, 0x00,
             // Duration.
             32,
             // Constant Subblock Duration.
             32});
}

TEST_F(ReconGainParamDefinitionTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  param_definition_->parameter_id_ = 0;
  param_definition_->parameter_rate_ = 1;
  param_definition_->constant_subblock_duration_ = 64;
  Init();

  TestWrite({// `parameter_id`.
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

TEST_F(ReconGainParamDefinitionTest,
       InvalidWhenConstantSubblockDurationIsZero) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {32, 32};
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(ReconGainParamDefinitionTest, InvalidWhenImpliedNumSubblocksIsNotOne) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 32;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(ReconGainParamDefinitionTest,
       InvalidWhenDurationDoesNotEqualConstantSubblockDuration) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 65;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

TEST_F(ReconGainParamDefinitionTest, InvalidWhenParamDefinitionModeIsOne) {
  param_definition_->param_definition_mode_ = true;
  Init();

  EXPECT_FALSE(param_definition_->Validate().ok());
}

class ExtendedParamDefinitionTest : public ParamDefinitionTestBase {
 public:
  ExtendedParamDefinitionTest() = default;
  void Init(ParamDefinition::ParameterDefinitionType param_definition_type) {
    auto extended_param_definition =
        std::make_unique<ExtendedParamDefinition>(param_definition_type);
    extended_param_definition_ = extended_param_definition.get();
    PopulateParameterDefinition(*extended_param_definition);
    param_definition_ = std::move(extended_param_definition);
    ParamDefinitionTestBase::Init();
  }

 protected:
  // Alias for accessing the sub-class data.
  ExtendedParamDefinition* extended_param_definition_;
};

TEST_F(ExtendedParamDefinitionTest, GetTypeHasCorrectValue) {
  Init(ParamDefinition::kParameterDefinitionReservedEnd);

  EXPECT_TRUE(param_definition_->GetType().has_value());
  EXPECT_EQ(param_definition_->GetType().value(),
            ParamDefinition::kParameterDefinitionReservedEnd);
}

TEST_F(ExtendedParamDefinitionTest, SizeMayBeZero) {
  Init(ParamDefinition::kParameterDefinitionReservedEnd);
  extended_param_definition_->param_definition_size_ = 0;
  extended_param_definition_->param_definition_bytes_ = {};

  TestWrite({// Param Definition Size.
             0x00});
}

TEST_F(ExtendedParamDefinitionTest, WritesOnlySizeAndParamDefinitionBytes) {
  Init(ParamDefinition::kParameterDefinitionReservedEnd);
  extended_param_definition_->param_definition_size_ = 4;
  extended_param_definition_->param_definition_bytes_ = {0x01, 0x02, 0x03,
                                                         0x04};

  TestWrite({// Param Definition Size.
             0x04,
             // Param Definition Bytes.
             0x01, 0x02, 0x03, 0x04});
}

TEST_F(ExtendedParamDefinitionTest, WriteFailsIfSizeIsInconsistent) {
  Init(ParamDefinition::kParameterDefinitionReservedEnd);
  extended_param_definition_->param_definition_size_ = 0;
  extended_param_definition_->param_definition_bytes_ = {100};

  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  TestWrite({});
}

TEST(ReadParamDefinitionTest, Mode1) {
  ParamDefinition param_definition;
  std::vector<uint8_t> source = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x80};
  ReadBitBuffer buffer(1024, &source);
  EXPECT_THAT(param_definition.ReadAndValidate(buffer), IsOk());
}

TEST(ReadParamDefinitionTest, Mode0NonZeroSubblockDuration) {
  ParamDefinition param_definition;
  std::vector<uint8_t> source = {
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
  ReadBitBuffer buffer(1024, &source);
  EXPECT_THAT(param_definition.ReadAndValidate(buffer), IsOk());
}

TEST(ReadParamDefinitionTest, Mode0SubblockArray) {
  ParamDefinition param_definition;
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
      // `subblock_duration`
      40,
      // `subblock_duration`
      24};
  ReadBitBuffer buffer(1024, &source);
  EXPECT_THAT(param_definition.ReadAndValidate(buffer), IsOk());
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
  ReadBitBuffer buffer(1024, &source);
  EXPECT_THAT(param_definition.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(param_definition.GetType().value(),
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
      // `subblock_duration`
      40,
      // `subblock_duration`
      24,
      // Default Mix Gain.
      0, 3};
  ReadBitBuffer buffer(1024, &source);
  EXPECT_THAT(param_definition.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(param_definition.GetType().value(),
            ParamDefinition::kParameterDefinitionMixGain);
  EXPECT_EQ(param_definition.default_mix_gain_, 3);
}

TEST(ReadReconGainParamDefinitionTest, Default) {
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
  ReadBitBuffer buffer(1024, &bitstream);
  ReconGainParamDefinition param_definition = ReconGainParamDefinition(0);
  EXPECT_TRUE(param_definition.ReadAndValidate(buffer).ok());
  EXPECT_EQ(param_definition.GetType().value(),
            ParamDefinition::kParameterDefinitionReconGain);
}

}  // namespace
}  // namespace iamf_tools
