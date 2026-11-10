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
#include "iamf/common/build_info.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::testing::IsEmpty;
using ::testing::Not;

TEST(ResolveBuildVersion, PrefersStampedVersionOverArchiveStrings) {
  EXPECT_EQ(ResolveBuildVersion("v4.0.0-3-g1234567-dirty", "v4.0.0", "4b6caa6"),
            "v4.0.0-3-g1234567-dirty");
}

TEST(ResolveBuildVersion, FormatsExactArchiveTagInLongForm) {
  EXPECT_EQ(ResolveBuildVersion("", "v4.0.0", "4b6caa6"), "v4.0.0-0-g4b6caa6");
}

TEST(ResolveBuildVersion, PreservesLongArchiveDescribePastTag) {
  EXPECT_EQ(ResolveBuildVersion("", "v4.0.0-12-gcdd1874", "cdd1874"),
            "v4.0.0-12-gcdd1874");
}

TEST(ResolveBuildVersion, FormatsExactTagContainingDashGInLongForm) {
  EXPECT_EQ(ResolveBuildVersion("", "v4.0.0-ga", "4b6caa6"),
            "v4.0.0-ga-0-g4b6caa6");
}

TEST(ResolveBuildVersion, FallsBackToArchiveCommitWhenNoTagReachable) {
  EXPECT_EQ(ResolveBuildVersion("", "", "cdd1874"), "cdd1874");
}

TEST(ResolveBuildVersion, ReturnsUnknownWhenPlaceholdersAreUnsubstituted) {
  EXPECT_EQ(
      ResolveBuildVersion("", "$Format:%(describe:tags=true)$", "$Format:%h$"),
      "unknown");
}

TEST(ResolveBuildVersion, ReturnsUnknownWhenAllInputsAreEmpty) {
  EXPECT_EQ(ResolveBuildVersion("", "", ""), "unknown");
}

TEST(GetBuildVersion, ReturnsNonEmptyVersion) {
  EXPECT_THAT(GetBuildVersion(), Not(IsEmpty()));
}

}  // namespace
}  // namespace iamf_tools
