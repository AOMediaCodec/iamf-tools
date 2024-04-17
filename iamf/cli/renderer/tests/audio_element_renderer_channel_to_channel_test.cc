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

#include "iamf/cli/renderer/audio_element_renderer_channel_to_channel.h"

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

// The IAMF spec recommends special rules for these 3.1.2 and 7.1.2 layouts.
const Layout k7_1_2Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem10_2_7_0}};
const Layout k3_1_2Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem11_2_3_0}};

const ScalableChannelLayoutConfig kBinauralScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig k7_1_4ScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout7_1_4_ch}}};

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownmixingStereoToMono) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kMonoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownmixing7_1_4To7_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    k7_1_4ScalableChannelLayoutConfig, k7_1_2Layout),
            nullptr);
}
TEST(CreateFromScalableChannelLayoutConfig, SupportsDownmixing7_1_4To3_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    k7_1_4ScalableChannelLayoutConfig, k3_1_2Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportBinaural) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kBinauralScalableChannelLayoutConfig, kBinauralLayout),
            nullptr);
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kBinauralScalableChannelLayoutConfig, kStereoLayout),
            nullptr);
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kBinauralLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportReservedLayout) {
  const Layout kReservedLayout = {
      .layout_type = Layout::kLayoutTypeReserved0,
      .specific_layout = LoudspeakersReservedBinauralLayout{.reserved = 0}};

  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kReservedLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportPassThroughStereo) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kStereoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, RenderLabeledFrameIsNotSupported) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(kStereoScalableChannelLayoutConfig,
                                            kMonoLayout);
  ASSERT_NE(renderer, nullptr);

  EXPECT_FALSE(renderer
                   ->RenderLabeledFrame(
                       {.label_to_samples = {{"L2", {50}}, {"R2", {100}}}})
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
