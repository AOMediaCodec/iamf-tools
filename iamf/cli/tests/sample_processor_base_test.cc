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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::status::StatusIs;

using absl::StatusCode::kFailedPrecondition;

constexpr uint32_t kMaxInputTicks = 4;
constexpr uint32_t kMaxOutputTicks = 4;
constexpr size_t kNumChannels = 2;

TEST(GetOutputSamplesAsSpan, ReturnsEmptyAfterConstruction) {
  const MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                           kMaxOutputTicks);
  EXPECT_TRUE(mock_resampler.GetOutputSamplesAsSpan().empty());
}

TEST(GetOutputSamplesAsSpan, SizeMatchesNumValidTicks) {
  EverySecondTickResampler every_second_tick_resampler(kMaxInputTicks,
                                                       kNumChannels);
  EXPECT_THAT(
      every_second_tick_resampler.PushFrame({{1, 2}, {3, 4}, {5, 6}, {7, 8}}),
      IsOk());
  EXPECT_EQ(every_second_tick_resampler.GetOutputSamplesAsSpan().size(), 2);

  EXPECT_THAT(every_second_tick_resampler.PushFrame({{9, 10}, {11, 12}}),
              IsOk());
  EXPECT_EQ(every_second_tick_resampler.GetOutputSamplesAsSpan().size(), 1);

  EXPECT_THAT(every_second_tick_resampler.Flush(), IsOk());
  EXPECT_TRUE(every_second_tick_resampler.GetOutputSamplesAsSpan().empty());
}

TEST(PushFrame, ReturnsFailedPreconditionWhenCalledAfterFlush) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  EXPECT_THAT(mock_resampler.PushFrame({}), IsOk());
  EXPECT_THAT(mock_resampler.Flush(), IsOk());

  EXPECT_THAT(mock_resampler.PushFrame({}), StatusIs(kFailedPrecondition));
}

TEST(PushFrame, InvalidIfInputSpanHasTooManyTicks) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<int32_t>> kTooManyTicks(
      kMaxInputTicks + 1, std::vector<int32_t>(kNumChannels));

  EXPECT_FALSE(
      mock_resampler.PushFrame(absl::MakeConstSpan(kTooManyTicks)).ok());
}

TEST(PushFrame, InvalidIfInputSpanHasTooFewChannels) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<int32_t>> kTooFewChannels(
      kMaxInputTicks, std::vector<int32_t>(kNumChannels - 1));

  EXPECT_FALSE(
      mock_resampler.PushFrame(absl::MakeConstSpan(kTooFewChannels)).ok());
}

TEST(PushFrame, InvalidIfInputSpanHasTooManyChannels) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<int32_t>> kTooManyChannels(
      kMaxInputTicks, std::vector<int32_t>(kNumChannels + 1));

  EXPECT_FALSE(
      mock_resampler.PushFrame(absl::MakeConstSpan(kTooManyChannels)).ok());
}

TEST(Flush, ReturnsFailedPreconditionWhenCalledTwice) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  EXPECT_THAT(mock_resampler.Flush(), IsOk());

  EXPECT_THAT(mock_resampler.Flush(), StatusIs(kFailedPrecondition));
}

}  // namespace
}  // namespace iamf_tools
