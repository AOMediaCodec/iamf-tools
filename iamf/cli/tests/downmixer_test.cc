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

#include "iamf/cli/downmixer.h"

#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::DoubleNear;
using ::testing::Not;
using ::testing::Pointwise;

using enum ChannelLabel::Label;

constexpr InternalSampleType kArbitrarySample = 100.0;
constexpr double kThreshold = 1e-6;

TEST(DownMixerTest, S7ToS5DownMixer) {
  DownMixingParams params{.alpha = 1.0, .beta = 0.866};
  LabelSamplesMap label_to_samples = {
      {kL7, {1.0}},      {kR7, {2.0}},      {kLss7, {1000.0}},
      {kRss7, {2000.0}}, {kLrs7, {3000.0}}, {kRrs7, {4000.0}},
  };

  EXPECT_THAT(S7ToS5DownMixer(params, label_to_samples), IsOk());

  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  EXPECT_THAT(label_to_samples[kL5], Pointwise(DoubleNear(kThreshold), {1.0}));
  EXPECT_THAT(label_to_samples[kR5], Pointwise(DoubleNear(kThreshold), {2.0}));
  EXPECT_THAT(label_to_samples[kLs5],
              Pointwise(DoubleNear(kThreshold), {3598.0}));
  EXPECT_THAT(label_to_samples[kRs5],
              Pointwise(DoubleNear(kThreshold), {5464.0}));
}

TEST(DownMixerTest, S5ToS3DownMixer) {
  DownMixingParams params{.delta = 0.707};
  LabelSamplesMap label_to_samples = {
      {kL5, {1000.0}},
      {kR5, {2000.0}},
      {kLs5, {4000.0}},
      {kRs5, {8000.0}},
  };

  EXPECT_THAT(S5ToS3DownMixer(params, label_to_samples), IsOk());

  // L3 = L5 + Ls5 * delta.
  EXPECT_THAT(label_to_samples[kL3],
              Pointwise(DoubleNear(kThreshold), {3828.0}));
  EXPECT_THAT(label_to_samples[kR3],
              Pointwise(DoubleNear(kThreshold), {7656.0}));
}

TEST(DownMixerTest, S3ToS2DownMixer) {
  const DownMixingParams unused_params{};
  LabelSamplesMap label_to_samples = {
      {kL3, {0.0, 100.0}},
      {kR3, {0.0, 100.0}},
      {kCentre, {100.0, 100.0}},
  };

  EXPECT_THAT(S3ToS2DownMixer(unused_params, label_to_samples), IsOk());

  // L2 = L3 + (C - 3 dB).
  // R2 = R3 + (C - 3 dB).
  EXPECT_THAT(label_to_samples[kL2],
              Pointwise(DoubleNear(kThreshold), {70.7, 170.7}));
  EXPECT_THAT(label_to_samples[kR2],
              Pointwise(DoubleNear(kThreshold), {70.7, 170.7}));
}

TEST(DownMixerTest, S2ToS1DownMixer) {
  const DownMixingParams unused_params{};
  LabelSamplesMap label_to_samples = {
      {kL2, {0.0, 100.0, 500.0, 1000.0}},
      {kR2, {100.0, 0.0, 500.0, 500.0}},
  };

  EXPECT_THAT(S2ToS1DownMixer(unused_params, label_to_samples), IsOk());

  // M = (L2 - 6 dB) + (R2 - 6 dB).
  EXPECT_THAT(label_to_samples[kMono],
              Pointwise(DoubleNear(kThreshold), {50.0, 50.0, 500.0, 750.0}));
}

TEST(DownMixerTest, T4ToT2DownMixer) {
  DownMixingParams params{.gamma = 0.707};
  LabelSamplesMap label_to_samples = {
      {kLtf4, {1000.0}},
      {kRtf4, {2000.0}},
      {kLtb4, {1000.0}},
      {kRtb4, {2000.0}},
  };

  EXPECT_THAT(T4ToT2DownMixer(params, label_to_samples), IsOk());

  // Ltf2 = Ltf4 + Ltb4 * gamma.
  EXPECT_THAT(label_to_samples[kLtf2],
              Pointwise(DoubleNear(kThreshold), {1707.0}));
  EXPECT_THAT(label_to_samples[kRtf2],
              Pointwise(DoubleNear(kThreshold), {3414.0}));
}

