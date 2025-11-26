/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/renderer/audio_element_renderer_binaural.h"

#include <cstddef>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ChannelLabel::Label;
using testing::Each;
using testing::SizeIs;

constexpr InternalSampleType kArbitrarySample1 = 0.000012345;
constexpr InternalSampleType kArbitrarySample2 = 0.000006789;
constexpr InternalSampleType kArbitrarySample3 = 0.000101112;
constexpr InternalSampleType kArbitrarySample4 = 0.009999999;
constexpr InternalSampleType kArbitrarySample5 = 0.987654321;
constexpr InternalSampleType kArbitrarySample6 = 0.000001024;

constexpr size_t kNumSamplesPerFrame = 32;
constexpr size_t kSampleRate = 48000;

const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};

using InputLoudSpeakerLayoutTest =
    ::testing::TestWithParam<ChannelAudioLayerConfig::LoudspeakerLayout>;
TEST_P(InputLoudSpeakerLayoutTest, CreationSucceeds) {
  const ScalableChannelLayoutConfig scalable_channel_layout_config = {
      .channel_audio_layer_configs = {{.loudspeaker_layout = GetParam()}}};

  EXPECT_NE(
      AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
          scalable_channel_layout_config, kNumSamplesPerFrame, kSampleRate),
      nullptr);
}

INSTANTIATE_TEST_SUITE_P(
    SupportedLayouts, InputLoudSpeakerLayoutTest,
    ::testing::Values<ChannelAudioLayerConfig::LoudspeakerLayout>(
        kLayoutMono, kLayoutStereo, kLayout5_1_ch, kLayout5_1_2_ch,
        kLayout5_1_4_ch, kLayout7_1_ch, kLayout7_1_2_ch, kLayout7_1_4_ch,
        kLayout3_1_2_ch));

using FullOrderAmbisonicsMonoTest = ::testing::TestWithParam<int>;
TEST_P(FullOrderAmbisonicsMonoTest, CreationSucceeds) {
  const int order = GetParam();

  // Set up inputs requireced by the creation method: `ambisonics_config`,
  // `audio_substream_ids`, and `substream_id_to_labels`.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsMonoArguments(
      order, ambisonics_config, audio_substream_ids, substream_id_to_labels);

  // Create and expect non-null.
  EXPECT_NE(AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
                ambisonics_config, audio_substream_ids, substream_id_to_labels,
                kNumSamplesPerFrame, kSampleRate),
            nullptr);
}

// TODO(b/459993192): Test order == 0 when OBR supports it.
INSTANTIATE_TEST_SUITE_P(SupportedAmbisonicsOrder, FullOrderAmbisonicsMonoTest,
                         ::testing::Values<int>(1, 2, 3, 4));

using MixedOrderAmbisonicsMonoTest = ::testing::TestWithParam<int>;
TEST_P(MixedOrderAmbisonicsMonoTest, CreationSucceeds) {
  const int order = GetParam();

  // Set up inputs. Starting from a full order ambisonics config.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsMonoArguments(
      order, ambisonics_config, audio_substream_ids, substream_id_to_labels);

  // Remove the second channel (index == 1) and mark it as omitted.
  const size_t kMissingChannelIndex = 1;
  ASSERT_LT(kMissingChannelIndex, audio_substream_ids.size());
  const auto kSubstreamIdToRemove = audio_substream_ids[kMissingChannelIndex];
  audio_substream_ids.erase(audio_substream_ids.begin() + kMissingChannelIndex);
  substream_id_to_labels.erase(kSubstreamIdToRemove);

  // Copy and rewrite the ambisonics mono config.
  auto ambisonics_mono_config =
      std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config);

  // Decrease the total substream count.
  ambisonics_mono_config.substream_count--;

  // Modify the channel mapping: now that there are one-fewer substreams,
  // remove the last element (corresponding to the last substream index) and
  // insert the inactive channel number at position `kMissingChannelIndex`.
  ambisonics_mono_config.channel_mapping.pop_back();
  ambisonics_mono_config.channel_mapping.insert(
      ambisonics_mono_config.channel_mapping.begin() + kMissingChannelIndex,
      AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber);
  ambisonics_config.ambisonics_config = ambisonics_mono_config;

  // Create and expect non-null.
  EXPECT_NE(AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
                ambisonics_config, audio_substream_ids, substream_id_to_labels,
                kNumSamplesPerFrame, kSampleRate),
            nullptr);
}

INSTANTIATE_TEST_SUITE_P(SupportedAmbisonicsOrder, MixedOrderAmbisonicsMonoTest,
                         ::testing::Values<int>(1, 2, 3, 4));

