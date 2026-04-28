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

#include "iamf/cli/renderer/gains/get_gains.h"

#include <cstddef>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::AllOf;
using ::testing::Each;
using ::testing::Not;
using ::testing::SizeIs;

constexpr absl::string_view kFOAInputKey = "A1";
constexpr size_t kExpectedFOAMatrixRows = 4;

constexpr absl::string_view kStereoOutputKey = "0+2+0";
constexpr size_t kExpectedStereoColumns = 2;

constexpr absl::string_view kUnknownInputKey = "UNKNOWN";
constexpr absl::string_view kUnknownOutputKey = "UNKNOWN";

auto HasShape(size_t rows, size_t cols) {
  return AllOf(SizeIs(rows), Each(SizeIs(cols)));
}

TEST(GetGainsForLayoutPair, ShapeAgreesWithInputKey) {
  const auto gains = GetGainsForLayoutPair(kFOAInputKey, kStereoOutputKey);

  EXPECT_THAT(gains, IsOkAndHolds(HasShape(kExpectedFOAMatrixRows,
                                           kExpectedStereoColumns)));
}

TEST(GetGainsForLayoutPair, ReturnsErrorWhenInputKeyIsUnknown) {
  EXPECT_THAT(GetGainsForLayoutPair(kUnknownInputKey, kStereoOutputKey),
              Not(IsOk()));
}

TEST(GetGainsForLayoutPair, ReturnsErrorWhenOutputKeyIsUnknown) {
  EXPECT_THAT(GetGainsForLayoutPair(kFOAInputKey, kUnknownOutputKey),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