TEST(DownMixerTest, T2ToTf2DownMixer) {
  DownMixingParams params{.delta = 0.707, .w = 0.25};
  LabelSamplesMap label_to_samples = {
      {kLtf2, {1000.0}},
      {kRtf2, {2000.0}},
      {kLs5, {4000.0}},
      {kRs5, {8000.0}},
  };

  EXPECT_THAT(T2ToTf2DownMixer(params, label_to_samples), IsOk());

  // Ltf3 = Ltf2 + Ls5 * w * delta.
  EXPECT_THAT(label_to_samples[kLtf3],
              Pointwise(DoubleNear(kThreshold), {1707.0}));
  EXPECT_THAT(label_to_samples[kRtf3],
              Pointwise(DoubleNear(kThreshold), {3414.0}));
}

struct DownMixerMissingLabelTestCase {
  DownMixer down_mixer;
  DownMixingParams params;
  std::vector<ChannelLabel::Label> required_labels;
};

using DownMixerMissingLabelTest =
    ::testing::TestWithParam<DownMixerMissingLabelTestCase>;

TEST_P(DownMixerMissingLabelTest, FailsWhenAnyLabelIsMissing) {
  const auto& [down_mixer, params, required_labels] = GetParam();
  LabelSamplesMap valid_label_to_samples;
  for (const auto& label : required_labels) {
    valid_label_to_samples[label] = {kArbitrarySample};
  }
  LabelSamplesMap map_to_test = valid_label_to_samples;

  ASSERT_THAT(down_mixer(params, map_to_test), IsOk());

  LabelSamplesMap empty_map;
  EXPECT_THAT(down_mixer(params, empty_map), Not(IsOk()));

  // Any missing label leads to failure.
  for (const auto& [label_to_omit, unused_samples] : valid_label_to_samples) {
    LabelSamplesMap incomplete_map = valid_label_to_samples;
    incomplete_map.erase(label_to_omit);
    EXPECT_THAT(down_mixer(params, incomplete_map), Not(IsOk()));
  }
}

INSTANTIATE_TEST_SUITE_P(S7ToS5, DownMixerMissingLabelTest,
                         ::testing::Values(DownMixerMissingLabelTestCase{
                             S7ToS5DownMixer,
                             {.alpha = 1.0, .beta = 0.5},
                             {kL7, kR7, kLss7, kLrs7, kRss7, kRrs7}}));

INSTANTIATE_TEST_SUITE_P(
    S5ToS3, DownMixerMissingLabelTest,
    ::testing::Values(DownMixerMissingLabelTestCase{
        S5ToS3DownMixer, {.delta = 0.5}, {kL5, kR5, kLs5, kRs5}}));

INSTANTIATE_TEST_SUITE_P(S3ToS2, DownMixerMissingLabelTest,
                         ::testing::Values(DownMixerMissingLabelTestCase{
                             S3ToS2DownMixer, {}, {kL3, kR3, kCentre}}));

INSTANTIATE_TEST_SUITE_P(S2ToS1, DownMixerMissingLabelTest,
                         ::testing::Values(DownMixerMissingLabelTestCase{
                             S2ToS1DownMixer, {}, {kL2, kR2}}));

INSTANTIATE_TEST_SUITE_P(
    T4ToT2, DownMixerMissingLabelTest,
    ::testing::Values(DownMixerMissingLabelTestCase{
        T4ToT2DownMixer, {.gamma = 0.5}, {kLtf4, kRtf4, kLtb4, kRtb4}}));

INSTANTIATE_TEST_SUITE_P(T2ToTf2, DownMixerMissingLabelTest,
                         ::testing::Values(DownMixerMissingLabelTestCase{
                             T2ToTf2DownMixer,
                             {.delta = 0.5, .w = 0.25},
                             {kLtf2, kRtf2, kLs5, kRs5}}));

}  // namespace
}  // namespace iamf_tools
