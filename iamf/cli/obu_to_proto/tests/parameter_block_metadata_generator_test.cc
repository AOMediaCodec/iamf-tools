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
#include "iamf/cli/obu_to_proto/parameter_block_metadata_generator.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/parameter_block.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kSubblockDuration = 99;
constexpr int16_t kStartPointValue = 100;
constexpr int16_t kEndPointValue = 200;
constexpr int16_t kControlPointValue = 300;
constexpr int16_t kControlPointRelativeTime = 68;

using enum MixGainParameterData::AnimationType;
using enum iamf_tools_cli_proto::AnimationType;

constexpr auto kStepMixGainParamData =
    MixGainParameterData{.animation_type = kAnimateStep,
                         .param_data = AnimationStepInt16{kStartPointValue}};

TEST(GenerateParameterSubblockMetadata,
     GetsSubblockMetadataForStepMixGainParameterSubblock) {
  constexpr ParameterSubblock kStepMixGainSubblock{
      .subblock_duration = kSubblockDuration,
      .param_data = kStepMixGainParamData};

  const auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kStepMixGainSubblock);
  ASSERT_THAT(subblock_metadata, IsOk());

  ASSERT_TRUE(subblock_metadata->has_mix_gain_parameter_data());
  EXPECT_EQ(subblock_metadata->mix_gain_parameter_data().animation_type(),
            ANIMATE_STEP);
  ASSERT_TRUE(
      subblock_metadata->mix_gain_parameter_data().param_data().has_step());
  EXPECT_EQ(subblock_metadata->mix_gain_parameter_data()
                .param_data()
                .step()
                .start_point_value(),
            kStartPointValue);
}

TEST(GenerateParameterSubblockMetadata,
     GetsSubblockMetadataForLinearMixGainParameterSubblock) {
  constexpr ParameterSubblock kLinearMixGainSubblock{
      .subblock_duration = kSubblockDuration,
      .param_data =
          MixGainParameterData{.animation_type = kAnimateLinear,
                               .param_data = AnimationLinearInt16{
                                   kStartPointValue, kEndPointValue}}};

  const auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kLinearMixGainSubblock);
  ASSERT_THAT(subblock_metadata, IsOk());

  ASSERT_TRUE(subblock_metadata->has_mix_gain_parameter_data());
  EXPECT_EQ(subblock_metadata->mix_gain_parameter_data().animation_type(),
            ANIMATE_LINEAR);
  ASSERT_TRUE(
      subblock_metadata->mix_gain_parameter_data().param_data().has_linear());
  const auto& linear_param_data =
      subblock_metadata->mix_gain_parameter_data().param_data().linear();
  EXPECT_EQ(linear_param_data.start_point_value(), kStartPointValue);
  EXPECT_EQ(linear_param_data.end_point_value(), kEndPointValue);
}

TEST(GenerateParameterSubblockMetadata,
     GetsSubblockMetadataForBezierMixGainParameterSubblock) {
  constexpr ParameterSubblock kBezierMixGainSubblock{
      .subblock_duration = kSubblockDuration,
      .param_data = MixGainParameterData{
          .animation_type = kAnimateBezier,
          .param_data = AnimationBezierInt16{kStartPointValue, kEndPointValue,
                                             kControlPointValue,
                                             kControlPointRelativeTime}}};

  const auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kBezierMixGainSubblock);
  ASSERT_THAT(subblock_metadata, IsOk());

  ASSERT_TRUE(subblock_metadata->has_mix_gain_parameter_data());
  EXPECT_EQ(subblock_metadata->mix_gain_parameter_data().animation_type(),
            ANIMATE_BEZIER);
  ASSERT_TRUE(
      subblock_metadata->mix_gain_parameter_data().param_data().has_bezier());
  const auto& bezier_param_data =
      subblock_metadata->mix_gain_parameter_data().param_data().bezier();
  EXPECT_EQ(bezier_param_data.start_point_value(), kStartPointValue);
  EXPECT_EQ(bezier_param_data.end_point_value(), kEndPointValue);
  EXPECT_EQ(bezier_param_data.control_point_value(), kControlPointValue);
  EXPECT_EQ(bezier_param_data.control_point_relative_time(),
            kControlPointRelativeTime);
}

TEST(GenerateParameterSubblockMetadata,
     ReturnsErrorForInconsistentAnimationType) {
  constexpr ParameterSubblock kInconsistentStepSubblock = {
      .subblock_duration = kSubblockDuration,
      .param_data = MixGainParameterData{.animation_type = kAnimateLinear,
                                         .param_data = AnimationStepInt16{}}};
  constexpr ParameterSubblock kInconsistentLinearSubblock = {
      .subblock_duration = kSubblockDuration,
      .param_data = MixGainParameterData{.animation_type = kAnimateStep,
                                         .param_data = AnimationLinearInt16{}}};
  constexpr ParameterSubblock kInconsistentBezierSubblock = {
      .subblock_duration = kSubblockDuration,
      .param_data = MixGainParameterData{.animation_type = kAnimateStep,
                                         .param_data = AnimationBezierInt16{}}};

  EXPECT_FALSE(
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kInconsistentStepSubblock)
          .ok());
  EXPECT_FALSE(
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kInconsistentLinearSubblock)
          .ok());
  EXPECT_FALSE(
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kInconsistentBezierSubblock)
          .ok());
}