using FullOrderAmbisonicsProjectionTest = ::testing::TestWithParam<int>;
TEST_P(FullOrderAmbisonicsProjectionTest, CreationSucceeds) {
  const int order = GetParam();

  // Set up inputs. requireced by the creation method: `ambisonics_config`,
  // `audio_substream_ids`, and `substream_id_to_labels`.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsProjectionArguments(
      order, ambisonics_config, audio_substream_ids, substream_id_to_labels);

  // Create and expect non-null.
  EXPECT_NE(AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
                ambisonics_config, audio_substream_ids, substream_id_to_labels,
                kNumSamplesPerFrame, kSampleRate),
            nullptr);
}

INSTANTIATE_TEST_SUITE_P(SupportedAmbisonicsOrder,
                         FullOrderAmbisonicsProjectionTest,
                         ::testing::Values<int>(1, 2, 3, 4));

using MixedOrderAmbisonicsProjectionTest = ::testing::TestWithParam<int>;
TEST_P(MixedOrderAmbisonicsProjectionTest, CreationSucceeds) {
  const int order = GetParam();

  // Set up inputs. Starting from a full order ambisonics config.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsProjectionArguments(
      order, ambisonics_config, audio_substream_ids, substream_id_to_labels);

  // Remove the second channel (index == 1) and mark it as omitted.
  const size_t kMissingChannelIndex = 1;
  ASSERT_LT(kMissingChannelIndex, audio_substream_ids.size());
  const auto kSubstreamIdToRemove = audio_substream_ids[kMissingChannelIndex];
  audio_substream_ids.erase(audio_substream_ids.begin() + kMissingChannelIndex);
  substream_id_to_labels.erase(kSubstreamIdToRemove);

  // Copy and rewrite the ambisonics projection config.
  auto ambisonics_projection_config =
      std::get<AmbisonicsProjectionConfig>(ambisonics_config.ambisonics_config);

  // Decrease the total substream count.
  ambisonics_projection_config.substream_count--;

  // Modify the demixing matrix: remove the column with index == 1.
  const int column_height = (order + 1) * (order + 1);
  auto column_iter_first =
      ambisonics_projection_config.demixing_matrix.begin() +
      kMissingChannelIndex * column_height;
  ambisonics_projection_config.demixing_matrix.erase(
      column_iter_first, column_iter_first + column_height);
  ambisonics_config.ambisonics_config = ambisonics_projection_config;

  // Create and expect non-null.
  EXPECT_NE(AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
                ambisonics_config, audio_substream_ids, substream_id_to_labels,
                kNumSamplesPerFrame, kSampleRate),
            nullptr);
}

INSTANTIATE_TEST_SUITE_P(SupportedAmbisonicsOrder,
                         MixedOrderAmbisonicsProjectionTest,
                         ::testing::Values<int>(1, 2, 3, 4));

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportTooSmallFrames) {
  const size_t kTooSmallNumSamplesPerFrame = 8;
  EXPECT_EQ(AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
                kStereoScalableChannelLayoutConfig, kTooSmallNumSamplesPerFrame,
                kSampleRate),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportSampleRatesDeathTest) {
  // TODO(b/451901158): We need better documentation about what sample rates are
  //                    supported, and a creation method that does not crash.
  const size_t kUnsupportedSampleRate = 48001;
  EXPECT_DEATH(
      AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kNumSamplesPerFrame,
          kUnsupportedSampleRate),
      "Unsupported sampling rates");
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportPassThroughBinaural) {
  // If the input layout is already binaural, then no further binaural rendering
  // should be performed. Such input should be handled by
  // `AudioElementRendererPassThrough`.
  const ScalableChannelLayoutConfig kBinauralScalableChannelLayoutConfig = {
      .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
  EXPECT_EQ(AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
                kBinauralScalableChannelLayoutConfig, kNumSamplesPerFrame,
                kSampleRate),
            nullptr);
}

std::vector<InternalSampleType> GetSampleVector(
    InternalSampleType sample_value, size_t num_samples = kNumSamplesPerFrame) {
  return std::vector<InternalSampleType>(num_samples, sample_value);
}

TEST(CreateFromScalableChannelLayoutConfig,
     IsFinalizedImmediatelyAfterFinalizeCall) {
  auto renderer =
      AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kNumSamplesPerFrame, kSampleRate);
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(
      renderer->RenderLabeledFrame(
          {.label_to_samples = {{kL2, GetSampleVector(kArbitrarySample1)},
                                {kR2, GetSampleVector(kArbitrarySample2)}}}),
      IsOk());
  EXPECT_THAT(renderer->Finalize(), IsOk());
  EXPECT_TRUE(renderer->IsFinalized());
}

