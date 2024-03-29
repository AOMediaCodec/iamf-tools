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
#include "iamf/cli/ia_sequence_header_generator.h"

#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_header.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

class IaSequenceHeaderGeneratorTest : public testing::Test {
 public:
  IaSequenceHeaderGeneratorTest() {
    ia_sequence_header_metadata_.set_ia_code(IASequenceHeaderObu::kIaCode);
    ia_sequence_header_metadata_.set_primary_profile(
        iamf_tools_cli_proto::PROFILE_VERSION_SIMPLE);
    ia_sequence_header_metadata_.set_additional_profile(
        iamf_tools_cli_proto::PROFILE_VERSION_SIMPLE);
  }

  void InitAndTestGenerate() {
    // Generate the OBUs.
    std::optional<IASequenceHeaderObu> output_obu;
    IaSequenceHeaderGenerator generator(ia_sequence_header_metadata_);
    EXPECT_EQ(generator.Generate(output_obu).code(),
              expected_generate_status_code_);
    EXPECT_EQ(output_obu, expected_obu_);
  }

 protected:
  iamf_tools_cli_proto::IASequenceHeaderObuMetadata
      ia_sequence_header_metadata_;

  absl::StatusCode expected_generate_status_code_ = absl::StatusCode::kOk;
  std::optional<IASequenceHeaderObu> expected_obu_;
};

TEST_F(IaSequenceHeaderGeneratorTest, DefaultSimpleProfile) {
  expected_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                        ProfileVersion::kIamfSimpleProfile,
                        ProfileVersion::kIamfSimpleProfile);
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, IaCodeMayBeOmitted) {
  ia_sequence_header_metadata_.clear_ia_code();
  expected_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                        ProfileVersion::kIamfSimpleProfile,
                        ProfileVersion::kIamfSimpleProfile);
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, RedundantCopy) {
  ia_sequence_header_metadata_.mutable_obu_header()->set_obu_redundant_copy(
      true);

  expected_obu_.emplace(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, ExtensionHeader) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        obu_extension_flag: true
        extension_header_size: 5
        extension_header_bytes: "extra"
      )pb",
      ia_sequence_header_metadata_.mutable_obu_header()));

  expected_obu_.emplace(
      ObuHeader{.obu_extension_flag = true,
                .extension_header_size = 5,
                .extension_header_bytes = {'e', 'x', 't', 'r', 'a'}},
      IASequenceHeaderObu::kIaCode, ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfSimpleProfile);
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, NoIaSequenceHeaderObus) {
  ia_sequence_header_metadata_.Clear();
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, BaseProfile) {
  ia_sequence_header_metadata_.set_primary_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE);
  ia_sequence_header_metadata_.set_additional_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE);

  expected_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                        ProfileVersion::kIamfBaseProfile,
                        ProfileVersion::kIamfBaseProfile);
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, ObeysInvalidIaCode) {
  // IAMF requires `ia_code == IASequenceHeaderObu::kIaCode`. But the generator
  // does not validate OBU requirements.
  const uint32_t kInvalidIaCode = 0;
  ASSERT_NE(kInvalidIaCode, IASequenceHeaderObu::kIaCode);
  ia_sequence_header_metadata_.set_ia_code(kInvalidIaCode);

  expected_obu_.emplace(ObuHeader(), kInvalidIaCode,
                        ProfileVersion::kIamfSimpleProfile,
                        ProfileVersion::kIamfSimpleProfile);
  InitAndTestGenerate();
}

TEST_F(IaSequenceHeaderGeneratorTest, InvalidProfileVersionEnum) {
  ia_sequence_header_metadata_.set_primary_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_INVALID);

  expected_generate_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestGenerate();
}

}  // namespace
}  // namespace iamf_tools
