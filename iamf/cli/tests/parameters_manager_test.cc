/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/parameters_manager.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::NotNull;

using ::testing::Not;

constexpr DecodedUleb128 kCodecConfigId = 1450;
constexpr DecodedUleb128 kSampleRate = 16000;
constexpr DecodedUleb128 kAudioElementId = 157;
constexpr DecodedUleb128 kFirstSubstreamId = 0;
constexpr DecodedUleb128 kSecondSubstreamId = 1;
constexpr DecodedUleb128 kParameterId = 995;
constexpr DecodedUleb128 kSecondParameterId = 996;
constexpr DecodedUleb128 kDuration = 8;
constexpr InternalTimestamp kDurationAsInternalTimestamp = 8;

constexpr DemixingInfoParameterData::DMixPMode kDMixPMode =
    DemixingInfoParameterData::kDMixPMode3_n;

void AppendParameterBlock(
    DecodedUleb128 parameter_id, InternalTimestamp start_timestamp,
    const ParamDefinition& param_definition,
    std::vector<ParameterBlockWithData>& parameter_blocks) {
  auto obu = ParameterBlockObu::CreateMode0(ObuHeader(), param_definition);
  ASSERT_THAT(obu, NotNull());

  parameter_blocks.emplace_back(
      ParameterBlockWithData{std::move(obu), start_timestamp,
                             start_timestamp + kDurationAsInternalTimestamp});
}

void AddOneDemixingParameterBlock(
    const ParamDefinition& param_definition, InternalTimestamp start_timestamp,
    std::vector<ParameterBlockWithData>& parameter_blocks) {
  AppendParameterBlock(kParameterId, start_timestamp, param_definition,
                       parameter_blocks);
  auto demixing_info_param_data = std::make_unique<DemixingInfoParameterData>();
  demixing_info_param_data->dmixp_mode = kDMixPMode;
  ParameterBlockObu& parameter_block_obu = *parameter_blocks.back().obu;
  parameter_block_obu.subblocks_[0].param_data =
      std::move(demixing_info_param_data);
}

void AddOneReconGainParameterBlock(
    const ParamDefinition& param_definition, InternalTimestamp start_timestamp,
    std::vector<ParameterBlockWithData>& parameter_blocks) {
  AppendParameterBlock(kSecondParameterId, start_timestamp, param_definition,
                       parameter_blocks);

  auto recon_gain_info_parameter_data =
      std::make_unique<ReconGainInfoParameterData>();
  recon_gain_info_parameter_data->recon_gain_elements.emplace_back(
      ReconGainElement{
          .recon_gain_flag = DecodedUleb128(1),
          .recon_gain = {0},
      });
  ParameterBlockObu& parameter_block_obu = *parameter_blocks.back().obu;
  parameter_block_obu.subblocks_[0].param_data =
      std::move(recon_gain_info_parameter_data);
}

class ParametersManagerTest : public testing::Test {
 public:
  ParametersManagerTest() {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus_);
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kAudioElementId, kCodecConfigId, {kFirstSubstreamId},
        codec_config_obus_, audio_elements_);

    auto& audio_element_obu = audio_elements_.at(kAudioElementId).obu;
    AddDemixingParamDefinition(kParameterId, kSampleRate, kDuration,
                               audio_element_obu);

    AddOneDemixingParameterBlock(
        std::get<DemixingParamDefinition>(
            audio_element_obu.audio_element_params_[0].param_definition),
        /*start_timestamp=*/0, demixing_parameter_blocks_);
  }

 protected:
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  std::vector<ParameterBlockWithData> demixing_parameter_blocks_;
  std::vector<ParameterBlockWithData> recon_gain_parameter_blocks_;
};

TEST_F(ParametersManagerTest, CreateSucceeds) {
  auto parameters_manager = ParametersManager::Create(audio_elements_);
  EXPECT_THAT(parameters_manager, IsOkAndHolds(NotNull()));
}