TEST(RenderLabeledFrame, ReturnsNumberOfTicks) {
  const int kNumTicks = 57;
  auto renderer =
      AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kNumTicks, kSampleRate);
  ASSERT_NE(renderer, nullptr);

  const auto num_ticks = renderer->RenderLabeledFrame(
      {.label_to_samples = {
           {kL2, std::vector<InternalSampleType>(kNumTicks, kArbitrarySample1)},
           {kR2,
            std::vector<InternalSampleType>(kNumTicks, kArbitrarySample2)}}});
  ASSERT_THAT(num_ticks, IsOk());

  EXPECT_EQ(*num_ticks, kNumTicks);
}

// TODO(b/451888880): Verify rendered samples have sensible values.
TEST(RenderLabeledFrame, RendersStereoToBinaural) {
  auto renderer =
      AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kNumSamplesPerFrame, kSampleRate);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kL2, GetSampleVector(-0.5)},
                                               {kR2, GetSampleVector(0.5)}}},
                         renderer.get(), rendered_samples);

  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples, Each(SizeIs(kNumSamplesPerFrame)));
}

TEST(RenderLabeledFrame, Renders7_1_4ToBinaural) {
  const ScalableChannelLayoutConfig k7_1_4ScalableChannelLayoutConfig = {
      .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout7_1_4_ch}}};
  auto renderer =
      AudioElementRendererBinaural::CreateFromScalableChannelLayoutConfig(
          k7_1_4ScalableChannelLayoutConfig, kNumSamplesPerFrame, kSampleRate);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kL7, GetSampleVector(kArbitrarySample1)},
                                  {kR7, GetSampleVector(kArbitrarySample1)},
                                  {kCentre, GetSampleVector(kArbitrarySample6)},
                                  {kLFE, GetSampleVector(kArbitrarySample6)},
                                  {kLss7, GetSampleVector(kArbitrarySample2)},
                                  {kRss7, GetSampleVector(kArbitrarySample2)},
                                  {kLrs7, GetSampleVector(kArbitrarySample3)},
                                  {kRrs7, GetSampleVector(kArbitrarySample3)},
                                  {kLtf4, GetSampleVector(kArbitrarySample4)},
                                  {kRtf4, GetSampleVector(kArbitrarySample4)},
                                  {kLtb4, GetSampleVector(kArbitrarySample5)},
                                  {kRtb4, GetSampleVector(kArbitrarySample5)},
                              }},
                         renderer.get(), rendered_samples);

  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples, Each(SizeIs(kNumSamplesPerFrame)));
}

TEST(RenderLabeledFrame, RendersFullOrderFoaMonoToBinaural) {
  const int kFirstOrder = 1;

  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsMonoArguments(kFirstOrder, ambisonics_config,
                                      audio_substream_ids,
                                      substream_id_to_labels);

  // Create and expect non-null.
  auto renderer = AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
      ambisonics_config, audio_substream_ids, substream_id_to_labels,
      kNumSamplesPerFrame, kSampleRate);
  ASSERT_NE(renderer, nullptr);

  // Render and check the output shape.
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kA0, GetSampleVector(kArbitrarySample1)},
                                  {kA1, GetSampleVector(kArbitrarySample2)},
                                  {kA2, GetSampleVector(kArbitrarySample3)},
                                  {kA3, GetSampleVector(kArbitrarySample4)},
                              }},
                         renderer.get(), rendered_samples);
  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples, Each(SizeIs(kNumSamplesPerFrame)));
}

TEST(RenderLabeledFrame, RendersMixedOrderFoaMonoToBinaural) {
  const int kFirstOrder = 1;

  // Set up inputs. Starting from a full order ambisonics config.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsMonoArguments(kFirstOrder, ambisonics_config,
                                      audio_substream_ids,
                                      substream_id_to_labels);

  // Remove the second channel (index == 1) and mark it as omitted.
  const size_t kMissingChannelIndex = 1;
  ASSERT_LT(kMissingChannelIndex, audio_substream_ids.size());
  const auto kSubstreamIdToRemove = audio_substream_ids[kMissingChannelIndex];
  audio_substream_ids.erase(audio_substream_ids.begin() + kMissingChannelIndex);
  substream_id_to_labels.erase(kSubstreamIdToRemove);

  // Copy and rewrite the ambisonics mono config.
  auto ambisonics_mono_config =
      std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config);

  // Decrease the total substream count.
  ambisonics_mono_config.substream_count--;

  // Modify the channel mapping: now that there are one-fewer substreams,
  // remove the last element (corresponding to the last substream index) and
  // insert the inactive channel number at position `kMissingChannelIndex`.
  ambisonics_mono_config.channel_mapping.pop_back();
  ambisonics_mono_config.channel_mapping.insert(
      ambisonics_mono_config.channel_mapping.begin() + kMissingChannelIndex,
      AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber);
  ambisonics_config.ambisonics_config = ambisonics_mono_config;

  // Create and expect non-null.
  auto renderer = AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
      ambisonics_config, audio_substream_ids, substream_id_to_labels,
      kNumSamplesPerFrame, kSampleRate);
  ASSERT_NE(renderer, nullptr);

  // Render and check the output shape.
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kA0, GetSampleVector(kArbitrarySample1)},
                                  /*Omitting samples for kA1*/
                                  {kA2, GetSampleVector(kArbitrarySample3)},
                                  {kA3, GetSampleVector(kArbitrarySample4)},
                              }},
                         renderer.get(), rendered_samples);
  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples, Each(SizeIs(kNumSamplesPerFrame)));
}

