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

#include <cstddef>

#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
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
using enum ChannelLabel::Label;

const Layout kMonoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem12_0_1_0}};
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};

constexpr size_t kNumSamplesPerFrame = 8;

const ScalableChannelLayoutConfig kBinauralChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};

const ScalableChannelLayoutConfig kMonoScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutMono}}};

const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};

const AmbisonicsConfig kFullZerothOrderAmbisonicsConfig = {
    .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
    .ambisonics_config = AmbisonicsMonoConfig{.output_channel_count = 1,
                                              .substream_count = 1,
                                              .channel_mapping = {0}}};

const ExtensionConfig kExtensionConfig = {.audio_element_config_bytes = {}};

const RenderingConfig kHeadphonesAsStereoRenderingConfig = {
    .headphones_rendering_mode =
        RenderingConfig::kHeadphonesRenderingModeStereo};

const RenderingConfig kHeadphonesAsBinauralRenderingConfig = {
    .headphones_rendering_mode =
        RenderingConfig::kHeadphonesRenderingModeBinaural};

TEST(CreateRendererForLayout, SupportsPassThroughRenderer) {
  const RendererFactory factory;

  EXPECT_NE(
      factory.CreateRendererForLayout(
          {0}, {{0, {kMono}}}, AudioElementObu::kAudioElementChannelBased,
          kMonoScalableChannelLayoutConfig, kHeadphonesAsStereoRenderingConfig,
          kMonoLayout, kNumSamplesPerFrame),
      nullptr);
}

TEST(CreateRendererForLayout, SupportsPassThroughBinauralRenderer) {
  const RendererFactory factory;

  EXPECT_NE(
      factory.CreateRendererForLayout(
          {0}, {{0, {kL2, kR2}}}, AudioElementObu::kAudioElementChannelBased,
          kBinauralChannelLayoutConfig, kHeadphonesAsBinauralRenderingConfig,
          kBinauralLayout, kNumSamplesPerFrame),
      nullptr);
}

TEST(CreateRendererForLayout,
     ReturnsNullPtrWhenTypeIsSceneBasedButConfigIsChannelBased) {
  const RendererFactory factory;

  EXPECT_EQ(
      factory.CreateRendererForLayout(
          {0}, {{0, {kA0}}}, AudioElementObu::kAudioElementSceneBased,
          kMonoScalableChannelLayoutConfig, kHeadphonesAsStereoRenderingConfig,
          kMonoLayout, kNumSamplesPerFrame),
      nullptr);
}

TEST(CreateRendererForLayout,
     ReturnsNullPtrWhenTypeIsChannelBasedButConfigIsAmbisonics) {
  const RendererFactory factory;

  EXPECT_EQ(
      factory.CreateRendererForLayout(
          {0}, {{0, {kMono}}}, AudioElementObu::kAudioElementChannelBased,
          kFullZerothOrderAmbisonicsConfig, kHeadphonesAsStereoRenderingConfig,
          kMonoLayout, kNumSamplesPerFrame),
      nullptr);
}

// TODO(b/282877209): Support channel-based to binaural renderer.
TEST(CreateRendererForLayout, ReturnsNullPtrForChannelToBinauralRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {kMono}}}, AudioElementObu::kAudioElementChannelBased,
                kMonoScalableChannelLayoutConfig,
                kHeadphonesAsBinauralRenderingConfig, kBinauralLayout,
                kNumSamplesPerFrame),
            nullptr);
}

TEST(CreateRendererForLayout, ReturnsNullPtrForUnknownExtension) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {kMono}}}, AudioElementObu::kAudioElementEndReserved,
                kExtensionConfig, kHeadphonesAsStereoRenderingConfig,
                kBinauralLayout, kNumSamplesPerFrame),
            nullptr);
}

TEST(CreateRendererForLayout, SupportsChannelToChannelRenderer) {
  const RendererFactory factory;

  EXPECT_NE(
      factory.CreateRendererForLayout(
          {0}, {{0, {kL2, kR2}}}, AudioElementObu::kAudioElementChannelBased,
          kStereoScalableChannelLayoutConfig,
          kHeadphonesAsStereoRenderingConfig, kMonoLayout, kNumSamplesPerFrame),
      nullptr);
}

TEST(CreateRendererForLayout, SupportsAmbisonicsToChannelRenderer) {
  const RendererFactory factory;

  EXPECT_NE(
      factory.CreateRendererForLayout(
          {0}, {{0, {kA0}}}, AudioElementObu::kAudioElementSceneBased,
          kFullZerothOrderAmbisonicsConfig, kHeadphonesAsStereoRenderingConfig,
          kMonoLayout, kNumSamplesPerFrame),
      nullptr);
}

// TODO(b/282877209): Support ambisonics to binaural renderer.
TEST(CreateRendererForLayout, ReturnsNullPtrForAmbisonicsToBinauralRenderer) {
  const RendererFactory factory;

  EXPECT_EQ(factory.CreateRendererForLayout(
                {0}, {{0, {kA0}}}, AudioElementObu::kAudioElementSceneBased,
                kFullZerothOrderAmbisonicsConfig,
                kHeadphonesAsBinauralRenderingConfig, kBinauralLayout,
                kNumSamplesPerFrame),
            nullptr);
}

}  // namespace
}  // namespace iamf_tools
