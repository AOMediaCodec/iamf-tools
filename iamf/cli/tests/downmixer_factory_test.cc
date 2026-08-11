/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/downmixer_factory.h"

#include <array>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using enum ChannelLabel::Label;

constexpr DecodedUleb128 kStereoSubstreamId = 2;
constexpr size_t kSamplesPerFrame = 4;

TEST(CreateScalableChannelDownmixers, EmptyConfigIsOk) {
  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      /*labels_to_downmix=*/{}, /*substream_id_to_labels=*/{});

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

TEST(CreateScalableChannelDownmixers, HasOneDownMixerForTwoLayerStereo) {
  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      /*labels_to_downmix=*/{kL2, kR2},
      /*substream_id_to_labels=*/{{0, {kMono}}, {1, {kL2}}});

  EXPECT_THAT(down_mixers, IsOkAndHolds(SizeIs(1)));
}

TEST(CreateScalableChannelDownmixers,
     OneLayerChannelBasedHasNoDownMixersWithStereo) {
  const absl::flat_hash_set<ChannelLabel::Label> kStereoInputLabels = {kL2,
                                                                       kR2};
  const SubstreamIdLabelsMap kOneLayerStereoOutputIdToLabels = {
      {kStereoSubstreamId, {kL2, kR2}}};

  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      kStereoInputLabels, kOneLayerStereoOutputIdToLabels);

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

TEST(CreateScalableChannelDownmixers,
     OneLayerChannelBasedHasNoDownMixersWith7_1_4) {
  const absl::flat_hash_set<ChannelLabel::Label> k7_1_4InputLabels = {
      kL7,   kR7,   kCentre, kLFE,  kLss7, kRss7,
      kLrs7, kRrs7, kLtf4,   kRtf4, kLtb4, kRtb4};
  const SubstreamIdLabelsMap kOneLayer7_1_4OutputIdToLabels = {
      {0, {kL7, kR7}},     {1, {kLss7, kRss7}}, {2, {kLrs7, kRrs7}},
      {3, {kLtf4, kRtf4}}, {4, {kLtb4, kRtb4}}, {5, {kCentre}},
      {6, {kLFE}}};

  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      k7_1_4InputLabels, kOneLayer7_1_4OutputIdToLabels);

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

TEST(CreateScalableChannelDownmixers, AmbisonicsHasNoDownMixers) {
  const absl::flat_hash_set<ChannelLabel::Label> kAmbisonicsInputLabels = {
      kA0, kA1, kA2, kA3};
  const SubstreamIdLabelsMap kAmbisonicsOutputIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}};

  auto down_mixers = DownmixerFactory::CreateScalableChannelDownmixers(
      kAmbisonicsInputLabels, kAmbisonicsOutputIdToLabels);

  EXPECT_THAT(down_mixers, IsOkAndHolds(IsEmpty()));
}

