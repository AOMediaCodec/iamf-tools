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

#include <array>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/tests/obu_test_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using absl_testing::IsOkAndHolds;

constexpr uint32_t kAudioElementId = 0;

// TODO(b/273545873): Add more "expected failure" tests. Add more "successful"
//                    test cases to existing tests. Test
//                    `PerIdParameterMetadata` settings more thoroughly.

TEST(AnimationStepInt16, ReadAndValidate) {
  std::vector<uint8_t> source_data = {
      // Start point value.
      0x02,
      0x01,
  };
  ReadBitBuffer buffer(1024, &source_data);

  AnimationStepInt16 step_animation;
  EXPECT_THAT(step_animation.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(step_animation.start_point_value, 0x0201);
}

TEST(AnimationLinearInt16, ReadAndValidate) {
  std::vector<uint8_t> source_data = {
      // Start point value.
      0x04,
      0x03,
      // End point value.
      0x02,
      0x01,
  };
  ReadBitBuffer buffer(1024, &source_data);

  AnimationLinearInt16 linear_animation;
  EXPECT_THAT(linear_animation.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(linear_animation.start_point_value, 0x0403);
  EXPECT_EQ(linear_animation.end_point_value, 0x0201);
}

TEST(AnimationBezierInt16, ReadAndValidate) {
  std::vector<uint8_t> source_data = {// Start point value.
                                      0x07, 0x06,
                                      // End point value.
                                      0x05, 0x04,
                                      // Control point value.
                                      0x03, 0x02,
                                      // Control point relative time.
                                      0x01};
  ReadBitBuffer buffer(1024, &source_data);

  AnimationBezierInt16 bezier_animation;
  EXPECT_THAT(bezier_animation.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(bezier_animation.start_point_value, 0x0706);
  EXPECT_EQ(bezier_animation.end_point_value, 0x0504);
  EXPECT_EQ(bezier_animation.control_point_value, 0x0302);
  EXPECT_EQ(bezier_animation.control_point_relative_time, 0x01);
}

TEST(MixGainParameterData, ReadAndValidateStep) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x00,
      // Start point value.
      0x02,
      0x01,
  };
  ReadBitBuffer buffer(1024, &source_data);

  MixGainParameterData mix_gain_param_data;
  EXPECT_THAT(mix_gain_param_data.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(mix_gain_param_data.animation_type,
            MixGainParameterData::kAnimateStep);
  EXPECT_TRUE(std::holds_alternative<AnimationStepInt16>(
      mix_gain_param_data.param_data));
}

TEST(MixGainParameterData, ReadAndValidateLinear) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x01,
      // Start point value.
      0x04,
      0x03,
      // End point value.
      0x02,
      0x01,
  };
  ReadBitBuffer buffer(1024, &source_data);

  MixGainParameterData mix_gain_param_data;
  EXPECT_THAT(mix_gain_param_data.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(mix_gain_param_data.animation_type,
            MixGainParameterData::kAnimateLinear);
  EXPECT_TRUE(std::holds_alternative<AnimationLinearInt16>(
      mix_gain_param_data.param_data));
}

TEST(MixGainParameterData, ReadAndValidateBezier) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x02,
      // Start point value.
      0x07,
      0x06,
      // End point value.
      0x05,
      0x04,
      // Control point value.
      0x03,
      0x02,
      // Control point relative time.
      0x01,
  };
  ReadBitBuffer buffer(1024, &source_data);

  MixGainParameterData mix_gain_param_data;
  EXPECT_THAT(mix_gain_param_data.ReadAndValidate(buffer), IsOk());
  EXPECT_EQ(mix_gain_param_data.animation_type,
            MixGainParameterData::kAnimateBezier);
  EXPECT_TRUE(std::holds_alternative<AnimationBezierInt16>(
      mix_gain_param_data.param_data));
}