TEST(GenerateParameterSubblockMetadata,
     GetsSubblockMetadataForDemixingSParameterSubblock) {
  const uint8_t kReserved = 99;
  const auto kExpectedDmixpMode = iamf_tools_cli_proto::DMIXP_MODE_1;
  DemixingInfoParameterData demixing_info_param_data;
  demixing_info_param_data.dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  demixing_info_param_data.reserved = kReserved;
  const ParameterSubblock subblock{.subblock_duration = kSubblockDuration,
                                   .param_data = demixing_info_param_data};

  const auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          subblock);
  ASSERT_THAT(subblock_metadata, IsOk());

  ASSERT_TRUE(subblock_metadata->has_demixing_info_parameter_data());
  EXPECT_EQ(subblock_metadata->demixing_info_parameter_data().dmixp_mode(),
            kExpectedDmixpMode);
  EXPECT_EQ(subblock_metadata->demixing_info_parameter_data().reserved(),
            kReserved);
}

TEST(GenerateParameterSubblockMetadata, GeneratesExtensionParameterSubblocks) {
  const std::vector<uint8_t> kParameterDataBytes = {0x01, 0x02, 0x03, 0x04,
                                                    0x05};
  constexpr absl::string_view kExpectedParameterData = "\x01\x02\x03\x04\x05";
  const DecodedUleb128 kParameterDataSize = kParameterDataBytes.size();

  const ParameterSubblock kExtensionSubblock{
      .subblock_duration = kSubblockDuration,
      .param_data =
          ExtensionParameterData{.parameter_data_size = kParameterDataSize,
                                 .parameter_data_bytes = kParameterDataBytes}};

  const auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kExtensionSubblock);
  ASSERT_THAT(subblock_metadata, IsOk());

  ASSERT_TRUE(subblock_metadata->has_parameter_data_extension());
  EXPECT_EQ(subblock_metadata->parameter_data_extension().parameter_data_size(),
            kParameterDataSize);
  EXPECT_THAT(
      subblock_metadata->parameter_data_extension().parameter_data_bytes(),
      kExpectedParameterData);
}

TEST(GenerateParameterSubblockMetadata, GenerateReconGainParameterSubblocks) {
  constexpr uint8_t kCenterReconGain = 100;
  constexpr uint8_t kRightReconGain = 200;
  constexpr size_t kExpectedNumLayers = 2;
  constexpr uint32_t kExpectedCenterReconGainLayer = 0;
  constexpr uint32_t kExpectedCenterReconGainIndex = 1;
  constexpr uint32_t kExpectedRightReconGainLayer = 1;
  constexpr uint32_t kExpectedRightReconGainIndex = 2;
  const ParameterSubblock kReconGainSubblock{
      .subblock_duration = kSubblockDuration,
      .param_data = ReconGainInfoParameterData{
          .recon_gain_elements = {
              {.recon_gain_flag = ReconGainElement::kReconGainFlagC,
               .recon_gain = {0, kCenterReconGain, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0}},
              {.recon_gain_flag = ReconGainElement::kReconGainFlagR,
               .recon_gain = {0, 0, kRightReconGain, 0, 0, 0, 0, 0, 0, 0, 0,
                              0}},
          }}};

  auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kReconGainSubblock);
  ASSERT_THAT(subblock_metadata, IsOk());
  ASSERT_TRUE(subblock_metadata->has_recon_gain_info_parameter_data());

  EXPECT_EQ(subblock_metadata->recon_gain_info_parameter_data()
                .recon_gains_for_layer_size(),
            kExpectedNumLayers);
  EXPECT_EQ(subblock_metadata->recon_gain_info_parameter_data()
                .recon_gains_for_layer(kExpectedCenterReconGainLayer)
                .recon_gain()
                .at(kExpectedCenterReconGainIndex),
            kCenterReconGain);
  EXPECT_EQ(subblock_metadata->recon_gain_info_parameter_data()
                .recon_gains_for_layer(kExpectedRightReconGainLayer)
                .recon_gain()
                .at(kExpectedRightReconGainIndex),
            kRightReconGain);
}

TEST(GenerateParameterSubblockMetadata, SetsDuration) {
  constexpr ParameterSubblock kSubblock{.subblock_duration = kSubblockDuration,
                                        .param_data = kStepMixGainParamData};

  const auto subblock_metadata =
      ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
          kSubblock);
  ASSERT_THAT(subblock_metadata, IsOk());

  EXPECT_EQ(subblock_metadata->subblock_duration(), kSubblockDuration);
}

}  // namespace
}  // namespace iamf_tools
