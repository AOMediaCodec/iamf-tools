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
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelLabel::Label;
using testing::DoubleEq;
using testing::ElementsAre;
using testing::ElementsAreArray;
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {0.0, 1.0, 2.0}),
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {50.0}),
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {0.0}),
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {1.0, 2.0}),
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {0.0, 0.0}),
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {100.0})));
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
  EXPECT_THAT(samples, ElementsAre(Pointwise(DoubleEq(), {1.0, 2.0})));
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

using AmbisonicsOrderTest = ::testing::TestWithParam<int>;
TEST_P(AmbisonicsOrderTest, SucceedsOnPerfectSquaredChannelCount) {
  const int expected_order = GetParam();
  const auto channel_count =
      static_cast<uint8_t>((expected_order + 1) * (expected_order + 1));
  int actual_order;
  EXPECT_TRUE(GetAmbisonicsOrder(channel_count, actual_order).ok());
  EXPECT_EQ(actual_order, expected_order);
}

TEST_P(AmbisonicsOrderTest, FailsOnNonPerfectSquaredChannelCount) {
  // In this test we check every number between (N - 1)^2 and N^2.
  const int order = GetParam();
  if (order == 0) {
    return;
  }
  const int previous_order = order - 1;
  for (int i = (previous_order + 1) * (previous_order + 1) + 1;
       i < (order + 1) * (order + 1); i++) {
    const auto channel_count = static_cast<uint8_t>(i);
    int actual_order;
    EXPECT_FALSE(GetAmbisonicsOrder(channel_count, actual_order).ok());
  }
}
INSTANTIATE_TEST_SUITE_P(GetAmbisonicsOrderForInRangeChannelCount,
                         AmbisonicsOrderTest, testing::Range(0, 15));

using ChannelCountTest = ::testing::TestWithParam<uint8_t>;
TEST_P(ChannelCountTest, FailsOnTooLargeChannelCount) {
  const uint8_t channel_count = GetParam();
  int actual_order;
  EXPECT_FALSE(GetAmbisonicsOrder(channel_count, actual_order).ok());
}
INSTANTIATE_TEST_SUITE_P(GetAmbisonicsOrderForOutRangeChannelCount,
                         ChannelCountTest, testing::Range<uint8_t>(226, 255));

