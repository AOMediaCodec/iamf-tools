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
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"

namespace iamf_tools {
namespace {

void PopulateParameterDefinition(ParamDefinition* param_definition) {
  param_definition->parameter_id_ = 0;
  param_definition->parameter_rate_ = 1;
  param_definition->param_definition_mode_ = 1;
  param_definition->reserved_ = 0;
}

class ParamDefinitionTestBase : public testing::Test {
 public:
  ParamDefinitionTestBase() = default;

  void InitAndTestWrite() {
    // Initialize the `subblock_durations_` vector then loop to populate it.
    param_definition_->InitializeSubblockDurations(
        static_cast<DecodedUleb128>(subblock_durations_.size()));
    for (int i = 0; i < subblock_durations_.size(); ++i) {
      EXPECT_TRUE(
          param_definition_->SetSubblockDuration(i, subblock_durations_[i])
              .ok());
    }

    ASSERT_NE(leb_generator_, nullptr);
    WriteBitBuffer wb(expected_data_.size(), *leb_generator_);
    EXPECT_EQ(param_definition_->ValidateAndWrite(wb).code(),
              expected_status_code_);
    if (expected_status_code_ == absl::StatusCode::kOk) {
      ValidateWriteResults(wb, expected_data_);
    }
  }

 protected:
  std::unique_ptr<ParamDefinition> param_definition_;
  std::vector<DecodedUleb128> subblock_durations_ = {};
  std::vector<uint8_t> expected_data_ = {};
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
    PopulateParameterDefinition(mix_gain.get());
    mix_gain->default_mix_gain_ = 0;
    param_definition_ = std::move(mix_gain);
  }

 protected:
  // Alias for accessing the sub-class data.
  MixGainParamDefinition* mix_gain_;
};

TEST_F(MixGainParamDefinitionTest, GetTypeHasCorrectValue) {
  EXPECT_TRUE(mix_gain_->GetType().has_value());
  EXPECT_EQ(mix_gain_->GetType().value(),
            ParamDefinition::kParameterDefinitionMixGain);
}

TEST_F(MixGainParamDefinitionTest, DefaultParamDefinitionMode1) {
  expected_data_ = {0x00, 1, 0x80, 0, 0};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, ParameterId) {
  param_definition_->parameter_id_ = 1;
  expected_data_ = {0x01, 1, 0x80, 0, 0};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, NonMinimalLeb) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  param_definition_->parameter_id_ = 1;
  param_definition_->parameter_rate_ = 5;