TEST_F(ParametersManagerTest, CreateWithTwoDemixingParametersFails) {
  // Add one more demixing parameter definition, which is disallowed.
  AddDemixingParamDefinition(kParameterId, kSampleRate, kDuration,
                             audio_elements_.at(kAudioElementId).obu);

  auto parameters_manager = ParametersManager::Create(audio_elements_);
  EXPECT_THAT(parameters_manager, Not(IsOk()));
}

TEST_F(ParametersManagerTest, CreateWithReconGainParameterSucceeds) {
  // Remove existing param definitions added in the constructor of the
  // test fixture.
  audio_elements_.at(kAudioElementId).obu.audio_element_params_.clear();
  AddReconGainParamDefinition(kSecondParameterId, kSampleRate, kDuration,
                              audio_elements_.at(kAudioElementId).obu);
  AddOneReconGainParameterBlock(
      std::get<ReconGainParamDefinition>(audio_elements_.at(kAudioElementId)
                                             .obu.audio_element_params_[0]
                                             .param_definition),
      /*start_timestamp=*/0, recon_gain_parameter_blocks_);
  auto parameters_manager = ParametersManager::Create(audio_elements_);
  EXPECT_THAT(parameters_manager, IsOkAndHolds(NotNull()));
}

// Creates and unwraps the `ParametersManager` from the `StatusOr`, to keep
// tests more direct.
std::unique_ptr<ParametersManager> CreateAndUnwrapParametersManager(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements) {
  auto parameters_manager = ParametersManager::Create(audio_elements);
  EXPECT_THAT(parameters_manager, IsOkAndHolds(NotNull()));

  return parameters_manager.ok() ? *std::move(parameters_manager) : nullptr;
}

TEST_F(ParametersManagerTest, DemixingParamDefinitionIsAvailable) {
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());

  EXPECT_TRUE(
      parameters_manager->DemixingParamDefinitionAvailable(kAudioElementId));
}

TEST_F(ParametersManagerTest, GetDownMixingParametersSucceeds) {
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  DownMixingParams down_mixing_params;
  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // Validate the values correspond to `kDMixPMode3_n`.
  EXPECT_FLOAT_EQ(down_mixing_params.alpha, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.beta, 0.866);
  EXPECT_FLOAT_EQ(down_mixing_params.gamma, 0.866);
  EXPECT_FLOAT_EQ(down_mixing_params.delta, 0.866);
  EXPECT_EQ(down_mixing_params.w_idx_offset, 1);
  EXPECT_EQ(down_mixing_params.w_idx_used, 0);
  EXPECT_FLOAT_EQ(down_mixing_params.w, 0.0);
}

TEST_F(ParametersManagerTest, GetReconGainInfoParameterDataSucceeds) {
  AddReconGainParamDefinition(kSecondParameterId, kSampleRate, kDuration,
                              audio_elements_.at(kAudioElementId).obu);
  AddOneReconGainParameterBlock(
      std::get<ReconGainParamDefinition>(audio_elements_.at(kAudioElementId)
                                             .obu.audio_element_params_[1]
                                             .param_definition),
      /*start_timestamp=*/0, recon_gain_parameter_blocks_);
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddReconGainParameterBlock(
      &recon_gain_parameter_blocks_[0]);

  ReconGainInfoParameterData recon_gain_info_parameter_data;
  EXPECT_THAT(
      parameters_manager->GetReconGainInfoParameterData(
          kAudioElementId, /*num_layers=*/1, recon_gain_info_parameter_data),
      IsOk());

  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements.size(), 1);
  ASSERT_TRUE(
      recon_gain_info_parameter_data.recon_gain_elements[0].has_value());
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0]->recon_gain_flag,
      DecodedUleb128(1));
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0]->recon_gain[0], 0);
}

TEST_F(ParametersManagerTest,
       GetReconGainInfoParameterDataSucceedsWithNoParameterBlocks) {
  AddReconGainParamDefinition(kSecondParameterId, kSampleRate, kDuration,
                              audio_elements_.at(kAudioElementId).obu);
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());

  ReconGainInfoParameterData recon_gain_info_parameter_data;
  EXPECT_THAT(
      parameters_manager->GetReconGainInfoParameterData(
          kAudioElementId, /*num_layers=*/1, recon_gain_info_parameter_data),
      IsOk());

  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements.size(), 1);
  ASSERT_TRUE(
      recon_gain_info_parameter_data.recon_gain_elements[0].has_value());
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0]->recon_gain_flag,
      DecodedUleb128(0));
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0]->recon_gain[0],
      255);
}

