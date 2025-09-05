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

#include "iamf/cli/renderer/audio_element_renderer_passthrough.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <numeric>
#include <thread>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
using enum ChannelLabel::Label;
using enum LoudspeakersSsConventionLayout::SoundSystem;

Layout GetScalableLayoutForSoundSystem(
    LoudspeakersSsConventionLayout::SoundSystem sound_system) {
  return {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
          .specific_layout =
              LoudspeakersSsConventionLayout{.sound_system = sound_system}};
}

ScalableChannelLayoutConfig
GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout) {
  return {.channel_audio_layer_configs = {
              {.loudspeaker_layout = kLayoutExpanded,
               .expanded_loudspeaker_layout = expanded_layout}}};
}

const Layout kMonoLayout =
    GetScalableLayoutForSoundSystem(kSoundSystem12_0_1_0);
const Layout kStereoLayout =
    GetScalableLayoutForSoundSystem(kSoundSystemA_0_2_0);
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};
const Layout k7_1_4Layout =
    GetScalableLayoutForSoundSystem(kSoundSystemJ_4_7_0);
const Layout k5_1_4Layout =
    GetScalableLayoutForSoundSystem(kSoundSystemD_4_5_0);

const ScalableChannelLayoutConfig kBinauralScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig kStereoChannelConfigWithTwoLayers = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono},
                                    {.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig k7_1_4ScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout7_1_4_ch}}};
const ScalableChannelLayoutConfig kMonoScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono}}};

constexpr size_t kFourSamplesPerFrame = 4;

TEST(CreateFromScalableChannelLayoutConfig, SupportsPassThroughBinaural) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kBinauralLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsPassThroughStereo) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughIfAnyLayerMatches) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportBinauralToStereo) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kStereoLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportStereoToBinaural) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kBinauralLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportIfNoLayerMatches) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kMonoLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportReservedLayout) {
  const Layout kReservedLayout = {
      .layout_type = Layout::kLayoutTypeReserved0,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};

  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kReservedLayout,
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportReservedLayoutWithNoEquivalentSoundSystem) {
  const Layout kLayoutWithNoEquivalentSoundSystem = {
      .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout =
          LoudspeakersSsConventionLayout{.sound_system = kSoundSystemG_4_9_0}};

  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig,
          kLayoutWithNoEquivalentSoundSystem, kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughFromExpandedLFETo7_1_4) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutLFE),
          k7_1_4Layout, kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassThroughFromExpandedLFETo7_1_2) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutLFE),
          GetScalableLayoutForSoundSystem(kSoundSystem10_2_7_0),
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughFromStereoSTo5_1_4) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutStereoS),
          k5_1_4Layout, kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsPassThroughFor9_1_6) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayout9_1_6_ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0),
          kFourSamplesPerFrame),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughFromTop6ChTo9_1_6) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutTop6Ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0),
          kFourSamplesPerFrame),
      nullptr);
}

