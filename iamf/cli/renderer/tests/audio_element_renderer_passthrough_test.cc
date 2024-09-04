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
using testing::DoubleEq;
using testing::Pointwise;

Layout GetScalableLayoutForSoundSystem(
    LoudspeakersSsConventionLayout::SoundSystem sound_system) {
  return {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
          .specific_layout =
              LoudspeakersSsConventionLayout{.sound_system = sound_system}};
}

ScalableChannelLayoutConfig
GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout) {
  return {.num_layers = 1,
          .channel_audio_layer_configs = {
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
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig kStereoChannelConfigWithTwoLayers = {
    .num_layers = 2,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono},
                                    {.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig k7_1_4ScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout7_1_4_ch}}};
const ScalableChannelLayoutConfig kMonoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono}}};

TEST(CreateFromScalableChannelLayoutConfig, SupportsPassThroughBinaural) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kBinauralLayout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsPassThroughStereo) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughIfAnyLayerMatches) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportBinauralToStereo) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kStereoLayout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportStereoToBinaural) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kBinauralLayout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportIfNoLayerMatches) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kMonoLayout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportReservedLayout) {
  const Layout kReservedLayout = {
      .layout_type = Layout::kLayoutTypeReserved0,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};

  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kReservedLayout),
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
          kLayoutWithNoEquivalentSoundSystem),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughFromExpandedLFETo7_1_4) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutLFE),
          k7_1_4Layout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassThroughFromExpandedLFETo7_1_2) {
  EXPECT_EQ(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutLFE),
          GetScalableLayoutForSoundSystem(kSoundSystem10_2_7_0)),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughFromStereoSTo5_1_4) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutStereoS),
          k5_1_4Layout),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsPassThroughFor9_1_6) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayout9_1_6_ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0)),
      nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsPassThroughFromTop6ChTo9_1_6) {
  EXPECT_NE(
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              kExpandedLayoutTop6Ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0)),
      nullptr);
}

const LabeledFrame kLabeledFrameWithL2AndR2 = {
    .label_to_samples = {{kL2, {1, 3, 5, 7}}, {kR2, {2, 4, 6, 8}}}};

