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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using iamf_tools_cli_proto::MixGainParameterData;
using iamf_tools_cli_proto::ParameterBlockObuMetadata;

MixGainParameterData CreateStepMixGainParameterData(
    const int32_t start_point_value) {
  MixGainParameterData mix_gain_parameter_data;
  mix_gain_parameter_data.set_animation_type(
      iamf_tools_cli_proto::ANIMATE_STEP);
  mix_gain_parameter_data.mutable_param_data()
      ->mutable_step()
      ->set_start_point_value(start_point_value);
  return mix_gain_parameter_data;
}

MixGainParameterData CreateLinearMixGainParameterData(
    const int32_t start_point_value, const int32_t end_point_value) {
  MixGainParameterData mix_gain_parameter_data;
  mix_gain_parameter_data.set_animation_type(
      iamf_tools_cli_proto::ANIMATE_LINEAR);
  auto* linear = mix_gain_parameter_data.mutable_param_data()->mutable_linear();
  linear->set_start_point_value(start_point_value);
  linear->set_end_point_value(end_point_value);
  return mix_gain_parameter_data;
}

MixGainParameterData CreateBezierMixGainParameterData(
    const int32_t start_point_value, const int32_t end_point_value,
    const int32_t control_point_value,
    const uint32_t control_point_relative_time) {
  MixGainParameterData mix_gain_parameter_data;
  mix_gain_parameter_data.set_animation_type(
      iamf_tools_cli_proto::ANIMATE_BEZIER);
  auto* bezier = mix_gain_parameter_data.mutable_param_data()->mutable_bezier();
  bezier->set_start_point_value(start_point_value);
  bezier->set_end_point_value(end_point_value);
  bezier->set_control_point_value(control_point_value);
  bezier->set_control_point_relative_time(control_point_relative_time);
  return mix_gain_parameter_data;
}

