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

#include <cstdint>
#include <functional>
#include <numeric>
#include <thread>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using enum LoudspeakersSsConventionLayout::SoundSystem;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;

const Layout kMonoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem12_0_1_0}};
const Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemA_0_2_0}};
const Layout k7_1_4Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemJ_4_7_0}};
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};

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

const LabeledFrame kLabeledFrameWithL2AndR2 = {
    .label_to_samples = {{"L2", {1, 3, 5, 7}}, {"R2", {2, 4, 6, 8}}}};

TEST(RenderLabeledFrame, RendersPassThroughStereo) {
  auto stereo_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout);
  EXPECT_NE(stereo_pass_through_renderer, nullptr);

  EXPECT_THAT(stereo_pass_through_renderer->RenderLabeledFrame(
                  kLabeledFrameWithL2AndR2),
              IsOk());
  EXPECT_THAT(stereo_pass_through_renderer->Finalize(), IsOk());
  EXPECT_TRUE(stereo_pass_through_renderer->IsFinalized());
  std::vector<int32_t> rendered_samples;
  EXPECT_THAT(stereo_pass_through_renderer->Flush(rendered_samples), IsOk());
  EXPECT_EQ(rendered_samples, std::vector<int32_t>({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(RenderLabeledFrame, RendersPassThroughBinaural) {
  auto binaural_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kBinauralScalableChannelLayoutConfig, kBinauralLayout);
  EXPECT_NE(binaural_pass_through_renderer, nullptr);

  EXPECT_THAT(binaural_pass_through_renderer->RenderLabeledFrame(
                  kLabeledFrameWithL2AndR2),
              IsOk());
  EXPECT_THAT(binaural_pass_through_renderer->Finalize(), IsOk());
  EXPECT_TRUE(binaural_pass_through_renderer->IsFinalized());
  std::vector<int32_t> rendered_samples;
  EXPECT_THAT(binaural_pass_through_renderer->Flush(rendered_samples), IsOk());
  EXPECT_EQ(rendered_samples, std::vector<int32_t>({1, 2, 3, 4, 5, 6, 7, 8}));
}

TEST(RenderLabeledFrame, RendersPassThrough7_1_4) {
  const LabeledFrame k7_1_4LabeledFrame = {.label_to_samples = {
                                               {"L7", {0, 100}},
                                               {"R7", {1, 101}},
                                               {"C", {2, 102}},
                                               {"LFE", {3, 103}},
                                               {"Lss7", {4, 104}},
                                               {"Rss7", {5, 105}},
                                               {"Lrs7", {6, 106}},
                                               {"Rrs7", {7, 107}},
                                               {"Ltf4", {8, 108}},
                                               {"Rtf4", {9, 109}},
                                               {"Ltb4", {10, 110}},
                                               {"Rtb4", {11, 111}},
                                           }};

  auto pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          k7_1_4ScalableChannelLayoutConfig, k7_1_4Layout);
  EXPECT_NE(pass_through_renderer, nullptr);

  EXPECT_THAT(pass_through_renderer->RenderLabeledFrame(k7_1_4LabeledFrame),
              IsOk());
  EXPECT_THAT(pass_through_renderer->Finalize(), IsOk());
  EXPECT_TRUE(pass_through_renderer->IsFinalized());
  std::vector<int32_t> rendered_samples;
  EXPECT_THAT(pass_through_renderer->Flush(rendered_samples), IsOk());
  EXPECT_EQ(rendered_samples,
            std::vector<int32_t>({0,   1,   2,   3,   4,   5,   6,   7,
                                  8,   9,   10,  11,  100, 101, 102, 103,
                                  104, 105, 106, 107, 108, 109, 110, 111}));
}

TEST(RenderLabeledFrame, RendersDemixedSamples) {
  const LabeledFrame kTwoLayerStereo = {
      .label_to_samples = {{"M", {999}}, {"L2", {1}}, {"D_R2", {2}}}};

  auto demixed_stereo_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoChannelConfigWithTwoLayers, kStereoLayout);
  EXPECT_NE(demixed_stereo_renderer, nullptr);

  EXPECT_THAT(demixed_stereo_renderer->RenderLabeledFrame(kTwoLayerStereo),
              IsOk());
  EXPECT_THAT(demixed_stereo_renderer->Finalize(), IsOk());
  EXPECT_TRUE(demixed_stereo_renderer->IsFinalized());
  std::vector<int32_t> rendered_samples;
  EXPECT_THAT(demixed_stereo_renderer->Flush(rendered_samples), IsOk());
  EXPECT_EQ(rendered_samples, std::vector<int32_t>({1, 2}));
}

TEST(RenderLabeledFrame, ReturnsNumberOfTicksToRender) {
  const LabeledFrame kStereoFrameWithTwoRenderedTicks = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 1,
      .label_to_samples = {{"L2", {999, 1, 2, 999}}, {"R2", {999, 1, 2, 999}}}};

  auto stereo_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kStereoScalableChannelLayoutConfig, kStereoLayout);
  const auto result = stereo_pass_through_renderer->RenderLabeledFrame(
      kStereoFrameWithTwoRenderedTicks);
  EXPECT_THAT(result, IsOk());
  EXPECT_EQ(*result, 2);
}

TEST(RenderLabeledFrame, EdgeCaseWithAllSamplesTrimmedReturnsZero) {
  const LabeledFrame kMono = {.samples_to_trim_at_start = 1,
                              .label_to_samples = {{"M", {1}}}};

  auto mono_pass_through_renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kMonoScalableChannelLayoutConfig, kMonoLayout);
  const auto result = mono_pass_through_renderer->RenderLabeledFrame(kMono);
  EXPECT_THAT(result, IsOk());
  EXPECT_EQ(*result, 0);
}

// Renders a sequence of `num_frames` frames, each with `samples_per_frame`
// samples. The sequence increases by one for each value in the sequence.
void RenderMonoSequence(int num_frames, int samples_per_frame,
                        AudioElementRendererPassThrough& renderer) {
  int sample = 0;
  for (int frame_index = 0; frame_index < num_frames; ++frame_index) {
    std::vector<int32_t> samples(samples_per_frame);
    std::iota(samples.begin(), samples.end(), sample);
    sample += samples_per_frame;

    EXPECT_THAT(
        renderer.RenderLabeledFrame({.label_to_samples = {{"M", samples}}}),
        IsOk());
  }
  EXPECT_THAT(renderer.Finalize(), IsOk());
}

// Collects all of the rendered samples from `renderer` into `rendered_samples`.
// This function blocks until the renderer is finalized.
void CollectRenderedSamples(AudioElementRendererPassThrough& renderer,
                            std::vector<int32_t>& rendered_samples) {
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
  std::vector<int32_t> rendered_samples;
  std::thread collector_thread(&CollectRenderedSamples,
                               std::ref(*mono_pass_through_renderer),
                               std::ref(rendered_samples));
  render_thread.join();
  collector_thread.join();

  // If the render was not thread safe, then we would expect trouble, such as
  // missing samples or samples coming back in the wrong order.
  std::vector<int32_t> expected_samples(kNumFrames * kSamplesPerFrame);
  std::iota(expected_samples.begin(), expected_samples.end(), 0);
  EXPECT_EQ(rendered_samples, expected_samples);
}

}  // namespace
}  // namespace iamf_tools
