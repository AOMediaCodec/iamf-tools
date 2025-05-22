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
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;

using absl::StatusCode::kFailedPrecondition;

constexpr uint32_t kMaxInputTicks = 4;
constexpr uint32_t kMaxOutputTicks = 4;
constexpr size_t kNumChannels = 2;

TEST(GetOutputSamplesAsSpan, ReturnsEmptyAfterConstruction) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  for (const auto& output_channel : mock_resampler.GetOutputSamplesAsSpan()) {
    EXPECT_TRUE(output_channel.empty());
  }
}

TEST(GetOutputSamplesAsSpan, SizeMatchesNumValidTicks) {
  EverySecondTickResampler every_second_tick_resampler(kMaxInputTicks,
                                                       kNumChannels);
  const std::vector<std::vector<InternalSampleType>> first_frame = {
      {0.1, 0.3, 0.5, 0.7}, {0.2, 0.4, 0.6, 0.8}};
  EXPECT_THAT(
      every_second_tick_resampler.PushFrame(MakeSpanOfConstSpans(first_frame)),
      IsOk());
  for (const auto& output_channel :
       every_second_tick_resampler.GetOutputSamplesAsSpan()) {
    EXPECT_EQ(output_channel.size(), 2);
  }

  const std::vector<std::vector<InternalSampleType>> second_frame = {
      {0.9, 0.10}, {0.11, 0.12}};
  EXPECT_THAT(
      every_second_tick_resampler.PushFrame(MakeSpanOfConstSpans(second_frame)),
      IsOk());
  for (const auto& output_channel :
       every_second_tick_resampler.GetOutputSamplesAsSpan()) {
    EXPECT_EQ(output_channel.size(), 1);
  }

  EXPECT_THAT(every_second_tick_resampler.Flush(), IsOk());
  for (const auto& output_channel :
       every_second_tick_resampler.GetOutputSamplesAsSpan()) {
    EXPECT_TRUE(output_channel.empty());
  }
}

TEST(PushFrame, ReturnsFailedPreconditionWhenCalledAfterFlush) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<InternalSampleType>> empty_frame = {{}, {}};
  EXPECT_THAT(mock_resampler.PushFrame(MakeSpanOfConstSpans(empty_frame)),
              IsOk());
  EXPECT_THAT(mock_resampler.Flush(), IsOk());

  EXPECT_THAT(mock_resampler.PushFrame(MakeSpanOfConstSpans(empty_frame)),
              StatusIs(kFailedPrecondition));
}

TEST(PushFrame, InvalidIfInputSpanHasTooManyTicks) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<InternalSampleType>> kTooManyTicks(
      kMaxInputTicks + 1, std::vector<InternalSampleType>(kNumChannels));

  EXPECT_FALSE(
      mock_resampler.PushFrame(MakeSpanOfConstSpans(kTooManyTicks)).ok());
}

TEST(PushFrame, InvalidIfInputSpanHasTooFewChannels) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<InternalSampleType>> kTooFewChannels(
      kMaxInputTicks, std::vector<InternalSampleType>(kNumChannels - 1));

  EXPECT_FALSE(
      mock_resampler.PushFrame(MakeSpanOfConstSpans(kTooFewChannels)).ok());
}

TEST(PushFrame, InvalidIfInputSpanHasTooManyChannels) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  const std::vector<std::vector<InternalSampleType>> kTooManyChannels(
      kMaxInputTicks, std::vector<InternalSampleType>(kNumChannels + 1));

  EXPECT_FALSE(
      mock_resampler.PushFrame(MakeSpanOfConstSpans(kTooManyChannels)).ok());
}

TEST(Flush, ReturnsFailedPreconditionWhenCalledTwice) {
  MockSampleProcessor mock_resampler(kMaxInputTicks, kNumChannels,
                                     kMaxOutputTicks);
  EXPECT_THAT(mock_resampler.Flush(), IsOk());

  EXPECT_THAT(mock_resampler.Flush(), StatusIs(kFailedPrecondition));
}

}  // namespace
}  // namespace iamf_tools