/*!\brief Creates a minimal parameter block OBU metadata.
 *
 * \param subblock_durations Input subblock durations.
 * \param mix_gain_parameter_data Input mix gain parameter datas or an empty
 *        vector to assume all gains as step with a value of 0.
 * \param full_parameter_block Output parameter block OBU metadata.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status CreateMinimalParameterBlockObuMetadata(
    const std::vector<uint32_t>& subblock_durations,
    const std::vector<MixGainParameterData>& mix_gain_parameter_data,
    ParameterBlockObuMetadata& full_parameter_block) {
  if (subblock_durations.empty()) {
    return absl::InvalidArgumentError("Subblock durations cannot be empty.");
  }
  std::vector<MixGainParameterData> mix_gains;
  if (mix_gain_parameter_data.empty()) {
    // Fill with steps with a value of 0 if the input argument is not present.
    mix_gains.resize(subblock_durations.size(),
                     CreateStepMixGainParameterData(0));
  } else {
    mix_gains = mix_gain_parameter_data;
  }

  // Calculate the duration from the input subblocks.
  uint32_t duration = 0;
  uint32_t constant_subblock_duration =
      ParameterBlockPartitioner::FindConstantSubblockDuration(
          subblock_durations);

  // Add the duration of the remaining blocks.
  for (auto& subblock_duration : subblock_durations) {
    duration += subblock_duration;
  }
  full_parameter_block.set_duration(duration);
  full_parameter_block.set_constant_subblock_duration(
      constant_subblock_duration);
  for (int i = 0; i < subblock_durations.size(); i++) {
    // Configure the subblocks with the input durations and mix gains.
    auto* subblock = full_parameter_block.add_subblocks();
    subblock->set_subblock_duration(subblock_durations[i]);
    *subblock->mutable_mix_gain_parameter_data() = mix_gains[i];
  }
  full_parameter_block.set_start_timestamp(0);

  return absl::OkStatus();
}

TEST(PartitionParameterBlock, IgnoredDeprecatedNumSubblocks) {
  ParameterBlockObuMetadata full_parameter_block;
  const std::vector<uint32_t> kSubblockDurations = {50, 100, 1000};
  // Slicing the hard-coded durations from [0, 150), should result in 2
  // subblocks.
  constexpr InternalTimestamp kStartTimestamp = 0;
  constexpr InternalTimestamp kEndTimestamp = 150;
  constexpr size_t kExpectedNumPartitionedNumSubblocks = 2;
  EXPECT_THAT(CreateMinimalParameterBlockObuMetadata(
                  kSubblockDurations,
                  std::vector<MixGainParameterData>(
                      3, CreateStepMixGainParameterData(0)),
                  full_parameter_block),
              IsOk());
  // Corrupt the deprecated `num_subblocks` field.
  constexpr auto kInconsistentNumSubblocks = 9999;
  full_parameter_block.set_num_subblocks(kInconsistentNumSubblocks);

  ParameterBlockObuMetadata partitioned_parameter_block;
  EXPECT_THAT(ParameterBlockPartitioner::PartitionParameterBlock(
                  full_parameter_block, kStartTimestamp, kEndTimestamp,
                  partitioned_parameter_block),
              IsOk());

  // Regardless, the slice has the correct number of subblocks.
  EXPECT_EQ(partitioned_parameter_block.subblocks_size(),
            kExpectedNumPartitionedNumSubblocks);
}

// TODO(b/277731089): Test `PartitionParameterBlock()` and
//                    `PartitionFrameAligned()` more thoroughly.

struct PartitionParameterBlocksTestCase {
  std::vector<uint32_t> input_subblock_durations;
  std::vector<MixGainParameterData> input_mix_gains;
  InternalTimestamp partition_start;
  InternalTimestamp partition_end;
  std::vector<uint32_t> expected_partition_durations;
  std::vector<MixGainParameterData> expected_output_mix_gains;
  uint32_t constant_subblock_duration;
  bool status_ok;
};

using PartitionParameterBlocks =
    ::testing::TestWithParam<PartitionParameterBlocksTestCase>;

TEST_P(PartitionParameterBlocks, PartitionParameterBlock) {
  const PartitionParameterBlocksTestCase& test_case = GetParam();

  // Create the parameter block to partition.
  ParameterBlockObuMetadata full_parameter_block;
  EXPECT_THAT(CreateMinimalParameterBlockObuMetadata(
                  test_case.input_subblock_durations, test_case.input_mix_gains,
                  full_parameter_block),
              IsOk());

  // Partition the parameter block.
  ParameterBlockObuMetadata partitioned_parameter_block;

  EXPECT_EQ(ParameterBlockPartitioner::PartitionParameterBlock(
                full_parameter_block, test_case.partition_start,
                test_case.partition_end, partitioned_parameter_block)
                .ok(),
            test_case.status_ok);

  if (test_case.status_ok) {
    // Validate the parameter block has as many subblocks in the partition as
    // expected.
    EXPECT_EQ(partitioned_parameter_block.subblocks_size(),
              test_case.expected_partition_durations.size());

    EXPECT_EQ(partitioned_parameter_block.constant_subblock_duration(),
              test_case.constant_subblock_duration);
    if (test_case.constant_subblock_duration == 0) {
      // If the subblocks are included validate the all match the expected
      // subblock.
      for (int i = 0; i < test_case.expected_partition_durations.size(); ++i) {
        EXPECT_EQ(partitioned_parameter_block.subblocks(i).subblock_duration(),
                  test_case.expected_partition_durations[i]);
      }
    }

    // Compare the expected mix gains if present.
    for (int i = 0; i < test_case.expected_output_mix_gains.size(); ++i) {
      const auto& actual_mix_gain =
          partitioned_parameter_block.subblocks(i).mix_gain_parameter_data();
      const auto& expected_mix_gain = test_case.expected_output_mix_gains[i];
      const auto& actual_param_data = actual_mix_gain.param_data();
      const auto& expected_param_data = expected_mix_gain.param_data();
      EXPECT_EQ(actual_mix_gain.animation_type(),
                expected_mix_gain.animation_type());
      switch (actual_mix_gain.animation_type()) {
        using enum iamf_tools_cli_proto::AnimationType;
        case ANIMATE_STEP:
          EXPECT_EQ(actual_param_data.step().start_point_value(),
                    expected_param_data.step().start_point_value());
          break;
        case ANIMATE_LINEAR:
          EXPECT_EQ(actual_param_data.linear().start_point_value(),
                    expected_param_data.linear().start_point_value());
          EXPECT_EQ(actual_param_data.linear().end_point_value(),
                    expected_param_data.linear().end_point_value());
          break;
        case ANIMATE_BEZIER:
          EXPECT_EQ(actual_param_data.bezier().start_point_value(),
                    expected_param_data.bezier().start_point_value());
          EXPECT_EQ(actual_param_data.bezier().end_point_value(),
                    expected_param_data.bezier().end_point_value());
          EXPECT_EQ(actual_param_data.bezier().control_point_value(),
                    expected_param_data.bezier().control_point_value());
          EXPECT_EQ(actual_param_data.bezier().control_point_relative_time(),
                    expected_param_data.bezier().control_point_relative_time());
          break;
        default:
          FAIL() << "Invalid animation type";
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(OneSubblock, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{8000}, {}, 0, 1, {1}, {}, 1, true},
                             {{8000}, {}, 0, 128, {128}, {}, 128, true},
                             {{8000}, {}, 0, 8000, {8000}, {}, 8000, true},
                         }));

INSTANTIATE_TEST_SUITE_P(
    TwoSubblocksConstantSubblockDurationNonzero, PartitionParameterBlocks,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000}, {}, 0, 3999, {3999}, {}, 3999, true},
        {{4000, 4000}, {}, 3950, 4050, {50, 50}, {}, 50, true},
        {{4000, 4000}, {}, 3950, 4025, {50, 25}, {}, 50, true},
    }));

INSTANTIATE_TEST_SUITE_P(
    TwoSubblocksConstantSubblockDuration0, PartitionParameterBlocks,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000, 4000}, {}, 3975, 4050, {25, 50}, {}, 0, true},
    }));

INSTANTIATE_TEST_SUITE_P(
    ManySubblocks, PartitionParameterBlocks,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{1, 2, 3, 10, 10, 10}, {}, 0, 35, {1, 2, 3, 10, 10, 9}, {}, 0, true},
        {{1, 2, 3, 10, 10, 10}, {}, 2, 35, {1, 3, 10, 10, 9}, {}, 0, true},
    }));

INSTANTIATE_TEST_SUITE_P(ErrorZeroDuration, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000}, {}, 0, 0, {}, {}, 0, false},
                         }));

INSTANTIATE_TEST_SUITE_P(ErrorNegativeDuration, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000}, {}, 10000, 0, {}, {}, 0, false},
                         }));

INSTANTIATE_TEST_SUITE_P(ErrorNotFullyCovered, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000}, {}, 4000, 8001, {}, {}, 0, false},
                         }));

INSTANTIATE_TEST_SUITE_P(Step, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000},
                              {CreateStepMixGainParameterData(10),
                               CreateStepMixGainParameterData(20)},
                              0,
                              3999,
                              {3999},
                              {CreateStepMixGainParameterData(10)},
                              3999,
                              true},
                             {{4000, 4000},
                              {CreateStepMixGainParameterData(10),
                               CreateStepMixGainParameterData(20)},
                              2000,
                              6000,
                              {2000, 2000},
                              {CreateStepMixGainParameterData(10),
                               CreateStepMixGainParameterData(20)},
                              2000,
                              true},
                         }));

INSTANTIATE_TEST_SUITE_P(Linear, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000},
                              {CreateLinearMixGainParameterData(0, 100),
                               CreateLinearMixGainParameterData(100, 1000)},
                              1000,
                              3000,
                              {2000},
                              {CreateLinearMixGainParameterData(25, 75)},
                              2000,
                              true},
                         }));

INSTANTIATE_TEST_SUITE_P(LinearTwoSubblocks, PartitionParameterBlocks,
                         testing::ValuesIn<PartitionParameterBlocksTestCase>({
                             {{4000, 4000},
                              {CreateLinearMixGainParameterData(0, 100),
                               CreateLinearMixGainParameterData(100, 1000)},
                              1000,
                              6000,
                              {3000, 2000},
                              {CreateLinearMixGainParameterData(25, 100),
                               CreateLinearMixGainParameterData(100, 550)},
                              3000,
                              true},
                         }));

INSTANTIATE_TEST_SUITE_P(
    BezierAligned, PartitionParameterBlocks,
    testing::ValuesIn<PartitionParameterBlocksTestCase>({
        {{4000},
         {CreateBezierMixGainParameterData(0, 100, 64, 100)},
         0,
         4000,
         {4000},
         {CreateBezierMixGainParameterData(0, 100, 64, 100)},
         4000,
         true},
    }));

TEST(PartitionParameterBlock, InvalidForUnknownOrMissingParameterData) {
  ParameterBlockObuMetadata full_parameter_block;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        start_timestamp: 0
        duration: 4000
        constant_subblock_duration: 4000
        subblocks {
          # Parameter data is missing.
        }
      )pb",
      &full_parameter_block);

  ParameterBlockObuMetadata unused_parameter_block;
  EXPECT_FALSE(ParameterBlockPartitioner::PartitionParameterBlock(
                   full_parameter_block, /*partitioned_start_time=*/0,
                   /*partitioned_end_time=*/4000, unused_parameter_block)
                   .ok());
}