TEST(RenderLabeledFrame, RendersFullOrderFoaProjectionToBinaural) {
  const int kFirstOrder = 1;

  // Set up inputs requireced by the creation method: `ambisonics_config`,
  // `audio_substream_ids`, and `substream_id_to_labels`.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsProjectionArguments(kFirstOrder, ambisonics_config,
                                            audio_substream_ids,
                                            substream_id_to_labels);

  // Create and expect non-null.
  auto renderer = AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
      ambisonics_config, audio_substream_ids, substream_id_to_labels,
      kNumSamplesPerFrame, kSampleRate);
  ASSERT_NE(renderer, nullptr);

  // Render and check the output shape.
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kA0, GetSampleVector(kArbitrarySample1)},
                                  {kA1, GetSampleVector(kArbitrarySample2)},
                                  {kA2, GetSampleVector(kArbitrarySample3)},
                                  {kA3, GetSampleVector(kArbitrarySample4)},
                              }},
                         renderer.get(), rendered_samples);
  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples, Each(SizeIs(kNumSamplesPerFrame)));
}

TEST(RenderLabeledFrame, RendersMixedOrderFoaProjectionToBinaural) {
  const int kFirstOrder = 1;

  // Set up inputs. Starting from a full order ambisonics config.
  AmbisonicsConfig ambisonics_config;
  std::vector<DecodedUleb128> audio_substream_ids;
  SubstreamIdLabelsMap substream_id_to_labels;
  GetFullOrderAmbisonicsProjectionArguments(kFirstOrder, ambisonics_config,
                                            audio_substream_ids,
                                            substream_id_to_labels);

  // Remove the second channel (index == 1) and mark it as omitted.
  const size_t kMissingChannelIndex = 1;
  ASSERT_LT(kMissingChannelIndex, audio_substream_ids.size());
  const auto kSubstreamIdToRemove = audio_substream_ids[kMissingChannelIndex];
  audio_substream_ids.erase(audio_substream_ids.begin() + kMissingChannelIndex);
  substream_id_to_labels.erase(kSubstreamIdToRemove);

  // Copy and rewrite the ambisonics projection config.
  auto ambisonics_projection_config =
      std::get<AmbisonicsProjectionConfig>(ambisonics_config.ambisonics_config);

  // Decrease the total substream count.
  ambisonics_projection_config.substream_count--;

  // Modify the demixing matrix: remove the column with index == 1.
  const int column_height = (kFirstOrder + 1) * (kFirstOrder + 1);
  auto column_iter_first =
      ambisonics_projection_config.demixing_matrix.begin() +
      kMissingChannelIndex * column_height;
  ambisonics_projection_config.demixing_matrix.erase(
      column_iter_first, column_iter_first + column_height);
  ambisonics_config.ambisonics_config = ambisonics_projection_config;

  // Create and expect non-null.
  auto renderer = AudioElementRendererBinaural::CreateFromAmbisonicsConfig(
      ambisonics_config, audio_substream_ids, substream_id_to_labels,
      kNumSamplesPerFrame, kSampleRate);
  ASSERT_NE(renderer, nullptr);

  // Render and check the output shape.
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kA0, GetSampleVector(kArbitrarySample1)},
                                  /*Omitting samples for kA1*/
                                  {kA2, GetSampleVector(kArbitrarySample3)},
                                  {kA3, GetSampleVector(kArbitrarySample4)},
                              }},
                         renderer.get(), rendered_samples);
  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples, Each(SizeIs(kNumSamplesPerFrame)));
}

// TODO(b/450471766): Add tests when rendering from expanded layouts is
//                    supported.

}  // namespace
}  // namespace iamf_tools
