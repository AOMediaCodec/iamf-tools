/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_metadata_converter.h"

#include <cstdint>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/parameter_block_generator.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::iamf_tools_cli_proto::DMIXP_MODE_3;
using ::iamf_tools_cli_proto::DMIXP_MODE_INVALID;
using ::iamf_tools_cli_proto::ParameterBlockObuMetadata;

TEST(ConvertToParameterBlockMetadata, ConvertsTopLevelFieldsAndObuHeader) {
  ParameterBlockObuMetadata proto_metadata;
  proto_metadata.set_parameter_id(100);
  proto_metadata.set_duration(8);
  proto_metadata.set_constant_subblock_duration(4);
  proto_metadata.set_start_timestamp(16);
  proto_metadata.mutable_obu_header()->set_obu_redundant_copy(true);
  proto_metadata.mutable_obu_header()->set_obu_extension_flag(true);
  proto_metadata.mutable_obu_header()->set_extension_header_bytes("extra");

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->parameter_id, 100);
  EXPECT_EQ(result->duration, 8);
  EXPECT_EQ(result->constant_subblock_duration, 4);
  EXPECT_EQ(result->start_timestamp, 16);
  EXPECT_TRUE(result->obu_header.obu_redundant_copy);
  EXPECT_TRUE(result->obu_header.GetExtensionHeaderFlag());
  EXPECT_EQ(result->obu_header.extension_header_bytes,
            (std::vector<uint8_t>{'e', 'x', 't', 'r', 'a'}));
  EXPECT_TRUE(result->subblocks.empty());
}

TEST(ConvertToParameterBlockMetadata, ConvertsStepMixGainSubblock) {
  ParameterBlockObuMetadata proto_metadata;
  auto* subblock = proto_metadata.add_subblocks();
  subblock->set_subblock_duration(8);
  subblock->mutable_mix_gain_parameter_data()
      ->mutable_param_data()
      ->mutable_step()
      ->set_start_point_value(-768);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  EXPECT_EQ(result->subblocks[0].subblock_duration, 8);
  ASSERT_TRUE(std::holds_alternative<MixGainParameterData>(
      result->subblocks[0].parameter_data));
  EXPECT_EQ(std::get<MixGainParameterData>(result->subblocks[0].parameter_data)
                .param_data,
            AnimatedParameterData<int16_t>::MakeStep(-768));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenStepMixGainStartPointOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  proto_metadata.add_subblocks()
      ->mutable_mix_gain_parameter_data()
      ->mutable_param_data()
      ->mutable_step()
      ->set_start_point_value(40000);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata, ConvertsLinearMixGainSubblock) {
  ParameterBlockObuMetadata proto_metadata;
  auto* subblock = proto_metadata.add_subblocks();
  subblock->set_subblock_duration(10);
  auto* linear = subblock->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_linear();
  linear->set_start_point_value(-256);
  linear->set_end_point_value(512);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  EXPECT_EQ(result->subblocks[0].subblock_duration, 10);
  ASSERT_TRUE(std::holds_alternative<MixGainParameterData>(
      result->subblocks[0].parameter_data));
  EXPECT_EQ(std::get<MixGainParameterData>(result->subblocks[0].parameter_data)
                .param_data,
            AnimatedParameterData<int16_t>::MakeLinear(-256, 512));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenLinearMixGainStartPointOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* linear = proto_metadata.add_subblocks()
                     ->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_linear();
  linear->set_start_point_value(-40000);
  linear->set_end_point_value(0);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenLinearMixGainEndPointOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* linear = proto_metadata.add_subblocks()
                     ->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_linear();
  linear->set_start_point_value(0);
  linear->set_end_point_value(40000);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata, ConvertsBezierMixGainSubblock) {
  ParameterBlockObuMetadata proto_metadata;
  auto* subblock = proto_metadata.add_subblocks();
  subblock->set_subblock_duration(12);
  auto* bezier = subblock->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_bezier();
  bezier->set_start_point_value(-512);
  bezier->set_end_point_value(1024);
  bezier->set_control_point_value(256);
  bezier->set_control_point_relative_time(128);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  EXPECT_EQ(result->subblocks[0].subblock_duration, 12);
  ASSERT_TRUE(std::holds_alternative<MixGainParameterData>(
      result->subblocks[0].parameter_data));
  EXPECT_EQ(std::get<MixGainParameterData>(result->subblocks[0].parameter_data)
                .param_data,
            AnimatedParameterData<int16_t>::MakeBezier(-512, 1024, 256, 128));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenBezierMixGainStartPointOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* bezier = proto_metadata.add_subblocks()
                     ->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_bezier();
  bezier->set_start_point_value(40000);
  bezier->set_end_point_value(0);
  bezier->set_control_point_value(0);
  bezier->set_control_point_relative_time(128);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenBezierMixGainEndPointOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* bezier = proto_metadata.add_subblocks()
                     ->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_bezier();
  bezier->set_start_point_value(0);
  bezier->set_end_point_value(-40000);
  bezier->set_control_point_value(0);
  bezier->set_control_point_relative_time(128);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenBezierMixGainControlPointOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* bezier = proto_metadata.add_subblocks()
                     ->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_bezier();
  bezier->set_start_point_value(0);
  bezier->set_end_point_value(0);
  bezier->set_control_point_value(40000);
  bezier->set_control_point_relative_time(128);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenBezierMixGainRelativeTimeOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* bezier = proto_metadata.add_subblocks()
                     ->mutable_mix_gain_parameter_data()
                     ->mutable_param_data()
                     ->mutable_bezier();
  bezier->set_start_point_value(0);
  bezier->set_end_point_value(0);
  bezier->set_control_point_value(0);
  bezier->set_control_point_relative_time(256);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenMixGainParameterDataCaseNotSet) {
  ParameterBlockObuMetadata proto_metadata;
  proto_metadata.add_subblocks()->mutable_mix_gain_parameter_data();

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata, ConvertsDemixingInfoSubblock) {
  ParameterBlockObuMetadata proto_metadata;
  auto* demixing =
      proto_metadata.add_subblocks()->mutable_demixing_info_parameter_data();
  demixing->set_dmixp_mode(DMIXP_MODE_3);
  demixing->set_reserved(5);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  ASSERT_TRUE(std::holds_alternative<DemixingInfoParameterData>(
      result->subblocks[0].parameter_data));
  EXPECT_EQ(
      std::get<DemixingInfoParameterData>(result->subblocks[0].parameter_data),
      DemixingInfoParameterData(DemixingInfoParameterData::kDMixPMode3, 5));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsNotFoundWhenDemixingModeIsInvalid) {
  ParameterBlockObuMetadata proto_metadata;
  proto_metadata.add_subblocks()
      ->mutable_demixing_info_parameter_data()
      ->set_dmixp_mode(DMIXP_MODE_INVALID);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kNotFound));
}

