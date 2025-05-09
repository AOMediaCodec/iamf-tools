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

#include "iamf/cli/renderer/renderer_utils.h"

#include <cstddef>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace renderer_utils {
namespace {

using ::absl_testing::IsOk;
using enum ChannelLabel::Label;
using testing::DoubleEq;
using testing::Pointwise;

TEST(ArrangeSamplesToRender, SucceedsOnEmptyFrame) {
  constexpr size_t kNumChannels = 2;
  std::vector<absl::Span<const InternalSampleType>> samples(kNumChannels);
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender({}, {}, {}, samples, num_valid_samples),
              IsOk());

  // `samples` remains the same size (number of channels), but
  // `num_valid_samples` is zero.
  EXPECT_EQ(samples.size(), kNumChannels);
  EXPECT_EQ(num_valid_samples, 0);
}

TEST(ArrangeSamplesToRender, ArrangesSamplesInChannelTimeAxes) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{kL2, {0, 1, 2}}, {kR2, {10, 11, 12}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(3, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender(kStereoLabeledFrame, kStereoArrangement,
                                     kEmptyChannel, samples, num_valid_samples),
              IsOk());
  EXPECT_THAT(samples,
              testing::ElementsAre(Pointwise(DoubleEq(), {0.0, 1.0, 2.0}),
                                   Pointwise(DoubleEq(), {10.0, 11.0, 12.0})));
}

