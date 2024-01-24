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
#include "iamf/cli/parameter_block_partitioner.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/ia.h"
#include "iamf/obu_header.h"
#include "iamf/param_definitions.h"
#include "iamf/parameter_block.h"

namespace iamf_tools {
namespace {

/*\!brief Creates a `ParameterBlockWithData` with the input subblock durations.
 *
 * \param param_definition_mode `param_definition_mode` of the per-ID metadata.
 * \param parameter_rate `parameter_rate` of the per-ID metadata.
 * \param subblock_durations Input subblock durations.
 * \param mix_gain_parameter_data Input mix gain parameter datas or an empty
 *     vector to assume all gains as step with a value of 0.
 * \param per_id_metadata Output per-ID parameter metadata.
 * \param parameter_block Output parameter block.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status GetMinimalParameterBlockWithData(
    const uint8_t param_definition_mode, const DecodedUleb128 parameter_rate,
    const std::vector<DecodedUleb128>& subblock_durations,
    const std::vector<MixGainParameterData>& mix_gain_parameter_data,
    PerIdParameterMetadata& per_id_metadata,
    ParameterBlockWithData& parameter_block) {
  if (subblock_durations.empty()) {
    return absl::InvalidArgumentError("");
  }
  std::vector<MixGainParameterData> mix_gains;
  if (mix_gain_parameter_data.empty()) {
    // Fill with steps with a value of 0 if the input argument is not present.
    mix_gains.resize(
        subblock_durations.size(),
        {.animation_type = MixGainParameterData::kAnimateStep,
         .param_data = AnimationStepInt16{.start_point_value = 0}});
  } else {
    mix_gains = mix_gain_parameter_data;
  }

  // Calculate the duration from the input subblocks.
  DecodedUleb128 duration = 0;
  DecodedUleb128 constant_subblock_duration =
      ParameterBlockPartitioner::FindConstantSubblockDuration(
          subblock_durations);

  // Add the duration of the remaining blocks.
  for (auto& subblock_duration : subblock_durations) {
    duration += subblock_duration;
  }

  // Create some default mix gain metadata.
  parameter_block.start_timestamp = 0;
  per_id_metadata.param_definition_type =
      ParamDefinition::kParameterDefinitionMixGain;
  per_id_metadata.param_definition.param_definition_mode_ =
      param_definition_mode;

  // Not used, but initialized to be extra safe.
  per_id_metadata.num_layers = 0;
  per_id_metadata.param_definition.parameter_id_ = 0;
  per_id_metadata.param_definition.parameter_rate_ = parameter_rate;

  // Configure the OBU.
  parameter_block.obu = std::make_unique<ParameterBlockObu>(
      ObuHeader(), per_id_metadata.param_definition.parameter_id_,
      &per_id_metadata);

  ParameterBlockObu& obu = *parameter_block.obu;
  RETURN_IF_NOT_OK(obu.InitializeSubblocks(
      duration, constant_subblock_duration,
      static_cast<DecodedUleb128>(subblock_durations.size())));
  for (int i = 0; i < subblock_durations.size(); i++) {
    // Configure the subblocks with the input durations and mix gains.
    RETURN_IF_NOT_OK(obu.SetSubblockDuration(i, subblock_durations[i]));
    std::get<MixGainParameterData>(obu.subblocks_[i].param_data) = mix_gains[i];
  }

  return absl::OkStatus();
}

// TODO(b/277731089): Test `PartitionParameterBlock()` and
//                    `PartitionFrameAligned()` more thoroughly, including with
//                    `param_definition_mode == 0`. Be careful of edge cases.

struct PartitionParameterBlocksTestCase {
  std::vector<DecodedUleb128> input_subblock_durations;
  std::vector<MixGainParameterData> input_mix_gains;
  int partition_start;
  int partition_end;
  std::vector<DecodedUleb128> expected_partition_durations;
  std::vector<MixGainParameterData> expected_output_mix_gains;
  DecodedUleb128 constant_subblock_duration;
  absl::StatusCode expected_status_code;
};

using PartitionParameterBlocksMode1 =
    ::testing::TestWithParam<PartitionParameterBlocksTestCase>;

TEST_P(PartitionParameterBlocksMode1, PartitionParameterBlock) {
  const PartitionParameterBlocksTestCase& test_case = GetParam();

  // Create the parameter block to partition.
  ParameterBlockWithData parameter_block;
  PerIdParameterMetadata per_id_metadata;
  EXPECT_TRUE(GetMinimalParameterBlockWithData(
                  /*param_definition_mode=*/1, /*parameter_rate=*/8000,
                  test_case.input_subblock_durations, test_case.input_mix_gains,
                  per_id_metadata, parameter_block)
                  .ok());

  // Partition the parameter block.
  ParameterBlockWithData partitioned_parameter_block;

  ParameterBlockPartitioner partitioner(ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(
      partitioner
          .PartitionParameterBlock(parameter_block, test_case.partition_start,
                                   test_case.partition_end, per_id_metadata,
                                   partitioned_parameter_block)
          .code(),
      test_case.expected_status_code);

  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    // Validate the parameter block has as many subblocks in the partition as
    // expected.
    const auto num_subblocks =
        partitioned_parameter_block.obu->GetNumSubblocks();
    EXPECT_EQ(test_case.expected_partition_durations.size(), num_subblocks);

    const auto constant_subblock_duration =
        partitioned_parameter_block.obu->GetConstantSubblockDuration();
    EXPECT_EQ(constant_subblock_duration, test_case.constant_subblock_duration);
    if (test_case.constant_subblock_duration == 0) {
      // If the subblocks are included validate the all match the expected
      // subblock.
      for (int i = 0; i < test_case.expected_partition_durations.size(); ++i) {
        const auto subblock_duration =
            partitioned_parameter_block.obu->GetSubblockDuration(i);
        EXPECT_EQ(test_case.expected_partition_durations[i],
                  subblock_duration.value());
      }
    }

    // Compare the expected mix gains if present.
    for (int i = 0; i < test_case.expected_output_mix_gains.size(); ++i) {
      EXPECT_EQ(std::get<MixGainParameterData>(
                    partitioned_parameter_block.obu->subblocks_[i].param_data),
                test_case.expected_output_mix_gains[i]);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    OneSubblock, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{8000}, {}, 0, 1, {1}, {}, 1, absl::StatusCode::kOk},
        {{8000}, {}, 0, 128, {128}, {}, 128, absl::StatusCode::kOk},
        {{8000}, {}, 0, 8000, {8000}, {}, 8000, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    TwoSubblocksConstantSubblockDurationNonzero, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000}, {}, 0, 3999, {3999}, {}, 3999, absl::StatusCode::kOk},
        {{4000, 4000}, {}, 3950, 4050, {50, 50}, {}, 50, absl::StatusCode::kOk},
        {{4000, 4000}, {}, 3950, 4025, {50, 25}, {}, 50, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    TwoSubblocksConstantSubblockDuration0, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000}, {}, 3975, 4050, {25, 50}, {}, 0, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(ManySubblocks, PartitionParameterBlocksMode1,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{1, 2, 3, 10, 10, 10},
                              {},
                              0,
                              35,
                              {1, 2, 3, 10, 10, 9},
                              {},
                              0,
                              absl::StatusCode::kOk},
                             {{1, 2, 3, 10, 10, 10},
                              {},
                              2,
                              35,
                              {1, 3, 10, 10, 9},
                              {},
                              0,
                              absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(
    ErrorZeroDuration, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000}, {}, 0, 0, {}, {}, 0, absl::StatusCode::kInvalidArgument},
    }));

