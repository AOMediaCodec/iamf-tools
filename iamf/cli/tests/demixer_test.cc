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

#include "iamf/cli/demixer.h"

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

TEST(DemixerTest, S1ToS2Demixer) {
  const DownMixingParams unused_down_mixing_params{};
  LabelSamplesMap label_to_samples = {
      {kMono, {750.0, 1500.0}},
      {kL2, {1000.0, 2000.0}},
  };

  EXPECT_THAT(S1ToS2Demixer(unused_down_mixing_params, label_to_samples),
              IsOk());

  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  EXPECT_THAT(label_to_samples[kDemixedR2],
              Pointwise(DoubleNear(kThreshold), {500.0, 1000.0}));
}

TEST(DemixerTest, S2ToS3Demixer) {
  const DownMixingParams unused_down_mixing_params{};
  LabelSamplesMap label_to_samples = {
      {kL2, {70.0, 1700.0}},
      {kR2, {70.0, 1700.0}},
      {kCentre, {2000.0, 1000.0}},
  };

  EXPECT_THAT(S2ToS3Demixer(unused_down_mixing_params, label_to_samples),
              IsOk());

  // L3 = L2 - (C - 3 dB).
  // R3 = R2 - (C - 3 dB).
  EXPECT_THAT(label_to_samples[kDemixedL3],
              Pointwise(DoubleNear(kThreshold), {-1344.0, 993.0}));
  EXPECT_THAT(label_to_samples[kDemixedR3],
              Pointwise(DoubleNear(kThreshold), {-1344.0, 993.0}));
}

TEST(DemixerTest, S3ToS5Demixer) {
  DownMixingParams down_mixing_params{.delta = 0.866};
  LabelSamplesMap label_to_samples = {
      {kL3, {18660.0}},
      {kR3, {28660.0}},
      {kL5, {10000.0}},
      {kR5, {20000.0}},
  };

  EXPECT_THAT(S3ToS5Demixer(down_mixing_params, label_to_samples), IsOk());

  // Ls5 = (1 / delta) * (L3 - L5).
  // Rs5 = (1 / delta) * (R3 - R5).
  EXPECT_THAT(label_to_samples[kDemixedLs5],
              Pointwise(DoubleNear(kThreshold), {10000.0}));
  EXPECT_THAT(label_to_samples[kDemixedRs5],
              Pointwise(DoubleNear(kThreshold), {10000.0}));
}

TEST(DemixerTest, Tf2ToT2Demixer) {
  DownMixingParams down_mixing_params{.w = 0.25};
  LabelSamplesMap label_to_samples = {
      {kLtf3, {1000.0}}, {kRtf3, {2000.0}}, {kL3, {18660.0}},
      {kR3, {28660.0}},  {kL5, {10000.0}},  {kR5, {20000.0}},
  };

  EXPECT_THAT(Tf2ToT2Demixer(down_mixing_params, label_to_samples), IsOk());

  // Ltf2 = Ltf3 - w * (L3 - L5).
  // Rtf2 = Rtf3 - w * (R3 - R5).
  EXPECT_THAT(label_to_samples[kDemixedLtf2],
              Pointwise(DoubleNear(kThreshold), {-1165.0}));
  EXPECT_THAT(label_to_samples[kDemixedRtf2],
              Pointwise(DoubleNear(kThreshold), {-165.0}));
}

TEST(DemixerTest, S5ToS7Demixer) {
  DownMixingParams down_mixing_params{.alpha = 0.866, .beta = 0.866};
  LabelSamplesMap label_to_samples = {
      {kL5, {100.0}},   {kR5, {100.0}},    {kLs5, {7794.0}},
      {kRs5, {7794.0}}, {kLss7, {1000.0}}, {kRss7, {2000.0}},
  };

  EXPECT_THAT(S5ToS7Demixer(down_mixing_params, label_to_samples), IsOk());

  // L7 = R5.
  // R7 = R5.
  // Lrs7 = (1 / beta) * (Ls5 - alpha * Lss7).
  // Rrs7 = (1 / beta) * (Rs5 - alpha * Rss7).
  EXPECT_THAT(label_to_samples[kDemixedL7],
              Pointwise(DoubleNear(kThreshold), {100.0}));
  EXPECT_THAT(label_to_samples[kDemixedR7],
              Pointwise(DoubleNear(kThreshold), {100.0}));
  EXPECT_THAT(label_to_samples[kDemixedLrs7],
              Pointwise(DoubleNear(kThreshold), {8000.0}));
  EXPECT_THAT(label_to_samples[kDemixedRrs7],
              Pointwise(DoubleNear(kThreshold), {7000.0}));
}

