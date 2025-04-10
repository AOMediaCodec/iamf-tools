/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/api/conversion/profile_conversion.h"

#include <utility>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::Not;
using ::testing::TestWithParam;

using ProfileVersionPair =
    std::pair<api::ProfileVersion, iamf_tools::ProfileVersion>;
using ApiToInternalType_ProfileVersion = TestWithParam<ProfileVersionPair>;

auto kApiOutputToInternalProfileVersionPairs = ::testing::Values(
    ProfileVersionPair(api::ProfileVersion::kIamfSimpleProfile,
                       ProfileVersion::kIamfSimpleProfile),
    ProfileVersionPair(api::ProfileVersion::kIamfBaseProfile,
                       ProfileVersion::kIamfBaseProfile),
    ProfileVersionPair(api::ProfileVersion::kIamfBaseEnhancedProfile,
                       ProfileVersion::kIamfBaseEnhancedProfile));

TEST_P(ApiToInternalType_ProfileVersion,
       ConvertsOutputProfileVersionToInternalProfileVersion) {
  const auto& [api_profile_version, expected_profile_version] = GetParam();

  const ProfileVersion resulting_profile_version =
      ApiToInternalType(api_profile_version);

  EXPECT_EQ(resulting_profile_version, expected_profile_version);
}

INSTANTIATE_TEST_SUITE_P(ApiToInternalType_ProfileVersion_Instantiation,
                         ApiToInternalType_ProfileVersion,
                         kApiOutputToInternalProfileVersionPairs);

using InternalTypeToApi_ProfileVersion = TestWithParam<ProfileVersionPair>;

TEST_P(InternalTypeToApi_ProfileVersion,
       ConvertsInternalProfileVersionToOutputProfileVersion) {
  const auto& [expected_api_output_layout, internal_profile_version] =
      GetParam();

  const auto api_output_layout = InternalToApiType(internal_profile_version);

  EXPECT_THAT(api_output_layout, IsOkAndHolds(expected_api_output_layout));
}

INSTANTIATE_TEST_SUITE_P(InternalTypeToApi_ProfileVersion_Instantiation,
                         InternalTypeToApi_ProfileVersion,
                         kApiOutputToInternalProfileVersionPairs);

TEST(InternalToApiType_InvalidProfileVersion, ReturnsError) {
  // Some special reserved internal types have no corresponding API type.
  EXPECT_THAT(InternalToApiType(ProfileVersion::kIamfReserved255Profile),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
