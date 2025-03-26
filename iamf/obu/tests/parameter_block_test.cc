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
#include "iamf/obu/parameter_block.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/extension_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/tests/obu_test_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using absl_testing::IsOkAndHolds;
using enum MixGainParameterData::AnimationType;
using enum DemixingInfoParameterData::DMixPMode;
using enum DemixingInfoParameterData::WIdxUpdateRule;

constexpr uint32_t kAudioElementId = 0;

// TODO(b/273545873): Add more "expected failure" tests. Add more "successful"
//                    test cases to existing tests.

TEST(CreateFromBuffer, InvalidWhenObuSizeIsTooSmallToReadParameterId) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {
      // Parameter ID (leb128).
      0x87,
      0x80,
      0x00,
      // Duration.
      0x0a,
      // Constant subblock duration.
      0x0a,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x09,
      0x88,
  };
  const int64_t kCorrectObuSize = source_data.size();
  constexpr int64_t kIncorrectObuSize = 1;
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterId, param_definition);

  // Sanity check that the OBU is valid.
  EXPECT_THAT(ParameterBlockObu::CreateFromBuffer(
                  ObuHeader{.obu_type = kObuIaParameterBlock}, kCorrectObuSize,
                  param_definitions, *buffer),
              IsOk());

  // But it would be invalid if the OBU size is too small.
  EXPECT_FALSE(ParameterBlockObu::CreateFromBuffer(
                   ObuHeader{.obu_type = kObuIaParameterBlock},
                   kIncorrectObuSize, param_definitions, *buffer)
                   .ok());
}

TEST(ParameterBlockObu, CreateFromBufferParamDefinitionMode1) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {
      // Parameter ID.
      kParameterId,
      // Duration.
      0x0a,
      // Constant subblock duration.
      0x00,
      // Number of subblocks.
      0x03,
      // Subblock duration.
      0x01,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x09,
      0x88,
      // Subblock duration.
      0x03,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x07,
      0x66,
      // Subblock duration.
      0x06,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x05,
      0x44,
  };
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterId, param_definition);
  auto parameter_block = ParameterBlockObu::CreateFromBuffer(
      ObuHeader{.obu_type = kObuIaParameterBlock}, payload_size,
      param_definitions, *buffer);
  EXPECT_THAT(parameter_block, IsOk());

  // Validate all the getters match the input data.
  EXPECT_EQ((*parameter_block)->parameter_id_, kParameterId);
  EXPECT_EQ((*parameter_block)->GetDuration(), 10);
  EXPECT_EQ((*parameter_block)->GetConstantSubblockDuration(), 0);
  EXPECT_EQ((*parameter_block)->GetNumSubblocks(), 3);
  EXPECT_THAT((*parameter_block)->GetSubblockDuration(0), IsOkAndHolds(1));
  EXPECT_THAT((*parameter_block)->GetSubblockDuration(1), IsOkAndHolds(3));
  EXPECT_THAT((*parameter_block)->GetSubblockDuration(2), IsOkAndHolds(6));

  float linear_mix_gain;
  // The first subblock covers [0, subblock_duration[0]).
  EXPECT_THAT((*parameter_block)->GetLinearMixGain(0, linear_mix_gain), IsOk());
  EXPECT_FLOAT_EQ(linear_mix_gain, 2.9961426f);
  EXPECT_THAT((*parameter_block)->GetLinearMixGain(1, linear_mix_gain), IsOk());
  EXPECT_FLOAT_EQ(linear_mix_gain, 2.343807f);
  EXPECT_THAT((*parameter_block)->GetLinearMixGain(4, linear_mix_gain), IsOk());
  EXPECT_FLOAT_EQ(linear_mix_gain, 1.8335015f);

  // Parameter blocks are open intervals.
  EXPECT_FALSE((*parameter_block)->GetLinearMixGain(10, linear_mix_gain).ok());
}