INSTANTIATE_TEST_SUITE_P(ErrorNegativeDuration, PartitionParameterBlocksMode1,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000},
                              {},
                              10000,
                              0,
                              {},
                              {},
                              0,
                              absl::StatusCode::kInvalidArgument},
                         }));

INSTANTIATE_TEST_SUITE_P(ErrorNotFullyCovered, PartitionParameterBlocksMode1,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000},
                              {},
                              4000,
                              8001,
                              {},
                              {},
                              0,
                              absl::StatusCode::kInvalidArgument},
                         }));

INSTANTIATE_TEST_SUITE_P(
    Step, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000},
         {{.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 10}},
          {.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 20}}},
         0,
         3999,
         {3999},
         {{.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 10}}},
         3999,
         absl::StatusCode::kOk},
        {{4000, 4000},
         {{.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 10}},
          {.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 20}}},
         2000,
         6000,
         {2000, 2000},
         {{.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 10}},
          {.animation_type = MixGainParameterData::kAnimateStep,
           .param_data = AnimationStepInt16{.start_point_value = 20}}},
         2000,
         absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    Linear, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000},
         {{.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 0,
                                              .end_point_value = 100}},
          {.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 100,
                                              .end_point_value = 1000}}},
         1000,
         3000,
         {2000},
         {{.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 25,
                                              .end_point_value = 75}}},
         2000,
         absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    LinearTwoSubblocks, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000},
         {{.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 0,
                                              .end_point_value = 100}},
          {.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 100,
                                              .end_point_value = 1000}}},
         1000,
         6000,
         {3000, 2000},
         {{.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 25,
                                              .end_point_value = 100}},
          {.animation_type = MixGainParameterData::kAnimateLinear,
           .param_data = AnimationLinearInt16{.start_point_value = 100,
                                              .end_point_value = 550}}},
         3000,
         absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    BezierAligned, PartitionParameterBlocksMode1,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000},
         {{.animation_type = MixGainParameterData::kAnimateBezier,
           .param_data =
               AnimationBezierInt16{.start_point_value = 0,
                                    .end_point_value = 100,
                                    .control_point_value = 64,
                                    .control_point_relative_time = 100}}},
         0,
         4000,
         {4000},
         {{.animation_type = MixGainParameterData::kAnimateBezier,
           .param_data =
               AnimationBezierInt16{.start_point_value = 0,
                                    .end_point_value = 100,
                                    .control_point_value = 64,
                                    .control_point_relative_time = 100}}},
         4000,
         absl::StatusCode::kOk},
    }));

