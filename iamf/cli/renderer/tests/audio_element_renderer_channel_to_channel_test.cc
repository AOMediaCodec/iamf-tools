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

#include <cstddef>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ChannelLabel::Label;
using enum LoudspeakersSsConventionLayout::SoundSystem;
using testing::DoubleEq;
using testing::Pointwise;

constexpr InternalSampleType kArbitrarySample1 = 0.000012345;
constexpr InternalSampleType kArbitrarySample2 = 0.000006789;
constexpr InternalSampleType kArbitrarySample3 = 0.000101112;
constexpr InternalSampleType kArbitrarySample4 = 0.009999999;
constexpr InternalSampleType kArbitrarySample5 = 0.987654321;
constexpr InternalSampleType kArbitrarySample6 = 0.000001024;
constexpr double kFloatingPointTolerance = 1e-9;

constexpr int kMonoChannelIndex = 0;
constexpr int kStereoL2ChannelIndex = 0;
constexpr int kStereoR2ChannelIndex = 1;
constexpr int k3_1_2LFEChannelIndex = 3;
constexpr int k5_1LFEChannelIndex = 3;
constexpr int k9_1_6LFEChannelIndex = 3;

constexpr size_t kOneSamplePerFrame = 1;

const Layout kMonoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem12_0_1_0}};
const Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemA_0_2_0}};
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};
const Layout k5_1_0Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemB_0_5_0}};
const Layout k5_1_4Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemD_4_5_0}};
const Layout k7_1_4Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemJ_4_7_0}};
const Layout k9_1_6Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem13_6_9_0}};

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
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig k5_1_0ScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout5_1_ch}}};
const ScalableChannelLayoutConfig k7_1_4ScalableChannelLayoutConfig = {
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout7_1_4_ch}}};

ScalableChannelLayoutConfig
GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout) {
  return {.channel_audio_layer_configs = {
              {.loudspeaker_layout = kLayoutExpanded,
               .expanded_loudspeaker_layout = expanded_layout}}};
}

constexpr DownMixingParams kDMixPMode1DownMixingParams = {.alpha = 1.0,
                                                          .beta = 1.0,
                                                          .gamma = 0.707,
                                                          .delta = 0.707,
                                                          .w = 0.707,
                                                          .in_bitstream = true};

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownMixingStereoToMono) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kMonoLayout,
                    kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownMixing7_1_4To7_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    k7_1_4ScalableChannelLayoutConfig, k7_1_2Layout,
                    kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownMixing7_1_4To3_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    k7_1_4ScalableChannelLayoutConfig, k3_1_2Layout,
                    kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedLFEToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLFE),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedLFETo7_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLFE),
                    k7_1_2Layout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassthroughExpandedLFETo7_1_4) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLFE),
                    k7_1_4Layout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedStereoSToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoS),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassthroughExpandedStereoSTo5_1_4) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoS),
                    k5_1_4Layout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpanded9_1_6ToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedStereoSSToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoSS),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassthroughExpandedStereoF9_1_6) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoF),
                    k9_1_6Layout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpanded10_2_9_3ToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayout10_2_9_3),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsExpandedLayoutLfePairToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLfePair),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsExpandedLayoutBottom3ChToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportExpandedLayoutReserved16ToStereo) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutReserved16),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportExpandedLayoutReserved255ToStereo) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutReserved255),
                    kStereoLayout, kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportBinaural) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kBinauralScalableChannelLayoutConfig, kBinauralLayout,
                    kOneSamplePerFrame),
            nullptr);
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kBinauralScalableChannelLayoutConfig, kStereoLayout,
                    kOneSamplePerFrame),
            nullptr);
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kBinauralLayout,
                    kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportReservedLayout) {
  const Layout kReservedLayout = {
      .layout_type = Layout::kLayoutTypeReserved0,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};

  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kReservedLayout,
                    kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, DoesNotSupportPassThroughStereo) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kStereoLayout,
                    kOneSamplePerFrame),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     IsFinalizedImmediatelyAfterFinalizeCall) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(kStereoScalableChannelLayoutConfig,
                                            kMonoLayout, kOneSamplePerFrame);
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->RenderLabeledFrame(
                  {.label_to_samples = {{kL2, {kArbitrarySample1}},
                                        {kR2, {kArbitrarySample2}}}}),
              IsOk());
  EXPECT_THAT(renderer->Finalize(), IsOk());
  EXPECT_TRUE(renderer->IsFinalized());
}