TEST(ParameterBlockObu, CreateFromBufferParamDefinitionMode0) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {
      // Parameter ID.
      kParameterId,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x09,
      0x88,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x07,
      0x66,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x05,
      0x44,
  };
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = 10;
  param_definition.constant_subblock_duration_ = 0;
  param_definition.InitializeSubblockDurations(3);
  ASSERT_THAT(param_definition.SetSubblockDuration(0, 1), IsOk());
  ASSERT_THAT(param_definition.SetSubblockDuration(1, 3), IsOk());
  ASSERT_THAT(param_definition.SetSubblockDuration(2, 6), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterId, param_definition);
  auto parameter_block = ParameterBlockObu::CreateFromBuffer(
      ObuHeader{.obu_type = kObuIaParameterBlock}, payload_size,
      param_definitions, *buffer);
  EXPECT_THAT(parameter_block, IsOk());

  // Validate all the getters match the input data. Note the getters return data
  // based on the `param_definition` and not the data in the OBU.
  EXPECT_EQ((*parameter_block)->parameter_id_, kParameterId);
  EXPECT_EQ((*parameter_block)->GetDuration(), 10);
  EXPECT_EQ((*parameter_block)->GetConstantSubblockDuration(), 0);
  EXPECT_EQ((*parameter_block)->GetNumSubblocks(), 3);
  EXPECT_THAT((*parameter_block)->GetSubblockDuration(0), IsOkAndHolds(1));
  EXPECT_THAT((*parameter_block)->GetSubblockDuration(1), IsOkAndHolds(3));
  EXPECT_THAT((*parameter_block)->GetSubblockDuration(2), IsOkAndHolds(6));

  float linear_mix_gain;
  // The first subblock covers [0, subblock_duration[0]).
  EXPECT_THAT((*parameter_block)->GetLinearMixGain(0, linear_mix_gain), IsOk());
  EXPECT_FLOAT_EQ(linear_mix_gain, 2.9961426f);
  EXPECT_THAT((*parameter_block)->GetLinearMixGain(1, linear_mix_gain), IsOk());
  EXPECT_FLOAT_EQ(linear_mix_gain, 2.343807f);
  EXPECT_THAT((*parameter_block)->GetLinearMixGain(4, linear_mix_gain), IsOk());
  EXPECT_FLOAT_EQ(linear_mix_gain, 1.8335015f);

  // Parameter blocks are open intervals.
  EXPECT_FALSE((*parameter_block)->GetLinearMixGain(10, linear_mix_gain).ok());
}

TEST(ParameterBlockObu,
     CreateFromBufferFailsWhenSubblockDurationsAreInconsistent) {
  const DecodedUleb128 kParameterId = 0x07;
  const DecodedUleb128 kTotalDuration = 0xaa;
  const DecodedUleb128 kFirstSubblockDuration = 0x01;
  std::vector<uint8_t> source_data = {
      // Parameter ID.
      kParameterId,
      // Duration.
      kTotalDuration,
      // Constant subblock duration.
      0x00,
      // Number of subblocks.
      0x01,
      // Subblock duration.
      kFirstSubblockDuration,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x09,
      0x88,
  };
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterId, param_definition);
  EXPECT_FALSE(ParameterBlockObu::CreateFromBuffer(
                   ObuHeader{.obu_type = kObuIaParameterBlock}, payload_size,
                   param_definitions, *buffer)
                   .ok());
}

TEST(ParameterBlockObu, CreateFromBufferParamRequiresParamDefinition) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {
      // Parameter ID.
      kParameterId,
      // Duration.
      0x0a,
      // Constant subblock duration.
      0x0a,
      // Animation type.
      kAnimateStep,
      // Start point value.
      0x09,
      0x88,
  };
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterId, param_definition);
  EXPECT_THAT(ParameterBlockObu::CreateFromBuffer(
                  ObuHeader{.obu_type = kObuIaParameterBlock}, payload_size,
                  param_definitions, *buffer),
              IsOk());

  // When there is no matching param definition, the parameter block cannot be
  // created.
  param_definitions.erase(kParameterId);
  auto buffer_to_use_without_metadata =
      MemoryBasedReadBitBuffer::CreateFromSpan(
          absl::MakeConstSpan(source_data));
  EXPECT_FALSE(ParameterBlockObu::CreateFromBuffer(
                   ObuHeader{.obu_type = kObuIaParameterBlock}, payload_size,
                   param_definitions, *buffer_to_use_without_metadata)
                   .ok());
}

TEST(ParameterBlockObu, CreateFromBufferDemixingParamDefinitionMode0) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {// Parameter ID.
                                      kParameterId,
                                      // `dmixp_mode`.
                                      kDMixPMode2 << 5};
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  DemixingParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = 10;
  param_definition.constant_subblock_duration_ = 10;
  param_definition.InitializeSubblockDurations(1);
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterId, param_definition);
  auto parameter_block = ParameterBlockObu::CreateFromBuffer(
      ObuHeader{.obu_type = kObuIaParameterBlock}, payload_size,
      param_definitions, *buffer);
  EXPECT_THAT(parameter_block, IsOk());

  // Validate all the getters match the input data. Note the getters return data
  // based on the `param_definition` and not the data in the OBU.
  EXPECT_EQ((*parameter_block)->parameter_id_, kParameterId);
  EXPECT_EQ((*parameter_block)->GetDuration(), 10);
  EXPECT_EQ((*parameter_block)->GetConstantSubblockDuration(), 10);
  EXPECT_EQ((*parameter_block)->GetNumSubblocks(), 1);

  auto demixing_info = static_cast<DemixingInfoParameterData*>(
      (*parameter_block)->subblocks_[0].param_data.get());

  EXPECT_EQ(demixing_info->dmixp_mode, kDMixPMode2);
}

class ParameterBlockObuTestBase : public ObuTestBase {
 public:
  ParameterBlockObuTestBase()
      : ObuTestBase(
            /*expected_header=*/{kObuIaParameterBlock << 3, 4},
            /*expected_payload=*/{}),
        obu_(nullptr),
        parameter_id_(3),
        metadata_args_({
            .parameter_rate = 1,
            .param_definition_mode = 0,
            .reserved = 0,
        }),
        duration_args_({
            .duration = 64,
            .constant_subblock_duration = 64,
        }) {}

