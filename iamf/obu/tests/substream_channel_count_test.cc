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
#include "iamf/obu/substream_channel_count.h"

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

TEST(MakeSingular, GetterReturnsOneChannel) {
  const auto channel_count = SubstreamChannelCount::MakeSingular();

  EXPECT_EQ(channel_count.num_channels(), 1);
}

TEST(CreateCoupled, GetterReturnsTwoChannels) {
  const auto channel_count = SubstreamChannelCount::MakeCoupled();

  EXPECT_EQ(channel_count.num_channels(), 2);
}

TEST(Create, SucceedsForOneChannel) {
  auto result = SubstreamChannelCount::Create(1);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->num_channels(), 1);
}

TEST(Create, SucceedsForTwoChannels) {
  auto result = SubstreamChannelCount::Create(2);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->num_channels(), 2);
}

TEST(Create, FailsForZeroChannels) {
  EXPECT_THAT(SubstreamChannelCount::Create(0), Not(IsOk()));
}

TEST(Create, FailsForThreeChannels) {
  EXPECT_THAT(SubstreamChannelCount::Create(3), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
