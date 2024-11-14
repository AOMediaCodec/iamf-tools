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
using enum ChannelLabel::Label;
using enum LoudspeakersSsConventionLayout::SoundSystem;
using testing::DoubleEq;
using testing::Pointwise;

constexpr InternalSampleType kArbitrarySample1 = 12345.0;
constexpr InternalSampleType kArbitrarySample2 = 6789.0;
constexpr InternalSampleType kArbitrarySample3 = 101112.0;
constexpr InternalSampleType kArbitrarySample4 = 9999999.0;
constexpr InternalSampleType kArbitrarySample5 = 987654321.0;
constexpr InternalSampleType kArbitrarySample6 = 1024.0;

constexpr int kStereoL2ChannelIndex = 0;
constexpr int kStereoR2ChannelIndex = 1;
constexpr int k3_1_2LFEChannelIndex = 3;
constexpr int k5_1LFEChannelIndex = 3;
constexpr int k9_1_6LFEChannelIndex = 3;

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
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutBinaural}}};
const ScalableChannelLayoutConfig kStereoScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayoutStereo}}};
const ScalableChannelLayoutConfig k5_1_0ScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout5_1_ch}}};
const ScalableChannelLayoutConfig k7_1_4ScalableChannelLayoutConfig = {
    .num_layers = 1,
    .channel_audio_layer_configs = {{.loudspeaker_layout = kLayout7_1_4_ch}}};

ScalableChannelLayoutConfig
GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
    ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout) {
  return {.num_layers = 1,
          .channel_audio_layer_configs = {
              {.loudspeaker_layout = kLayoutExpanded,
               .expanded_loudspeaker_layout = expanded_layout}}};
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownMixingStereoToMono) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    kStereoScalableChannelLayoutConfig, kMonoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownMixing7_1_4To7_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    k7_1_4ScalableChannelLayoutConfig, k7_1_2Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig, SupportsDownMixing7_1_4To3_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    k7_1_4ScalableChannelLayoutConfig, k3_1_2Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedLFEToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLFE),
                    kStereoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedLFETo7_1_2) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLFE),
                    k7_1_2Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassthroughExpandedLFETo7_1_4) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutLFE),
                    k7_1_4Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedStereoSToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoS),
                    kStereoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassthroughExpandedStereoSTo5_1_4) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoS),
                    k5_1_4Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpanded9_1_6ToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
                    kStereoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     SupportsDownMixingExpandedStereoSSToStereo) {
  EXPECT_NE(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoSS),
                    kStereoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportPassthroughExpandedStereoF9_1_6) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutStereoF),
                    k9_1_6Layout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportExpandedLayoutReserved13ToStereo) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutReserved13),
                    kStereoLayout),
            nullptr);
}

TEST(CreateFromScalableChannelLayoutConfig,
     DoesNotSupportExpandedLayoutReserved255ToStereo) {
  EXPECT_EQ(AudioElementRendererChannelToChannel::
                CreateFromScalableChannelLayoutConfig(
                    GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
                        ChannelAudioLayerConfig::kExpandedLayoutReserved255),
                    kStereoLayout),
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
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};

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

TEST(CreateFromScalableChannelLayoutConfig,
     IsFinalizedImmediatelyAfterFinalizeCall) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(kStereoScalableChannelLayoutConfig,
                                            kMonoLayout);
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
                                            kMonoLayout);
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
                                            kMonoLayout);

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk(
      {.label_to_samples = {{kL2, kL2Samples}, {kR2, kR2Samples}}},
      renderer.get(), rendered_samples);

  EXPECT_THAT(rendered_samples, Pointwise(DoubleEq(), kExpectedMonoSamples));
}

TEST(RenderLabeledFrame,
     StereoOutputIsSymmetricWhenInputIsLeftRightSymmetric7_1_4) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(k7_1_4ScalableChannelLayoutConfig,
                                            kStereoLayout);
  constexpr InternalSampleType kSymmetricL7R7Input = kArbitrarySample1;
  constexpr InternalSampleType kSymmetricLss7Rss7Input = kArbitrarySample2;
  constexpr InternalSampleType kSymmetricLrs7Rss7Input = kArbitrarySample3;
  constexpr InternalSampleType kSymmetricLtf4Rtf4Input = kArbitrarySample4;
  constexpr InternalSampleType kSymmetricLtb4Rtb4Input = kArbitrarySample5;
  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples =
                              {
                                  {kL7, {kSymmetricL7R7Input}},
                                  {kR7, {kSymmetricL7R7Input}},
                                  {kCentre, {123456.0}},
                                  {kLFE, {1234.0}},
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
  EXPECT_DOUBLE_EQ(rendered_samples[kStereoL2ChannelIndex],
                   rendered_samples[kStereoR2ChannelIndex]);
}