TEST(SampleProcessorToDownMixer, WrapsSampleProcessorAndRoutesChannels) {
  // Arrange labels in an unexpected order.
  constexpr std::array<ChannelLabel::Label, 3> kThreeLabels = {kA1, kA2, kA0};
  // Resampler processor with 3 channels that outputs odd index samples.
  constexpr int kNumChannels = 3;
  auto processor = std::make_unique<EverySecondTickResampler>(kSamplesPerFrame,
                                                              kNumChannels);

  auto down_mixer = DownmixerFactory::SampleProcessorToDownMixer(
      kThreeLabels, std::move(processor));
  ASSERT_THAT(down_mixer, IsOk());

  // Operate on the returned down-mixer,
  const DownMixingParams unused_params{};
  LabelSamplesMap label_to_samples = {
      {kA0, {0.0, 1.0, 0.0, 1.1}},
      {kA1, {0.0, 2.0, 0.0, 2.1}},
      {kA2, {0.0, 3.0, 0.0, 3.1}},
  };
  EXPECT_THAT((*down_mixer)(unused_params, label_to_samples), IsOk());
  // `EverySecondTickResampler` keeps every second tick (indices 1 and 3).
  EXPECT_THAT(label_to_samples,
              UnorderedElementsAre(Pair(kA0, ElementsAre(1.0, 1.1)),
                                   Pair(kA1, ElementsAre(2.0, 2.1)),
                                   Pair(kA2, ElementsAre(3.0, 3.1))));

  // Operate on a second tick to verify state is reset across frames.
  LabelSamplesMap second_frame_samples = {
      {kA0, {0.0, 100.0, 0.0, 100.1}},
      {kA1, {0.0, 200.0, 0.0, 200.1}},
      {kA2, {0.0, 300.0, 0.0, 300.1}},
  };
  EXPECT_THAT((*down_mixer)(unused_params, second_frame_samples), IsOk());
  EXPECT_THAT(second_frame_samples,
              UnorderedElementsAre(Pair(kA0, ElementsAre(100.0, 100.1)),
                                   Pair(kA1, ElementsAre(200.0, 200.1)),
                                   Pair(kA2, ElementsAre(300.0, 300.1))));
}

TEST(SampleProcessorToDownMixer, ReturnsErrorIfMissingRequiredLabel) {
  constexpr int kNumChannels = 2;
  auto processor = std::make_unique<MockSampleProcessor>(
      kSamplesPerFrame, kNumChannels, kSamplesPerFrame);
  std::vector<ChannelLabel::Label> ordered_input_labels = {kA0, kA1};
  auto down_mixer = DownmixerFactory::SampleProcessorToDownMixer(
      ordered_input_labels, std::move(processor));
  ASSERT_THAT(down_mixer, IsOk());

  // Operate on the returned down-mixer,
  const DownMixingParams unused_params{};
  LabelSamplesMap label_to_samples_missing_kA0 = {
      {kA1, {2.0}},
  };
  EXPECT_THAT((*down_mixer)(unused_params, label_to_samples_missing_kA0),
              Not(IsOk()));
}

TEST(SampleProcessorToDownMixer, ErrorIfMismatchedLabelsSize) {
  constexpr int kNumChannels = 2;
  auto processor = std::make_unique<testing::NiceMock<MockSampleProcessor>>(
      kSamplesPerFrame, kNumChannels, kSamplesPerFrame);
  std::array<ChannelLabel::Label, 1> missing_input_labels = {kA0};

  EXPECT_THAT(DownmixerFactory::SampleProcessorToDownMixer(
                  missing_input_labels, std::move(processor)),
              Not(IsOk()));
}

TEST(SampleProcessorToDownMixer, NullProcessorReturnsError) {
  std::unique_ptr<MockSampleProcessor> null_processor;
  constexpr absl::Span<const ChannelLabel::Label> kNoLabels = {};

  EXPECT_THAT(DownmixerFactory::SampleProcessorToDownMixer(
                  kNoLabels, std::move(null_processor)),
              Not(IsOk()));
}

TEST(SampleProcessorToDownMixer, PropagatesErrorFromPushFrame) {
  constexpr int kNumChannels = 1;
  auto processor = std::make_unique<MockSampleProcessor>(
      kSamplesPerFrame, kNumChannels, kSamplesPerFrame);
  std::vector<ChannelLabel::Label> ordered_input_labels = {kA0};
  EXPECT_CALL(*processor, PushFrameDerived(ElementsAre(ElementsAre(1.0, 1.1))))
      .WillOnce(Return(absl::InternalError("PushFrame failure.")));

  auto down_mixer = DownmixerFactory::SampleProcessorToDownMixer(
      ordered_input_labels, std::move(processor));
  ASSERT_THAT(down_mixer, IsOk());

  const DownMixingParams unused_params{};
  LabelSamplesMap label_to_samples = {{kA0, {1.0, 1.1}}};
  EXPECT_THAT((*down_mixer)(unused_params, label_to_samples), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