void ExpectHasOneSubblockWithDMixPMode(
    const ParameterBlockObuMetadata& parameter_block_metadata,
    const iamf_tools_cli_proto::DMixPMode& expected_dmixp_mode) {
  EXPECT_EQ(parameter_block_metadata.subblocks().size(), 1);
  ASSERT_TRUE(
      parameter_block_metadata.subblocks(0).has_demixing_info_parameter_data());
  EXPECT_EQ(parameter_block_metadata.subblocks(0)
                .demixing_info_parameter_data()
                .dmixp_mode(),
            expected_dmixp_mode);
}

TEST(PartitionParameterBlock,
     IsEquivalentWhenSubblockBoundaryIsNotCrossedForDemixing) {
  ParameterBlockObuMetadata full_parameter_block;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        start_timestamp: 0
        duration: 12000
        constant_subblock_duration: 4000
        # t = [0, 4000).
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_1 } }
        # t = [4000, 8000).
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_3 } }
        # t = [8000, 12000).
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_2 } }
      )pb",
      &full_parameter_block);

  // OK if it spans the whole (semi-open) range.
  ParameterBlockObuMetadata partition_from_first_subblock;
  EXPECT_THAT(ParameterBlockPartitioner::PartitionParameterBlock(
                  full_parameter_block, /*partitioned_start_time=*/0,
                  /*partitioned_end_time=*/4000, partition_from_first_subblock),
              IsOk());
  ExpectHasOneSubblockWithDMixPMode(partition_from_first_subblock,
                                    iamf_tools_cli_proto::DMIXP_MODE_1);
  // OK if the new duration is shorter than the original subblock duration.
  ParameterBlockObuMetadata partition_from_third_subblock;
  EXPECT_THAT(ParameterBlockPartitioner::PartitionParameterBlock(
                  full_parameter_block, /*partitioned_start_time=*/9000,
                  /*partitioned_end_time=*/9001, partition_from_third_subblock),
              IsOk());
  ExpectHasOneSubblockWithDMixPMode(partition_from_third_subblock,
                                    iamf_tools_cli_proto::DMIXP_MODE_2);
}