  ~ParameterBlockObuTestBase() = default;

 protected:
  void InitExpectOk() override {
    InitParamDefinition();
    InitMainParameterBlockObu();
    InitParameterBlockTypeSpecificFields();
  }

  virtual void CreateParamDefinition() = 0;
  virtual void InitParameterBlockTypeSpecificFields() = 0;

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  std::unique_ptr<ParameterBlockObu> obu_;
  std::unique_ptr<ParamDefinition> param_definition_;

  DecodedUleb128 parameter_id_;

  struct {
    DecodedUleb128 parameter_rate;
    bool param_definition_mode;
    uint8_t reserved;

    // From the Audio Element. Only used when `param_definition_type ==
    // kParameterDefinitionReconGain`.
    uint8_t num_layers;
    std::vector<bool> recon_gain_is_present_flags;
  } metadata_args_;

  // Values to track subblock durations. These are stored in different locations
  // depending on `param_definition_mode`.
  struct {
    DecodedUleb128 duration;
    DecodedUleb128 constant_subblock_duration;
    DecodedUleb128 num_subblocks;
    // Length `num_subblocks`.
    std::vector<DecodedUleb128> subblock_durations;
  } duration_args_;

 private:
  void InitParamDefinition() {
    CreateParamDefinition();
    ASSERT_NE(param_definition_, nullptr);
    ASSERT_TRUE(param_definition_->GetType().has_value());
    param_definition_->parameter_id_ = parameter_id_;
    param_definition_->parameter_rate_ = metadata_args_.parameter_rate;
    param_definition_->param_definition_mode_ =
        metadata_args_.param_definition_mode;
    param_definition_->reserved_ = metadata_args_.reserved;

    if (param_definition_->param_definition_mode_ == 0) {
      // Values will be referenced from `metadata_.param_definition`; overwrite
      // them with those from `duration_args_`.
      param_definition_->duration_ = duration_args_.duration;
      param_definition_->constant_subblock_duration_ =
          duration_args_.constant_subblock_duration;

      // Initialize memory for the metadata. This would typically be the
      // responsibility of the OBU that this Parameter Block references.
      param_definition_->InitializeSubblockDurations(
          duration_args_.num_subblocks);
      for (int i = 0; i < duration_args_.subblock_durations.size(); i++) {
        EXPECT_THAT(param_definition_->SetSubblockDuration(
                        i, duration_args_.subblock_durations[i]),
                    IsOk());
      }
    }
  }

  void InitMainParameterBlockObu() {
    // Copy over all arguments into the `ParameterBlockObu`.
    //
    // Code within `iamf_tools` will find the associated Audio Element or Mix
    // Presentation OBU and use that metadata. For testing here the metadata is
    // initialized based on `metadata_args_`.
    obu_ = std::make_unique<ParameterBlockObu>(header_, parameter_id_,
                                               *param_definition_);
    if (param_definition_->param_definition_mode_ == 1) {
      EXPECT_THAT(
          obu_->InitializeSubblocks(duration_args_.duration,
                                    duration_args_.constant_subblock_duration,
                                    duration_args_.num_subblocks),
          IsOk());

      // With all memory allocated set the subblock durations.
      for (int i = 0; i < duration_args_.subblock_durations.size(); i++) {
        EXPECT_THAT(
            obu_->SetSubblockDuration(i, duration_args_.subblock_durations[i]),
            IsOk());
      }
    } else {
      EXPECT_THAT(obu_->InitializeSubblocks(), IsOk());
    }
  }
};

class MixGainParameterBlockTest : public ParameterBlockObuTestBase,
                                  public testing::Test {
 public:
  MixGainParameterBlockTest()
      : ParameterBlockObuTestBase(),
        mix_gain_parameter_data_({{kAnimateStep, AnimationStepInt16{1}}}) {}

 protected:
  void CreateParamDefinition() override {
    param_definition_ = std::make_unique<MixGainParamDefinition>();
  }

  void InitParameterBlockTypeSpecificFields() override {
    ASSERT_EQ(obu_->subblocks_.size(), mix_gain_parameter_data_.size());
    for (int i = 0; i < obu_->subblocks_.size(); i++) {
      obu_->subblocks_[i].param_data =
          std::make_unique<MixGainParameterData>(mix_gain_parameter_data_[i]);
    }
  }

  std::vector<MixGainParameterData> mix_gain_parameter_data_;
};

TEST_F(MixGainParameterBlockTest, ConstructSetsObuType) {
  InitExpectOk();
  EXPECT_EQ(obu_->header_.obu_type, kObuIaParameterBlock);
}

TEST_F(MixGainParameterBlockTest, DefaultOneSubblockParamDefinitionMode0) {
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `mix_gain_parameter_data`.
                       kAnimateStep, 0, 1};

  InitAndTestWrite();
}