TEST(MixGainParameterData,
     ReadAndValidateReturnsErrorWhenAnimationTypeIsUnknown) {
  std::vector<uint8_t> source_data = {
      // Animation type.
      0x03,
  };
  ReadBitBuffer buffer(1024, &source_data);

  MixGainParameterData mix_gain_param_data;
  EXPECT_FALSE(mix_gain_param_data.ReadAndValidate(buffer).ok());
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
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x09,
      0x88,
      // Subblock duration.
      0x03,
      // Animation type.
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x07,
      0x66,
      // Subblock duration.
      0x06,
      // Animation type.
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x05,
      0x44,
  };
  ReadBitBuffer buffer(1024, &source_data);
  // Usually metadata would live in the descriptor OBUs.
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      per_param_metadata;
  per_param_metadata[kParameterId] = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition = MixGainParamDefinition(),
  };
  per_param_metadata[kParameterId].param_definition.parameter_id_ =
      kParameterId;
  per_param_metadata[kParameterId].param_definition.parameter_rate_ = 1;
  per_param_metadata[kParameterId].param_definition.param_definition_mode_ = 1;
  auto parameter_block = ParameterBlockObu::CreateFromBuffer(
      ObuHeader{.obu_type = kObuIaParameterBlock}, per_param_metadata, buffer);
  EXPECT_THAT(parameter_block, IsOk());

  // Validate all the getters match the input data.
  EXPECT_EQ(parameter_block->parameter_id_, kParameterId);
  EXPECT_EQ(parameter_block->GetDuration(), 10);
  EXPECT_EQ(parameter_block->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(parameter_block->GetNumSubblocks(), 3);
  EXPECT_THAT(parameter_block->GetSubblockDuration(0), IsOkAndHolds(1));
  EXPECT_THAT(parameter_block->GetSubblockDuration(1), IsOkAndHolds(3));
  EXPECT_THAT(parameter_block->GetSubblockDuration(2), IsOkAndHolds(6));

  int16_t mix_gain;
  // The first subblock covers [0, subblock_duration[0]).
  EXPECT_THAT(parameter_block->GetMixGain(0, mix_gain), IsOk());
  EXPECT_EQ(mix_gain, 0x0988);
  EXPECT_THAT(parameter_block->GetMixGain(1, mix_gain), IsOk());
  EXPECT_EQ(mix_gain, 0x0766);
  EXPECT_THAT(parameter_block->GetMixGain(4, mix_gain), IsOk());
  EXPECT_EQ(mix_gain, 0x0544);

  // Parameter blocks are open intervals.
  EXPECT_FALSE(parameter_block->GetMixGain(10, mix_gain).ok());
}