TEST(PartitionParameterBlock, InvalidWhenSubblockBoundaryIsCrossedForDemixing) {
  ParameterBlockObuMetadata full_parameter_block;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        start_timestamp: 0
        duration: 12000
        constant_subblock_duration: 4000
        # t = [0, 4000).
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_1 } }
        # t = [4000, 8000).
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_3 } }
        # t = [8000, 12000).
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_2 } }
      )pb",
      &full_parameter_block);

  ParameterBlockObuMetadata unused_partitioned_parameter_block;
  EXPECT_FALSE(ParameterBlockPartitioner::PartitionParameterBlock(
                   full_parameter_block, /*partitioned_start_time=*/3950,
                   /*partitioned_end_time=*/4500,
                   unused_partitioned_parameter_block)
                   .ok());
  EXPECT_FALSE(ParameterBlockPartitioner::PartitionParameterBlock(
                   full_parameter_block, /*partitioned_start_time=*/3999,
                   /*partitioned_end_time=*/4001,
                   unused_partitioned_parameter_block)
                   .ok());
}

TEST(PartitionParameterBlock,
     IsEquivalentWhenSubblockBoundaryIsNotCrossedForReconGain) {
  const InternalTimestamp kStartDuration = 0;
  const InternalTimestamp kEndDuration = 4000;
  const InternalTimestamp kExpectedDuration = kEndDuration - kStartDuration;
  ParameterBlockObuMetadata full_parameter_block;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        start_timestamp: 0
        duration: 8000
        constant_subblock_duration: 8000
        subblocks {
          recon_gain_info_parameter_data {
            recon_gains_for_layer {}
            recon_gains_for_layer { recon_gain { key: 2 value: 200 } }
          }
        }
      )pb",
      &full_parameter_block);
  const size_t kNumLayers = 2;
  const size_t kNumReconGainsForSecondLayer = 1;
  const size_t kSecondLayerReconGainValueForKey2 = 200;

  ParameterBlockObuMetadata partitioned_parameter_block;
  EXPECT_THAT(ParameterBlockPartitioner::PartitionParameterBlock(
                  full_parameter_block, kStartDuration, kEndDuration,
                  partitioned_parameter_block),
              IsOk());

  EXPECT_EQ(partitioned_parameter_block.duration(), kExpectedDuration);
  EXPECT_EQ(partitioned_parameter_block.subblocks().size(), 1);
  ASSERT_TRUE(partitioned_parameter_block.subblocks(0)
                  .has_recon_gain_info_parameter_data());
  const auto& recon_gain_info_parameter_data =
      partitioned_parameter_block.subblocks(0).recon_gain_info_parameter_data();
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gains_for_layer().size(),
            kNumLayers);
  EXPECT_TRUE(recon_gain_info_parameter_data.recon_gains_for_layer(0)
                  .recon_gain()
                  .empty());
  const auto& second_layer_recon_gains =
      recon_gain_info_parameter_data.recon_gains_for_layer(1);
  EXPECT_EQ(second_layer_recon_gains.recon_gain().size(),
            kNumReconGainsForSecondLayer);
  EXPECT_EQ(second_layer_recon_gains.recon_gain().at(2),
            kSecondLayerReconGainValueForKey2);
}

TEST(PartitionParameterBlock,
     InvalidWhenSubblockBoundaryIsCrossedForReconGain) {
  ParameterBlockObuMetadata full_parameter_block;
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        start_timestamp: 0
        duration: 8000
        constant_subblock_duration: 4000
        # t = [0, 4000).
        subblocks {
          recon_gain_info_parameter_data {
            recon_gains_for_layer {}
            recon_gains_for_layer { recon_gain { key: 2 value: 200 } }
          }
        }
        # t = [4000, 8000).
        subblocks {
          recon_gain_info_parameter_data {
            recon_gains_for_layer {}
            recon_gains_for_layer { recon_gain { key: 2 value: 100 } }
          }
        }
      )pb",
      &full_parameter_block);

  ParameterBlockObuMetadata partitioned_parameter_block;
  EXPECT_FALSE(ParameterBlockPartitioner::PartitionParameterBlock(
                   full_parameter_block, /*partitioned_start_time=*/3999,
                   /*partitioned_end_time=*/4001, partitioned_parameter_block)
                   .ok());
}

struct FindConstantSubblockDurationTestCase {
  std::vector<uint32_t> input_subblock_durations;
  uint32_t expected_constant_subblock_duration;
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