TEST(RenderLabeledFrame, ReturnsNumberOfTicks) {
  const int kNumTicks = 3;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(kStereoScalableChannelLayoutConfig,
                                            kMonoLayout, kNumTicks);
  ASSERT_NE(renderer, nullptr);

  const auto num_ticks = renderer->RenderLabeledFrame(
      {.label_to_samples = {
           {kL2, std::vector<InternalSampleType>(kNumTicks, kArbitrarySample1)},
           {kR2,
            std::vector<InternalSampleType>(kNumTicks, kArbitrarySample2)}}});
  ASSERT_THAT(num_ticks, IsOk());

  EXPECT_EQ(*num_ticks, kNumTicks);
}

TEST(RenderLabeledFrame, RendersStereoToMono) {
  const std::vector<InternalSampleType> kL2Samples = {50, 100, 10000};
  const std::vector<InternalSampleType> kR2Samples = {100, 50, 0};
  const std::vector<InternalSampleType> kExpectedMonoSamples = {75, 75, 5000};

  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(kStereoScalableChannelLayoutConfig,
                                            kMonoLayout, kL2Samples.size());

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(
      {.label_to_samples = {{kL2, kL2Samples}, {kR2, kR2Samples}}},
      renderer.get(), rendered_samples);

  EXPECT_THAT(rendered_samples[kMonoChannelIndex],
              Pointwise(DoubleEq(), kExpectedMonoSamples));
}

TEST(RenderLabeledFrame,
     StereoOutputIsSymmetricWhenInputIsLeftRightSymmetric7_1_4) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(k7_1_4ScalableChannelLayoutConfig,
                                            kStereoLayout, kOneSamplePerFrame);
  constexpr InternalSampleType kSymmetricL7R7Input = kArbitrarySample1;
  constexpr InternalSampleType kSymmetricLss7Rss7Input = kArbitrarySample2;
  constexpr InternalSampleType kSymmetricLrs7Rss7Input = kArbitrarySample3;
  constexpr InternalSampleType kSymmetricLtf4Rtf4Input = kArbitrarySample4;
  constexpr InternalSampleType kSymmetricLtb4Rtb4Input = kArbitrarySample5;
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kL7, {kSymmetricL7R7Input}},
                                  {kR7, {kSymmetricL7R7Input}},
                                  {kCentre, {0.123456}},
                                  {kLFE, {0.001234}},
                                  {kLss7, {kSymmetricLss7Rss7Input}},
                                  {kRss7, {kSymmetricLss7Rss7Input}},
                                  {kLrs7, {kSymmetricLrs7Rss7Input}},
                                  {kRrs7, {kSymmetricLrs7Rss7Input}},
                                  {kLtf4, {kSymmetricLtf4Rtf4Input}},
                                  {kRtf4, {kSymmetricLtf4Rtf4Input}},
                                  {kLtb4, {kSymmetricLtb4Rtb4Input}},
                                  {kRtb4, {kSymmetricLtb4Rtb4Input}},
                              }},
                         renderer.get(), rendered_samples);

  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples[kStereoL2ChannelIndex],
              Pointwise(DoubleEq(), rendered_samples[kStereoR2ChannelIndex]));
}

TEST(RenderLabeledFrame, Renders5_1_0WithDemixingParametersToStereo) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(k5_1_0ScalableChannelLayoutConfig,
                                            kStereoLayout, kOneSamplePerFrame);
  ASSERT_NE(renderer, nullptr);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(
      {
          .label_to_samples =
              {
                  {kL5, {1.0}},
                  {kR5, {2.0}},
                  {kCentre, {3.0}},
                  {kLFE, {100.0}},  // LFE should be ignored.
                  {kLs5, {4.0}},
                  {kRs5, {5.0}},
              },
          .demixing_params = kDMixPMode1DownMixingParams,
      },
      renderer.get(), rendered_samples);

  // Just check that rendering successfully completed and there are two output
  // channels.
  EXPECT_EQ(rendered_samples.size(), 2);
}