TEST(ParameterBlockObu, CreateFromBufferParamDefinitionMode0) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {
      // Parameter ID.
      kParameterId,
      // Animation type.
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x09,
      0x88,
      // Animation type.
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x07,
      0x66,
      // Animation type.
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x05,
      0x44,
  };
  ReadBitBuffer buffer(1024, &source_data);
  // Usually metadata would live in the descriptor OBUs.
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      per_param_metadata;
  per_param_metadata[kParameterId] = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition = MixGainParamDefinition(),
  };
  auto& param_definition = per_param_metadata[kParameterId].param_definition;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = 10;
  param_definition.constant_subblock_duration_ = 0;
  param_definition.InitializeSubblockDurations(3);
  ASSERT_THAT(param_definition.SetSubblockDuration(0, 1), IsOk());
  ASSERT_THAT(param_definition.SetSubblockDuration(1, 3), IsOk());
  ASSERT_THAT(param_definition.SetSubblockDuration(2, 6), IsOk());
  auto parameter_block = ParameterBlockObu::CreateFromBuffer(
      ObuHeader{.obu_type = kObuIaParameterBlock}, per_param_metadata, buffer);
  EXPECT_THAT(parameter_block, IsOk());

  // Validate all the getters match the input data. Note the getters return data
  // based on the `param_definition` and not the data in the OBU.
  EXPECT_EQ(parameter_block->parameter_id_, kParameterId);
  EXPECT_EQ(parameter_block->GetDuration(), 10);
  EXPECT_EQ(parameter_block->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(parameter_block->GetNumSubblocks(), 3);
  EXPECT_THAT(parameter_block->GetSubblockDuration(0), IsOkAndHolds(1));
  EXPECT_THAT(parameter_block->GetSubblockDuration(1), IsOkAndHolds(3));
  EXPECT_THAT(parameter_block->GetSubblockDuration(2), IsOkAndHolds(6));

  int16_t mix_gain;
  // The first subblock covers [0, subblock_duration[0]).
  EXPECT_THAT(parameter_block->GetMixGain(0, mix_gain), IsOk());
  EXPECT_EQ(mix_gain, 0x0988);
  EXPECT_THAT(parameter_block->GetMixGain(1, mix_gain), IsOk());
  EXPECT_EQ(mix_gain, 0x0766);
  EXPECT_THAT(parameter_block->GetMixGain(4, mix_gain), IsOk());
  EXPECT_EQ(mix_gain, 0x0544);

  // Parameter blocks are open intervals.
  EXPECT_FALSE(parameter_block->GetMixGain(10, mix_gain).ok());
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
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x09,
      0x88,
  };
  ReadBitBuffer buffer(1024, &source_data);
  // Usually metadata would live in the descriptor OBUs.
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      per_param_metadata;
  per_param_metadata[kParameterId] = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition = MixGainParamDefinition(),
  };
  per_param_metadata[kParameterId].param_definition.parameter_id_ =
      kParameterId;
  per_param_metadata[kParameterId].param_definition.parameter_rate_ = 1;
  per_param_metadata[kParameterId].param_definition.param_definition_mode_ = 1;

  EXPECT_FALSE(ParameterBlockObu::CreateFromBuffer(
                   ObuHeader{.obu_type = kObuIaParameterBlock},
                   per_param_metadata, buffer)
                   .ok());
}

TEST(ParameterBlockObu, CreateFromBufferParamRequiresPerIdParameterMetadata) {
  const DecodedUleb128 kParameterId = 0x07;
  std::vector<uint8_t> source_data = {
      // Parameter ID.
      kParameterId,
      // Duration.
      0x0a,
      // Constant subblock duration.
      0x0a,
      // Animation type.
      MixGainParameterData::kAnimateStep,
      // Start point value.
      0x09,
      0x88,
  };
  ReadBitBuffer buffer(1024, &source_data);
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      per_param_metadata;
  per_param_metadata[kParameterId] = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition = MixGainParamDefinition(),
  };
  per_param_metadata[kParameterId].param_definition.parameter_id_ =
      kParameterId;
  per_param_metadata[kParameterId].param_definition.parameter_rate_ = 1;
  per_param_metadata[kParameterId].param_definition.param_definition_mode_ = 1;
  EXPECT_THAT(ParameterBlockObu::CreateFromBuffer(
                  ObuHeader{.obu_type = kObuIaParameterBlock},
                  per_param_metadata, buffer),
              absl_testing::IsOk());

  // When there is no matching metadata, the parameter block cannot be created.
  per_param_metadata.erase(kParameterId);
  ReadBitBuffer buffer_to_use_without_metadata(1024, &source_data);
  EXPECT_FALSE(ParameterBlockObu::CreateFromBuffer(
                   ObuHeader{.obu_type = kObuIaParameterBlock},
                   per_param_metadata, buffer_to_use_without_metadata)
                   .ok());
}

class ParameterBlockObuTestBase : public ObuTestBase {
 public:
  ParameterBlockObuTestBase(ParamDefinition param_definition)
      : ObuTestBase(
            /*expected_header=*/{kObuIaParameterBlock << 3, 4},
            /*expected_payload=*/{}),
        obu_(nullptr),
        metadata_({.param_definition = param_definition}),
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
    InitMainParameterBlockObu();

