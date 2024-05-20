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
#include "iamf/cli/loudness_calculator.h"

#include <cstdint>
#include <limits>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

const LoudnessInfo kLoudnessInfo = {
    .info_type = LoudnessInfo::kTruePeak | LoudnessInfo::kAnchoredLoudness,
    .integrated_loudness = 100,
    .digital_peak = 200,
    .true_peak = 300,
    .anchored_loudness = {
        .num_anchored_loudness = 1,
        .anchor_elements = {
            {AnchoredLoudnessElement::kAnchorElementDialogue, 400}}}};

TEST(LoudnessCalculatorUserProvidedLoudness,
     AccumulateLoudnessForSamplesAlwaysReturnsOk) {
  LoudnessCalculatorUserProvidedLoudness calculator(kLoudnessInfo);

  EXPECT_THAT(calculator.AccumulateLoudnessForSamples({1, 2, 3, 4}), IsOk());
  EXPECT_THAT(calculator.AccumulateLoudnessForSamples({}), IsOk());
  EXPECT_THAT(calculator.AccumulateLoudnessForSamples(
                  {std::numeric_limits<int32_t>::max()}),
              IsOk());
}

TEST(LoudnessCalculatorUserProvidedLoudness, QueryUserLoudnessAlwaysReturnsOk) {
  LoudnessCalculatorUserProvidedLoudness calculator(kLoudnessInfo);

  EXPECT_THAT(calculator.QueryLoudness(), IsOk());
}

TEST(LoudnessCalculatorUserProvidedLoudness,
     QueryUserLoudnessAlwaysReturnsInputLoudness) {
  LoudnessCalculatorUserProvidedLoudness calculator(kLoudnessInfo);

  EXPECT_EQ(*calculator.QueryLoudness(), kLoudnessInfo);
}

TEST(LoudnessCalculatorUserProvidedLoudness, IgnoresAccumulatedSamples) {
  LoudnessCalculatorUserProvidedLoudness calculator(kLoudnessInfo);

  EXPECT_THAT(calculator.AccumulateLoudnessForSamples({1, 2, 3, 4}), IsOk());
  EXPECT_THAT(calculator.AccumulateLoudnessForSamples({99999}), IsOk());
  EXPECT_EQ(*calculator.QueryLoudness(), kLoudnessInfo);
}

}  // namespace
}  // namespace iamf_tools