TEST(RenderLabeledFrame,
     AppliesGammaToHeightChannelsWhenDownmixing7_1_4to7_1_2) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(k7_1_4ScalableChannelLayoutConfig,
                                            k7_1_2Layout, kOneSamplePerFrame);
  ASSERT_NE(renderer, nullptr);
  constexpr DownMixingParams kDownMixParams = {.gamma = 0.5,
                                               .in_bitstream = true};
  const InternalSampleType kLtbSample = kArbitrarySample1;
  const InternalSampleType kRtbSample = kArbitrarySample2;

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kL7, {0}},
                                               {kR7, {0}},
                                               {kCentre, {0}},
                                               {kLFE, {0}},
                                               {kLss7, {0}},
                                               {kRss7, {0}},
                                               {kLrs7, {0}},
                                               {kRrs7, {0}},
                                               {kLtf4, {0}},
                                               {kRtf4, {0}},
                                               {kLtb4, {kLtbSample}},
                                               {kRtb4, {kRtbSample}}},
                          .demixing_params = kDownMixParams},
                         renderer.get(), rendered_samples);

  // The output is 7.1.2 layout, which has 10 channels:
  // L7, R7, C, LFE, Lss7, Rss7, Lrs7, Rrs7, Ltf2, Rtf2
  // We expect Ltb4 to be mixed into Ltf2, and Rtb4 into Rtf2, both scaled by
  // gamma.
  constexpr int k7_1_2Ltf2ChannelIndex = 8;
  constexpr int k7_1_2Rtf2ChannelIndex = 9;
  EXPECT_EQ(rendered_samples.size(), 10);
  EXPECT_NEAR(rendered_samples[k7_1_2Ltf2ChannelIndex][0],
              kDownMixParams.gamma * kLtbSample, kFloatingPointTolerance);
  EXPECT_NEAR(rendered_samples[k7_1_2Rtf2ChannelIndex][0],
              kDownMixParams.gamma * kRtbSample, kFloatingPointTolerance);
}

TEST(RenderLabeledFrame, PassThroughLFE) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutLFE),
          k3_1_2Layout, kOneSamplePerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kLFE, {kArbitrarySample1}}}},
                         renderer.get(), rendered_samples);

  EXPECT_THAT(rendered_samples[k3_1_2LFEChannelIndex],
              Pointwise(DoubleEq(), {kArbitrarySample1}));
}

TEST(RenderLabeledFrame, DropsLFEPairToStereo) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutLfePair),
          kStereoLayout, kOneSamplePerFrame);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kLFE, {kArbitrarySample1}},
                                               {kLFE2, {kArbitrarySample2}}}},
                         renderer.get(), rendered_samples);

  EXPECT_THAT(rendered_samples[0], Pointwise(DoubleEq(), {0}));
  EXPECT_THAT(rendered_samples[1], Pointwise(DoubleEq(), {0}));
}

TEST(RenderLabeledFrame, LFEPassesThroughFrom9_1_6) {
  constexpr InternalSampleType kLFESample = 1234.0;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          k5_1_0Layout, kOneSamplePerFrame);
  ASSERT_NE(renderer, nullptr);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kFL, {kArbitrarySample1}},
                                               {kFR, {kArbitrarySample1}},
                                               {kFC, {kArbitrarySample1}},
                                               {kLFE, {kLFESample}},
                                               {kBL, {kArbitrarySample1}},
                                               {kBR, {kArbitrarySample1}},
                                               {kFLc, {kArbitrarySample1}},
                                               {kFRc, {kArbitrarySample1}},
                                               {kSiL, {kArbitrarySample1}},
                                               {kSiR, {kArbitrarySample1}},
                                               {kTpFL, {kArbitrarySample1}},
                                               {kTpFR, {kArbitrarySample1}},
                                               {kTpBL, {kArbitrarySample1}},
                                               {kTpBR, {kArbitrarySample1}},
                                               {kTpSiL, {kArbitrarySample1}},
                                               {kTpSiR, {kArbitrarySample1}}}},
                         renderer.get(), rendered_samples);

  ASSERT_GE(rendered_samples.size(), k5_1LFEChannelIndex);
  EXPECT_THAT(rendered_samples[k5_1LFEChannelIndex],
              Pointwise(DoubleEq(), {kLFESample}));
}

