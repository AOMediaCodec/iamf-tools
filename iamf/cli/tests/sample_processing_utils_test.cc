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

#include "iamf/cli/sample_processing_utils.h"

#include <cstddef>
#include <limits>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using enum ChannelLabel::Label;
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::Pointwise;

TEST(ArrangeSamples, SucceedsOnEmptyFrame) {
  constexpr size_t kNumChannels = 2;
  std::vector<absl::Span<const InternalSampleType>> samples(kNumChannels);
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples({}, {}, {}, TrimmingSettings{}, samples,
                             num_valid_samples),
              IsOk());

  // `samples` remains the same size (number of channels), but
  // `num_valid_samples` is zero.
  EXPECT_EQ(samples.size(), kNumChannels);
  EXPECT_EQ(num_valid_samples, 0);
}

TEST(ArrangeSamples, ArrangesSamplesInChannelTimeAxes) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{kL2, {0, 1, 2}}, {kR2, {10, 11, 12}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(3, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(
      ArrangeSamples(kStereoLabeledFrame, kStereoArrangement, kEmptyChannel,
                     TrimmingSettings{}, samples, num_valid_samples),
      IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {0.0, 1.0, 2.0}),
                                   Pointwise(DoubleEq(), {10.0, 11.0, 12.0})));
}

TEST(ArrangeSamples, FindsDemixedLabels) {
  const LabeledFrame kDemixedTwoLayerStereoFrame = {
      .label_to_samples = {{kMono, {75}}, {kL2, {50}}, {kDemixedR2, {100}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kDemixedTwoLayerStereoFrame, kStereoArrangement,
                             kEmptyChannel, TrimmingSettings{}, samples,
                             num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {50.0}),
                                   Pointwise(DoubleEq(), {100.0})));
}

TEST(ArrangeSamples, IgnoresExtraLabels) {
  const LabeledFrame kStereoLabeledFrameWithExtraLabel = {
      .label_to_samples = {{kL2, {0}}, {kR2, {10}}, {kLFE, {999}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kStereoLabeledFrameWithExtraLabel,
                             kStereoArrangement, kEmptyChannel,
                             TrimmingSettings{}, samples, num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {0.0}),
                                   Pointwise(DoubleEq(), {10.0})));
}

TEST(ArrangeSamples, LeavesOmittedLabelsZeroForMixedOrderAmbisonics) {
  const LabeledFrame kMixedFirstOrderAmbisonicsFrame = {
      .label_to_samples = {
          {kA0, {1, 2}}, {kA2, {201, 202}}, {kA3, {301, 302}}}};
  const std::vector<ChannelLabel::Label> kMixedFirstOrderAmbisonicsArrangement =
      {kA0, kOmitted, kA2, kA3};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kMixedFirstOrderAmbisonicsArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(
      ArrangeSamples(kMixedFirstOrderAmbisonicsFrame,
                     kMixedFirstOrderAmbisonicsArrangement, kEmptyChannel,
                     TrimmingSettings{}, samples, num_valid_samples),
      IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {1.0, 2.0}),
                                   Pointwise(DoubleEq(), {0.0, 0.0}),
                                   Pointwise(DoubleEq(), {201.0, 202.0}),
                                   Pointwise(DoubleEq(), {301.0, 302.0})));
}

TEST(ArrangeSamples, LeavesOmittedLabelsZeroForChannelBasedLayout) {
  const LabeledFrame kLFEOnlyFrame = {.label_to_samples = {{kLFE, {1, 2}}}};
  const std::vector<ChannelLabel::Label> kLFEAsSecondChannelArrangement = {
      kOmitted, kOmitted, kLFE, kOmitted};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kLFEAsSecondChannelArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kLFEOnlyFrame, kLFEAsSecondChannelArrangement,
                             kEmptyChannel, TrimmingSettings{}, samples,
                             num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {0.0, 0.0}),
                                   Pointwise(DoubleEq(), {0.0, 0.0}),
                                   Pointwise(DoubleEq(), {1.0, 2.0}),
                                   Pointwise(DoubleEq(), {0.0, 0.0})));
}

TEST(ArrangeSamples, FailsWithOnlyOmittedLabels) {
  const LabeledFrame kFrame = {.label_to_samples = {}};
  constexpr size_t kNumChannels = 1;
  constexpr size_t kNumSamples = 100;
  const std::vector<ChannelLabel::Label> kOmittedOnlyArrangement = {kOmitted};
  const std::vector<InternalSampleType> kEmptyChannel(kNumSamples, 0.0);
  std::vector<absl::Span<const InternalSampleType>> samples(kNumChannels);
  size_t num_valid_samples = 0;

  EXPECT_THAT(ArrangeSamples(kFrame, kOmittedOnlyArrangement, kEmptyChannel,
                             TrimmingSettings{}, samples, num_valid_samples),
              Not(IsOk()));
}