  expected_data_ = {0x81, 0x00, 0x85, 0x00, 0x80, 0, 0};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, ParameterRate) {
  param_definition_->parameter_rate_ = 64;
  expected_data_ = {0x00, 64, 0x80, 0, 0};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, InvalidParameterRate) {
  param_definition_->parameter_rate_ = 0;
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, DefaultMixGain) {
  mix_gain_->default_mix_gain_ = 3;
  expected_data_ = {0x00, 1, 0x80, 0, 3};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest,
       ParamDefinitionMode0ConstantSubblockDuration) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 3;
  param_definition_->constant_subblock_duration_ = 3;

  expected_data_ = {0x00, 1, 0, 3, 3, 0, 0};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, ParamDefinitionMode0ExplicitSubblocks) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 10;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {1, 2, 3, 4};

  expected_data_ = {0x00, 1, 0, 10, 0, 4, 1, 2, 3, 4, 0, 0};
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, ParamDefinitionMode0InconsistentDuration) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 11;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {1, 2, 3, 4};

  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest, ParamDefinitionMode0IllegalDurationZero) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 0;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {0};

  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(MixGainParamDefinitionTest,
       ParamDefinitionMode0IllegalSubblockDurationZero) {
  param_definition_->param_definition_mode_ = false;
  param_definition_->duration_ = 10;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {5, 0, 5};

  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

class DemixingParamDefinitionTest : public ParamDefinitionTestBase {
 public:
  DemixingParamDefinitionTest() {
    auto demixing = std::make_unique<DemixingParamDefinition>();
    demixing_ = demixing.get();
    PopulateParameterDefinition(demixing.get());
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
  expected_data_ = {
      0x00, 0x01, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, ParameterId) {
  param_definition_->parameter_id_ = 1;
  expected_data_ = {
      0x01, 0x01, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, ParameterRate) {
  param_definition_->parameter_rate_ = 2;
  expected_data_ = {
      0x00, 0x02, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode1 << 5, 0};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, Duration) {
  param_definition_->duration_ = 32;
  param_definition_->constant_subblock_duration_ = 32;
  expected_data_ = {0x00, 0x01, 0x00, 32, 32, 0, 0};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, DefaultDmixPMode) {
  demixing_->default_demixing_info_parameter_data_.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode2;
  expected_data_ = {
      0x00, 0x01, 0x00, 64, 64, DemixingInfoParameterData::kDMixPMode2 << 5, 0};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, DefaultW) {
  demixing_->default_demixing_info_parameter_data_.default_w = 1;
  expected_data_ = {0x00,  0x01, 0x00,
                    64,    64,   DemixingInfoParameterData::kDMixPMode1 << 5,
                    1 << 4};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  param_definition_->parameter_id_ = 0;
  param_definition_->parameter_rate_ = 1;
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 64;
  expected_data_ = {// `parameter_id`.
                    0x80, 0x00,
                    // `parameter_rate`.
                    0x81, 0x00,
                    // `param_definition_mode` (1), reserved (7).
                    0x00,
                    // `duration`.
                    0xc0, 0x00,
                    // `constant_subblock_duration`.
                    0xc0, 0x00, DemixingInfoParameterData::kDMixPMode1 << 5, 0};
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, InvalidMoreThanOneSubblock) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {32, 32};
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, InvalidInconsistentDuration) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 65;
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(DemixingParamDefinitionTest, InvalidParamDefinitionMode1) {
  param_definition_->param_definition_mode_ = true;
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

class ReconGainParamDefinitionTest : public ParamDefinitionTestBase {
 public:
  ReconGainParamDefinitionTest() {
    auto recon_gain = std::make_unique<ReconGainParamDefinition>(0);
    PopulateParameterDefinition(recon_gain.get());
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
  expected_data_ = {0x00, 0x01, 0x00, 64, 64};
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, ParameterId) {
  param_definition_->parameter_id_ = 1;
  expected_data_ = {0x01, 0x01, 0x00, 64, 64};
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, ParameterRate) {
  param_definition_->parameter_id_ = 1;
  expected_data_ = {0x01, 0x01, 0x00, 64, 64};
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, Duration) {
  param_definition_->duration_ = 32;
  param_definition_->constant_subblock_duration_ = 32;
  expected_data_ = {0x00, 0x01, 0x00, 32, 32};
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  param_definition_->parameter_id_ = 0;
  param_definition_->parameter_rate_ = 1;
  param_definition_->constant_subblock_duration_ = 64;

  expected_data_ = {// `parameter_id`.
                    0x80, 0x00,
                    // `parameter_rate`.
                    0x81, 0x00,
                    // `param_definition_mode` (1), reserved (7).
                    0x00,
                    // `duration`.
                    0x80 | 64, 0x00,
                    // `constant_subblock_duration`.
                    0x80 | 64, 0x00};
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, InvalidMoreThanOneSubblock) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 0;
  subblock_durations_ = {32, 32};
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, InvalidInconsistentDuration) {
  param_definition_->duration_ = 64;
  param_definition_->constant_subblock_duration_ = 65;
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(ReconGainParamDefinitionTest, InvalidParamDefinitionMode1) {
  param_definition_->param_definition_mode_ = true;
  expected_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

}  // namespace
}  // namespace iamf_tools