struct FindConstantSubblockDurationTestCase {
  std::vector<DecodedUleb128> input_subblock_durations;
  DecodedUleb128 expected_constant_subblock_duration;
};

using FindConstantSubblockDurationTest =
    ::testing::TestWithParam<FindConstantSubblockDurationTestCase>;

TEST_P(FindConstantSubblockDurationTest, PartitionParameterBlock) {
  const FindConstantSubblockDurationTestCase& test_case = GetParam();

  EXPECT_EQ(test_case.expected_constant_subblock_duration,
            ParameterBlockPartitioner::FindConstantSubblockDuration(
                test_case.input_subblock_durations));
}

INSTANTIATE_TEST_SUITE_P(
    OneSubblock, FindConstantSubblockDurationTest,
    testing::ValuesIn<FindConstantSubblockDurationTestCase>({
        {{1}, 1},
        {{4000}, 4000},
        {{UINT32_MAX}, UINT32_MAX},
    }));

INSTANTIATE_TEST_SUITE_P(
    TwoSubblocksFirstLonger, FindConstantSubblockDurationTest,
    testing::ValuesIn<FindConstantSubblockDurationTestCase>({
        {{2, 1}, 2},
    }));

INSTANTIATE_TEST_SUITE_P(
    TwoSubblocksFirstShorter, FindConstantSubblockDurationTest,
    testing::ValuesIn<FindConstantSubblockDurationTestCase>({
        {{1, 2}, 0},
    }));

INSTANTIATE_TEST_SUITE_P(
    ManySubblocksEqual, FindConstantSubblockDurationTest,
    testing::ValuesIn<FindConstantSubblockDurationTestCase>({
        {{99, 99, 99, 99}, 99},
        {{4, 4, 4, 4}, 4},
    }));

INSTANTIATE_TEST_SUITE_P(
    ManySubblocksLastShorter, FindConstantSubblockDurationTest,
    testing::ValuesIn<FindConstantSubblockDurationTestCase>({
        {{99, 99, 99, 97}, 99},
        {{4, 4, 4, 3}, 4},
    }));

INSTANTIATE_TEST_SUITE_P(
    ManySubblocksUnequal, FindConstantSubblockDurationTest,
    testing::ValuesIn<FindConstantSubblockDurationTestCase>({
        {{4, 4, 4, 5}, 0},
        {{99, 100, 101, 102}, 0},
    }));

}  // namespace
}  // namespace iamf_tools
