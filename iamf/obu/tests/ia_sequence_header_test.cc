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
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr uint32_t kInvalidIACode = 0xff;

TEST(IaSequenceHeaderConstructor, SetsObuType) {
  IASequenceHeaderObu obu({}, ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu.header_.obu_type, kObuIaSequenceHeader);
}

TEST(Validate, SucceedsWithSimpleProfile) {
  const IASequenceHeaderObu simple_profile_obu(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfSimpleProfile);

  EXPECT_THAT(simple_profile_obu.Validate(), IsOk());
}

TEST(Validate, SucceedsWithBaseProfile) {
  const IASequenceHeaderObu base_profile_obu(ObuHeader(),
                                             ProfileVersion::kIamfBaseProfile,
                                             ProfileVersion::kIamfBaseProfile);

  EXPECT_THAT(base_profile_obu.Validate(), IsOk());
}

TEST(Validate, SucceedsWithDifferentProfiles) {
  const IASequenceHeaderObu obu_with_different_profiles(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);

  EXPECT_THAT(obu_with_different_profiles.Validate(), IsOk());
}

TEST(Validate, SucceedsWithBaseEnhancedProfile) {
  const IASequenceHeaderObu base_enhanced_profile_obu(
      ObuHeader(), ProfileVersion::kIamfBaseEnhancedProfile,
      ProfileVersion::kIamfSimpleProfile);

  EXPECT_THAT(base_enhanced_profile_obu.Validate(), IsOk());
}

TEST(Validate, FailsWithUnsupportedPrimaryProfile3) {
  const IASequenceHeaderObu profile_3_obu(ObuHeader(),
                                          static_cast<ProfileVersion>(3),
                                          ProfileVersion::kIamfSimpleProfile);

  EXPECT_FALSE(profile_3_obu.Validate().ok());
}

TEST(Validate, FailsWithUnsupportedPrimaryProfileReserved255) {
  const IASequenceHeaderObu reserved_profile_255_obu(
      ObuHeader(), ProfileVersion::kIamfReserved255Profile,
      ProfileVersion::kIamfSimpleProfile);

  EXPECT_FALSE(reserved_profile_255_obu.Validate().ok());
}

TEST(ValidateAndWrite, FailsWhenObuIsInvalid) {
  const IASequenceHeaderObu obu_with_invalid_ia_code(
      ObuHeader(), ProfileVersion::kIamfReserved255Profile,
      ProfileVersion::kIamfSimpleProfile);

  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_with_invalid_ia_code.ValidateAndWriteObu(unused_wb).ok());
}

TEST(ValidateAndWrite, FailsWhenPrimaryProfileIsUnknown3) {
  const IASequenceHeaderObu obu_with_invalid_primary_profile(
      ObuHeader(), static_cast<ProfileVersion>(3),
      ProfileVersion::kIamfSimpleProfile);

  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(
      obu_with_invalid_primary_profile.ValidateAndWriteObu(unused_wb).ok());
}

TEST(ValidateAndWrite, FailsWhenPrimaryProfileIsUnknown255) {
  const IASequenceHeaderObu obu_with_invalid_primary_profile(
      ObuHeader(), static_cast<ProfileVersion>(255),
      ProfileVersion::kIamfSimpleProfile);

  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(
      obu_with_invalid_primary_profile.ValidateAndWriteObu(unused_wb).ok());
}

TEST(CreateFromBuffer, SimpleAndBaseProfile) {
  std::vector<uint8_t> source = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  const int64_t payload_size = source.size();
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  ObuHeader header;

  absl::StatusOr<IASequenceHeaderObu> obu =
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetPrimaryProfile(), ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu->GetAdditionalProfile(), ProfileVersion::kIamfBaseProfile);
}

TEST(CreateFromBuffer, BaseEnhancedProfile) {
  std::vector<uint8_t> source = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile)};
  const int64_t payload_size = source.size();
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  ObuHeader header;

  absl::StatusOr<IASequenceHeaderObu> obu =
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetPrimaryProfile(), ProfileVersion::kIamfBaseEnhancedProfile);
  EXPECT_EQ(obu->GetAdditionalProfile(),
            ProfileVersion::kIamfBaseEnhancedProfile);
}