TEST_F(MixGainParameterBlockTest,
       ValidateAndWriteObuFailsWithIllegalRedundantCopyForSimpleOrBaseProfile) {
  header_.obu_redundant_copy = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixGainParameterBlockTest,
       ValidateAndWriteObuIllegalTrimmingStatusFlag) {
  header_.obu_trimming_status_flag = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixGainParameterBlockTest, ExtensionHeader) {
  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaParameterBlock << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      10,
                      // `extension_header_size`.
                      5,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};

  expected_payload_ = {// `parameter_id`.
                       3,
                       // `mix_gain_parameter_data`.
                       kAnimateStep, 0, 1};

  InitAndTestWrite();
}

TEST_F(MixGainParameterBlockTest, OneSubblockParamDefinitionMode1) {
  metadata_args_.param_definition_mode = 1;

  expected_header_ = {kObuIaParameterBlock << 3, 6};
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `duration`.
                       64,
                       // `constant_subblock_duration`.
                       64,
                       // `mix_gain_parameter_data`.
                       kAnimateStep, 0, 1};

  InitAndTestWrite();
}

TEST_F(MixGainParameterBlockTest,
       ValidateAndWriteObuFailsWithIllegalDurationInconsistent) {
  metadata_args_.param_definition_mode = 1;

  duration_args_ = {
      .duration = 64,
      .constant_subblock_duration = 0,
      .num_subblocks = 2,
      .subblock_durations = {32, 31},  // Does not sum to `duration`.
  };

  mix_gain_parameter_data_ = {{kAnimateStep, AnimationStepInt16{0}},
                              {kAnimateStep, AnimationStepInt16{0}}};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(MixGainParameterBlockTest, MultipleSubblocksParamDefinitionMode1) {
  metadata_args_.param_definition_mode = 1;

  duration_args_ = {
      .duration = 21,
      .constant_subblock_duration = 0,
      .num_subblocks = 3,
      .subblock_durations = {6, 7, 8},
  };

  mix_gain_parameter_data_ = {
      {kAnimateStep, AnimationStepInt16{9}},
      {kAnimateLinear, AnimationLinearInt16{10, 11}},
      {kAnimateBezier, AnimationBezierInt16{12, 13, 14, 15}}};

  expected_header_ = {kObuIaParameterBlock << 3, 23};
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `duration`.
                       21,
                       // `constant_subblock_duration`.
                       0,
                       // `num_subblocks`.
                       3,
                       // Start `subblocks[0]`.
                       // `subblock_duration`.
                       6,
                       // `mix_gain_parameter_data`.
                       kAnimateStep, 0, 9,
                       // Start `subblocks[1]`.
                       // `subblock_duration`.
                       7,
                       // `mix_gain_parameter_data`.
                       kAnimateLinear, 0, 10, 0, 11,
                       // Start `subblocks[2]`.
                       // `subblock_duration`.
                       8,
                       // `mix_gain_parameter_data`.
                       kAnimateBezier, 0, 12, 0, 13, 0, 14, 15};
  InitAndTestWrite();
}

TEST_F(MixGainParameterBlockTest, MultipleSubblocksParamDefinitionMode0) {
  duration_args_ = {
      .duration = 21,
      .constant_subblock_duration = 0,
      .num_subblocks = 3,
      .subblock_durations = {6, 7, 8},
  };

  mix_gain_parameter_data_ = {
      {kAnimateStep, AnimationStepInt16{9}},
      {kAnimateLinear, AnimationLinearInt16{10, 11}},
      {kAnimateBezier, AnimationBezierInt16{12, 13, 14, 15}}};

  expected_header_ = {kObuIaParameterBlock << 3, 17};
  expected_payload_ = {// `parameter_id`.
                       3,
                       // Start `subblocks[0]`.
                       // `mix_gain_parameter_data`.
                       kAnimateStep, 0, 9,
                       // Start `subblocks[1]`.
                       // `mix_gain_parameter_data`.
                       kAnimateLinear, 0, 10, 0, 11,
                       // Start `subblocks[2]`.
                       // `mix_gain_parameter_data`.
                       kAnimateBezier, 0, 12, 0, 13, 0, 14, 15};

  InitAndTestWrite();
}