TEST(RenderLabeledFrame, LFEPassesThroughTo9_1_6) {
  constexpr InternalSampleType kLFESample = 1234.0;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(k5_1_0ScalableChannelLayoutConfig,
                                            k9_1_6Layout, kOneSamplePerFrame);
  ASSERT_NE(renderer, nullptr);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kL5, {kArbitrarySample1}},
                                  {kR5, {kArbitrarySample1}},
                                  {kCentre, {kArbitrarySample1}},
                                  {kLFE, {kLFESample}},
                                  {kLs5, {kArbitrarySample1}},
                                  {kRs5, {kArbitrarySample1}},
                              }},
                         renderer.get(), rendered_samples);

  ASSERT_GE(rendered_samples.size(), k9_1_6LFEChannelIndex);
  EXPECT_THAT(rendered_samples[k9_1_6LFEChannelIndex],
              Pointwise(DoubleEq(), {kLFESample}));
}

TEST(RenderLabeledFrame, PassThroughStereoS) {
  constexpr int k5_1_0Ls5ChannelIndex = 4;
  constexpr int k5_1_0Rs5ChannelIndex = 5;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutStereoS),
          k5_1_0Layout, kOneSamplePerFrame);
  ASSERT_NE(renderer, nullptr);

  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kLs5, {kArbitrarySample1}},
                                               {kRs5, {kArbitrarySample2}}}},
                         renderer.get(), rendered_samples);

  EXPECT_EQ(rendered_samples.size(), 6);
  EXPECT_THAT(rendered_samples[k5_1_0Ls5ChannelIndex],
              Pointwise(DoubleEq(), {kArbitrarySample1}));
  EXPECT_THAT(rendered_samples[k5_1_0Rs5ChannelIndex],
              Pointwise(DoubleEq(), {kArbitrarySample2}));
}

struct ExpandedLayoutAndRelatedLoudspeakerLayout {
  const ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout;
  const LabelSamplesMap expanded_layout_labeled_frame;
  const ScalableChannelLayoutConfig related_scalable_layout_config;
  const LabelSamplesMap related_loudspeaker_layout_labeled_frame;
  const Layout output_layout;
};

using ExpandedLayoutAndRelatedLoudspeakerLayoutTest =
    ::testing::TestWithParam<ExpandedLayoutAndRelatedLoudspeakerLayout>;