TEST(GetChannelLabelsForAmbisonicsTest, FullZerothOrderAmbisonicsMono) {
  const AmbisonicsConfig kFullZerothOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config = AmbisonicsMonoConfig{.output_channel_count = 1,
                                                .substream_count = 1,
                                                .channel_mapping = {0}}};
  const std::vector<DecodedUleb128> kFullZerothOrderAudioSubstreamIds = {100};
  const SubstreamIdLabelsMap kZerothOrderSubstreamIdToLabels = {{100, {kA0}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(kFullZerothOrderAmbisonicsConfig,
                                            kFullZerothOrderAudioSubstreamIds,
                                            kZerothOrderSubstreamIdToLabels,
                                            channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels, ElementsAre(kA0));
}

TEST(GetChannelLabelsForAmbisonicsTest, FullFirstOrderAmbisonicsMono) {
  const AmbisonicsConfig kFullFirstOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config =
          AmbisonicsMonoConfig{.output_channel_count = 4,
                               .substream_count = 4,
                               .channel_mapping = {0, 1, 2, 3}}};
  const std::vector<DecodedUleb128> kFullFirstOrderAudioSubstreamIds = {
      100, 101, 102, 103};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {100, {kA0}}, {101, {kA1}}, {102, {kA2}}, {103, {kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(kFullFirstOrderAmbisonicsConfig,
                                            kFullFirstOrderAudioSubstreamIds,
                                            kFirstOrderSubstreamIdToLabels,
                                            channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels, ElementsAre(kA0, kA1, kA2, kA3));
}

TEST(GetChannelLabelsForAmbisonicsTest, FullSecondOrderAmbisonicsMono) {
  const AmbisonicsConfig kFullSecondOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config =
          AmbisonicsMonoConfig{.output_channel_count = 9,
                               .substream_count = 9,
                               .channel_mapping = {0, 1, 2, 3, 4, 5, 6, 7, 8}}};
  const std::vector<DecodedUleb128> kFullSecondOrderAudioSubstreamIds = {
      100, 101, 102, 103, 104, 105, 106, 107, 108};
  const SubstreamIdLabelsMap kSecondOrderSubstreamIdToLabels = {
      {100, {kA0}}, {101, {kA1}}, {102, {kA2}}, {103, {kA3}}, {104, {kA4}},
      {105, {kA5}}, {106, {kA6}}, {107, {kA7}}, {108, {kA8}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(kFullSecondOrderAmbisonicsConfig,
                                            kFullSecondOrderAudioSubstreamIds,
                                            kSecondOrderSubstreamIdToLabels,
                                            channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels,
              ElementsAre(kA0, kA1, kA2, kA3, kA4, kA5, kA6, kA7, kA8));
}

TEST(GetChannelLabelsForAmbisonicsTest, FullThirdOrderAmbisonicsMono) {
  const AmbisonicsConfig kFullThirdOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config =
          AmbisonicsMonoConfig{.output_channel_count = 16,
                               .substream_count = 16,
                               .channel_mapping = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                                   10, 11, 12, 13, 14, 15}}};
  const std::vector<DecodedUleb128> kFullThirdOrderAudioSubstreamIds = {
      100, 101, 102, 103, 104, 105, 106, 107,
      108, 109, 110, 111, 112, 113, 114, 115};
  const SubstreamIdLabelsMap kThirdOrderSubstreamIdToLabels = {
      {100, {kA0}},  {101, {kA1}},  {102, {kA2}},  {103, {kA3}},
      {104, {kA4}},  {105, {kA5}},  {106, {kA6}},  {107, {kA7}},
      {108, {kA8}},  {109, {kA9}},  {110, {kA10}}, {111, {kA11}},
      {112, {kA12}}, {113, {kA13}}, {114, {kA14}}, {115, {kA15}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(kFullThirdOrderAmbisonicsConfig,
                                            kFullThirdOrderAudioSubstreamIds,
                                            kThirdOrderSubstreamIdToLabels,
                                            channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels,
              ElementsAreArray({kA0, kA1, kA2, kA3, kA4, kA5, kA6, kA7, kA8,
                                kA9, kA10, kA11, kA12, kA13, kA14, kA15}));
}

TEST(GetChannelLabelsForAmbisonicsTest, FullFourthOrderAmbisonicsMono) {
  const AmbisonicsConfig kFullFourthOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config = AmbisonicsMonoConfig{
          .output_channel_count = 25,
          .substream_count = 25,
          .channel_mapping = {0,  1,  2,  3,  4,  5,  6,  7,  8,
                              9,  10, 11, 12, 13, 14, 15, 16, 17,
                              18, 19, 20, 21, 22, 23, 24}}};
  const std::vector<DecodedUleb128> kFullFourthOrderAudioSubstreamIds = {
      100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
      113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124};
  const SubstreamIdLabelsMap kFourthOrderSubstreamIdToLabels = {
      {100, {kA0}},  {101, {kA1}},  {102, {kA2}},  {103, {kA3}},
      {104, {kA4}},  {105, {kA5}},  {106, {kA6}},  {107, {kA7}},
      {108, {kA8}},  {109, {kA9}},  {110, {kA10}}, {111, {kA11}},
      {112, {kA12}}, {113, {kA13}}, {114, {kA14}}, {115, {kA15}},
      {116, {kA16}}, {117, {kA17}}, {118, {kA18}}, {119, {kA19}},
      {120, {kA20}}, {121, {kA21}}, {122, {kA22}}, {123, {kA23}},
      {124, {kA24}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(kFullFourthOrderAmbisonicsConfig,
                                            kFullFourthOrderAudioSubstreamIds,
                                            kFourthOrderSubstreamIdToLabels,
                                            channel_labels)
                  .ok());
  EXPECT_THAT(
      channel_labels,
      ElementsAreArray({kA0,  kA1,  kA2,  kA3,  kA4,  kA5,  kA6,  kA7,  kA8,
                        kA9,  kA10, kA11, kA12, kA13, kA14, kA15, kA16, kA17,
                        kA18, kA19, kA20, kA21, kA22, kA23, kA24}));
}

TEST(GetChannelLabelsForAmbisonicsTest, MixedFirstOrderAmbisonicsMono) {
  // Only 3 substreams provided for a total of 4 channels; missing channel
  // index = 1.
  const AmbisonicsConfig kMixedFirstOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config = AmbisonicsMonoConfig{
          .output_channel_count = 4,
          .substream_count = 3,
          .channel_mapping = {
              0, AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber, 1,
              2}}};
  const std::vector<DecodedUleb128> kMixedFirstOrderAudioSubstreamIds = {
      100, 102, 103};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {100, {kA0}}, {101, {kA1}}, {102, {kA2}}, {103, {kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(kMixedFirstOrderAmbisonicsConfig,
                                            kMixedFirstOrderAudioSubstreamIds,
                                            kFirstOrderSubstreamIdToLabels,
                                            channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels, ElementsAre(kA0, kOmitted, kA2, kA3));
}

TEST(GetChannelLabelsForAmbisonicsTest, FullFirstOrderAmbisonicsProjection) {
  // Values in the demixing matrix doesn't matter here.
  const std::vector<int16_t> kAllZeroDemixingMatrix(16, 0);
  const AmbisonicsConfig kAmbisonicsProjectionConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config = AmbisonicsProjectionConfig{
          .output_channel_count = 4,
          .substream_count = 4,
          .coupled_substream_count = 0,
          .demixing_matrix = kAllZeroDemixingMatrix}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {200, 201,
                                                                    202, 203};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {200, {kA0}}, {201, {kA1}}, {202, {kA2}}, {203, {kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(
                  kAmbisonicsProjectionConfig, kFirstOrderAudioSubstreamIds,
                  kFirstOrderSubstreamIdToLabels, channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels, ElementsAre(kA0, kA1, kA2, kA3));
}

TEST(GetChannelLabelsForAmbisonicsTest,
     FullFirstOrderAmbisonicsProjectionWithCoupledSubstreams) {
  // Values in the demixing matrix doesn't matter here.
  const std::vector<int16_t> kAllZeroDemixingMatrix(16, 0);
  const AmbisonicsConfig kAmbisonicsProjectionConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config = AmbisonicsProjectionConfig{
          .output_channel_count = 4,
          .substream_count = 2,
          .coupled_substream_count = 2,
          .demixing_matrix = kAllZeroDemixingMatrix}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {200, 201};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {200, {kA0, kA1}}, {201, {kA2, kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(
                  kAmbisonicsProjectionConfig, kFirstOrderAudioSubstreamIds,
                  kFirstOrderSubstreamIdToLabels, channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels, ElementsAre(kA0, kA1, kA2, kA3));
}

TEST(GetChannelLabelsForAmbisonicsTest, MixedFirstOrderAmbisonicsProjection) {
  // Values in the demixing matrix doesn't matter here.
  // Missing one channel, so there are only 3 rows in the demixing matrix,
  // each having 4 elements (= 12 elements in total).
  const std::vector<int16_t> kAllZeroDemixingMatrix(12, 0);
  const AmbisonicsConfig kAmbisonicsProjectionConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config = AmbisonicsProjectionConfig{
          .output_channel_count = 4,
          .substream_count = 3,
          .coupled_substream_count = 0,
          .demixing_matrix = kAllZeroDemixingMatrix}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {
      200, /*missing 201, */ 202, 203};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {200, {kA0}}, {201, {kA1}}, {202, {kA2}}, {203, {kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_TRUE(GetChannelLabelsForAmbisonics(
                  kAmbisonicsProjectionConfig, kFirstOrderAudioSubstreamIds,
                  kFirstOrderSubstreamIdToLabels, channel_labels)
                  .ok());
  EXPECT_THAT(channel_labels, ElementsAre(kA0, /*missing kA1, */ kA2, kA3));
}

TEST(GetChannelLabelsForAmbisonicsTest, InvalidMonoModeWithProjectionConfig) {
  // Values in the demixing matrix doesn't matter here.
  const std::vector<int16_t> kAllZeroDemixingMatrix(16, 0);

  // Construct an invalid ambisonics config, where the mode is mono but the
  // field `.ambisonics_config` contains an `AmbisonicsProjectionConfig`.
  const AmbisonicsConfig kInvalidAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config = AmbisonicsProjectionConfig{
          .output_channel_count = 4,
          .substream_count = 4,
          .coupled_substream_count = 0,
          .demixing_matrix = kAllZeroDemixingMatrix}};
  const std::vector<DecodedUleb128> kFullFirstOrderAudioSubstreamIds = {
      100, 101, 102, 103};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {100, {kA0}}, {101, {kA1}}, {102, {kA2}}, {103, {kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_FALSE(GetChannelLabelsForAmbisonics(
                   kInvalidAmbisonicsConfig, kFullFirstOrderAudioSubstreamIds,
                   kFirstOrderSubstreamIdToLabels, channel_labels)
                   .ok());
}

TEST(GetChannelLabelsForAmbisonicsTest, InvalidProjectionModeWithMonoConfig) {
  // Construct an invalid ambisonics config, where the mode is projection but
  // the field `.ambisonics_config` contains an `AmbisonicsMonoConfig`.
  const AmbisonicsConfig kInvalidAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config =
          AmbisonicsMonoConfig{.output_channel_count = 4,
                               .substream_count = 4,
                               .channel_mapping = {0, 1, 2, 3}}};
  const std::vector<DecodedUleb128> kFullFirstOrderAudioSubstreamIds = {
      100, 101, 102, 103};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {100, {kA0}}, {101, {kA1}}, {102, {kA2}}, {103, {kA3}}};

  std::vector<ChannelLabel::Label> channel_labels;
  EXPECT_FALSE(GetChannelLabelsForAmbisonics(
                   kInvalidAmbisonicsConfig, kFullFirstOrderAudioSubstreamIds,
                   kFirstOrderSubstreamIdToLabels, channel_labels)
                   .ok());
}

TEST(ProjectSamplesToRender, ProjectionReordersChannelsAndHalvesValues) {
  // Create a demixing matrix that reorders channels to indices {3, 2, 1, 0},
  // with a gain values corresponding to 0.5.
  const int16_t kHalfGain = std::numeric_limits<int16_t>::max() / 2 + 1;
  const std::vector<int16_t> demixing_matrix = {
      // clang-format off
      /*       Output channel: 0,         1,         2,         3*/
      /* Input channel 0: */         0,         0,         0, kHalfGain,
      /* Input channel 1: */         0,         0, kHalfGain,         0,
      /* Input channel 2: */         0, kHalfGain,         0,         0,
      /* Input channel 3: */ kHalfGain,         0,         0,         0,
      // clang-format on
  };
  const std::vector<std::vector<InternalSampleType>> input_samples = {
      {0.8}, {0.6}, {0.4}, {0.2}};
  std::vector<std::vector<InternalSampleType>> projected_samples;
  EXPECT_TRUE(ProjectSamplesToRender(MakeSpanOfConstSpans(input_samples),
                                     demixing_matrix, projected_samples)
                  .ok());

  // Expect the output have the channels reversed and values halved.
  const std::vector<std::vector<InternalSampleType>>
      expected_projected_samples = {{0.1}, {0.2}, {0.3}, {0.4}};
  EXPECT_THAT(projected_samples,
              InternalSamples2DMatch(expected_projected_samples));
}

TEST(ProjectSamplesToRender, ProjectionAveragesEveryTwoChannels) {
  // Create a demixing matrix that outputs 2 channels, which are averages
  // of input channels {0, 1} and {2, 3} respectively.
  const int16_t kHalfGain = std::numeric_limits<int16_t>::max() / 2 + 1;
  const std::vector<int16_t> demixing_matrix = {
      // clang-format off
      /*             Output channel: 0,         1*/
      /* Input channel 0: */ kHalfGain, 0,
      /* Input channel 1: */ kHalfGain, 0,
      /* Input channel 2: */ 0,         kHalfGain,
      /* Input channel 3: */ 0,         kHalfGain,
      // clang-format on
  };
  const std::vector<std::vector<InternalSampleType>> input_samples = {
      {0.8}, {0.6}, {0.4}, {0.2}};
  std::vector<std::vector<InternalSampleType>> projected_samples;
  EXPECT_TRUE(ProjectSamplesToRender(MakeSpanOfConstSpans(input_samples),
                                     demixing_matrix, projected_samples)
                  .ok());

  // Expect the output have the channels reversed and values halved.
  const std::vector<std::vector<InternalSampleType>>
      expected_projected_samples = {{0.7}, {0.3}};
  EXPECT_THAT(projected_samples,
              InternalSamples2DMatch(expected_projected_samples));
}

TEST(ProjectSamplesToRender, FailsOnIncompatibleMatrices) {
  // Create a demixing matrix whose number of elements is not divisible
  // by the number of input channels, leading the function to fail.
  // of input channels {0, 1} and {2, 3} respectively.
  // Element values do not matter here.
  const std::vector<std::vector<InternalSampleType>> input_samples(4, {0});

  // 17 % 4 != 0.
  const std::vector<int16_t> demixing_matrix(17, 0);

  std::vector<std::vector<InternalSampleType>> projected_samples;
  EXPECT_FALSE(ProjectSamplesToRender(MakeSpanOfConstSpans(input_samples),
                                      demixing_matrix, projected_samples)
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