    InitParameterBlockTypeSpecificFields();
  }

  virtual void InitParameterBlockTypeSpecificFields() = 0;

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  std::unique_ptr<ParameterBlockObu> obu_;
  PerIdParameterMetadata metadata_;

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
  void InitMainParameterBlockObu() {
    // Copy over all arguments into the `ParameterBlockObu`.
    //
    // Code within `iamf_tools` will find the associated Audio Element or Mix
    // Presentation OBU and use that metadata. For testing here the metadata is
    // initialized based on `metadata_args_`.
    ASSERT_TRUE(metadata_.param_definition.GetType().has_value());
    metadata_.param_definition_type = *metadata_.param_definition.GetType();
    metadata_.param_definition.parameter_id_ = parameter_id_;
    metadata_.param_definition.parameter_rate_ = metadata_args_.parameter_rate;
    metadata_.param_definition.param_definition_mode_ =
        metadata_args_.param_definition_mode;
    metadata_.param_definition.reserved_ = metadata_args_.reserved;
    metadata_.num_layers = metadata_args_.num_layers;

    // Copy the `recon_gain_is_present_flags` vector. Code within `iamf_tools`
    // will already have this array allocated and populated by the
    // `ParameterBlockGenerator`.
    metadata_.recon_gain_is_present_flags =
        metadata_args_.recon_gain_is_present_flags;

    obu_ =
        std::make_unique<ParameterBlockObu>(header_, parameter_id_, &metadata_);
    EXPECT_THAT(
        obu_->InitializeSubblocks(duration_args_.duration,
                                  duration_args_.constant_subblock_duration,
                                  duration_args_.num_subblocks),
        IsOk());

    // Initialize memory for the metadata. This would typically be the
    // responsibility of the OBU that this Parameter Block references.
    metadata_.param_definition.InitializeSubblockDurations(
        duration_args_.num_subblocks);

    // With all memory allocated set the subblock durations. This may write to
    // the `metadata` or `obu` depending on the mode.
    for (int i = 0; i < duration_args_.subblock_durations.size(); i++) {
      EXPECT_THAT(
          obu_->SetSubblockDuration(i, duration_args_.subblock_durations[i]),
          IsOk());
    }
  }
};

class MixGainParameterBlockTest : public ParameterBlockObuTestBase,
                                  public testing::Test {
 public:
  MixGainParameterBlockTest()
      : ParameterBlockObuTestBase(MixGainParamDefinition()),
        mix_gain_param_datas_(
            {{.animation_type = MixGainParameterData::kAnimateStep,
              .param_data = AnimationStepInt16{1}}}) {}

 protected:
  void InitParameterBlockTypeSpecificFields() override {
    ASSERT_EQ(obu_->subblocks_.size(), mix_gain_param_datas_.size());

    // Copy over the mix gain parameter subblocks.
    for (int i = 0; i < obu_->subblocks_.size(); i++) {
      obu_->subblocks_[i].param_data = mix_gain_param_datas_[i];
    }
  }

  std::vector<MixGainParameterData> mix_gain_param_datas_;
};

TEST_F(MixGainParameterBlockTest, ConstructSetsObuType) {
  InitExpectOk();
  EXPECT_EQ(obu_->header_.obu_type, kObuIaParameterBlock);
}