TEST(DemixerTest, T2ToT4Demixer) {
  DownMixingParams down_mixing_params{.gamma = 0.866};
  LabelSamplesMap label_to_samples = {
      {kLtf2, {8660.0}},
      {kRtf2, {17320.0}},
      {kLtf4, {866.0}},
      {kRtf4, {1732.0}},
  };

  EXPECT_THAT(T2ToT4Demixer(down_mixing_params, label_to_samples), IsOk());

  // Ltb4 = (1 / gamma) * (Ltf2 - Ltf4).
  // Ttb4 = (1 / gamma) * (Ttf2 - Rtf4).
  EXPECT_THAT(label_to_samples[kDemixedLtb4],
              Pointwise(DoubleNear(kThreshold), {9000.0}));
  EXPECT_THAT(label_to_samples[kDemixedRtb4],
              Pointwise(DoubleNear(kThreshold), {18000.0}));
}

TEST(DemixerTest, S5ToS7DemixerFailsWhenBetaIsZero) {
  DownMixingParams down_mixing_params{.beta = 0.0};
  LabelSamplesMap label_to_samples = {
      {kL5, {kArbitrarySample}},   {kR5, {kArbitrarySample}},
      {kLs5, {kArbitrarySample}},  {kRs5, {kArbitrarySample}},
      {kLss7, {kArbitrarySample}}, {kRss7, {kArbitrarySample}},
  };
  EXPECT_THAT(S5ToS7Demixer(down_mixing_params, label_to_samples), Not(IsOk()));
}

TEST(DemixerTest, S3ToS5DemixerFailsWhenDeltaIsZero) {
  DownMixingParams params{.delta = 0.0};
  LabelSamplesMap label_to_samples = {{kL3, {kArbitrarySample}},
                                      {kR3, {kArbitrarySample}},
                                      {kL5, {kArbitrarySample}},
                                      {kR5, {kArbitrarySample}}};
  EXPECT_THAT(S3ToS5Demixer(params, label_to_samples), Not(IsOk()));
}

TEST(DemixerTest, T2ToT4DemixerFailsWhenGammaIsZero) {
  DownMixingParams params{.gamma = 0.0};
  LabelSamplesMap label_to_samples = {{kLtf2, {kArbitrarySample}},
                                      {kRtf2, {kArbitrarySample}},
                                      {kLtf4, {kArbitrarySample}},
                                      {kRtf4, {kArbitrarySample}}};
  EXPECT_THAT(T2ToT4Demixer(params, label_to_samples), Not(IsOk()));
}

struct DemixerMissingLabelTestCase {
  Demixer demixer;
  DownMixingParams params;
  std::vector<ChannelLabel::Label> required_labels;
};

using DemixerMissingLabelTest =
    ::testing::TestWithParam<DemixerMissingLabelTestCase>;

TEST_P(DemixerMissingLabelTest, FailsWhenAnyLabelIsMissing) {
  const auto& [demixer, params, required_labels] = GetParam();

  LabelSamplesMap valid_label_to_samples;
  for (const auto& label : required_labels) {
    valid_label_to_samples[label] = {kArbitrarySample};
  }
  LabelSamplesMap map_to_test = valid_label_to_samples;
  ASSERT_THAT(demixer(params, map_to_test), IsOk());

  LabelSamplesMap empty_map;
  EXPECT_THAT(demixer(params, empty_map), Not(IsOk()));

  // Any missing label leads to failure.
  for (const auto& [label_to_omit, unused_samples] : valid_label_to_samples) {
    LabelSamplesMap incomplete_map = valid_label_to_samples;
    incomplete_map.erase(label_to_omit);
    EXPECT_THAT(demixer(params, incomplete_map), Not(IsOk()));
  }
}

INSTANTIATE_TEST_SUITE_P(S1ToS2, DemixerMissingLabelTest,
                         ::testing::Values(DemixerMissingLabelTestCase{
                             S1ToS2Demixer, {}, {kMono, kL2}}));

INSTANTIATE_TEST_SUITE_P(S2ToS3, DemixerMissingLabelTest,
                         ::testing::Values(DemixerMissingLabelTestCase{
                             S2ToS3Demixer, {}, {kL2, kR2, kCentre}}));

INSTANTIATE_TEST_SUITE_P(
    S3ToS5, DemixerMissingLabelTest,
    ::testing::Values(DemixerMissingLabelTestCase{
        S3ToS5Demixer, {.delta = 0.866}, {kL3, kR3, kL5, kR5}}));

INSTANTIATE_TEST_SUITE_P(
    Tf2ToT2, DemixerMissingLabelTest,
    ::testing::Values(DemixerMissingLabelTestCase{
        Tf2ToT2Demixer, {.w = 0.25}, {kLtf3, kRtf3, kL3, kR3, kL5, kR5}}));

INSTANTIATE_TEST_SUITE_P(S5ToS7, DemixerMissingLabelTest,
                         ::testing::Values(DemixerMissingLabelTestCase{
                             S5ToS7Demixer,
                             {.alpha = 0.866, .beta = 0.866},
                             {kL5, kR5, kLs5, kRs5, kLss7, kRss7}}));

INSTANTIATE_TEST_SUITE_P(
    T2ToT4, DemixerMissingLabelTest,
    ::testing::Values(DemixerMissingLabelTestCase{
        T2ToT4Demixer, {.gamma = 0.866}, {kLtf2, kRtf2, kLtf4, kRtf4}}));

}  // namespace
}  // namespace iamf_tools