TEST(RenderLabeledFrame, RendersPassThroughStereo) {
  const LabeledFrame kLabeledFrameWithL2AndR2 = {
      .label_to_samples = {{kL2, {0.1, 0.3, 0.5, 0.7}},
                           {kR2, {0.2, 0.4, 0.6, 0.8}}}};
  auto stereo_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout,
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(kLabeledFrameWithL2AndR2,
                         stereo_pass_through_renderer.get(), rendered_samples);
  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0.1, 0.3, 0.5, 0.7}, {0.2, 0.4, 0.6, 0.8}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughBinaural) {
  const LabeledFrame kLabeledFrameWithL2AndR2 = {
      .label_to_samples = {{kL2, {0.1, 0.3, 0.5, 0.7}},
                           {kR2, {0.2, 0.4, 0.6, 0.8}}}};
  auto binaural_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kBinauralLayout,
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(kLabeledFrameWithL2AndR2,
                         binaural_pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0.1, 0.3, 0.5, 0.7}, {0.2, 0.4, 0.6, 0.8}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThrough7_1_4) {
  const LabeledFrame k7_1_4LabeledFrame = {.label_to_samples = {
                                               {kL7, {0.000, 0.100}},
                                               {kR7, {0.001, 0.101}},
                                               {kCentre, {0.002, 0.102}},
                                               {kLFE, {0.003, 0.103}},
                                               {kLss7, {0.004, 0.104}},
                                               {kRss7, {0.005, 0.105}},
                                               {kLrs7, {0.006, 0.106}},
                                               {kRrs7, {0.007, 0.107}},
                                               {kLtf4, {0.008, 0.108}},
                                               {kRtf4, {0.009, 0.109}},
                                               {kLtb4, {0.010, 0.110}},
                                               {kRtb4, {0.011, 0.111}},
                                           }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          k7_1_4ScalableChannelLayoutConfig, k7_1_4Layout,
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(k7_1_4LabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0.000, 0.100}, {0.001, 0.101}, {0.002, 0.102}, {0.003, 0.103},
      {0.004, 0.104}, {0.005, 0.105}, {0.006, 0.106}, {0.007, 0.107},
      {0.008, 0.108}, {0.009, 0.109}, {0.010, 0.110}, {0.011, 0.111}};

  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughLFE) {
  const LabeledFrame kLFELabeledFrame = {
      .label_to_samples = {{kLFE, {0.003, 0.103}}}};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutLFE),
          GetScalableLayoutForSoundSystem(kSoundSystemJ_4_7_0),
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(kLFELabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0, 0}, {0, 0}, {0, 0}, {0.003, 0.103}, {0, 0}, {0, 0},
      {0, 0}, {0, 0}, {0, 0}, {0, 0},         {0, 0}, {0, 0}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughStereoS) {
  const LabeledFrame kStereoSLabeledFrame = {.label_to_samples = {
                                                 {kLs5, {0.04, 0.104}},
                                                 {kRs5, {0.05, 0.105}},
                                             }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutStereoS),
          GetScalableLayoutForSoundSystem(kSoundSystemD_4_5_0),
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(kStereoSLabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0, 0},        {0, 0}, {0, 0}, {0, 0}, {0.04, 0.104},
      {0.05, 0.105}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThrough3_0_Ch) {
  const LabeledFrame k7_1_4LabeledFrame = {.label_to_samples = {
                                               {kL7, {0.000, 0.100}},
                                               {kR7, {0.001, 0.101}},
                                               {kCentre, {0.002, 0.102}},
                                           }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout3_0_ch),
          GetScalableLayoutForSoundSystem(kSoundSystemJ_4_7_0),
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(k7_1_4LabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0.000, 0.100}, {0.001, 0.101}, {0.002, 0.102}, {0, 0}, {0, 0}, {0, 0},
      {0, 0},         {0, 0},         {0, 0},         {0, 0}, {0, 0}, {0, 0}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThrough9_1_6) {
  const LabeledFrame k9_1_6LabeledFrame = {.label_to_samples = {
                                               {kFL, {0.000, 0.100}},
                                               {kFR, {0.001, 0.101}},
                                               {kFC, {0.002, 0.102}},
                                               {kLFE, {0.003, 0.103}},
                                               {kBL, {0.004, 0.104}},
                                               {kBR, {0.005, 0.105}},
                                               {kFLc, {0.006, 0.106}},
                                               {kFRc, {0.007, 0.107}},
                                               {kSiL, {0.008, 0.108}},
                                               {kSiR, {0.009, 0.109}},
                                               {kTpFL, {0.010, 0.110}},
                                               {kTpFR, {0.011, 0.111}},
                                               {kTpBL, {0.012, 0.112}},
                                               {kTpBR, {0.013, 0.113}},
                                               {kTpSiL, {0.014, 0.114}},
                                               {kTpSiR, {0.015, 0.115}},
                                           }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0),
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(k9_1_6LabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0.000, 0.100}, {0.001, 0.101}, {0.002, 0.102}, {0.003, 0.103},
      {0.004, 0.104}, {0.005, 0.105}, {0.006, 0.106}, {0.007, 0.107},
      {0.008, 0.108}, {0.009, 0.109}, {0.010, 0.110}, {0.011, 0.111},
      {0.012, 0.112}, {0.013, 0.113}, {0.014, 0.114}, {0.015, 0.115}};

  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughStereoF) {
  const LabeledFrame kStreoFLabeledFrame = {.label_to_samples = {
                                                {kFL, {0.000, 0.100}},
                                                {kFR, {0.001, 0.101}},
                                            }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutStereoF),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0),
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(kStreoFLabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0.000, 0.100}, {0.001, 0.101}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughTop6Ch) {
  const LabeledFrame kTop6ChLabeledFrame = {.label_to_samples = {
                                                {kTpFL, {0.010, 0.110}},
                                                {kTpFR, {0.011, 0.111}},
                                                {kTpBL, {0.012, 0.112}},
                                                {kTpBR, {0.013, 0.113}},
                                                {kTpSiL, {0.014, 0.114}},
                                                {kTpSiR, {0.015, 0.115}},
                                            }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutTop6Ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0),
          kFourSamplesPerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(kTop6ChLabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<std::vector<InternalSampleType>> expected_samples = {
      {0, 0},         {0, 0},         {0, 0},         {0, 0},
      {0, 0},         {0, 0},         {0, 0},         {0, 0},
      {0, 0},         {0, 0},         {0.010, 0.110}, {0.011, 0.111},
      {0.012, 0.112}, {0.013, 0.113}, {0.014, 0.114}, {0.015, 0.115}};
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, RendersDemixedSamples) {
  const LabeledFrame kTwoLayerStereo = {
      .label_to_samples = {{kMono, {999}}, {kL2, {0.1}}, {kDemixedR2, {0.2}}}};

  auto demixed_stereo_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoChannelConfigWithTwoLayers, kStereoLayout,
          kFourSamplesPerFrame);
  EXPECT_NE(demixed_stereo_renderer, nullptr);

  EXPECT_THAT(demixed_stereo_renderer->RenderLabeledFrame(kTwoLayerStereo),
              IsOk());
  EXPECT_THAT(demixed_stereo_renderer->Finalize(), IsOk());
  EXPECT_TRUE(demixed_stereo_renderer->IsFinalized());

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  demixed_stereo_renderer->Flush(rendered_samples);
  const std::vector<std::vector<InternalSampleType>> expected_samples = {{0.1},
                                                                         {0.2}};

  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

TEST(RenderLabeledFrame, ReturnsNumberOfTicksToRender) {
  const LabeledFrame kStereoFrameWithTwoRenderedTicks = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 1,
      .label_to_samples = {{kL2, {0.999, 0.001, 0.002, 999}},
                           {kR2, {0.999, 0.001, 0.002, 0.999}}}};

  auto stereo_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout,
          kFourSamplesPerFrame);
  const auto result = stereo_pass_through_renderer->RenderLabeledFrame(
      kStereoFrameWithTwoRenderedTicks);
  EXPECT_THAT(result, IsOk());
  EXPECT_EQ(*result, 2);
}