TEST_F(MixGainParameterBlockTest, DefaultOneSubblockParamDefinitionMode0) {
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `mix_gain_parameter_data`.
                       MixGainParameterData::kAnimateStep, 0, 1};

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
                       MixGainParameterData::kAnimateStep, 0, 1};

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
                       MixGainParameterData::kAnimateStep, 0, 1};

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

  mix_gain_param_datas_ = {
      {.animation_type = MixGainParameterData::kAnimateStep,
       .param_data = AnimationStepInt16{0}},
      {.animation_type = MixGainParameterData::kAnimateStep,
       .param_data = AnimationStepInt16{0}}};

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

  mix_gain_param_datas_ = {
      {.animation_type = MixGainParameterData::kAnimateStep,
       .param_data = AnimationStepInt16{9}},
      {.animation_type = MixGainParameterData::kAnimateLinear,
       .param_data = AnimationLinearInt16{10, 11}},
      {.animation_type = MixGainParameterData::kAnimateBezier,
       .param_data = AnimationBezierInt16{12, 13, 14, 15}}};

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
                       MixGainParameterData::kAnimateStep, 0, 9,
                       // Start `subblocks[1]`.
                       // `subblock_duration`.
                       7,
                       // `mix_gain_parameter_data`.
                       MixGainParameterData::kAnimateLinear, 0, 10, 0, 11,
                       // Start `subblocks[2]`.
                       // `subblock_duration`.
                       8,
                       // `mix_gain_parameter_data`.
                       MixGainParameterData::kAnimateBezier, 0, 12, 0, 13, 0,
                       14, 15};
  InitAndTestWrite();
}

TEST_F(MixGainParameterBlockTest, MultipleSubblocksParamDefinitionMode0) {
  duration_args_ = {
      .duration = 21,
      .constant_subblock_duration = 0,
      .num_subblocks = 3,
      .subblock_durations = {6, 7, 8},
  };

  mix_gain_param_datas_ = {
      {.animation_type = MixGainParameterData::kAnimateStep,
       .param_data = AnimationStepInt16{9}},
      {.animation_type = MixGainParameterData::kAnimateLinear,
       .param_data = AnimationLinearInt16{10, 11}},
      {.animation_type = MixGainParameterData::kAnimateBezier,
       .param_data = AnimationBezierInt16{12, 13, 14, 15}}};

  expected_header_ = {kObuIaParameterBlock << 3, 17};
  expected_payload_ = {// `parameter_id`.
                       3,
                       // Start `subblocks[0]`.
                       // `mix_gain_parameter_data`.
                       MixGainParameterData::kAnimateStep, 0, 9,
                       // Start `subblocks[1]`.
                       // `mix_gain_parameter_data`.
                       MixGainParameterData::kAnimateLinear, 0, 10, 0, 11,
                       // Start `subblocks[2]`.
                       // `mix_gain_parameter_data`.
                       MixGainParameterData::kAnimateBezier, 0, 12, 0, 13, 0,
                       14, 15};

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

  mix_gain_param_datas_ = {
      {.animation_type = MixGainParameterData::kAnimateStep,
       .param_data = AnimationStepInt16{9}},
      {.animation_type = MixGainParameterData::kAnimateStep,
       .param_data = AnimationStepInt16{10}}};

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
      0x80 | MixGainParameterData::kAnimateStep, 0x00, 0, 9,
      // Start `subblocks[1]`.
      // `duration` is affected by the `LebGenerator`.
      0x80 | 7, 0x00,
      // `mix_gain_parameter_data`.
      0x80 | MixGainParameterData::kAnimateStep, 0x00, 0, 10};

  InitAndTestWrite();
}

class DemixingParameterBlockTest : public ParameterBlockObuTestBase,
                                   public testing::Test {
 public:
  DemixingParameterBlockTest()
      : ParameterBlockObuTestBase(DemixingParamDefinition()),
        demixing_info_args_(
            {.dmixp_mode = {DemixingInfoParameterData::kDMixPMode1},
             .reserved = {0}}) {
    expected_header_ = {kObuIaParameterBlock << 3, 2};
  }

 protected:
  void InitParameterBlockTypeSpecificFields() override {
    ASSERT_EQ(demixing_info_args_.dmixp_mode.size(),
              demixing_info_args_.reserved.size());
    // Loop over and populate the demixing parameter for each subblock.
    for (int i = 0; i < demixing_info_args_.dmixp_mode.size(); i++) {
      DemixingInfoParameterData demixing_param_data;
      demixing_param_data.dmixp_mode = demixing_info_args_.dmixp_mode[i];
      demixing_param_data.reserved = demixing_info_args_.reserved[i];

      obu_->subblocks_[i].param_data = demixing_param_data;
    }
  }

  struct {
    std::vector<DemixingInfoParameterData::DMixPMode> dmixp_mode;
    std::vector<uint8_t> reserved;
  } demixing_info_args_;
};