TEST(RenderLabeledFrame, PassThroughLFE) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutLFE),
          k3_1_2Layout);

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kLFE, {kArbitrarySample1}}}},
                         renderer.get(), rendered_samples);

  EXPECT_DOUBLE_EQ(rendered_samples[k3_1_2LFEChannelIndex], kArbitrarySample1);
}

TEST(RenderLabeledFrame, LFEPassesThroughFrom9_1_6) {
  constexpr InternalSampleType kLFESample = 1234.0;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          k5_1_0Layout);
  ASSERT_NE(renderer, nullptr);

  std::vector<InternalSampleType> rendered_samples;
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
  EXPECT_DOUBLE_EQ(rendered_samples[k5_1LFEChannelIndex], kLFESample);
}

TEST(RenderLabeledFrame, LFEPassesThroughTo9_1_6) {
  constexpr InternalSampleType kLFESample = 1234.0;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(k5_1_0ScalableChannelLayoutConfig,
                                            k9_1_6Layout);
  ASSERT_NE(renderer, nullptr);

  std::vector<InternalSampleType> rendered_samples;
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
  EXPECT_DOUBLE_EQ(rendered_samples[k9_1_6LFEChannelIndex], kLFESample);
}

TEST(RenderLabeledFrame, PassThroughStereoS) {
  constexpr int k5_1_0Ls5ChannelIndex = 4;
  constexpr int k5_1_0Rs5ChannelIndex = 5;
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayoutStereoS),
          k5_1_0Layout);
  ASSERT_NE(renderer, nullptr);

  std::vector<InternalSampleType> rendered_samples;
  RenderAndFlushExpectOk({.label_to_samples = {{kLs5, {kArbitrarySample1}},
                                               {kRs5, {kArbitrarySample2}}}},
                         renderer.get(), rendered_samples);

  EXPECT_DOUBLE_EQ(rendered_samples.size(), 6);
  EXPECT_DOUBLE_EQ(rendered_samples[k5_1_0Ls5ChannelIndex], kArbitrarySample1);
  EXPECT_DOUBLE_EQ(rendered_samples[k5_1_0Rs5ChannelIndex], kArbitrarySample2);
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
          GetParam().output_layout);
  auto renderer_related_loudspeaker_layout =
      AudioElementRendererChannelToChannel::
          CreateFromScalableChannelLayoutConfig(
              GetParam().related_scalable_layout_config,
              GetParam().output_layout);

  std::vector<InternalSampleType> expanded_layout_rendered_samples;
  RenderAndFlushExpectOk(
      LabeledFrame{.label_to_samples =
                       GetParam().expanded_layout_labeled_frame},
      renderer_expanded_layout.get(), expanded_layout_rendered_samples);
  std::vector<InternalSampleType> related_layout_rendered_samples;
  RenderAndFlushExpectOk(
      {LabeledFrame{.label_to_samples =
                        GetParam().related_loudspeaker_layout_labeled_frame}},
      renderer_related_loudspeaker_layout.get(),
      related_layout_rendered_samples);

  EXPECT_THAT(expanded_layout_rendered_samples,
              Pointwise(DoubleEq(), related_layout_rendered_samples));
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

TEST(RenderLabeledFrame,
     StereoOutputIsSymmetricWhenInputIsLeftRightSymmetric9_1_6) {
  auto renderer = AudioElementRendererChannelToChannel::
      CreateFromScalableChannelLayoutConfig(
          GetScalableChannelLayoutConfigForExpandedLayoutSoundSystem(
              ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch),
          kStereoLayout);
  constexpr InternalSampleType kSymmetricFLFRInput = kArbitrarySample1;
  constexpr InternalSampleType kSymmetricBLBRInput = kArbitrarySample2;
  constexpr InternalSampleType kSymmetricFLcFRcInput = kArbitrarySample3;
  constexpr InternalSampleType kSymmetricSiLSiRInput = kArbitrarySample4;
  constexpr InternalSampleType kSymmetricTpFLTpFRInput = kArbitrarySample5;
  constexpr InternalSampleType kSymmetricTpBLTpBRInput = kArbitrarySample6;
  constexpr InternalSampleType kSymmetricTpSiLTpSiRInput = 999999999.0;
  std::vector<InternalSampleType> rendered_samples;
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
  EXPECT_DOUBLE_EQ(rendered_samples[kStereoL2ChannelIndex],
                   rendered_samples[kStereoR2ChannelIndex]);
}

}  // namespace
}  // namespace iamf_tools
