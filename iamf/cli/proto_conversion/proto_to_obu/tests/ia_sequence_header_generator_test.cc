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
#include "iamf/cli/proto_conversion/proto_to_obu/ia_sequence_header_generator.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using iamf_tools_cli_proto::IASequenceHeaderObuMetadata;

IASequenceHeaderObuMetadata GetSimpleProfileMetadata() {
  IASequenceHeaderObuMetadata metadata;
  metadata.set_primary_profile(iamf_tools_cli_proto::PROFILE_VERSION_SIMPLE);
  metadata.set_additional_profile(iamf_tools_cli_proto::PROFILE_VERSION_SIMPLE);
  return metadata;
}

TEST(Generate, GeneratesSimpleProfile) {
  auto metadata = GetSimpleProfileMetadata();

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(output_obu->GetAdditionalProfile(),
            ProfileVersion::kIamfSimpleProfile);
}

TEST(Generate, GeneratesValidObu) {
  auto metadata = GetSimpleProfileMetadata();

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_THAT(output_obu->Validate(), IsOk());
}

TEST(Generate, GeneratesValidObuWithDefaultIaCode) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.clear_ia_code();

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_THAT(output_obu->Validate(), IsOk());
}

TEST(Generate, SetsObuRedundantCopy) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.mutable_obu_header()->set_obu_redundant_copy(true);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->header_.obu_redundant_copy, true);
}

TEST(Generate, SetsExtensionHeader) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.mutable_obu_header()->set_obu_extension_flag(true);
  metadata.mutable_obu_header()->set_extension_header_bytes("extra");

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->header_.obu_extension_flag, true);
  EXPECT_EQ(output_obu->header_.extension_header_size, 5);
  EXPECT_EQ(output_obu->header_.extension_header_bytes,
            std::vector<uint8_t>({'e', 'x', 't', 'r', 'a'}));
}

TEST(Generate, IgnoredDeprecatedInvalidIaCode) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_ia_code(0x12345678);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);

  // The invalid IA code is automatically fixed.
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());
  EXPECT_THAT(output_obu->Validate(), IsOk());
}

TEST(Generate, SetsPrimaryProfileBase) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_primary_profile(iamf_tools_cli_proto::PROFILE_VERSION_BASE);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->GetPrimaryProfile(), ProfileVersion::kIamfBaseProfile);
}

TEST(Generate, SetsPrimaryProfileBaseEnhanced) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_primary_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE_ENHANCED);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->GetPrimaryProfile(),
            ProfileVersion::kIamfBaseEnhancedProfile);
}

TEST(Generate, InvalidWhenPrimaryProfileReserved255) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_primary_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_RESERVED_255);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);

  EXPECT_FALSE(generator.Generate(output_obu).ok());
}

TEST(Generate, SetsAdditionalProfileBase) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_additional_profile(iamf_tools_cli_proto::PROFILE_VERSION_BASE);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->GetAdditionalProfile(),
            ProfileVersion::kIamfBaseProfile);
}

TEST(Generate, SetsAdditionalProfileBaseEnhanced) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_additional_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE_ENHANCED);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());
  ASSERT_TRUE(output_obu.has_value());

  EXPECT_EQ(output_obu->GetAdditionalProfile(),
            ProfileVersion::kIamfBaseEnhancedProfile);
}

TEST(Generate, SetsAdditionalProfileReserved255) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_additional_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_RESERVED_255);

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata);

  EXPECT_FALSE(generator.Generate(output_obu).ok());
}

TEST(Generate, InvalidWhenEnumIsInvalid) {
  auto metadata = GetSimpleProfileMetadata();
  metadata.set_additional_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_INVALID);

  std::optional<IASequenceHeaderObu> unused_output_obu;
  const IaSequenceHeaderGenerator generator(metadata);
  EXPECT_FALSE(generator.Generate(unused_output_obu).ok());
}

TEST(Generate, NoIaSequenceHeaderObus) {
  IASequenceHeaderObuMetadata metadata_with_no_obus;

  std::optional<IASequenceHeaderObu> output_obu;
  const IaSequenceHeaderGenerator generator(metadata_with_no_obus);
  EXPECT_THAT(generator.Generate(output_obu), IsOk());

  EXPECT_FALSE(output_obu.has_value());
}

}  // namespace
}  // namespace iamf_tools
