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
#include <optional>

#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/demixing_info_parameter_data.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::Optional;
using ::testing::SizeIs;

constexpr absl::string_view kFOAInputKey = "A1";
constexpr absl::string_view k7_1_4InputKey = "4+7+0";
constexpr size_t kExpectedFOAMatrixRows = 4;

constexpr absl::string_view kStereoOutputKey = "0+2+0";
constexpr absl::string_view k3_1_2OutputKey = "3.1.2";
constexpr size_t kExpectedStereoColumns = 2;

constexpr absl::string_view kUnknownInputKey = "UNKNOWN";
constexpr absl::string_view kUnknownOutputKey = "UNKNOWN";

// Returns a matcher that checks if the container has the given number of rows
// and columns. Gain matrices in this context are 2D vectors, with the number
// of rows corresponding to the number of input channels and the number of
// columns corresponding to the number of output channels.
auto HasShape(size_t rows, size_t cols) {
  return AllOf(SizeIs(rows), Each(SizeIs(cols)));
}

TEST(LookupPrecomputedGains, SucceedsForKnownPrecomputedGains) {
  EXPECT_THAT(LookupPrecomputedGains(kFOAInputKey, kStereoOutputKey), IsOk());
}

TEST(LookupPrecomputedGains, ShapeAgreesWithInputKey) {
  const auto gains = LookupPrecomputedGains(kFOAInputKey, kStereoOutputKey);

  EXPECT_THAT(gains, IsOkAndHolds(HasShape(kExpectedFOAMatrixRows,
                                           kExpectedStereoColumns)));
}

TEST(LookupPrecomputedGains, ReturnsErrorWhenInputKeyIsUnknown) {
  EXPECT_FALSE(LookupPrecomputedGains(kUnknownInputKey, kStereoOutputKey).ok());
}

TEST(LookupPrecomputedGains, ReturnsErrorWhenOutputKeyIsUnknown) {
  EXPECT_FALSE(LookupPrecomputedGains(kFOAInputKey, kUnknownOutputKey).ok());
}

constexpr DownMixingParams kDMixPMode1DownMixingParams = {.alpha = 1.0,
                                                          .beta = 1.0,
                                                          .gamma = 0.707,
                                                          .delta = 0.707,
                                                          .w = 0.707,
                                                          .in_bitstream = true};

TEST(MaybeComputeDynamicGains, ReturnsNulloptForUnknownLayouts) {
  const auto gains = MaybeComputeDynamicGains(
      kDMixPMode1DownMixingParams, kUnknownInputKey, kUnknownOutputKey);

  // Unknown or strange layouts  should be cleared to signal fallback to
  // precomputed gains.
  EXPECT_EQ(gains, std::nullopt);
}

TEST(MaybeComputeDynamicGains, ReturnsGainsForKnownLayouts) {
  const auto gains = MaybeComputeDynamicGains(kDMixPMode1DownMixingParams,
                                              k7_1_4InputKey, k3_1_2OutputKey);

  constexpr size_t kExpectedInput7_1_4Rows = 12;
  constexpr size_t kExpectedOutput3_1_2Cols = 6;
  // Just check that the shape of the input gains agrees with the input and
  // output layouts. ""
  EXPECT_THAT(gains, Optional(HasShape(kExpectedInput7_1_4Rows,
                                       kExpectedOutput3_1_2Cols)));
}

TEST(MaybeComputeDynamicGains, ReturnsNulloptWhenGainsNotInBitstream) {
  const DownMixingParams kAbsentDownMixingParams = {.in_bitstream = false};
  auto gains = MaybeComputeDynamicGains(kAbsentDownMixingParams, k7_1_4InputKey,
                                        k3_1_2OutputKey);

  EXPECT_EQ(gains, std::nullopt);
}

}  // namespace
}  // namespace iamf_tools