TEST_F(DemixingParameterBlockTest, DefaultParamDefinitionMode0) {
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `demixing_info_parameter_data`.
                       DemixingInfoParameterData::kDMixPMode1 << 5};

  InitAndTestWrite();
}

TEST_F(DemixingParameterBlockTest, DMixPMode2) {
  demixing_info_args_.dmixp_mode = {DemixingInfoParameterData::kDMixPMode2};
  expected_payload_ = {// `parameter_id`.
                       3,
                       // `demixing_info_parameter_data`.
                       DemixingInfoParameterData::kDMixPMode2 << 5};

  InitAndTestWrite();
}

TEST_F(DemixingParameterBlockTest,
       ValidateAndWriteObuFailsWhenParamDefinitionMode1TooManySubblocks) {
  // TODO(b/295173212): Modify this test case when the restriction of
  //                    `num_subblocks` on recon gain parameter blocks
  //                    is enforced. Current it is only enforced when
  //                    `param_definition_mode == 1`.
  metadata_args_.param_definition_mode = true;

  duration_args_ = {
      .duration = 4,
      .constant_subblock_duration = 0,
      .num_subblocks = 5,
      .subblock_durations = {6, 7, 8, 9, 10},
  };

  demixing_info_args_ = {
      .dmixp_mode = {DemixingInfoParameterData::kDMixPMode1,
                     DemixingInfoParameterData::kDMixPMode2,
                     DemixingInfoParameterData::kDMixPMode3,
                     DemixingInfoParameterData::kDMixPMode1_n,
                     DemixingInfoParameterData::kDMixPMode2_n},
      .reserved = {0, 0, 0, 0, 0}};

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
  ReconGainBlockTest()
      : ParameterBlockObuTestBase(ReconGainParamDefinition(kAudioElementId)) {}

 protected:
  void InitParameterBlockTypeSpecificFields() override {
    // Loop over and populate the recon gain parameter for each layer within
    // each subblock.
    const DecodedUleb128 num_subblocks = obu_->GetNumSubblocks();

    // Each element in `recon_gain_flags` and `recon_gains` corresponds to a
    // single subblock.
    ASSERT_EQ(recon_gain_args_.recon_gain_flags.size(), num_subblocks);
    ASSERT_EQ(recon_gain_args_.recon_gains.size(), num_subblocks);
    for (int i = 0; i < recon_gain_args_.recon_gain_flags.size(); i++) {
      // Each element in `recon_gain_flags[i]` and `recon_gains[i]` corresponds
      // to a single layer.
      ASSERT_EQ(recon_gain_args_.recon_gain_flags[i].size(),
                metadata_.num_layers);
      ASSERT_EQ(recon_gain_args_.recon_gains[i].size(), metadata_.num_layers);
      ReconGainInfoParameterData recon_gain_param_data;
      recon_gain_param_data.recon_gain_elements.resize(metadata_.num_layers);
      for (int j = 0; j < recon_gain_args_.recon_gain_flags[i].size(); j++) {
        auto& recon_gain_element = recon_gain_param_data.recon_gain_elements[j];
        recon_gain_element.recon_gain_flag =
            recon_gain_args_.recon_gain_flags[i][j];
        recon_gain_element.recon_gain = recon_gain_args_.recon_gains[i][j];
      }

      obu_->subblocks_[i].param_data = recon_gain_param_data;
    }
  }

  struct {
    // recon_gain_flags[i][j] represents the ith subblock and jth layer.
    std::vector<std::vector<uint16_t>> recon_gain_flags;

    // recon_gains[i][j][k] represents the ith subblock and jth layer with the
    // kth element of `recon_gain`.
    std::vector<std::vector<std::array<uint8_t, 12>>> recon_gains;
  } recon_gain_args_;
};