TEST(ArrangeSamples, ExcludesSamplesToBeTrimmed) {
  const LabeledFrame kMonoLabeledFrameWithSamplesToTrim = {
      .samples_to_trim_at_end = 2,
      .samples_to_trim_at_start = 1,
      .label_to_samples = {{kMono, {999, 100, 999, 999}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(4, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kMonoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kMonoLabeledFrameWithSamplesToTrim,
                             kMonoArrangement, kEmptyChannel,
                             TrimmingSettings{}, samples, num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {100.0})));
}

TEST(ArrangeSamples, OverwritesInputVector) {
  const LabeledFrame kMonoLabeledFrame = {
      .label_to_samples = {{kMono, {1, 2}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  const std::vector<InternalSampleType> original_input_samples = {999, 999};
  std::vector<absl::Span<const InternalSampleType>> samples = {
      absl::MakeConstSpan(original_input_samples)};
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kMonoLabeledFrame, kMonoArrangement, kEmptyChannel,
                             TrimmingSettings{}, samples, num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {1.0, 2.0})));
}

TEST(ArrangeSamples, TrimmingAllFramesFromStartIsResultsInEmptyChannels) {
  const LabeledFrame kMonoLabeledFrameWithSamplesToTrim = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = 4,
      .label_to_samples = {{kMono, {999, 999, 999, 999}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(4, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kMonoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kMonoLabeledFrameWithSamplesToTrim,
                             kMonoArrangement, kEmptyChannel,
                             TrimmingSettings{}, samples, num_valid_samples),
              IsOk());
  for (const auto& channel : samples) {
    EXPECT_TRUE(channel.empty());
  }
}

TEST(ArrangeSamples, InvalidWhenRequestedLabelsHaveDifferentNumberOfSamples) {
  const LabeledFrame kStereoLabeledFrameWithMissingSample = {
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kStereoLabeledFrameWithMissingSample,
                             kStereoArrangement, kEmptyChannel,
                             TrimmingSettings{}, samples, num_valid_samples),
              Not(IsOk()));
}

TEST(ArrangeSamples, InvalidWhenEmptyChannelHasTooFewSamples) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10, 11}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};

  // Other labels have two samples, but the empty channel has only one.
  const std::vector<InternalSampleType> kEmptyChannelWithTooManySamples(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kStereoLabeledFrame, kStereoArrangement,
                             kEmptyChannelWithTooManySamples,
                             TrimmingSettings{}, samples, num_valid_samples),
              Not(IsOk()));
}

TEST(ArrangeSamples, InvalidWhenTrimIsImplausible) {
  const LabeledFrame kFrameWithExcessSamplesTrimmed = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 2,
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10, 11}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamples(kFrameWithExcessSamplesTrimmed, kStereoArrangement,
                             kEmptyChannel, TrimmingSettings{}, samples,
                             num_valid_samples),
              Not(IsOk()));
}

TEST(ArrangeSamples, InvalidWhenTrimOverflows) {
  // Set trim values such that their sum overflows a `DecodedUleb128`.
  const LabeledFrame kFrameWithExcessSamplesTrimmed = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = std::numeric_limits<DecodedUleb128>::max(),
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10, 11}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);
  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;

  EXPECT_THAT(ArrangeSamples(kFrameWithExcessSamplesTrimmed, kStereoArrangement,
                             kEmptyChannel, TrimmingSettings{}, samples,
                             num_valid_samples),
              Not(IsOk()));
}

TEST(ArrangeSamples, InvalidMissingLabel) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{kL2, {0}}, {kR2, {10}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> unused_samples(
      kMonoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(
      ArrangeSamples(kStereoLabeledFrame, kMonoArrangement, kEmptyChannel,
                     TrimmingSettings{}, unused_samples, num_valid_samples),
      Not(IsOk()));
}

TEST(ArrangeSamples, IncludesStartSamplesWhenTrimBeginningIsFalse) {
  const LabeledFrame kFrame = {.samples_to_trim_at_end = 0,
                               .samples_to_trim_at_start = 1,
                               .label_to_samples = {{kMono, {999, 100}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);
  std::vector<absl::Span<const InternalSampleType>> samples(1);
  size_t num_valid_samples = 0;

  EXPECT_THAT(ArrangeSamples(kFrame, kMonoArrangement, kEmptyChannel,
                             TrimmingSettings{
                                 .trim_beginning = false,
                             },
                             samples, num_valid_samples),
              IsOk());

  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {999.0, 100.0})));
}

TEST(ArrangeSamples, IncludesEndSamplesWhenTrimEndIsFalse) {
  const LabeledFrame kFrame = {.samples_to_trim_at_end = 1,
                               .samples_to_trim_at_start = 0,
                               .label_to_samples = {{kMono, {100, 999}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);
  std::vector<absl::Span<const InternalSampleType>> samples(1);
  size_t num_valid_samples = 0;

  EXPECT_THAT(ArrangeSamples(kFrame, kMonoArrangement, kEmptyChannel,
                             TrimmingSettings{
                                 .trim_end = false,
                             },
                             samples, num_valid_samples),
              IsOk());

  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {100.0, 999.0})));
}

TEST(ArrangeSamples, IncludesAllSamplesWhenTrimmingIsDisabled) {
  const LabeledFrame kFrame = {.samples_to_trim_at_end = 1,
                               .samples_to_trim_at_start = 1,
                               .label_to_samples = {{kMono, {999, 100, 999}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(3, 0.0);
  std::vector<absl::Span<const InternalSampleType>> samples(1);
  size_t num_valid_samples = 0;

  EXPECT_THAT(ArrangeSamples(kFrame, kMonoArrangement, kEmptyChannel,
                             TrimmingSettings{
                                 .trim_beginning = false,
                                 .trim_end = false,
                             },
                             samples, num_valid_samples),
              IsOk());

  EXPECT_THAT(samples,
              ElementsAre(Pointwise(DoubleEq(), {999.0, 100.0, 999.0})));
}

}  // namespace
}  // namespace iamf_tools