TEST_F(MixGainParameterBlockTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  // Initialize a test has several `DecodedUleb128` explicitly in the bitstream.
  duration_args_ = {
      .duration = 13,
      .constant_subblock_duration = 0,
      .num_subblocks = 2,
      .subblock_durations = {6, 7},
  };
  metadata_args_.param_definition_mode = true;

  mix_gain_parameter_data_ = {{kAnimateStep, AnimationStepInt16{9}},
                              {kAnimateStep, AnimationStepInt16{10}}};

  // Configure the `LebGenerator`.
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);

  expected_header_ = {kObuIaParameterBlock << 3,
                      // `obu_size` is affected by the `LebGenerator`.
                      0x080 | 20, 0x00};
  expected_payload_ = {
      // `parameter_id` is affected by the `LebGenerator`.
      0x80 | 3, 0x00,
      // `duration` is affected by the `LebGenerator`.
      0x80 | 13, 0x00,
      // `constant_subblock_duration` is affected by the `LebGenerator`.
      0x80 | 0, 0x00,
      // `num_subblocks` is affected by the `LebGenerator`.
      0x80 | 2, 0x00,
      // Start `subblocks[0]`.
      // `duration` is affected by the `LebGenerator`.
      0x80 | 6, 0x00,
      // `mix_gain_parameter_data`.
      0x80 | kAnimateStep, 0x00, 0, 9,
      // Start `subblocks[1]`.
      // `duration` is affected by the `LebGenerator`.
      0x80 | 7, 0x00,
      // `mix_gain_parameter_data`.
      0x80 | kAnimateStep, 0x00, 0, 10};

  InitAndTestWrite();
}

class DemixingParameterBlockTest : public ParameterBlockObuTestBase,
                                   public testing::Test {
 public:
  DemixingParameterBlockTest()
      : ParameterBlockObuTestBase(),
        demixing_info_parameter_data_({{kDMixPMode1, 0}}) {
    expected_header_ = {kObuIaParameterBlock << 3, 2};
  }

 protected:
  void CreateParamDefinition() override {
    param_definition_ = std::make_unique<DemixingParamDefinition>();
  }

  void InitParameterBlockTypeSpecificFields() override {
    ASSERT_EQ(demixing_info_parameter_data_.size(), obu_->subblocks_.size());
    for (int i = 0; i < demixing_info_parameter_data_.size(); i++) {
      obu_->subblocks_[i].param_data =
          std::make_unique<DemixingInfoParameterData>(
              demixing_info_parameter_data_[i]);
    }
  }

  std::vector<DemixingInfoParameterData> demixing_info_parameter_data_;
};

TEST_F(DemixingParameterBlockTest, DefaultParamDefinitionMode0) {
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `demixing_info_parameter_data`.
                       kDMixPMode1 << 5};

  InitAndTestWrite();
}

TEST_F(DemixingParameterBlockTest, DMixPMode2) {
  demixing_info_parameter_data_ = {{kDMixPMode2, 0}};
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `demixing_info_parameter_data`.
                       kDMixPMode2 << 5};

  InitAndTestWrite();
}