TEST_F(ParametersManagerTest,
       GetReconGainInfoParameterDataSucceedsWithNoParamDefinition) {
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());

  ReconGainInfoParameterData recon_gain_info_parameter_data;
  EXPECT_THAT(
      parameters_manager->GetReconGainInfoParameterData(
          kAudioElementId, /*num_layers=*/1, recon_gain_info_parameter_data),
      IsOk());

  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements.size(), 1);
  ASSERT_TRUE(
      recon_gain_info_parameter_data.recon_gain_elements[0].has_value());
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0]->recon_gain_flag,
      DecodedUleb128(0));
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0]->recon_gain[0],
      255);
}

TEST_F(ParametersManagerTest, GetMultipleReconGainParametersSucceeds) {
  // Tests that multiple recon gain parameters are returned correctly when there
  // are multiple recon gain parameter blocks within the same substream, with
  // consecutive timestamps.
  AddReconGainParamDefinition(kSecondParameterId, kSampleRate, kDuration,
                              audio_elements_.at(kAudioElementId).obu);
  AddOneReconGainParameterBlock(
      std::get<ReconGainParamDefinition>(audio_elements_.at(kAudioElementId)
                                             .obu.audio_element_params_[1]
                                             .param_definition),
      /*start_timestamp=*/0, recon_gain_parameter_blocks_);
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddReconGainParameterBlock(
      &recon_gain_parameter_blocks_[0]);

  // First recon gain parameter block.
  ReconGainInfoParameterData recon_gain_parameter_data_0;
  EXPECT_THAT(
      parameters_manager->GetReconGainInfoParameterData(
          kAudioElementId, /*num_layers=*/1, recon_gain_parameter_data_0),
      IsOk());
  EXPECT_EQ(recon_gain_parameter_data_0.recon_gain_elements.size(), 1);
  ASSERT_TRUE(recon_gain_parameter_data_0.recon_gain_elements[0].has_value());
  EXPECT_EQ(recon_gain_parameter_data_0.recon_gain_elements[0]->recon_gain_flag,
            DecodedUleb128(1));
  EXPECT_EQ(recon_gain_parameter_data_0.recon_gain_elements[0]->recon_gain[0],
            0);

  EXPECT_THAT(parameters_manager->UpdateReconGainState(
                  kAudioElementId,
                  /*expected_next_timestamp=*/kDuration),
              IsOk());

  // Second recon gain parameter block.
  AddOneReconGainParameterBlock(
      std::get<ReconGainParamDefinition>(audio_elements_.at(kAudioElementId)
                                             .obu.audio_element_params_[1]
                                             .param_definition),
      /*start_timestamp=*/kDurationAsInternalTimestamp,
      recon_gain_parameter_blocks_);
  parameters_manager->AddReconGainParameterBlock(
      &recon_gain_parameter_blocks_[1]);
  ReconGainInfoParameterData recon_gain_parameter_data_1;
  EXPECT_THAT(
      parameters_manager->GetReconGainInfoParameterData(
          kAudioElementId, /*num_layers=*/1, recon_gain_parameter_data_1),
      IsOk());
  EXPECT_EQ(recon_gain_parameter_data_1.recon_gain_elements.size(), 1);
  ASSERT_TRUE(recon_gain_parameter_data_1.recon_gain_elements[0].has_value());
  EXPECT_EQ(recon_gain_parameter_data_1.recon_gain_elements[0]->recon_gain_flag,
            DecodedUleb128(1));
  EXPECT_EQ(recon_gain_parameter_data_1.recon_gain_elements[0]->recon_gain[0],
            0);
  // Updating should succeed a second time with the expected timestamp now
  // offset by the duration of the parameter block.
  EXPECT_THAT(parameters_manager->UpdateReconGainState(
                  kAudioElementId,
                  /*expected_next_timestamp=*/kDuration + kDuration),
              IsOk());
}