TEST(CreateFromBuffer, InvalidWhenPrimaryProfileIs3) {
  std::vector<uint8_t> source = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      3,
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  const int64_t payload_size = source.size();
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  ObuHeader header;

  EXPECT_FALSE(
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer)
          .ok());
}

TEST(CreateFromBuffer, InvalidWhenPrimaryProfileIs255) {
  std::vector<uint8_t> source = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfReserved255Profile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  const int64_t payload_size = source.size();
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  ObuHeader header;

  EXPECT_FALSE(
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer)
          .ok());
}

class IaSequenceHeaderObuTestBase : public ObuTestBase {
 public:
  IaSequenceHeaderObuTestBase()
      : ObuTestBase(
            /*expected_header=*/{kObuIaSequenceHeader << 3, 06},
            /*expected_payload=*/
            {// `ia_code`.
             0x69, 0x61, 0x6d, 0x66,
             // `primary_profile`.
             static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
             // `additional_profile`.
             static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile)}) {}

  ~IaSequenceHeaderObuTestBase() override = default;

 protected:
  void InitExpectOk() override {
    obu_ = std::make_unique<IASequenceHeaderObu>(header_, primary_profile_,
                                                 additional_profile_);
  }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  ProfileVersion primary_profile_ = ProfileVersion::kIamfSimpleProfile;
  ProfileVersion additional_profile_ = ProfileVersion::kIamfSimpleProfile;

  std::unique_ptr<IASequenceHeaderObu> obu_;
};

class IaSequenceHeaderObuTest : public IaSequenceHeaderObuTestBase,
                                public testing::Test {};

TEST_F(IaSequenceHeaderObuTest, DefaultSimpleProfile) { InitAndTestWrite(); }

TEST_F(IaSequenceHeaderObuTest, BaseProfile) {
  primary_profile_ = ProfileVersion::kIamfBaseProfile;
  additional_profile_ = ProfileVersion::kIamfBaseProfile;
  expected_payload_ = {// `ia_code`.
                       0x69, 0x61, 0x6d, 0x66,
                       // `primary_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile),
                       // `additional_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  InitAndTestWrite();
}

TEST_F(IaSequenceHeaderObuTest, RedundantCopy) {
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

TEST_F(IaSequenceHeaderObuTest,
       ValidateAndWriteFailsWithIllegalTrimmingStatusFlag) {
  header_.obu_trimming_status_flag = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(IaSequenceHeaderObuTest, BaseProfileBackwardsCompatible) {
  additional_profile_ = ProfileVersion::kIamfBaseProfile;
  expected_payload_ = {// `ia_code`.
                       0x69, 0x61, 0x6d, 0x66,
                       // `primary_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
                       // `additional_profile`.
                       static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile)};
  InitAndTestWrite();
}

TEST_F(IaSequenceHeaderObuTest, BaseEnhancedForBothProfiles) {
  primary_profile_ = ProfileVersion::kIamfBaseEnhancedProfile;
  additional_profile_ = ProfileVersion::kIamfBaseEnhancedProfile;
  expected_payload_ = {
      // `ia_code`.
      0x69,
      0x61,
      0x6d,
      0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile),
  };
  InitAndTestWrite();
}

TEST_F(IaSequenceHeaderObuTest, BaseEnhancedProfileBackwardsCompatible2) {
  additional_profile_ = ProfileVersion::kIamfBaseEnhancedProfile;
  expected_payload_ = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile)};
  InitAndTestWrite();
}

TEST_F(IaSequenceHeaderObuTest, UnknownProfileBackwardsCompatibleReserved255) {
  additional_profile_ = ProfileVersion::kIamfReserved255Profile;
  expected_payload_ = {
      // `ia_code`.
      0x69, 0x61, 0x6d, 0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfReserved255Profile)};
  InitAndTestWrite();
}

TEST_F(IaSequenceHeaderObuTest, ExtensionHeader) {
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

TEST_F(IaSequenceHeaderObuTest, NonMinimalLebGeneratorAffectsObuHeader) {
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

}  // namespace
}  // namespace iamf_tools
