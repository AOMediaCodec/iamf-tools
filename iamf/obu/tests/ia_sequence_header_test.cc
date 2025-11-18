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

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

using ::absl::MakeConstSpan;

constexpr int64_t kInitialBufferSize = 16;

constexpr auto kObuHeader =
    std::to_array<uint8_t>({kObuIaSequenceHeader << 3, 6});

constexpr auto kSimpleProfilePayload = std::to_array<uint8_t>({
    // `ia_code`.
    0x69,
    0x61,
    0x6d,
    0x66,
    // `primary_profile`.
    static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
    // `additional_profile`.
    static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
});

constexpr auto kBaseProfilePayload = std::to_array<uint8_t>({
    // `ia_code`.
    0x69,
    0x61,
    0x6d,
    0x66,
    // `primary_profile`.
    static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile),
    // `additional_profile`.
    static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile),
});

constexpr auto kBaseEnhancedProfilePayload = std::to_array<uint8_t>({
    // `ia_code`.
    0x69,
    0x61,
    0x6d,
    0x66,
    // `primary_profile`.
    static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile),
    // `additional_profile`.
    static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile),
});

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

  EXPECT_THAT(profile_3_obu.Validate(), Not(IsOk()));
}

TEST(Validate, FailsWithUnsupportedPrimaryProfileReserved255) {
  const IASequenceHeaderObu reserved_profile_255_obu(
      ObuHeader(), ProfileVersion::kIamfReserved255Profile,
      ProfileVersion::kIamfSimpleProfile);

  EXPECT_THAT(reserved_profile_255_obu.Validate(), Not(IsOk()));
}

TEST(ValidateAndWrite, FailsWhenObuIsInvalid) {
  const IASequenceHeaderObu obu_with_invalid_ia_code(
      ObuHeader(), ProfileVersion::kIamfReserved255Profile,
      ProfileVersion::kIamfSimpleProfile);

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(obu_with_invalid_ia_code.ValidateAndWriteObu(unused_wb),
              Not(IsOk()));
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
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;

  absl::StatusOr<IASequenceHeaderObu> obu =
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetPrimaryProfile(), ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu->GetAdditionalProfile(), ProfileVersion::kIamfBaseProfile);
}

TEST(CreateFromBuffer, BaseEnhancedProfile) {
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(kBaseEnhancedProfilePayload));
  ObuHeader header;

  absl::StatusOr<IASequenceHeaderObu> obu =
      IASequenceHeaderObu::CreateFromBuffer(
          header, kBaseEnhancedProfilePayload.size(), *buffer);

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
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;

  EXPECT_THAT(
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer),
      Not(IsOk()));
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
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;

  EXPECT_THAT(
      IASequenceHeaderObu::CreateFromBuffer(header, payload_size, *buffer),
      Not(IsOk()));
}

TEST(ValidateAndWrite, SimpleProfile) {
  IASequenceHeaderObu obu(ObuHeader(), ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfSimpleProfile);
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kObuHeader),
                          MakeConstSpan(kSimpleProfilePayload));
}

TEST(ValidateAndWrite, BaseProfile) {
  IASequenceHeaderObu obu(ObuHeader(), ProfileVersion::kIamfBaseProfile,
                          ProfileVersion::kIamfBaseProfile);
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kObuHeader),
                          MakeConstSpan(kBaseProfilePayload));
}

TEST(ValidateAndWrite, RedundantCopy) {
  IASequenceHeaderObu obu(ObuHeader{.obu_redundant_copy = true},
                          ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfSimpleProfile);
  constexpr std::array<uint8_t, 2> kExpectedHeader = {
      (kObuIaSequenceHeader << 3) | ObuTestBase::kObuRedundantCopyBitMask, 6};
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kExpectedHeader),
                          MakeConstSpan(kSimpleProfilePayload));
}

TEST(ValidateAndWrite, TrimmingStatusFlag) {
  IASequenceHeaderObu obu(ObuHeader{.obu_trimming_status_flag = true},
                          ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfSimpleProfile);
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), Not(IsOk()));
}