TEST_F(ParametersManagerTest,
       GetMultipleReconGainParametersFailsWithoutUpdatingState) {
  AddReconGainParamDefinition(kSecondParameterId, kSampleRate, kDuration,
                              audio_elements_.at(kAudioElementId).obu);
  AddOneReconGainParameterBlock(
      std::get<ReconGainParamDefinition>(audio_elements_.at(kAudioElementId)
                                             .obu.audio_element_params_[1]
                                             .param_definition),
      /*start_timestamp=*/0, recon_gain_parameter_blocks_);
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddReconGainParameterBlock(
      &recon_gain_parameter_blocks_[0]);

  // First recon gain parameter block.
  ReconGainInfoParameterData recon_gain_parameter_data_0;
  EXPECT_THAT(
      parameters_manager->GetReconGainInfoParameterData(
          kAudioElementId, /*num_layers=*/1, recon_gain_parameter_data_0),
      IsOk());

  // Second recon gain parameter block.
  AddOneReconGainParameterBlock(
      std::get<ReconGainParamDefinition>(audio_elements_.at(kAudioElementId)
                                             .obu.audio_element_params_[1]
                                             .param_definition),
      /*start_timestamp=*/kDurationAsInternalTimestamp,
      recon_gain_parameter_blocks_);
  parameters_manager->AddReconGainParameterBlock(
      &recon_gain_parameter_blocks_[1]);
  ReconGainInfoParameterData recon_gain_parameter_data_1;
  EXPECT_FALSE(parameters_manager
                   ->GetReconGainInfoParameterData(kAudioElementId,
                                                   /*num_layers=*/1,
                                                   recon_gain_parameter_data_1)
                   .ok());
}

TEST_F(ParametersManagerTest, ParameterBlocksRunOutReturnsDefault) {
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());

  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  DownMixingParams down_mixing_params;
  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  EXPECT_THAT(parameters_manager->UpdateDemixingState(
                  kAudioElementId,
                  /*expected_next_timestamp=*/kDurationAsInternalTimestamp),
              IsOk());

  // Get the parameters for the second time. Since there is only one
  // parameter block and is already used up the previous time, the function
  // will not find a parameter block and will return default values.
  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // Validate the values correspond to `kDMixPMode1` and `default_w = 10`,
  // which are the default set in `AddDemixingParamDefinition()`.
  EXPECT_FLOAT_EQ(down_mixing_params.alpha, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.beta, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.gamma, 0.707);
  EXPECT_FLOAT_EQ(down_mixing_params.delta, 0.707);
  EXPECT_EQ(down_mixing_params.w_idx_offset, -1);
  EXPECT_EQ(down_mixing_params.w_idx_used, 10);
  EXPECT_FLOAT_EQ(down_mixing_params.w, 0.5);

  // `UpdateDemixingState()` also succeeds with some arbitrary timestamp,
  // because technically there's nothing to update.
  const DecodedUleb128 kArbitraryTimestamp = 972;
  EXPECT_THAT(parameters_manager->UpdateDemixingState(
                  kAudioElementId,
                  /*expected_next_timestamp=*/kArbitraryTimestamp),
              IsOk());
}

TEST_F(ParametersManagerTest, ParameterIdNotFoundReturnsDefault) {
  // Modify the parameter definition of the audio element so it does not
  // correspond to any parameter blocks inside `parameter_blocks_`.
  std::get<DemixingParamDefinition>(audio_elements_.at(kAudioElementId)
                                        .obu.audio_element_params_[0]
                                        .param_definition)
      .parameter_id_ = kParameterId + 1;

  // Create the parameters manager and get down mixing parameters; default
  // values are returned because the parameter ID is different from those
  // in the `parameter_blocks_`.
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  DownMixingParams down_mixing_params;
  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // Validate the values correspond to `kDMixPMode1` and `default_w = 10`,
  // which are the default set in `AddDemixingParamDefinition()`.
  EXPECT_FLOAT_EQ(down_mixing_params.alpha, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.beta, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.gamma, 0.707);
  EXPECT_FLOAT_EQ(down_mixing_params.delta, 0.707);
  EXPECT_EQ(down_mixing_params.w_idx_offset, -1);
  EXPECT_EQ(down_mixing_params.w_idx_used, 10);
  EXPECT_FLOAT_EQ(down_mixing_params.w, 0.5);
}

