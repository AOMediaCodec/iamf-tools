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
#include "iamf/cli/renderer/loudspeakers_renderer.h"

#include <cstddef>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr absl::string_view kFOAInputKey = "A1";
constexpr size_t kExpectedFOAMatrixRows = 4;

constexpr absl::string_view kStereoOutputKey = "0+2+0";
constexpr size_t kExpectedStereoColumns = 2;

constexpr absl::string_view kUnknownInputKey = "UNKNOWN";
constexpr absl::string_view kUnknownOutputKey = "UNKNOWN";

TEST(LookupPrecomputedGains, SucceedsForKnownPrecomputedGains) {
  EXPECT_THAT(LookupPrecomputedGains(kFOAInputKey, kStereoOutputKey), IsOk());
}

TEST(LookupPrecomputedGains, FirstDimensionAgreesWithInputKey) {
  const auto gains = LookupPrecomputedGains(kFOAInputKey, kStereoOutputKey);
  ASSERT_THAT(gains, IsOk());

  EXPECT_EQ(gains->size(), kExpectedFOAMatrixRows);
}

TEST(LookupPrecomputedGains, SecondDimensionAgreesWithOutputKey) {
  const auto gains = LookupPrecomputedGains(kFOAInputKey, kStereoOutputKey);
  ASSERT_THAT(gains, IsOk());
  ASSERT_FALSE(gains->empty());

  EXPECT_EQ(gains->at(0).size(), kExpectedStereoColumns);
}

TEST(LookupPrecomputedGains, ReturnsErrorWhenInputKeyIsUnknown) {
  EXPECT_FALSE(LookupPrecomputedGains(kUnknownInputKey, kStereoOutputKey).ok());
}

TEST(LookupPrecomputedGains, ReturnsErrorWhenOutputKeyIsUnknown) {
  EXPECT_FALSE(LookupPrecomputedGains(kFOAInputKey, kUnknownOutputKey).ok());
}

}  // namespace
}  // namespace iamf_tools
