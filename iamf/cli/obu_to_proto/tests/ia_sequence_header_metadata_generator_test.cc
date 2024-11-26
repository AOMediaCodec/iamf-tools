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
#include "iamf/cli/obu_to_proto/ia_sequence_header_metadata_generator.h"

#include <optional>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_to_obu/ia_sequence_header_generator.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(IaSequenceHeaderMetadataGeneratorGenerate, SetsIaCode) {
  const IASequenceHeaderObu kIaSequenceHeaderObu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);

  const auto result =
      IaSequenceHeaderMetadataGenerator::Generate(kIaSequenceHeaderObu);
  ASSERT_THAT(result, IsOk());

  EXPECT_EQ(result->ia_code(), IASequenceHeaderObu::kIaCode);
}

TEST(IaSequenceHeaderMetadataGeneratorGenerate, SetsPrimaryProfile) {
  const IASequenceHeaderObu kSimpleProfileIaSequenceHeaderObu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);

  const auto result = IaSequenceHeaderMetadataGenerator::Generate(
      kSimpleProfileIaSequenceHeaderObu);
  ASSERT_THAT(result, IsOk());

  EXPECT_EQ(result->primary_profile(),
            iamf_tools_cli_proto::ProfileVersion::PROFILE_VERSION_SIMPLE);
}

TEST(IaSequenceHeaderMetadataGeneratorGenerate, SetsAdditionalProfile) {
  const IASequenceHeaderObu kBaseEnhancedProfileIiaSequenceHeaderObu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseEnhancedProfile);

  const auto result = IaSequenceHeaderMetadataGenerator::Generate(
      kBaseEnhancedProfileIiaSequenceHeaderObu);
  ASSERT_THAT(result, IsOk());

  EXPECT_EQ(
      result->additional_profile(),
      iamf_tools_cli_proto::ProfileVersion::PROFILE_VERSION_BASE_ENHANCED);
}

TEST(IaSequenceHeaderMetadataGeneratorGenerate,
     InvalidWhenPrimaryProfileIsUnknown) {
  const IASequenceHeaderObu kBaseEnhancedProfileIiaSequenceHeaderObu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfReserved255Profile,
      ProfileVersion::kIamfSimpleProfile);

  EXPECT_FALSE(IaSequenceHeaderMetadataGenerator::Generate(
                   kBaseEnhancedProfileIiaSequenceHeaderObu)
                   .ok());
}

TEST(IaSequenceHeaderMetadataGeneratorGenerate,
     InvalidWhenAdditionalProfileIsUnknown) {
  const IASequenceHeaderObu kBaseEnhancedProfileIiaSequenceHeaderObu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfReserved255Profile);

  EXPECT_FALSE(IaSequenceHeaderMetadataGenerator::Generate(
                   kBaseEnhancedProfileIiaSequenceHeaderObu)
                   .ok());
}

TEST(IaSequenceHeaderMetadataGeneratorGenerate, SetsObuHeader) {
  const IASequenceHeaderObu kRedundantCopyIaSequenceHeaderObu(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);

  const auto result = IaSequenceHeaderMetadataGenerator::Generate(
      kRedundantCopyIaSequenceHeaderObu);
  ASSERT_THAT(result, IsOk());

  EXPECT_EQ(result->obu_header().obu_redundant_copy(), true);
}

void ExpectIsSymmetricWithGenerator(
    const IASequenceHeaderObu& original_ia_sequence_header) {
  const auto proto_result =
      IaSequenceHeaderMetadataGenerator::Generate(original_ia_sequence_header);
  ASSERT_THAT(proto_result, IsOk());
  IaSequenceHeaderGenerator generator(*proto_result);
  std::optional<IASequenceHeaderObu> round_trip_result;
  EXPECT_THAT(generator.Generate(round_trip_result), IsOk());
  EXPECT_EQ(round_trip_result, original_ia_sequence_header);
}

TEST(IaSequenceHeaderMetadataGeneratorGenerate,
     IsSymmetricWithGetHeaderFromMetadata) {
  const IASequenceHeaderObu kIaSequenceHeaderObu(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);

  ExpectIsSymmetricWithGenerator(kIaSequenceHeaderObu);
}

}  // namespace
}  // namespace iamf_tools