TEST_F(DemixingParameterBlockTest,
       ValidateAndWriteObuFailsWhenParamDefinitionMode1HasTooManySubblocks) {
  metadata_args_.param_definition_mode = true;

  duration_args_ = {
      .duration = 4,
      .constant_subblock_duration = 0,
      .num_subblocks = 5,
      .subblock_durations = {6, 7, 8, 9, 10},
  };

  demixing_info_parameter_data_ = {{kDMixPMode1, 0},
                                   {kDMixPMode2, 0},
                                   {kDMixPMode3, 0},
                                   {kDMixPMode1_n, 0},
                                   {kDMixPMode2_n, 0}};

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(DemixingParameterBlockTest,
       ValidateAndWriteObuFailsWithInvalidWhenParamDefinitionModeIsOne) {
  metadata_args_.param_definition_mode = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

class ReconGainBlockTest : public ParameterBlockObuTestBase,
                           public testing::Test {
 public:
  ReconGainBlockTest() : ParameterBlockObuTestBase() {}

 protected:
  void CreateParamDefinition() override {
    auto recon_gain_param_definition =
        std::make_unique<ReconGainParamDefinition>(kAudioElementId);

    // Copy additional recon gain-related data over.
    recon_gain_param_definition->aux_data_.resize(metadata_args_.num_layers);
    for (int i = 0; i < metadata_args_.num_layers; i++) {
      auto& aux_data = recon_gain_param_definition->aux_data_[i];
      aux_data.recon_gain_is_present_flag =
          metadata_args_.recon_gain_is_present_flags[i];
      // `aux_data.channel_number_for_layer` not used in these tests.
    }

    param_definition_ = std::move(recon_gain_param_definition);
  }

  void InitParameterBlockTypeSpecificFields() override {
    // Loop over and populate the recon gain parameter for each layer within
    // each subblock.
    ASSERT_EQ(recon_gain_parameter_data_.size(), obu_->subblocks_.size());
    for (int i = 0; i < recon_gain_parameter_data_.size(); i++) {
      recon_gain_parameter_data_[i].recon_gain_is_present_flags =
          metadata_args_.recon_gain_is_present_flags;

      // Each element in `recon_gain_elements` and
      // `recon_gain_is_present_flags` corresponds to a single layer.
      ASSERT_EQ(recon_gain_parameter_data_[i].recon_gain_elements.size(),
                metadata_args_.num_layers);
      ASSERT_EQ(
          recon_gain_parameter_data_[i].recon_gain_is_present_flags.size(),
          metadata_args_.num_layers);
      obu_->subblocks_[i].param_data =
          std::make_unique<ReconGainInfoParameterData>(
              recon_gain_parameter_data_[i]);
    }
  }

  std::vector<ReconGainInfoParameterData> recon_gain_parameter_data_;
};

TEST_F(ReconGainBlockTest, TwoLayerParamDefinitionMode0) {
  metadata_args_.num_layers = 2;
  metadata_args_.recon_gain_is_present_flags = {false, true};
  recon_gain_parameter_data_.resize(1);
  auto& recon_gain_elements = recon_gain_parameter_data_[0].recon_gain_elements;
  recon_gain_elements.emplace_back(ReconGainElement{
      0,
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
            {  0, 0,  0,       0,       0,   0,   0,   0,   0,   0,   0,   0},
      // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagR,
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
            {  0, 0,  1,       0,       0,   0,   0,   0,   0,   0,   0,   0},
      // clang-format on
  });

  expected_header_ = {kObuIaParameterBlock << 3, 3};
  expected_payload_ = {
      // `parameter_id`.
      3,
      // Layer 0 is omitted due to `recon_gain_is_present_flags`.
      // `layer[1]`.
      ReconGainElement::kReconGainFlagR, 1};

  InitAndTestWrite();
}

TEST_F(ReconGainBlockTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  metadata_args_.num_layers = 2;
  metadata_args_.recon_gain_is_present_flags = {false, true};
  recon_gain_parameter_data_.resize(1);
  auto& recon_gain_elements = recon_gain_parameter_data_[0].recon_gain_elements;
  recon_gain_elements.emplace_back(ReconGainElement{
      0,
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
            {  0, 0,  0,       0,       0,   0,   0,   0,   0,   0,   0,   0},
      // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagR,
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
            {  0, 0,  1,       0,       0,   0,   0,   0,   0,   0,   0,   0},
      // clang-format on
  });

  // Configure the `LebGenerator`.
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 3);

  expected_header_ = {kObuIaParameterBlock << 3,
                      // `obu_size` is affected by the `LebGenerator`.
                      0x80 | 7, 0x80, 0x00};
  expected_payload_ = {
      // `parameter_id`. is affected by the `LebGenerator`.
      0x80 | 3, 0x80, 0x00,
      // Layer 0 is omitted due to `recon_gain_is_present_flags`.
      // `layer[1]`.
      // `recon_gain_flags` is affected by the `LebGenerator`.
      0x80 | ReconGainElement::kReconGainFlagR, 0x80, 0x00,
      // `recon_gain[1][0]
      1};

  // N.B.: `recon_gain_flags` never has semantic meaning beyond the first two
  //       bytes, but it MAY be encoded using additional bytes.

  InitAndTestWrite();
}

TEST_F(ReconGainBlockTest, MaxLayer7_1_4) {
  metadata_args_.num_layers = 6;
  metadata_args_.recon_gain_is_present_flags = {false, true, true,
                                                true,  true, true};
  recon_gain_parameter_data_.resize(1);
  auto& recon_gain_elements = recon_gain_parameter_data_[0].recon_gain_elements;
  recon_gain_elements.emplace_back(ReconGainElement{
      0,  // Mono.
          // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
          // clang-format off
             {  0, 0,  0,       0,       0,   0,   0,   0,   0,   0,   0,   0},
          // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagR,  // M + R stereo.
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
             {  0, 0,  1,       0,       0,   0,   0,   0,   0,   0,   0,   0},
      // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagRss |
          ReconGainElement::kReconGainFlagLss,  // 5.1.0.
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
             {  0, 0,  0,       2,       3,   0,   0,   0,   0,   0,   0,   0},
      // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagLrs |
          ReconGainElement::kReconGainFlagRrs,  // 7.1.0.
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
             {  0, 0,  0,       0,       0,   0,   0,   4,   5,   0,   0,   0},
      // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagLtf |
          ReconGainElement::kReconGainFlagRtf,  // 7.1.2.
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
             {  0, 0,  0,       0,       0,   6,   7,   0,   0,   0,   0,   0},
      // clang-format on
  });
  recon_gain_elements.emplace_back(ReconGainElement{
      ReconGainElement::kReconGainFlagLtb |
          ReconGainElement::kReconGainFlagRtb,  // 7.1.4.
      // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
      // clang-format off
             {  0, 0,  0,       0,       0,   0,   0,   0,   0,   8,   9,   0},
      // clang-format on
  });

  expected_header_ = {kObuIaParameterBlock << 3, 17};
  expected_payload_ = {
      // `parameter_id`.
      3,
      // Layer 0 is omitted due to `recon_gain_is_present_flags`.
      // `layer[1]`.
      ReconGainElement::kReconGainFlagR, 1,
      // `layer[2]`.
      ReconGainElement::kReconGainFlagRss | ReconGainElement::kReconGainFlagLss,
      2, 3,
      // `layer[3]`.
      0x80,
      (ReconGainElement::kReconGainFlagLrs >> 7) |
          (ReconGainElement::kReconGainFlagRrs >> 7),
      4, 5,
      // `layer[4]`.
      ReconGainElement::kReconGainFlagLtf | ReconGainElement::kReconGainFlagRtf,
      6, 7,
      // `layer[5]`.
      0x80,
      (ReconGainElement::kReconGainFlagLtb >> 7) |
          (ReconGainElement::kReconGainFlagRtb >> 7),
      8, 9};

  InitAndTestWrite();
}

TEST_F(ReconGainBlockTest, ValidateAndWriteObuFailsWithMoreThanOneSubblock) {
  metadata_args_.num_layers = 2;
  metadata_args_.recon_gain_is_present_flags = {false, true};

  // In Spec, for recon gain param definition: `num_subblocks` SHALL be set
  // to 1, and `constant_subblock_duration` SHALL be same as `duration`.
  // The following violates these rules.
  duration_args_ = {
      .duration = 64, .constant_subblock_duration = 32, .num_subblocks = 2};
  recon_gain_parameter_data_.resize(2);
  auto& recon_gain_elements_0 =
      recon_gain_parameter_data_[0].recon_gain_elements;
  recon_gain_elements_0.emplace_back(
      ReconGainElement{0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});
  recon_gain_elements_0.emplace_back(
      ReconGainElement{ReconGainElement::kReconGainFlagR,
                       {0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0}});
  auto& recon_gain_elements_1 =
      recon_gain_parameter_data_[1].recon_gain_elements;
  recon_gain_elements_1.emplace_back(
      ReconGainElement{0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});
  recon_gain_elements_1.emplace_back(
      ReconGainElement{ReconGainElement::kReconGainFlagR,
                       {0, 0, 254, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(ReconGainBlockTest,
       ValidateAndWriteObuFailsWithWhenParamDefinitionModeIsOne) {
  metadata_args_.param_definition_mode = true;
  metadata_args_.num_layers = 2;
  metadata_args_.recon_gain_is_present_flags = {false, true};

  recon_gain_parameter_data_.resize(1);
  auto& recon_gain_elements = recon_gain_parameter_data_[0].recon_gain_elements;
  recon_gain_elements.emplace_back(
      ReconGainElement{0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});
  recon_gain_elements.emplace_back(
      ReconGainElement{ReconGainElement::kReconGainFlagR,
                       {0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

class ExtensionParameterBlockTest : public ParameterBlockObuTestBase,
                                    public testing::Test {
 public:
  ExtensionParameterBlockTest()
      : ParameterBlockObuTestBase(), parameter_block_extensions_({{0, {}}}) {}

 protected:
  void CreateParamDefinition() override {
    param_definition_ = std::make_unique<ExtendedParamDefinition>(
        ParamDefinition::kParameterDefinitionReservedStart);
  }

  void InitParameterBlockTypeSpecificFields() override {
    ASSERT_EQ(parameter_block_extensions_.size(), obu_->subblocks_.size());
    for (int i = 0; i < parameter_block_extensions_.size(); i++) {
      ASSERT_EQ(parameter_block_extensions_[i].parameter_data_size,
                parameter_block_extensions_[i].parameter_data_bytes.size());
      obu_->subblocks_[i].param_data = std::make_unique<ExtensionParameterData>(
          parameter_block_extensions_[i]);
    }
  }

  std::vector<ExtensionParameterData> parameter_block_extensions_;
};

TEST_F(ExtensionParameterBlockTest, DefaultOneSubblockParamDefinitionMode0) {
  expected_header_ = {kObuIaParameterBlock << 3, 2};
  expected_payload_ = {3, 0};

  InitAndTestWrite();
}

TEST_F(ExtensionParameterBlockTest, MaxParamDefinitionType) {
  param_definition_ = std::make_unique<ExtendedParamDefinition>(
      ParamDefinition::kParameterDefinitionReservedEnd);

  expected_header_ = {kObuIaParameterBlock << 3, 2};
  expected_payload_ = {3, 0};

  InitAndTestWrite();
}

TEST_F(ExtensionParameterBlockTest,
       OneSubblockNonzeroSizeParamDefinitionMode0) {
  parameter_block_extensions_ = {{5, {'e', 'x', 't', 'r', 'a'}}};

  expected_header_ = {kObuIaParameterBlock << 3, 7};
  expected_payload_ = {3, 5, 'e', 'x', 't', 'r', 'a'};

  InitAndTestWrite();
}

TEST_F(ExtensionParameterBlockTest, TwoSubblocksParamDefinitionMode0) {
  duration_args_ = {.duration = 64, .constant_subblock_duration = 32};

  parameter_block_extensions_ = {{5, {'f', 'i', 'r', 's', 't'}},
                                 {6, {'s', 'e', 'c', 'o', 'n', 'd'}}};

  expected_header_ = {kObuIaParameterBlock << 3, 14};
  expected_payload_ = {3, 5,   'f', 'i', 'r', 's', 't',
                       6, 's', 'e', 'c', 'o', 'n', 'd'};

  InitAndTestWrite();
}

TEST_F(ExtensionParameterBlockTest, TwoSubblocksParamDefinitionMode1) {
  metadata_args_.param_definition_mode = true;

  duration_args_ = {.duration = 64, .constant_subblock_duration = 32};

  parameter_block_extensions_ = {{5, {'f', 'i', 'r', 's', 't'}},
                                 {6, {'s', 'e', 'c', 'o', 'n', 'd'}}};

  expected_header_ = {kObuIaParameterBlock << 3, 16};
  expected_payload_ = {3,   64, 32,  5,   'f', 'i', 'r', 's',
                       't', 6,  's', 'e', 'c', 'o', 'n', 'd'};

  InitAndTestWrite();
}

struct InterpolateMixGainParameterDataTestCase {
  MixGainParameterData mix_gain_parameter_data;
  InternalTimestamp start_time;
  InternalTimestamp end_time;
  InternalTimestamp target_time;
  int16_t expected_target_mix_gain;

  absl::Status expected_status;
};

using InterpolateMixGainParameter =
    ::testing::TestWithParam<InterpolateMixGainParameterDataTestCase>;

TEST_P(InterpolateMixGainParameter, InterpolateMixGainParameter) {
  const InterpolateMixGainParameterDataTestCase& test_case = GetParam();
  float target_mix_gain_db;
  EXPECT_EQ(ParameterBlockObu::InterpolateMixGainParameterData(
                &test_case.mix_gain_parameter_data, test_case.start_time,
                test_case.end_time, test_case.target_time, target_mix_gain_db),
            test_case.expected_status);

  if (test_case.expected_status.ok()) {
    int16_t target_mix_gain_q7_8;
    EXPECT_THAT(FloatToQ7_8(target_mix_gain_db, target_mix_gain_q7_8), IsOk());
    EXPECT_EQ(target_mix_gain_q7_8, test_case.expected_target_mix_gain);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Step, InterpolateMixGainParameter,
    testing::ValuesIn<InterpolateMixGainParameterDataTestCase>({
        {.mix_gain_parameter_data =
             {kAnimateStep, AnimationStepInt16{.start_point_value = 0}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 0,
         .expected_target_mix_gain = 0,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {kAnimateStep, AnimationStepInt16{.start_point_value = 55}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 55,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {kAnimateStep, AnimationStepInt16{.start_point_value = 55}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 100,
         .expected_target_mix_gain = 55,
         .expected_status = absl::OkStatus()},
    }));

INSTANTIATE_TEST_SUITE_P(
    Linear, InterpolateMixGainParameter,
    testing::ValuesIn<InterpolateMixGainParameterDataTestCase>({
        {.mix_gain_parameter_data =
             {kAnimateLinear, AnimationLinearInt16{.start_point_value = 0,
                                                   .end_point_value = 1000}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 500,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {kAnimateLinear, AnimationLinearInt16{.start_point_value = 0,
                                                   .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 0,
         .expected_target_mix_gain = 0,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {kAnimateLinear, AnimationLinearInt16{.start_point_value = 0,
                                                   .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 1024,
         .expected_target_mix_gain = 3,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {kAnimateLinear, AnimationLinearInt16{.start_point_value = 0,
                                                   .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 3076,
         .expected_target_mix_gain = 9,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {kAnimateLinear, AnimationLinearInt16{.start_point_value = 0,
                                                   .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 4096,
         .expected_target_mix_gain = 13,
         .expected_status = absl::OkStatus()},
    }));

INSTANTIATE_TEST_SUITE_P(
    Bezier, InterpolateMixGainParameter,
    testing::ValuesIn<InterpolateMixGainParameterDataTestCase>({
        {.mix_gain_parameter_data = {kAnimateBezier,
                                     AnimationBezierInt16{
                                         .start_point_value = 0,
                                         .end_point_value = 768,
                                         .control_point_value = 384,
                                         .control_point_relative_time = 192}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 293,
         .expected_status = absl::OkStatus()},
    }));

// With some values of `param_data` the bezier animation approximates a linear
// function.
INSTANTIATE_TEST_SUITE_P(
    BezierAsLinear, InterpolateMixGainParameter,
    testing::ValuesIn<InterpolateMixGainParameterDataTestCase>({
        {.mix_gain_parameter_data = {kAnimateBezier,

                                     AnimationBezierInt16{
                                         .start_point_value = 200,
                                         .end_point_value = 768,
                                         .control_point_value = 484,
                                         .control_point_relative_time = 128}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 484,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data = {kAnimateBezier,
                                     AnimationBezierInt16{
                                         .start_point_value = 200,
                                         .end_point_value = 768,
                                         .control_point_value = 484,
                                         .control_point_relative_time = 128}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 0,
         .expected_target_mix_gain = 200,
         .expected_status = absl::OkStatus()},
    }));

}  // namespace
}  // namespace iamf_tools
