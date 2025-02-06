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
#include "iamf/cli/proto_conversion/obu_to_proto/obu_header_metadata_generator.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAreArray;

TEST(IaSequenceHeaderMetadataGeneratorGenerate, SetsObuRedundantCopy) {
  const ObuHeader kObuHeaderWithRedundantCopy{.obu_redundant_copy = true};
  auto result =
      ObuHeaderMetadataGenerator::Generate(kObuHeaderWithRedundantCopy);
  ASSERT_THAT(result, IsOk());

  EXPECT_TRUE(result->obu_redundant_copy());
}

TEST(ObuHeaderMetadataGeneratorGenerate,
     SetsObuTrimmingStatusFlagWithZeroTrim) {
  const ObuHeader kObuHeaderWithTrimmingStatusFlag{.obu_trimming_status_flag =
                                                       true};
  auto result =
      ObuHeaderMetadataGenerator::Generate(kObuHeaderWithTrimmingStatusFlag);
  ASSERT_THAT(result, IsOk());

  EXPECT_TRUE(result->obu_trimming_status_flag());
  EXPECT_EQ(result->num_samples_to_trim_at_end(), 0);
  EXPECT_EQ(result->num_samples_to_trim_at_start(), 0);
}

TEST(ObuHeaderMetadataGeneratorGenerate,
     SetsObuTrimmingStatusFlagWithNonZeroTrim) {
  constexpr uint32_t kNumSamplesToTrimAtEnd = 5;
  constexpr uint32_t kNumSamplesToTrimAtStart = 10;
  const ObuHeader kObuHeaderWithNonZeroTrim{
      .obu_trimming_status_flag = true,
      .num_samples_to_trim_at_end = kNumSamplesToTrimAtEnd,
      .num_samples_to_trim_at_start = kNumSamplesToTrimAtStart};
  auto result = ObuHeaderMetadataGenerator::Generate(kObuHeaderWithNonZeroTrim);
  ASSERT_THAT(result, IsOk());

  EXPECT_TRUE(result->obu_trimming_status_flag());
  EXPECT_EQ(result->num_samples_to_trim_at_end(), kNumSamplesToTrimAtEnd);
  EXPECT_EQ(result->num_samples_to_trim_at_start(), kNumSamplesToTrimAtStart);
}

TEST(ObuHeaderMetadataGeneratorGenerate, SetsEmptyObuExtension) {
  const ObuHeader kObuHeaderWithEmptyExtension{.obu_extension_flag = true};
  auto result =
      ObuHeaderMetadataGenerator::Generate(kObuHeaderWithEmptyExtension);
  ASSERT_THAT(result, IsOk());

  EXPECT_TRUE(result->obu_extension_flag());
  EXPECT_EQ(result->extension_header_size(), 0);
  EXPECT_TRUE(result->extension_header_bytes().empty());
}

TEST(ObuHeaderMetadataGeneratorTest, SetsNonEmptyObuExtension) {
  constexpr DecodedUleb128 kExtensionHeaderSize = 10;
  const std::vector<uint8_t> extension_header_bytes = {0, 1, 2, 3, 4,
                                                       5, 6, 7, 8, 9};
  const ObuHeader kObuHeaderWithNonEmptyExtension{
      .obu_extension_flag = true,
      .extension_header_size = kExtensionHeaderSize,
      .extension_header_bytes = extension_header_bytes};
  auto result =
      ObuHeaderMetadataGenerator::Generate(kObuHeaderWithNonEmptyExtension);
  ASSERT_THAT(result, IsOk());

  EXPECT_TRUE(result->obu_extension_flag());
  EXPECT_EQ(result->extension_header_size(), kExtensionHeaderSize);
  EXPECT_THAT(result->extension_header_bytes(),
              ElementsAreArray(extension_header_bytes));
}

TEST(ObuHeaderMetadataGeneratorGenerate, InvalidWhenExtensionSizeMismatch) {
  constexpr DecodedUleb128 kExtensionHeaderSizeMismatch = 99;
  const std::vector<uint8_t> extension_header_bytes = {0, 1, 2, 3, 4,
                                                       5, 6, 7, 8, 9};
  const ObuHeader kObuHeaderWithExtensionSizeMismatch{
      .obu_extension_flag = true,
      .extension_header_size = kExtensionHeaderSizeMismatch,
      .extension_header_bytes = extension_header_bytes};

  EXPECT_FALSE(
      ObuHeaderMetadataGenerator::Generate(kObuHeaderWithExtensionSizeMismatch)
          .ok());
}

void ExpectIsSymmetricWithGenerator(const ObuHeader& original_obu_header) {
  const auto proto_result =
      ObuHeaderMetadataGenerator::Generate(original_obu_header);
  ASSERT_THAT(proto_result, IsOk());
  const auto round_trip_result = GetHeaderFromMetadata(*proto_result);

  EXPECT_EQ(round_trip_result, original_obu_header);
}

TEST(ObuHeaderMetadataGeneratorGenerate, IsSymmetricWithGetHeaderFromMetadata) {
  const ObuHeader kObuHeaderWithManyFieldsSet{
      .obu_redundant_copy = true,
      .obu_trimming_status_flag = true,
      .obu_extension_flag = true,
      .num_samples_to_trim_at_end = 5,
      .num_samples_to_trim_at_start = 10,
      .extension_header_size = 10,
      .extension_header_bytes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};

  ExpectIsSymmetricWithGenerator(kObuHeaderWithManyFieldsSet);
}

}  // namespace
}  // namespace iamf_tools
