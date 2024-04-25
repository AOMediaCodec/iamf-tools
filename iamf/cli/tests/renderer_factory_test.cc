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
#include "iamf/cli/renderer_factory.h"

#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
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
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};

const ScalableChannelLayoutConfig kMonoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono}}};

const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};

const AmbisonicsConfig kFullZerothOrderAmbisonicsConfig = {
    .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
    .ambisonics_config = AmbisonicsMonoConfig{.output_channel_count = 1,
                                              .substream_count = 1,
                                              .channel_mapping = {0}}};

TEST(CreateRendererForLayout, ReturnsNullPtrForPassThroughRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {"M"}}}, AudioElementObu::kAudioElementChannelBased,
                kMonoScalableChannelLayoutConfig, kMonoLayout),
            nullptr);
}

TEST(CreateRendererForLayout, ReturnsNullPtrForChannelToBinauralRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {"M"}}}, AudioElementObu::kAudioElementChannelBased,
                kMonoScalableChannelLayoutConfig, kBinauralLayout),
            nullptr);
}

TEST(CreateRendererForLayout, ReturnsNullPtrForChannelToChannelRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(
      factory.CreateRendererForLayout(
          {0}, {{0, {"L2", "R2"}}}, AudioElementObu::kAudioElementChannelBased,
          kStereoScalableChannelLayoutConfig, kMonoLayout),
      nullptr);
}

TEST(CreateRendererForLayout, ReturnsNullPtrForAmbisonicsToChannelRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {"A0"}}}, AudioElementObu::kAudioElementSceneBased,
                kFullZerothOrderAmbisonicsConfig, kMonoLayout),
            nullptr);
}

TEST(CreateRendererForLayout, ReturnsNullPtrForAmbisonicsToBinauralRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {"A0"}}}, AudioElementObu::kAudioElementSceneBased,
                kFullZerothOrderAmbisonicsConfig, kBinauralLayout),
            nullptr);
}

}  // namespace
}  // namespace iamf_tools