TEST(ArrangeSamplesToRender, FindsDemixedLabels) {
  const LabeledFrame kDemixedTwoLayerStereoFrame = {
      .label_to_samples = {{kMono, {75}}, {kL2, {50}}, {kDemixedR2, {100}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(
      ArrangeSamplesToRender(kDemixedTwoLayerStereoFrame, kStereoArrangement,
                             kEmptyChannel, samples, num_valid_samples),
      IsOk());
  EXPECT_THAT(samples, testing::ElementsAre(Pointwise(DoubleEq(), {50.0}),
                                            Pointwise(DoubleEq(), {100.0})));
}

TEST(ArrangeSamplesToRender, IgnoresExtraLabels) {
  const LabeledFrame kStereoLabeledFrameWithExtraLabel = {
      .label_to_samples = {{kL2, {0}}, {kR2, {10}}, {kLFE, {999}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender(kStereoLabeledFrameWithExtraLabel,
                                     kStereoArrangement, kEmptyChannel, samples,
                                     num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, testing::ElementsAre(Pointwise(DoubleEq(), {0.0}),
                                            Pointwise(DoubleEq(), {10.0})));
}

TEST(ArrangeSamplesToRender, LeavesOmittedLabelsZeroForMixedOrderAmbisonics) {
  const LabeledFrame kMixedFirstOrderAmbisonicsFrame = {
      .label_to_samples = {
          {kA0, {1, 2}}, {kA2, {201, 202}}, {kA3, {301, 302}}}};
  const std::vector<ChannelLabel::Label> kMixedFirstOrderAmbisonicsArrangement =
      {kA0, kOmitted, kA2, kA3};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kMixedFirstOrderAmbisonicsArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender(kMixedFirstOrderAmbisonicsFrame,
                                     kMixedFirstOrderAmbisonicsArrangement,
                                     kEmptyChannel, samples, num_valid_samples),
              IsOk());
  EXPECT_THAT(samples,
              testing::ElementsAre(Pointwise(DoubleEq(), {1.0, 2.0}),
                                   Pointwise(DoubleEq(), {0.0, 0.0}),
                                   Pointwise(DoubleEq(), {201.0, 202.0}),
                                   Pointwise(DoubleEq(), {301.0, 302.0})));
}

TEST(ArrangeSamplesToRender, LeavesOmittedLabelsZeroForChannelBasedLayout) {
  const LabeledFrame kLFEOnlyFrame = {.label_to_samples = {{kLFE, {1, 2}}}};
  const std::vector<ChannelLabel::Label> kLFEAsSecondChannelArrangement = {
      kOmitted, kOmitted, kLFE, kOmitted};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kLFEAsSecondChannelArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(
      ArrangeSamplesToRender(kLFEOnlyFrame, kLFEAsSecondChannelArrangement,
                             kEmptyChannel, samples, num_valid_samples),
      IsOk());
  EXPECT_THAT(samples, testing::ElementsAre(Pointwise(DoubleEq(), {0.0, 0.0}),
                                            Pointwise(DoubleEq(), {0.0, 0.0}),
                                            Pointwise(DoubleEq(), {1.0, 2.0}),
                                            Pointwise(DoubleEq(), {0.0, 0.0})));
}

TEST(ArrangeSamplesToRender, ExcludesSamplesToBeTrimmed) {
  const LabeledFrame kMonoLabeledFrameWithSamplesToTrim = {
      .samples_to_trim_at_end = 2,
      .samples_to_trim_at_start = 1,
      .label_to_samples = {{kMono, {999, 100, 999, 999}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(4, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kMonoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender(kMonoLabeledFrameWithSamplesToTrim,
                                     kMonoArrangement, kEmptyChannel, samples,
                                     num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, testing::ElementsAre(Pointwise(DoubleEq(), {100.0})));
}

TEST(ArrangeSamplesToRender, OverwritesInputVector) {
  const LabeledFrame kMonoLabeledFrame = {
      .label_to_samples = {{kMono, {1, 2}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  const std::vector<InternalSampleType> original_input_samples = {999, 999};
  std::vector<absl::Span<const InternalSampleType>> samples = {
      absl::MakeConstSpan(original_input_samples)};
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender(kMonoLabeledFrame, kMonoArrangement,
                                     kEmptyChannel, samples, num_valid_samples),
              IsOk());
  EXPECT_THAT(samples, testing::ElementsAre(Pointwise(DoubleEq(), {1.0, 2.0})));
}

TEST(ArrangeSamplesToRender,
     TrimmingAllFramesFromStartIsResultsInEmptyChannels) {
  const LabeledFrame kMonoLabeledFrameWithSamplesToTrim = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = 4,
      .label_to_samples = {{kMono, {999, 999, 999, 999}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(4, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kMonoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_THAT(ArrangeSamplesToRender(kMonoLabeledFrameWithSamplesToTrim,
                                     kMonoArrangement, kEmptyChannel, samples,
                                     num_valid_samples),
              IsOk());
  for (const auto& channel : samples) {
    EXPECT_TRUE(channel.empty());
  }
}

TEST(ArrangeSamplesToRender,
     InvalidWhenRequestedLabelsHaveDifferentNumberOfSamples) {
  const LabeledFrame kStereoLabeledFrameWithMissingSample = {
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_FALSE(ArrangeSamplesToRender(kStereoLabeledFrameWithMissingSample,
                                      kStereoArrangement, kEmptyChannel,
                                      samples, num_valid_samples)
                   .ok());
}

TEST(ArrangeSamplesToRender, InvalidWhenEmptyChannelHasTooFewSamples) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10, 11}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};

  // Other labels have two samples, but the empty channel has only one.
  const std::vector<InternalSampleType> kEmptyChannelWithTooManySamples(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_FALSE(ArrangeSamplesToRender(kStereoLabeledFrame, kStereoArrangement,
                                      kEmptyChannelWithTooManySamples, samples,
                                      num_valid_samples)
                   .ok());
}

TEST(ArrangeSamplesToRender, InvalidWhenTrimIsImplausible) {
  const LabeledFrame kFrameWithExcessSamplesTrimmed = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 2,
      .label_to_samples = {{kL2, {0, 1}}, {kR2, {10, 11}}}};
  const std::vector<ChannelLabel::Label> kStereoArrangement = {kL2, kR2};
  const std::vector<InternalSampleType> kEmptyChannel(2, 0.0);

  std::vector<absl::Span<const InternalSampleType>> samples(
      kStereoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_FALSE(ArrangeSamplesToRender(kFrameWithExcessSamplesTrimmed,
                                      kStereoArrangement, kEmptyChannel,
                                      samples, num_valid_samples)
                   .ok());
}

TEST(ArrangeSamplesToRender, InvalidMissingLabel) {
  const LabeledFrame kStereoLabeledFrame = {
      .label_to_samples = {{kL2, {0}}, {kR2, {10}}}};
  const std::vector<ChannelLabel::Label> kMonoArrangement = {kMono};
  const std::vector<InternalSampleType> kEmptyChannel(1, 0.0);

  std::vector<absl::Span<const InternalSampleType>> unused_samples(
      kMonoArrangement.size());
  size_t num_valid_samples = 0;
  EXPECT_FALSE(ArrangeSamplesToRender(kStereoLabeledFrame, kMonoArrangement,
                                      kEmptyChannel, unused_samples,
                                      num_valid_samples)
                   .ok());
}

TEST(LookupOutputKeyFromPlaybackLayout, SucceedsForChannelBasedLayout) {
  EXPECT_THAT(
      LookupOutputKeyFromPlaybackLayout(
          {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
           .specific_layout =
               LoudspeakersSsConventionLayout{
                   .sound_system =
                       LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}}),
      IsOk());
}

TEST(LookupOutputKeyFromPlaybackLayout, SucceedsFor9_1_6) {
  EXPECT_THAT(
      LookupOutputKeyFromPlaybackLayout(
          {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
           .specific_layout =
               LoudspeakersSsConventionLayout{
                   .sound_system =
                       LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0}}),
      IsOk());
}
TEST(LookupOutputKeyFromPlaybackLayout, FailsOnBinauralBasedLayout) {
  EXPECT_FALSE(LookupOutputKeyFromPlaybackLayout(
                   {.layout_type = Layout::kLayoutTypeBinaural,
                    .specific_layout = LoudspeakersReservedOrBinauralLayout{}})
                   .ok());
}

TEST(LookupOutputKeyFromPlaybackLayout, FailsOnReservedLayout) {
  EXPECT_FALSE(LookupOutputKeyFromPlaybackLayout(
                   {.layout_type = Layout::kLayoutTypeReserved0,
                    .specific_layout = LoudspeakersReservedOrBinauralLayout{}})
                   .ok());
}

}  // namespace
}  // namespace renderer_utils
}  // namespace iamf_tools
