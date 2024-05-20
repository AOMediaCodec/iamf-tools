/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/ia_sequence_header.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(IaSequenceHeaderConstructor, SetsObuType) {
  IASequenceHeaderObu obu({}, IASequenceHeaderObu::kIaCode,
                          ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu.header_.obu_type, kObuIaSequenceHeader);
}

struct IASequenceHeaderInitArgs {
  uint32_t ia_code;
  ProfileVersion primary_profile;
  ProfileVersion additional_profile;
};

class IASequenceHeaderObuTestBase : public ObuTestBase {
 public:
  IASequenceHeaderObuTestBase()
      : ObuTestBase(
            /*expected_header=*/{kObuIaSequenceHeader << 3, 06},
            /*expected_payload=*/
            {// `ia_code`.
             0x69, 0x61, 0x6d, 0x66,
             // `primary_profile`.
             static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
             // `additional_profile`.
             static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile)}),
        init_args_({
            .ia_code = IASequenceHeaderObu::kIaCode,
            .primary_profile = ProfileVersion::kIamfSimpleProfile,
            .additional_profile = ProfileVersion::kIamfSimpleProfile,
        }) {}

  ~IASequenceHeaderObuTestBase() override = default;

 protected:
  void InitExpectOk() override {
    obu_ = std::make_unique<IASequenceHeaderObu>(header_, init_args_.ia_code,
                                                 init_args_.primary_profile,
                                                 init_args_.additional_profile);
  }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  IASequenceHeaderInitArgs init_args_;

  std::unique_ptr<IASequenceHeaderObu> obu_;
};

class IASequenceHeaderObuTest : public IASequenceHeaderObuTestBase,
                                public testing::Test {};

TEST_F(IASequenceHeaderObuTest,
       ValidateAndWriteFailsWithUnsupportedPrimaryProfile2) {
  init_args_.primary_profile = static_cast<ProfileVersion>(2);

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(IASequenceHeaderObuTest,
       ValidateAndWriteFailsWithUnsupportedPrimaryProfile255) {
  init_args_.primary_profile = static_cast<ProfileVersion>(255);

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(IASequenceHeaderObuTest, ValidateAndWriteFailsWithIllegalIACode) {
  init_args_.ia_code = 0;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(IASequenceHeaderObuTest,
       ValidateAndWriteFailsWithIllegalIACodeUppercase) {
  init_args_.ia_code = 0x49414d46u;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(IASequenceHeaderObuTest, DefaultSimpleProfile) { InitAndTestWrite(); }

TEST_F(IASequenceHeaderObuTest, BaseProfile) {
  init_args_.primary_profile = ProfileVersion::kIamfBaseProfile;
  init_args_.additional_profile = ProfileVersion::kIamfBaseProfile;
  expected_payload_ = {// `ia_code`.
                       0x69, 0x61, 0x6d, 0x66,
                       // `primary_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile),
                       // `additional_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  InitAndTestWrite();
}

TEST_F(IASequenceHeaderObuTest, RedundantCopy) {
  header_.obu_redundant_copy = true;
  expected_header_ = {(kObuIaSequenceHeader << 3) | kObuRedundantCopyBitMask,
                      0x06};
  expected_payload_ = {
      // `ia_code`.
      0x69,
      0x61,
      0x6d,
      0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
  };
  InitAndTestWrite();
}

TEST_F(IASequenceHeaderObuTest,
       ValidateAndWriteFailsWithIllegalTrimmingStatusFlag) {
  header_.obu_trimming_status_flag = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(IASequenceHeaderObuTest, BaseProfileBackwardsCompatible) {
  init_args_.additional_profile = ProfileVersion::kIamfBaseProfile;
  expected_payload_ = {// `ia_code`.
                       0x69, 0x61, 0x6d, 0x66,
                       // `primary_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
                       // `additional_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  InitAndTestWrite();
}

TEST_F(IASequenceHeaderObuTest, UnknownProfileBackwardsCompatible2) {
  init_args_.additional_profile = static_cast<ProfileVersion>(2);
  expected_payload_ = {// `ia_code`.
                       0x69, 0x61, 0x6d, 0x66,
                       // `primary_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
                       // `additional_profile`.
                       2};
  InitAndTestWrite();
}

TEST_F(IASequenceHeaderObuTest, UnknownProfileBackwardsCompatible255) {
  init_args_.additional_profile = static_cast<ProfileVersion>(255);
  expected_payload_ = {// `ia_code`.
                       0x69, 0x61, 0x6d, 0x66,
                       // `primary_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
                       // `additional_profile`.
                       255};
  InitAndTestWrite();
}

TEST_F(IASequenceHeaderObuTest, ExtensionHeader) {
  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaSequenceHeader << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      12,
                      // `extension_header_size`.
                      5,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};
  InitAndTestWrite();
}

TEST_F(IASequenceHeaderObuTest, NonMinimalLebGeneratorAffectsObuHeader) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);

  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaSequenceHeader << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      0x80 | 13, 0x00,
                      // `extension_header_size`.
                      0x80 | 5, 0x00,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};
  InitAndTestWrite();
}

TEST(CreateFromBuffer, SimpleAndBaseProfile) {
  std::vector<uint8_t> source = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  ReadBitBuffer buffer(1024, &source);
  ObuHeader header;
  absl::StatusOr<IASequenceHeaderObu> obu =
      IASequenceHeaderObu::CreateFromBuffer(header, buffer);
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu.value().GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu.value().GetAdditionalProfile(),
            ProfileVersion::kIamfBaseProfile);
}

}  // namespace
}  // namespace iamf_tools