TEST(ValidateAndWrite, ExtensionHeader) {
  IASequenceHeaderObu obu(
      ObuHeader{.extension_header_bytes =
                    std::vector<uint8_t>{'e', 'x', 't', 'r', 'a'}},
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);
  constexpr auto kExpectedHeader = std::to_array<uint8_t>(
      {(kObuIaSequenceHeader << 3) | ObuTestBase::kObuExtensionFlagBitMask, 12,
       5, 'e', 'x', 't', 'r', 'a'});
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kExpectedHeader),
                          MakeConstSpan(kSimpleProfilePayload));
}

TEST(ValidateAndWrite, BaseProfileBackwardsCompatible) {
  IASequenceHeaderObu obu(ObuHeader(), ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfBaseProfile);
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `ia_code`.
      0x69,
      0x61,
      0x6d,
      0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseProfile),
  });
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kObuHeader),
                          MakeConstSpan(kExpectedPayload));
}

TEST(ValidateAndWrite, BaseEnhancedProfile) {
  IASequenceHeaderObu obu(ObuHeader(), ProfileVersion::kIamfBaseEnhancedProfile,
                          ProfileVersion::kIamfBaseEnhancedProfile);
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kObuHeader),
                          MakeConstSpan(kBaseEnhancedProfilePayload));
}

TEST(ValidateAndWrite, BaseEnhancedProfileBackwardsCompatibleWithSimple) {
  IASequenceHeaderObu obu(ObuHeader(), ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfBaseEnhancedProfile);
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `ia_code`.
      0x69,
      0x61,
      0x6d,
      0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfBaseEnhancedProfile),
  });
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kObuHeader),
                          MakeConstSpan(kExpectedPayload));
}

TEST(ValidateAndWrite, UnknownProfileBackwardsCompatibleReserved255) {
  IASequenceHeaderObu obu(ObuHeader(), ProfileVersion::kIamfSimpleProfile,
                          ProfileVersion::kIamfReserved255Profile);
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `ia_code`.
      0x69,
      0x61,
      0x6d,
      0x66,
      // `primary_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfSimpleProfile),
      // `additional_profile`.
      static_cast<uint8_t>(ProfileVersion::kIamfReserved255Profile),
  });
  WriteBitBuffer wb(kInitialBufferSize);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kObuHeader),
                          MakeConstSpan(kExpectedPayload));
}

TEST(ValidateAndWrite, NonMinimalLebGeneratorAffectsObuHeader) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  ASSERT_NE(leb_generator, nullptr);
  IASequenceHeaderObu obu(
      ObuHeader{.extension_header_bytes =
                    std::vector<uint8_t>{'e', 'x', 't', 'r', 'a'}},
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);
  constexpr auto kExpectedHeader = std::to_array<uint8_t>(
      {(kObuIaSequenceHeader << 3) | ObuTestBase::kObuExtensionFlagBitMask,
       // `obu_size`.
       0x80 | 13, 0x00,
       // `extension_header_size`.
       0x80 | 5, 0x00,
       // `extension_header_bytes`.
       'e', 'x', 't', 'r', 'a'});

  WriteBitBuffer wb(kInitialBufferSize, *leb_generator);

  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, MakeConstSpan(kExpectedHeader),
                          MakeConstSpan(kSimpleProfilePayload));
}

TEST(ValidateAndWrite, FailsWhenPrimaryProfileIsUnknown3) {
  const IASequenceHeaderObu obu_with_invalid_primary_profile(
      ObuHeader(), static_cast<ProfileVersion>(3),
      ProfileVersion::kIamfSimpleProfile);

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(obu_with_invalid_primary_profile.ValidateAndWriteObu(unused_wb),
              Not(IsOk()));
}

TEST(ValidateAndWrite, FailsWhenPrimaryProfileIsUnknown255) {
  const IASequenceHeaderObu obu_with_invalid_primary_profile(
      ObuHeader(), static_cast<ProfileVersion>(255),
      ProfileVersion::kIamfSimpleProfile);

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(obu_with_invalid_primary_profile.ValidateAndWriteObu(unused_wb),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
