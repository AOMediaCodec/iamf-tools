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
#include "iamf/cli/itu_1770_4/loudness_calculator_itu_1770_4.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

const int16_t kMaxLoudness = std::numeric_limits<int16_t>::max();
const int16_t kMinLoudness = std::numeric_limits<int16_t>::min();

const uint32_t kNumSamplesPerFrame = 1024;
// Changing this effectively changes the frequencies of the samples; loudness
// is dependent on frequency.
const uint32_t kSampleRate = 48000;
const uint32_t kMaxBitDepthToMeasureLoudness = 32;

const Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};

const Layout kMonoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0}};

const LoudnessInfo kLoudnessInfoWithMaxLoudness = {
    .info_type = LoudnessInfo::kTruePeak,
    .integrated_loudness = kMaxLoudness,
    .digital_peak = kMaxLoudness,
    .true_peak = kMaxLoudness};

const MixPresentationLayout kStereoLayoutWithMaxUserLoudness = {
    .loudness_layout = kStereoLayout, .loudness = kLoudnessInfoWithMaxLoudness};

TEST(CreateForLayout, ReturnsNonNullForKnownLayouts) {
  EXPECT_NE(LoudnessCalculatorItu1770_4::CreateForLayout(
                kStereoLayoutWithMaxUserLoudness, kNumSamplesPerFrame,
                kSampleRate, kMaxBitDepthToMeasureLoudness),
            nullptr);
}

TEST(CreateForLayout, ReturnsNullForReservedLayouts) {
  const MixPresentationLayout kReservedLayout = {
      .loudness_layout = {
          .layout_type = Layout::kLayoutTypeReserved0,
          .specific_layout = LoudspeakersReservedOrBinauralLayout{}}};
  EXPECT_EQ(LoudnessCalculatorItu1770_4::CreateForLayout(
                kReservedLayout, kNumSamplesPerFrame, kSampleRate,
                kMaxBitDepthToMeasureLoudness),
            nullptr);
}

TEST(LoudnessCalculatorItu1770_4, ReturnsNullptrForUnsupportedBitDepth) {
  const auto kUnsupportedBitDepth = 12;
  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kStereoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kUnsupportedBitDepth);

  ASSERT_EQ(calculator, nullptr);
}

TEST(LoudnessCalculatorItu1770_4, ProvidesMinimumLoudnessForEmptySequence) {
  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kStereoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);

  const auto& calculated_loudness = calculator->QueryLoudness();
  ASSERT_THAT(calculated_loudness, IsOk());

  EXPECT_EQ((*calculated_loudness).integrated_loudness, kMinLoudness);
  EXPECT_EQ((*calculated_loudness).digital_peak, kMinLoudness);
  EXPECT_EQ((*calculated_loudness).true_peak, kMinLoudness);
}

TEST(LoudnessCalculatorItu1770_4, ProvidesMinimumLoudnessForShortSequences) {
  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kStereoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);

  const std::vector<std::vector<int32_t>> samples = {
      {std::numeric_limits<int32_t>::min(),
       std::numeric_limits<int32_t>::max()},
      {std::numeric_limits<int32_t>::min(),
       std::numeric_limits<int32_t>::max()}};
  EXPECT_THAT(
      calculator->AccumulateLoudnessForSamples(MakeSpanOfConstSpans(samples)),
      IsOk());
  const auto& calculated_loudness = calculator->QueryLoudness();
  ASSERT_THAT(calculated_loudness, IsOk());

  EXPECT_EQ((*calculated_loudness).integrated_loudness, kMinLoudness);
  EXPECT_EQ((*calculated_loudness).digital_peak, kMinLoudness);
  EXPECT_EQ((*calculated_loudness).true_peak, kMinLoudness);
}

TEST(LoudnessCalculatorItu1770_4, AlwaysCopiesAnchoredLoudness) {
  const uint8_t kExpectedNumAnchoredLoudness = 1;
  const auto kExpectedAnchorElement =
      AnchoredLoudnessElement::kAnchorElementDialogue;
  const int16_t kExpectedDialogueLoudness = 123;
  const MixPresentationLayout kLayoutWithAnchoredLoudness = {
      .loudness_layout = kStereoLayout,
      .loudness = {.info_type = LoudnessInfo::kTruePeak,
                   .anchored_loudness = {
                       .anchor_elements = {
                           {.anchor_element = kExpectedAnchorElement,
                            .anchored_loudness = kExpectedDialogueLoudness}}}}};

  const auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kLayoutWithAnchoredLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);
  const auto& calculated_loudness = calculator->QueryLoudness();
  ASSERT_THAT(calculated_loudness, IsOk());

  EXPECT_EQ(calculated_loudness->anchored_loudness.anchor_elements.size(),
            kExpectedNumAnchoredLoudness);
  ASSERT_FALSE(calculated_loudness->anchored_loudness.anchor_elements.empty());
  EXPECT_EQ(
      calculated_loudness->anchored_loudness.anchor_elements[0].anchor_element,
      kExpectedAnchorElement);
  EXPECT_EQ(calculated_loudness->anchored_loudness.anchor_elements[0]
                .anchored_loudness,
            kExpectedDialogueLoudness);
}

