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
#include "iamf/cli/proto_conversion/proto_utils.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(CopyDemixingInfoParameterData, Basic) {
  iamf_tools_cli_proto::DemixingInfoParameterData
      demixing_info_parameter_data_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        dmixp_mode: DMIXP_MODE_3 reserved: 0
      )pb",
      &demixing_info_parameter_data_metadata));
  DemixingInfoParameterData demixing_info_parameter_data;
  EXPECT_THAT(
      CopyDemixingInfoParameterData(demixing_info_parameter_data_metadata,
                                    demixing_info_parameter_data),
      IsOk());

  EXPECT_EQ(demixing_info_parameter_data.dmixp_mode,
            DemixingInfoParameterData::kDMixPMode3);
  EXPECT_EQ(demixing_info_parameter_data.reserved, 0);
}

TEST(CopyDMixPMode, CopiesValue) {
  constexpr auto kTestValue = DemixingInfoParameterData::kDMixPMode3;
  constexpr auto kExpectedProtoValue = iamf_tools_cli_proto::DMIXP_MODE_3;

  iamf_tools_cli_proto::DMixPMode output_dmixp_mode;
  EXPECT_THAT(CopyDMixPMode(kTestValue, output_dmixp_mode), IsOk());

  EXPECT_EQ(output_dmixp_mode, kExpectedProtoValue);
}

TEST(GetHeaderFromMetadata, Default) {
  iamf_tools_cli_proto::ObuHeaderMetadata obu_header_metadata;
  ObuHeader header_ = GetHeaderFromMetadata(obu_header_metadata);
  // `ObuHeader` is initialized with reasonable default values for typical use
  // cases.
  EXPECT_EQ(header_.obu_redundant_copy, false);
  EXPECT_EQ(header_.obu_trimming_status_flag, false);
  EXPECT_EQ(header_.GetExtensionHeaderFlag(), false);
}

TEST(GetHeaderFromMetadata, MostValuesModified) {
  iamf_tools_cli_proto::ObuHeaderMetadata obu_header_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        obu_redundant_copy: true
        obu_trimming_status_flag: true
        obu_extension_flag: true
        num_samples_to_trim_at_end: 1
        num_samples_to_trim_at_start: 2
        extension_header_bytes: "extra"
      )pb",
      &obu_header_metadata));
  ObuHeader header_ = GetHeaderFromMetadata(obu_header_metadata);

  EXPECT_EQ(header_.obu_redundant_copy, true);
  EXPECT_EQ(header_.obu_trimming_status_flag, true);
  EXPECT_EQ(header_.GetExtensionHeaderFlag(), true);
  EXPECT_EQ(header_.num_samples_to_trim_at_end, 1);
  EXPECT_EQ(header_.num_samples_to_trim_at_start, 2);
  EXPECT_EQ(header_.GetExtensionHeaderSize(), 5);
  EXPECT_EQ(header_.extension_header_bytes,
            (std::vector<uint8_t>{'e', 'x', 't', 'r', 'a'}));
}

TEST(GetHeaderFromMetadata, IgnoresDeprecatedExtensionHeaderSize) {
  iamf_tools_cli_proto::ObuHeaderMetadata obu_header_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        obu_extension_flag: true extension_header_bytes: "extra"
      )pb",
      &obu_header_metadata));
  constexpr auto kInconsistentExtensionHeaderSize = 100;
  constexpr auto kExpectedExtensionHeaderSize = 5;
  // Set the deprecated `extension_header_size` to an unexpected value.
  obu_header_metadata.set_extension_header_size(
      kInconsistentExtensionHeaderSize);
  ObuHeader header_ = GetHeaderFromMetadata(obu_header_metadata);

  // Regardless, the true size is inferred from the size of the
  // `extension_header_bytes`.
  EXPECT_EQ(header_.GetExtensionHeaderSize(), kExpectedExtensionHeaderSize);
  ASSERT_TRUE(header_.extension_header_bytes.has_value());
  EXPECT_EQ(header_.extension_header_bytes->size(),
            kExpectedExtensionHeaderSize);
}