TEST(RenderLabeledFrame, EdgeCaseWithAllSamplesTrimmedReturnsZero) {
  const LabeledFrame kMonoFrame = {.samples_to_trim_at_start = 1,
                                   .label_to_samples = {{kMono, {1}}}};

  auto mono_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kMonoScalableChannelLayoutConfig, kMonoLayout, kFourSamplesPerFrame);
  const auto result =
      mono_pass_through_renderer->RenderLabeledFrame(kMonoFrame);
  EXPECT_THAT(result, IsOk());
  EXPECT_EQ(*result, 0);
}

// Renders a sequence of `num_frames` frames, each with `samples_per_frame`
// samples. The sequence increases by one for each value in the sequence.
void RenderMonoSequence(int num_frames, int samples_per_frame,
                        AudioElementRendererPassThrough& renderer) {
  int sample = 0;
  for (int frame_index = 0; frame_index < num_frames; ++frame_index) {
    std::vector<InternalSampleType> samples(samples_per_frame);
    std::iota(samples.begin(), samples.end(), sample);
    sample += samples_per_frame;

    EXPECT_THAT(
        renderer.RenderLabeledFrame({.label_to_samples = {{kMono, samples}}}),
        IsOk());
  }
  EXPECT_THAT(renderer.Finalize(), IsOk());
}

// Collects all of the rendered samples from `renderer` into `rendered_samples`.
// This function blocks until the renderer is finalized.
void CollectRenderedSamples(
    AudioElementRendererPassThrough& renderer,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  while (!renderer.IsFinalized()) {
    // In practice threads would be better off sleeping between calls. But
    // calling it very often is more likely to detect a problem.
    renderer.Flush(rendered_samples);
  }
  renderer.Flush(rendered_samples);
}

TEST(RenderLabeledFrame, IsThreadSafe) {
  constexpr int kSamplesPerFrame = 10;
  auto mono_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kMonoScalableChannelLayoutConfig, kMonoLayout, kSamplesPerFrame);
  EXPECT_NE(mono_pass_through_renderer, nullptr);
  constexpr int kNumFrames = 1000;

  // Spawn a thread to render an increasing sequence.
  std::thread render_thread(&RenderMonoSequence, kNumFrames, kSamplesPerFrame,
                            std::ref(*mono_pass_through_renderer));
  // Spawn a thread to collect all of the rendered samples.
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  std::thread collector_thread(&CollectRenderedSamples,
                               std::ref(*mono_pass_through_renderer),
                               std::ref(rendered_samples));
  render_thread.join();
  collector_thread.join();

  // If the render was not thread safe, then we would expect trouble, such as
  // missing samples or samples coming back in the wrong order.
  std::vector<std::vector<InternalSampleType>> expected_samples = {
      std::vector<InternalSampleType>(kNumFrames * kSamplesPerFrame)};
  std::iota(expected_samples[0].begin(), expected_samples[0].end(), 0);
  EXPECT_THAT(rendered_samples, InternalSamples2DMatch(expected_samples));
}

}  // namespace
}  // namespace iamf_tools