TEST_F(ReconGainBlockTest, TwoLayerParamDefinitionMode0) {
  metadata_args_.num_layers = 2;
  metadata_args_.recon_gain_is_present_flags = {false, true};

  recon_gain_args_ =
      {.recon_gain_flags = {{0, ReconGainElement::kReconGainFlagR}},
       .recon_gains = {{
           // clang-format off
           // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
           {  0, 0,  0,       0,       0,   0,   0,   0,   0,   0,   0,   0},
           {  0, 0,  1,       0,       0,   0,   0,   0,   0,   0,   0,   0},
           // clang-format on
       }}},

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

  recon_gain_args_ = {
      .recon_gain_flags = {{0, ReconGainElement::kReconGainFlagR}},
      .recon_gains = {{
          // clang-format off
           // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
           {  0, 0,  0,       0,       0,   0,   0,   0,   0,   0,   0,   0},
           {  0, 0,  1,       0,       0,   0,   0,   0,   0,   0,   0,   0},
          // clang-format on
      }}};

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
  recon_gain_args_ =
      {.recon_gain_flags = {{
           0,                                  // Mono.
           ReconGainElement::kReconGainFlagR,  // M + R stereo.
           ReconGainElement::kReconGainFlagRss |
               ReconGainElement::kReconGainFlagLss,  // 5.1.0.
           ReconGainElement::kReconGainFlagLrs |
               ReconGainElement::kReconGainFlagRrs,  // 7.1.0.
           ReconGainElement::kReconGainFlagLtf |
               ReconGainElement::kReconGainFlagRtf,  // 7.1.2.
           ReconGainElement::kReconGainFlagLtb |
               ReconGainElement::kReconGainFlagRtb,  // 7.1.4.
       }},
       .recon_gains = {{
           // clang-format off
           // L, C,  R, Ls(Lss), Rs(Rss), Ltf, Rtf, Lrs, Rrs, Ltb, Rtb, LFE.
           {  0, 0,  0,       0,       0,   0,   0,   0,   0,   0,   0,   0},
           {  0, 0,  1,       0,       0,   0,   0,   0,   0,   0,   0,   0},
           {  0, 0,  0,       2,       3,   0,   0,   0,   0,   0,   0,   0},
           {  0, 0,  0,       0,       0,   0,   0,   4,   5,   0,   0,   0},
           {  0, 0,  0,       0,       0,   6,   7,   0,   0,   0,   0,   0},
           {  0, 0,  0,       0,       0,   0,   0,   0,   0,   8,   9,   0},
           // clang-format on
       }}},

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

  duration_args_ = {
      .duration = 64, .constant_subblock_duration = 32, .num_subblocks = 2};

  recon_gain_args_ =
      {.recon_gain_flags = {{0, ReconGainElement::kReconGainFlagR},
                            {0, ReconGainElement::kReconGainFlagR}},
       .recon_gains = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                        {0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
                       {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                        {0, 0, 254, 0, 0, 0, 0, 0, 0, 0, 0, 0}}}},

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(ReconGainBlockTest,
       ValidateAndWriteObuFailsWithWhenParamDefinitionModeIsOne) {
  metadata_args_.param_definition_mode = true;
  metadata_args_.num_layers = 2;
  metadata_args_.recon_gain_is_present_flags = {false, true};

  recon_gain_args_ =
      {.recon_gain_flags = {{0, ReconGainElement::kReconGainFlagR}},
       .recon_gains = {{{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                        {0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0}}}},

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

class ExtensionParameterBlockTest : public ParameterBlockObuTestBase,
                                    public testing::Test {
 public:
  ExtensionParameterBlockTest()
      : ParameterBlockObuTestBase(ExtendedParamDefinition(
            ParamDefinition::kParameterDefinitionReservedStart)),
        parameter_block_extensions_({{0, {}}}) {}

 protected:
  void InitParameterBlockTypeSpecificFields() override {
    const DecodedUleb128 num_subblocks = obu_->GetNumSubblocks();
    ASSERT_EQ(parameter_block_extensions_.size(), num_subblocks);

    for (int i = 0; i < num_subblocks; i++) {
      ASSERT_EQ(parameter_block_extensions_[i].parameter_data_size,
                parameter_block_extensions_[i].parameter_data_bytes.size());
      obu_->subblocks_[i].param_data = parameter_block_extensions_[i];
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
  metadata_.param_definition =
      ExtendedParamDefinition(ParamDefinition::kParameterDefinitionReservedEnd);

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
  int32_t start_time;
  int32_t end_time;
  int32_t target_time;
  int16_t expected_target_mix_gain;

  absl::Status expected_status;
};

using InterpolateMixGainParameter =
    ::testing::TestWithParam<InterpolateMixGainParameterDataTestCase>;

TEST_P(InterpolateMixGainParameter, InterpolateMixGainParameter) {
  const InterpolateMixGainParameterDataTestCase& test_case = GetParam();
  int16_t target_mix_gain;
  EXPECT_EQ(ParameterBlockObu::InterpolateMixGainParameterData(
                test_case.mix_gain_parameter_data, test_case.start_time,
                test_case.end_time, test_case.target_time, target_mix_gain),
            test_case.expected_status);

  if (test_case.expected_status.ok()) {
    EXPECT_EQ(target_mix_gain, test_case.expected_target_mix_gain);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Step, InterpolateMixGainParameter,
    testing::ValuesIn<InterpolateMixGainParameterDataTestCase>({
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateStep,
              .param_data = AnimationStepInt16{.start_point_value = 0}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 0,
         .expected_target_mix_gain = 0,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateStep,
              .param_data = AnimationStepInt16{.start_point_value = 55}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 55,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateStep,
              .param_data = AnimationStepInt16{.start_point_value = 55}},
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
             {.animation_type = MixGainParameterData::kAnimateLinear,
              .param_data = AnimationLinearInt16{.start_point_value = 0,
                                                 .end_point_value = 1000}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 500,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateLinear,
              .param_data = AnimationLinearInt16{.start_point_value = 0,
                                                 .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 0,
         .expected_target_mix_gain = 0,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateLinear,
              .param_data = AnimationLinearInt16{.start_point_value = 0,
                                                 .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 1024,
         .expected_target_mix_gain = 3,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateLinear,
              .param_data = AnimationLinearInt16{.start_point_value = 0,
                                                 .end_point_value = 768}},
         .start_time = 0,
         .end_time = 240640,
         .target_time = 3076,
         .expected_target_mix_gain = 9,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateLinear,
              .param_data = AnimationLinearInt16{.start_point_value = 0,
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
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateBezier,
              .param_data =
                  AnimationBezierInt16{.start_point_value = 0,
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
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateBezier,
              .param_data =
                  AnimationBezierInt16{.start_point_value = 200,
                                       .end_point_value = 768,
                                       .control_point_value = 484,
                                       .control_point_relative_time = 128}},
         .start_time = 0,
         .end_time = 100,
         .target_time = 50,
         .expected_target_mix_gain = 484,
         .expected_status = absl::OkStatus()},
        {.mix_gain_parameter_data =
             {.animation_type = MixGainParameterData::kAnimateBezier,
              .param_data =
                  AnimationBezierInt16{.start_point_value = 200,
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