TEST(CreateLebGenerator, EquivalentGenerateLebMinimumFactories) {
  // Create user config to set GenerationMode to kMinimum.
  iamf_tools_cli_proto::Leb128Generator proto_user_config;
  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_MINIMUM
                )pb",
                &proto_user_config),
            true);
  // Create three generators, all should have generation mode kMinimum.
  auto user_metadata_generator = CreateLebGenerator(proto_user_config);
  auto default_argument_generator = LebGenerator::Create();
  auto argument_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kMinimum);

  ASSERT_NE(default_argument_generator, nullptr);
  ASSERT_NE(argument_generator, nullptr);
  ASSERT_NE(user_metadata_generator, nullptr);

  EXPECT_EQ(*argument_generator, *user_metadata_generator);
  EXPECT_EQ(*argument_generator, *default_argument_generator);
}

TEST(CreateLebGenerator, ConfigProtoDefaultsToGenerateLebMinimum) {
  auto user_metadata_generator =
      CreateLebGenerator(/*leb_generator_metadata=*/{});
  auto minimum_size_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kMinimum);

  ASSERT_NE(user_metadata_generator, nullptr);
  EXPECT_EQ(*user_metadata_generator, *minimum_size_generator);
}

TEST(CreateLebGenerator, EquivalentGenerateLebFixedSizeFactories) {
  // Create a user config to set GenerationMode to kFixedSize, size of 5.
  iamf_tools_cli_proto::Leb128Generator proto_user_config;
  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_FIXED_SIZE fixed_size: 5
                )pb",
                &proto_user_config),
            true);
  // Create one with the user config, one with the explicit arguments.
  auto user_metadata_generator = CreateLebGenerator(proto_user_config);
  auto argument_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);

  ASSERT_NE(user_metadata_generator, nullptr);
  ASSERT_NE(argument_generator, nullptr);
  EXPECT_EQ(*user_metadata_generator, *user_metadata_generator);
}

TEST(CreateLebGenerator, ValidatesUserMetadataWhenFixedSizeIsTooSmall) {
  iamf_tools_cli_proto::Leb128Generator proto_user_config;

  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_FIXED_SIZE fixed_size: 0
                )pb",
                &proto_user_config),
            true);

  EXPECT_EQ(CreateLebGenerator(proto_user_config), nullptr);
}

TEST(CreateLebGenerator, ValidatesUserMetadataWhenFixedSizeIsTooLarge) {
  iamf_tools_cli_proto::Leb128Generator proto_user_config;
  // 9 is larger than the max allowed size of 8 (kMaxLeb128Size).
  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_FIXED_SIZE fixed_size: 9
                )pb",
                &proto_user_config),
            true);

  EXPECT_EQ(CreateLebGenerator(proto_user_config), nullptr);
}

TEST(CreateLebGenerator, ValidatesUserMetadataWhenModeIsInvalid) {
  iamf_tools_cli_proto::Leb128Generator proto_user_config;

  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_INVALID
                )pb",
                &proto_user_config),
            true);
  EXPECT_EQ(CreateLebGenerator(proto_user_config), nullptr);
}

TEST(CopyParamDefinition, IgnoredDeprecatedNumSubblocks) {
  iamf_tools_cli_proto::ParamDefinition param_definition_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_rate: 1
        param_definition_mode: false
        reserved: 0
        duration: 1000
        constant_subblock_duration: 0
        subblock_durations: [ 700, 300 ]
      )pb",
      &param_definition_proto));
  constexpr auto kInconsistentNumSubblocks = 10;
  param_definition_proto.set_num_subblocks(kInconsistentNumSubblocks);

  MixGainParamDefinition mix_gain_param_definition;
  EXPECT_THAT(
      CopyParamDefinition(param_definition_proto, mix_gain_param_definition),
      IsOk());

  // Despite signalling an inconsistent number of subblocks, the deprecated
  // field is ignored.
  EXPECT_EQ(mix_gain_param_definition.GetNumSubblocks(), 2);
}

}  // namespace
}  // namespace iamf_tools
