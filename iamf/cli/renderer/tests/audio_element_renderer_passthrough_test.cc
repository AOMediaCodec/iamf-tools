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

#include "gtest/gtest.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
namespace iamf_tools {
namespace {

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
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};

const ScalableChannelLayoutConfig kBinauralScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};
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
  const ScalableChannelLayoutConfig kStereoChannelConfigWithTwoLayers = {
      .num_layers = 2,
      .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono},
                                      {.loudspeaker_layout = kLayoutStereo}}};

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
      .specific_layout = LoudspeakersReservedBinauralLayout{.reserved = 0}};

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

TEST(CreateFromScalableChannelLayoutConfig, RenderLabeledFrameIsNotSupported) {
  auto renderer =
      AudioElementRendererPassThrough::CreateFromScalableChannelLayoutConfig(
          kMonoScalableChannelLayoutConfig, kMonoLayout);
  ASSERT_NE(renderer, nullptr);

  EXPECT_FALSE(
      renderer->RenderLabeledFrame({.label_to_samples = {{"M", {1, 2, 3}}}})
          .ok());
}

}  // namespace
}  // namespace iamf_tools
