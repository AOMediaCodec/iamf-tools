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

#include "iamf/cli/resampler_base.h"

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

constexpr uint32_t kMaxNumSamplesPerFrame = 4;
constexpr size_t kNumChannels = 2;

class MockResampler : public ResamplerBase {
 public:
  MockResampler(uint32_t max_num_samples_per_frame, size_t num_channels)
      : ResamplerBase(max_num_samples_per_frame, num_channels) {}

  MOCK_METHOD(absl::Status, PushFrame,
              (absl::Span<const std::vector<int32_t>> time_channel_samples),
              (override));

  MOCK_METHOD(absl::Status, Flush, (), (override));
};

TEST(GetOutputSamplesAsSpan, ReturnsEmptyAfterConstruction) {
  MockResampler mock_resampler(kMaxNumSamplesPerFrame, kNumChannels);
  EXPECT_TRUE(mock_resampler.GetOutputSamplesAsSpan().empty());
}

TEST(GetOutputSamplesAsSpan, SizeMatchesNumValidTicks) {
  EverySecondTickResampler every_second_tick_resampler(kMaxNumSamplesPerFrame,
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

}  // namespace
}  // namespace iamf_tools