TEST_F(ParametersManagerTest, GetDownMixingParametersTwiceDifferentW) {
  // Add another parameter block, so we can get down-mix parameters twice.
  AddOneDemixingParameterBlock(
      std::get<DemixingParamDefinition>(audio_elements_.at(kAudioElementId)
                                            .obu.audio_element_params_[0]
                                            .param_definition),
      /*start_timestamp=*/kDuration, demixing_parameter_blocks_);
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  // Get down-mix parameters for the first time.
  DownMixingParams down_mixing_params;
  ASSERT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());
  EXPECT_THAT(parameters_manager->UpdateDemixingState(
                  kAudioElementId,
                  /*expected_next_timestamp=*/kDuration),
              IsOk());

  // The first time `w_idx` is 0, and the corresponding `w` is 0.
  const double kWFirst = 0.0;
  const double kWSecond = 0.0179;
  EXPECT_FLOAT_EQ(down_mixing_params.w, kWFirst);

  // Add and get down-mix parameters for the second time.
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[1]);
  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // Validate the values correspond to `kDMixPMode3_n`. Since `w_idx` has
  // been updated to 1, `w` becomes 0.0179.
  EXPECT_FLOAT_EQ(down_mixing_params.alpha, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.beta, 0.866);
  EXPECT_FLOAT_EQ(down_mixing_params.gamma, 0.866);
  EXPECT_FLOAT_EQ(down_mixing_params.delta, 0.866);
  EXPECT_EQ(down_mixing_params.w_idx_offset, 1);
  EXPECT_EQ(down_mixing_params.w_idx_used, 1);

  // Updated `w`, different from the first time above.
  EXPECT_FLOAT_EQ(down_mixing_params.w, kWSecond);
}

TEST_F(ParametersManagerTest, GetDownMixingParametersTwiceWithoutUpdateSameW) {
  // Add another parameter block, so it is possible to get down-mix parameters
  // twice.
  AddOneDemixingParameterBlock(
      std::get<DemixingParamDefinition>(audio_elements_.at(kAudioElementId)
                                            .obu.audio_element_params_[0]
                                            .param_definition),
      /*start_timestamp=*/kDuration, demixing_parameter_blocks_);

  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  // Get down-mix parameters twice without calling
  // `AddDemixingParameterBlock()` and `UpdateDemixngState()`; the same
  // down-mix parameters will be returned.
  DownMixingParams down_mixing_params;
  ASSERT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // The first time `w_idx` is 0, and the corresponding `w` is 0.
  EXPECT_EQ(down_mixing_params.w_idx_used, 0);
  EXPECT_FLOAT_EQ(down_mixing_params.w, 0.0);

  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // Validate the values correspond to `kDMixPMode3_n`. Since `w_idx` has
  // NOT been updated, `w` remains 0.0.
  EXPECT_FLOAT_EQ(down_mixing_params.alpha, 1.0);
  EXPECT_FLOAT_EQ(down_mixing_params.beta, 0.866);
  EXPECT_FLOAT_EQ(down_mixing_params.gamma, 0.866);
  EXPECT_FLOAT_EQ(down_mixing_params.delta, 0.866);
  EXPECT_EQ(down_mixing_params.w_idx_offset, 1);
  EXPECT_EQ(down_mixing_params.w_idx_used, 0);
  EXPECT_FLOAT_EQ(down_mixing_params.w, 0.0);
}