TEST(RenderLabeledFrame, RendersPassThroughStereo) {
  auto stereo_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout);

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(kLabeledFrameWithL2AndR2,
                         stereo_pass_through_renderer.get(), rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughBinaural) {
  auto binaural_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kBinauralLayout);

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(kLabeledFrameWithL2AndR2,
                         binaural_pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {1, 2, 3, 4, 5, 6, 7, 8});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThrough7_1_4) {
  const LabeledFrame k7_1_4LabeledFrame = {.label_to_samples = {
                                               {kL7, {0, 100}},
                                               {kR7, {1, 101}},
                                               {kCentre, {2, 102}},
                                               {kLFE, {3, 103}},
                                               {kLss7, {4, 104}},
                                               {kRss7, {5, 105}},
                                               {kLrs7, {6, 106}},
                                               {kRrs7, {7, 107}},
                                               {kLtf4, {8, 108}},
                                               {kRtf4, {9, 109}},
                                               {kLtb4, {10, 110}},
                                               {kRtb4, {11, 111}},
                                           }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          k7_1_4ScalableChannelLayoutConfig, k7_1_4Layout);

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(k7_1_4LabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,
       100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughLFE) {
  const LabeledFrame kLFELabeledFrame = {
      .label_to_samples = {{kLFE, {3, 103}}}};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutLFE),
          GetScalableLayoutForSoundSystem(kSoundSystemJ_4_7_0));

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(kLFELabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0, 0, 0, 3,   0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 103, 0, 0, 0, 0, 0, 0, 0, 0});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughStereoS) {
  const LabeledFrame kStereoSLabeledFrame = {.label_to_samples = {
                                                 {kLs5, {4, 104}},
                                                 {kRs5, {5, 105}},
                                             }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutStereoS),
          GetScalableLayoutForSoundSystem(kSoundSystemD_4_5_0));

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(kStereoSLabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0, 0, 0, 0, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 104, 105, 0, 0, 0, 0});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThrough3_0_Ch) {
  const LabeledFrame k7_1_4LabeledFrame = {.label_to_samples = {
                                               {kL7, {0, 100}},
                                               {kR7, {1, 101}},
                                               {kCentre, {2, 102}},
                                           }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout3_0_ch),
          GetScalableLayoutForSoundSystem(kSoundSystemJ_4_7_0));

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(k7_1_4LabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0,   1,   2,   0, 0, 0, 0, 0, 0, 0, 0, 0,
       100, 101, 102, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThrough9_1_6) {
  const LabeledFrame k9_1_6LabeledFrame = {.label_to_samples = {
                                               {kFL, {0, 100}},
                                               {kFR, {1, 101}},
                                               {kFC, {2, 102}},
                                               {kLFE, {3, 103}},
                                               {kBL, {4, 104}},
                                               {kBR, {5, 105}},
                                               {kFLc, {6, 106}},
                                               {kFRc, {7, 107}},
                                               {kSiL, {8, 108}},
                                               {kSiR, {9, 109}},
                                               {kTpFL, {10, 110}},
                                               {kTpFR, {11, 111}},
                                               {kTpBL, {12, 112}},
                                               {kTpBR, {13, 113}},
                                               {kTpSiL, {14, 114}},
                                               {kTpSiR, {15, 115}},
                                           }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0));

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(k9_1_6LabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,
       11,  12,  13,  14,  15,  100, 101, 102, 103, 104, 105,
       106, 107, 108, 109, 110, 111, 112, 113, 114, 115});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughStereoF) {
  const LabeledFrame kStreoFLabeledFrame = {.label_to_samples = {
                                                {kFL, {0, 100}},
                                                {kFR, {1, 101}},
                                            }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutStereoF),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0));

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(kStreoFLabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0,   1,   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       100, 101, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersPassThroughTop6Ch) {
  const LabeledFrame kTop6ChLabeledFrame = {.label_to_samples = {
                                                {kTpFL, {10, 110}},
                                                {kTpFR, {11, 111}},
                                                {kTpBL, {12, 112}},
                                                {kTpBR, {13, 113}},
                                                {kTpSiL, {14, 114}},
                                                {kTpSiR, {15, 115}},
                                            }};
  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutTop6Ch),
          GetScalableLayoutForSoundSystem(kSoundSystem13_6_9_0));

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(kTop6ChLabeledFrame, pass_through_renderer.get(),
                         rendered_samples);

  const std::vector<InternalSampleType> expected_samples(
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,  11,  12,  13,  14,  15,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 110, 111, 112, 113, 114, 115});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, RendersDemixedSamples) {
  const LabeledFrame kTwoLayerStereo = {
      .label_to_samples = {{kMono, {999}}, {kL2, {1}}, {kDemixedR2, {2}}}};

  auto demixed_stereo_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoChannelConfigWithTwoLayers, kStereoLayout);
  EXPECT_NE(demixed_stereo_renderer, nullptr);

  EXPECT_THAT(demixed_stereo_renderer->RenderLabeledFrame(kTwoLayerStereo),
              IsOk());
  EXPECT_THAT(demixed_stereo_renderer->Finalize(), IsOk());
  EXPECT_TRUE(demixed_stereo_renderer->IsFinalized());
  std::vector<InternalSampleType> rendered_samples;
  EXPECT_THAT(demixed_stereo_renderer->Flush(rendered_samples), IsOk());
  const std::vector<InternalSampleType> expected_samples({1, 2});
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

TEST(RenderLabeledFrame, ReturnsNumberOfTicksToRender) {
  const LabeledFrame kStereoFrameWithTwoRenderedTicks = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 1,
      .label_to_samples = {{kL2, {999, 1, 2, 999}}, {kR2, {999, 1, 2, 999}}}};

  auto stereo_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout);
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
          kMonoScalableChannelLayoutConfig, kMonoLayout);
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
void CollectRenderedSamples(AudioElementRendererPassThrough& renderer,
                            std::vector<InternalSampleType>& rendered_samples) {
  while (!renderer.IsFinalized()) {
    // In practice threads would be better off sleeping between calls. But
    // calling it very often is more likely to detect a problem.
    EXPECT_THAT(renderer.Flush(rendered_samples), IsOk());
  }
  EXPECT_THAT(renderer.Flush(rendered_samples), IsOk());
}

TEST(RenderLabeledFrame, IsThreadSafe) {
  auto mono_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kMonoScalableChannelLayoutConfig, kMonoLayout);
  EXPECT_NE(mono_pass_through_renderer, nullptr);
  constexpr int kNumFrames = 1000;
  constexpr int kSamplesPerFrame = 10;

  // Spawn a thread to render an increasing sequence.
  std::thread render_thread(&RenderMonoSequence, kNumFrames, kSamplesPerFrame,
                            std::ref(*mono_pass_through_renderer));
  // Spawn a thread to collect all of the rendered samples.
  std::vector<InternalSampleType> rendered_samples;
  std::thread collector_thread(&CollectRenderedSamples,
                               std::ref(*mono_pass_through_renderer),
                               std::ref(rendered_samples));
  render_thread.join();
  collector_thread.join();

  // If the render was not thread safe, then we would expect trouble, such as
  // missing samples or samples coming back in the wrong order.
  std::vector<InternalSampleType> expected_samples(kNumFrames *
                                                   kSamplesPerFrame);
  std::iota(expected_samples.begin(), expected_samples.end(), 0);
  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), expected_samples));
}

}  // namespace
}  // namespace iamf_tools