TEST(LoudnessCalculatorItu1770_4, MeasuresLoudnessWithSharpPeak) {
  constexpr size_t kNumTicks = 10;
  constexpr size_t kNumChannels = 1;
  const std::vector<std::vector<int32_t>> kQuietSignal(
      kNumChannels, std::vector<int32_t>(kNumTicks, 0));
  const std::vector<std::vector<int32_t>> kSignalWithHighTruePeak = {
      {0, 0, 0, 0, std::numeric_limits<int32_t>::max(), 0, 0, 0, 0, 0}};
  const MixPresentationLayout kMonoLayoutWithMaxUserLoudness = {
      .loudness_layout = kMonoLayout, .loudness = kLoudnessInfoWithMaxLoudness};

  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kMonoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);

  // Create a sequence that is generally quiet, but has a single sharp peak.
  for (int i = 0; i < 1000; i++) {
    EXPECT_THAT(calculator->AccumulateLoudnessForSamples(
                    MakeSpanOfConstSpans(kQuietSignal)),
                IsOk());
  }
  EXPECT_THAT(calculator->AccumulateLoudnessForSamples(
                  MakeSpanOfConstSpans(kSignalWithHighTruePeak)),
              IsOk());
  for (int i = 0; i < 1000; i++) {
    EXPECT_THAT(calculator->AccumulateLoudnessForSamples(
                    MakeSpanOfConstSpans(kQuietSignal)),
                IsOk());
  }

  const auto& calculated_loudness = calculator->QueryLoudness();
  ASSERT_THAT(calculated_loudness, IsOk());

  EXPECT_NE((*calculated_loudness).integrated_loudness, kMinLoudness);
  // Digital and true peaks should be 0 dBFS.
  EXPECT_EQ((*calculated_loudness).true_peak, 0);
  EXPECT_EQ((*calculated_loudness).digital_peak, 0);
}

TEST(AccumulateLoudnessForSamples, SucceedsWithExactlyEnoughSamples) {
  const MixPresentationLayout kMonoLayoutWithMaxUserLoudness = {
      .loudness_layout = kMonoLayout, .loudness = kLoudnessInfoWithMaxLoudness};
  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kMonoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);

  constexpr size_t kNumChannels = 1;
  const std::vector<std::vector<int32_t>> kExactlyEnoughSamples(
      kNumChannels, std::vector<int32_t>(kNumSamplesPerFrame, 0));
  EXPECT_THAT(calculator->AccumulateLoudnessForSamples(
                  MakeSpanOfConstSpans(kExactlyEnoughSamples)),
              IsOk());
}

TEST(AccumulateLoudnessForSamples,
     ReturnsErrorWhenThereAreNotAMultipleOfNumChannels) {
  const MixPresentationLayout kStereoLayoutWithMaxUserLoudness = {
      .loudness_layout = kStereoLayout,
      .loudness = kLoudnessInfoWithMaxLoudness};
  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kStereoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);

  // The calculator is configured for stereo, but there is only one channel.
  constexpr size_t kNumTicks = 10;
  constexpr size_t kTooFewChannels = 1;
  const std::vector<std::vector<int32_t>> kSamplesWithMissingChannels(
      kTooFewChannels, std::vector<int32_t>(kNumTicks, 0));
  EXPECT_FALSE(calculator
                   ->AccumulateLoudnessForSamples(
                       MakeSpanOfConstSpans(kSamplesWithMissingChannels))
                   .ok());
}

TEST(AccumulateLoudnessForSamples, ReturnsErrorWhenThereAreTooManySamples) {
  const MixPresentationLayout kMonoLayoutWithMaxUserLoudness = {
      .loudness_layout = kMonoLayout, .loudness = kLoudnessInfoWithMaxLoudness};
  auto calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      kMonoLayoutWithMaxUserLoudness, kNumSamplesPerFrame, kSampleRate,
      kMaxBitDepthToMeasureLoudness);
  ASSERT_NE(calculator, nullptr);

  // The calculator is configured to accept only `kNumSamplesPerFrame` samples.
  // It is invalid to provide more samples per call.
  constexpr size_t kTooManySamples = kNumSamplesPerFrame + 1;
  constexpr size_t kNumChannels = 2;
  const std::vector<std::vector<int32_t>> kSamplesWithTooManySamples(
      kNumChannels, std::vector<int32_t>(kTooManySamples, 0));
  EXPECT_FALSE(calculator
                   ->AccumulateLoudnessForSamples(
                       MakeSpanOfConstSpans(kSamplesWithTooManySamples))
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