TEST_F(ParametersManagerTest,
       TwoAudioElementGettingParameterBlocksWithDifferentTimestampsFails) {
  // Add another parameter block, so we can get down-mix parameters twice.
  AddOneDemixingParameterBlock(
      std::get<DemixingParamDefinition>(audio_elements_.at(kAudioElementId)
                                            .obu.audio_element_params_[0]
                                            .param_definition),
      /*start_timestamp=*/kDuration, demixing_parameter_blocks_);

  // Add a second audio element sharing the same demixing parameter.
  constexpr DecodedUleb128 kAudioElementId2 = kAudioElementId + 1;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId2, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus_, audio_elements_);
  auto& second_audio_element_obu = audio_elements_.at(kAudioElementId2).obu;
  AddDemixingParamDefinition(kParameterId, kSampleRate, kDuration,
                             second_audio_element_obu);

  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  // Get down-mix parameters for the first audio element corresponding to the
  // first frame; the `w` value is 0.
  const double kWFirst = 0.0;
  const double kWSecond = 0.0179;
  DownMixingParams down_mixing_params;
  ASSERT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());
  EXPECT_THAT(parameters_manager->UpdateDemixingState(
                  kAudioElementId,
                  /*expected_next_timestamp=*/kDuration),
              IsOk());
  EXPECT_FLOAT_EQ(down_mixing_params.w, kWFirst);

  // Add the parameter block for the first audio element corresponding to the
  // second frame.
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[1]);
  ASSERT_THAT(parameters_manager->GetDownMixingParameters(kAudioElementId,
                                                          down_mixing_params),
              IsOk());
  EXPECT_FLOAT_EQ(down_mixing_params.w, kWSecond);

  // Get down-mix parameters for the second audio element. The second audio
  // element shares the same parameter ID, but is still expecting the
  // parameter block for the first frame (while the manager is already
  // holding the parameter block for the second frame). So the getter fails.
  EXPECT_FALSE(
      parameters_manager
          ->GetDownMixingParameters(kAudioElementId2, down_mixing_params)
          .ok());
}

TEST_F(ParametersManagerTest, DemixingParamDefinitionIsNotAvailableForWrongId) {
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  const DecodedUleb128 kWrongAudioElementId = kAudioElementId + 1;
  EXPECT_FALSE(parameters_manager->DemixingParamDefinitionAvailable(
      kWrongAudioElementId));

  // However, `GetDownMixingParameters()` still succeeds.
  DownMixingParams down_mixing_params;
  EXPECT_THAT(parameters_manager->GetDownMixingParameters(kWrongAudioElementId,
                                                          down_mixing_params),
              IsOk());

  // `UpdateDemixingState()` also succeeds.
  EXPECT_THAT(parameters_manager->UpdateDemixingState(
                  kWrongAudioElementId,
                  /*expected_next_timestamp=*/kDuration),
              IsOk());
}

TEST_F(ParametersManagerTest, UpdateFailsWithWrongTimestamps) {
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  // The second frame starts with timestamp = 8, so updating with a different
  // timestamp fails.
  constexpr InternalTimestamp kWrongNextTimestamp = 17;
  EXPECT_FALSE(parameters_manager
                   ->UpdateDemixingState(kAudioElementId, kWrongNextTimestamp)
                   .ok());
}

TEST_F(ParametersManagerTest, UpdateNotValidatingWhenParameterIdNotFound) {
  // Modify the parameter definition of the audio element so it does not
  // correspond to any parameter blocks inside `parameter_blocks_`.
  std::get<DemixingParamDefinition>(audio_elements_.at(kAudioElementId)
                                        .obu.audio_element_params_[0]
                                        .param_definition)
      .parameter_id_ = kParameterId + 1;

  // Create the parameters manager and get down mixing parameters; default
  // values are returned because the parameter ID is not found.
  auto parameters_manager = CreateAndUnwrapParametersManager(audio_elements_);
  ASSERT_THAT(parameters_manager, NotNull());
  parameters_manager->AddDemixingParameterBlock(&demixing_parameter_blocks_[0]);

  // `UpdateDemixingState()` succeeds with any timestamp passed in,
  // because no validation is performed.
  for (const InternalTimestamp timestamp : {0, 8, -200, 61, 4772}) {
    EXPECT_THAT(
        parameters_manager->UpdateDemixingState(kAudioElementId, timestamp),
        IsOk());
  }
}

}  // namespace
}  // namespace iamf_tools