TEST(ConvertToParameterBlockMetadata, ConvertsReconGainInfoSubblock) {
  ParameterBlockObuMetadata proto_metadata;
  auto* recon_gain_info =
      proto_metadata.add_subblocks()->mutable_recon_gain_info_parameter_data();
  auto* layer_0 = recon_gain_info->add_recon_gains_for_layer();
  (*layer_0->mutable_recon_gain())[0] = 128;
  (*layer_0->mutable_recon_gain())[2] = 200;
  recon_gain_info->add_recon_gains_for_layer();

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  ASSERT_TRUE(std::holds_alternative<ReconGainInfoParameterData>(
      result->subblocks[0].parameter_data));
  const auto& recon_gain_data =
      std::get<ReconGainInfoParameterData>(result->subblocks[0].parameter_data);
  ASSERT_EQ(recon_gain_data.recon_gain_elements.size(), 2);
  ASSERT_TRUE(recon_gain_data.recon_gain_elements[0].has_value());
  EXPECT_EQ(recon_gain_data.recon_gain_elements[0]->recon_gain_flag, 5);
  EXPECT_EQ(recon_gain_data.recon_gain_elements[0]->recon_gain[0], 128);
  EXPECT_EQ(recon_gain_data.recon_gain_elements[0]->recon_gain[2], 200);
  ASSERT_TRUE(recon_gain_data.recon_gain_elements[1].has_value());
  EXPECT_EQ(recon_gain_data.recon_gain_elements[1]->recon_gain_flag, 0);
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenReconGainBitPositionOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* layer_0 = proto_metadata.add_subblocks()
                      ->mutable_recon_gain_info_parameter_data()
                      ->add_recon_gains_for_layer();
  (*layer_0->mutable_recon_gain())[12] = 100;

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     ReturnsInvalidArgumentWhenReconGainValueOutOfRange) {
  ParameterBlockObuMetadata proto_metadata;
  auto* layer_0 = proto_metadata.add_subblocks()
                      ->mutable_recon_gain_info_parameter_data()
                      ->add_recon_gains_for_layer();
  (*layer_0->mutable_recon_gain())[0] = 256;

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertToParameterBlockMetadata,
     LeavesParameterDataMonostateWhenSubblockDataNotSet) {
  ParameterBlockObuMetadata proto_metadata;
  proto_metadata.add_subblocks()->set_subblock_duration(8);

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  EXPECT_EQ(result->subblocks[0].subblock_duration, 8);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(
      result->subblocks[0].parameter_data));
}

TEST(ConvertToParameterBlockMetadata,
     LeavesParameterDataMonostateForParameterDataExtensionSubblock) {
  ParameterBlockObuMetadata proto_metadata;
  proto_metadata.add_subblocks()
      ->mutable_parameter_data_extension()
      ->set_parameter_data_bytes("ext");

  const auto result = ConvertToParameterBlockMetadata(proto_metadata);

  ASSERT_THAT(result, IsOk());
  ASSERT_EQ(result->subblocks.size(), 1);
  EXPECT_TRUE(std::holds_alternative<std::monostate>(
      result->subblocks[0].parameter_data));
}

}  // namespace
}  // namespace iamf_tools