TEST_P(ExpandedLayoutAndRelatedLoudspeakerLayoutTest, Equivalent) {
  auto renderer_expanded_layout = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              GetParam().expanded_layout),
          GetParam().output_layout, kOneSamplePerFrame);
  auto renderer_related_loudspeaker_layout =
      AudioElementRendererChannelToChannel::
          CreateFromScalableChannelLayoutConfig(
              GetParam().related_scalable_layout_config,
              GetParam().output_layout, kOneSamplePerFrame);

  std::vector<std::vector<InternalSampleType>> expanded_layout_rendered_samples;
  RenderAndFlushExpectOk(
      LabeledFrame{.label_to_samples =
                       GetParam().expanded_layout_labeled_frame},
      renderer_expanded_layout.get(), expanded_layout_rendered_samples);
  std::vector<std::vector<InternalSampleType>> related_layout_rendered_samples;
  RenderAndFlushExpectOk(
      {LabeledFrame{.label_to_samples =
                        GetParam().related_loudspeaker_layout_labeled_frame}},
      renderer_related_loudspeaker_layout.get(),
      related_layout_rendered_samples);

  EXPECT_THAT(expanded_layout_rendered_samples,
              InternalSamples2DMatch(related_layout_rendered_samples));
}

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoSEquivalentTo5_1_4,
    ExpandedLayoutAndRelatedLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndRelatedLoudspeakerLayout>(
        {{.expanded_layout = ChannelAudioLayerConfig::kExpandedLayoutStereoS,
          .expanded_layout_labeled_frame = {{kLs5, {kArbitrarySample1}},
                                            {kRs5, {kArbitrarySample2}}},
          .related_scalable_layout_config = k5_1_0ScalableChannelLayoutConfig,
          .related_loudspeaker_layout_labeled_frame =
              {{kL5, {0}},
               {kR5, {0}},
               {kCentre, {0}},
               {kLFE, {0}},
               {kLs5, {kArbitrarySample1}},
               {kRs5, {kArbitrarySample2}}},
          .output_layout = k3_1_2Layout}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutTop4ChEquivalentTo7_1_4,
    ExpandedLayoutAndRelatedLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndRelatedLoudspeakerLayout>(
        {{.expanded_layout = ChannelAudioLayerConfig::kExpandedLayoutTop4Ch,
          .expanded_layout_labeled_frame = {{kLtf4, {kArbitrarySample1}},
                                            {kRtf4, {kArbitrarySample2}},
                                            {kLtb4, {kArbitrarySample3}},
                                            {kRtb4, {kArbitrarySample4}}},
          .related_scalable_layout_config = k7_1_4ScalableChannelLayoutConfig,
          .related_loudspeaker_layout_labeled_frame =
              {{kL7, {0}},
               {kR7, {0}},
               {kCentre, {0}},
               {kLFE, {0}},
               {kLss7, {0}},
               {kRss7, {0}},
               {kLrs7, {0}},
               {kRrs7, {0}},
               {kLtf4, {kArbitrarySample1}},
               {kRtf4, {kArbitrarySample2}},
               {kLtb4, {kArbitrarySample3}},
               {kRtb4, {kArbitrarySample4}}},
          .output_layout = k3_1_2Layout}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutTop6ChEquivalentTo9_1_6,
    ExpandedLayoutAndRelatedLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndRelatedLoudspeakerLayout>(
        {{.expanded_layout = ChannelAudioLayerConfig::kExpandedLayoutTop6Ch,
          .expanded_layout_labeled_frame = {{kTpFL, {kArbitrarySample1}},
                                            {kTpFR, {kArbitrarySample2}},
                                            {kTpSiL, {kArbitrarySample3}},
                                            {kTpSiR, {kArbitrarySample4}},
                                            {kTpBL, {kArbitrarySample5}},
                                            {kTpBR, {kArbitrarySample6}}},
          .related_scalable_layout_config =
              GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                  ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          .related_loudspeaker_layout_labeled_frame =
              {{kFL, {0}},
               {kFR, {0}},
               {kFC, {0}},
               {kLFE, {0}},
               {kBL, {0}},
               {kBR, {0}},
               {kFLc, {0}},
               {kFRc, {0}},
               {kSiL, {0}},
               {kSiR, {0}},
               {kTpFL, {kArbitrarySample1}},
               {kTpFR, {kArbitrarySample2}},
               {kTpSiL, {kArbitrarySample3}},
               {kTpSiR, {kArbitrarySample4}},
               {kTpBL, {kArbitrarySample5}},
               {kTpBR, {kArbitrarySample6}}},
          .output_layout = k3_1_2Layout}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutLfePairEquivalentTo10_2_9_3,
    ExpandedLayoutAndRelatedLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndRelatedLoudspeakerLayout>(
        {{.expanded_layout = ChannelAudioLayerConfig::kExpandedLayoutLfePair,
          .expanded_layout_labeled_frame = {{kLFE, {kArbitrarySample1}},
                                            {kLFE2, {kArbitrarySample2}}},
          .related_scalable_layout_config =
              GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                  ChannelAudioLayerConfig::kExpandedLayout10_2_9_3),
          .related_loudspeaker_layout_labeled_frame =
              {
                  {kFLc, {0}},
                  {kFRc, {0}},
                  {kFL, {0}},
                  {kFR, {0}},
                  {kSiL, {0}},
                  {kSiR, {0}},
                  {kBL, {0}},
                  {kBR, {0}},
                  {kTpFL, {0}},
                  {kTpFR, {0}},
                  {kTpSiL, {0}},
                  {kTpSiR, {0}},
                  {kTpBL, {0}},
                  {kTpBR, {0}},
                  {kTpFL, {0}},
                  {kBtFL, {0}},
                  {kBtFR, {0}},
                  {kFC, {0}},
                  {kBC, {0}},
                  {kTpFC, {0}},
                  {kTpC, {0}},
                  {kTpBC, {0}},
                  {kBtFC, {0}},
                  {kLFE, {kArbitrarySample1}},
                  {kLFE2, {kArbitrarySample2}},
              },
          .output_layout = k3_1_2Layout}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutBottom3ChEquivalentTo10_2_9_3,
    ExpandedLayoutAndRelatedLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndRelatedLoudspeakerLayout>(
        {{.expanded_layout = ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch,
          .expanded_layout_labeled_frame = {{kBtFL, {kArbitrarySample1}},
                                            {kBtFR, {kArbitrarySample2}},
                                            {kBtFC, {kArbitrarySample3}}},
          .related_scalable_layout_config =
              GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                  ChannelAudioLayerConfig::kExpandedLayout10_2_9_3),
          .related_loudspeaker_layout_labeled_frame =
              {
                  {kFLc, {0}},
                  {kFRc, {0}},
                  {kFL, {0}},
                  {kFR, {0}},
                  {kSiL, {0}},
                  {kSiR, {0}},
                  {kBL, {0}},
                  {kBR, {0}},
                  {kTpFL, {0}},
                  {kTpFR, {0}},
                  {kTpSiL, {0}},
                  {kTpSiR, {0}},
                  {kTpBL, {0}},
                  {kTpBR, {0}},
                  {kTpFL, {0}},
                  {kBtFL, {kArbitrarySample1}},
                  {kBtFR, {kArbitrarySample2}},
                  {kFC, {0}},
                  {kBC, {0}},
                  {kTpFC, {0}},
                  {kTpC, {0}},
                  {kTpBC, {0}},
                  {kBtFC, {kArbitrarySample3}},
                  {kLFE, {0}},
                  {kLFE2, {0}},
              },
          .output_layout = k3_1_2Layout}}));

TEST(RenderLabeledFrame,
     StereoOutputIsSymmetricWhenInputIsLeftRightSymmetric9_1_6) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          kStereoLayout, kOneSamplePerFrame);
  constexpr InternalSampleType kSymmetricFLFRInput = kArbitrarySample1;
  constexpr InternalSampleType kSymmetricBLBRInput = kArbitrarySample2;
  constexpr InternalSampleType kSymmetricFLcFRcInput = kArbitrarySample3;
  constexpr InternalSampleType kSymmetricSiLSiRInput = kArbitrarySample4;
  constexpr InternalSampleType kSymmetricTpFLTpFRInput = kArbitrarySample5;
  constexpr InternalSampleType kSymmetricTpBLTpBRInput = kArbitrarySample6;
  constexpr InternalSampleType kSymmetricTpSiLTpSiRInput = 999999999.0;
  std::vector<std::vector<InternalSampleType>> rendered_samples;
  RenderAndFlushExpectOk(
      {.label_to_samples = {{kFL, {kSymmetricFLFRInput}},
                            {kFR, {kSymmetricFLFRInput}},
                            {kFC, {999.0}},
                            {kLFE, {9999.0}},
                            {kBL, {kSymmetricBLBRInput}},
                            {kBR, {kSymmetricBLBRInput}},
                            {kFLc, {kSymmetricFLcFRcInput}},
                            {kFRc, {kSymmetricFLcFRcInput}},
                            {kSiL, {kSymmetricSiLSiRInput}},
                            {kSiR, {kSymmetricSiLSiRInput}},
                            {kTpFL, {kSymmetricTpFLTpFRInput}},
                            {kTpFR, {kSymmetricTpFLTpFRInput}},
                            {kTpBL, {kSymmetricTpBLTpBRInput}},
                            {kTpBR, {kSymmetricTpBLTpBRInput}},
                            {kTpSiL, {kSymmetricTpSiLTpSiRInput}},
                            {kTpSiR, {kSymmetricTpSiLTpSiRInput}}}},
      renderer.get(), rendered_samples);

  EXPECT_EQ(rendered_samples.size(), 2);
  EXPECT_THAT(rendered_samples[kStereoL2ChannelIndex],
              Pointwise(DoubleEq(), rendered_samples[kStereoR2ChannelIndex]));
}

}  // namespace
}  // namespace iamf_tools
