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

#include "iamf/cli/renderer/audio_element_renderer_ambisonics_to_channel.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"
namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelLabel::Label;

using enum LoudspeakersSsConventionLayout::SoundSystem;

const Layout kMonoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem12_0_1_0}};
const Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemA_0_2_0}};
const Layout k9_1_6Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem13_6_9_0}};
const Layout kBinauralLayout = {.layout_type = Layout::kLayoutTypeBinaural};

// The IAMF spec recommends special rules for some layouts.
const Layout k7_1_2Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystem10_2_7_0}};

const AmbisonicsConfig kFullZerothOrderAmbisonicsConfig = {
    .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
    .ambisonics_config = AmbisonicsMonoConfig{.output_channel_count = 1,
                                              .substream_count = 1,
                                              .channel_mapping = {0}}};
const std::vector<DecodedUleb128> kFullZerothOrderAudioSubstreamIds = {0};

const AmbisonicsConfig kFullFirstOrderAmbisonicsConfig = {
    .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
    .ambisonics_config = AmbisonicsMonoConfig{.output_channel_count = 4,
                                              .substream_count = 4,
                                              .channel_mapping = {0, 1, 2, 3}}};
const std::vector<DecodedUleb128> kFullFirstOrderAudioSubstreamIds = {0, 1, 2,
                                                                      3};

const AmbisonicsConfig kFullThirdOrderAmbisonicsConfig = {
    .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
    .ambisonics_config =
        AmbisonicsMonoConfig{.output_channel_count = 16,
                             .substream_count = 16,
                             .channel_mapping = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                                 10, 11, 12, 13, 14, 15}}};
const std::vector<DecodedUleb128> kFullThirdOrderAudioSubstreamIds = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

const AmbisonicsConfig kFullFourthOrderAmbisonicsConfig = {
    .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
    .ambisonics_config = AmbisonicsMonoConfig{
        .output_channel_count = 25,
        .substream_count = 25,
        .channel_mapping = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                            13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24}}};
const std::vector<DecodedUleb128> kFullFourthOrderAudioSubstreamIds = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
    13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};

// =========== Full-order ambisonics mono config ===========
TEST(CreateFromAmbisonicsConfig, SupportsZerothOrderToMono) {
  const SubstreamIdLabelsMap kZerothOrderSubstreamIdToLabels = {{0, {kA0}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullZerothOrderAmbisonicsConfig, kFullZerothOrderAudioSubstreamIds,
          kZerothOrderSubstreamIdToLabels, kMonoLayout),
      nullptr);
}

TEST(CreateFromAmbisonicsConfig, SupportsZerothOrderToStereo) {
  const SubstreamIdLabelsMap kZerothOrderSubstreamIdToLabels = {{0, {kA0}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullZerothOrderAmbisonicsConfig, kFullZerothOrderAudioSubstreamIds,
          kZerothOrderSubstreamIdToLabels, kStereoLayout),
      nullptr);
}

TEST(CreateFromAmbisonicsConfig, SupportsZerothOrderTo9_1_6) {
  const SubstreamIdLabelsMap kZerothOrderSubstreamIdToLabels = {{0, {kA0}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullZerothOrderAmbisonicsConfig, kFullZerothOrderAudioSubstreamIds,
          kZerothOrderSubstreamIdToLabels, k9_1_6Layout),
      nullptr);
}

TEST(CreateFromAmbisonicsConfig, SupportsFirstOrderTo7_1_2) {
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullFirstOrderAmbisonicsConfig, kFullFirstOrderAudioSubstreamIds,
          kFirstOrderSubstreamIdToLabels, k7_1_2Layout),
      nullptr);
}

TEST(CreateFromAmbisonicsConfig, SupportsThirdOrderToStereo) {
  const SubstreamIdLabelsMap kThirdOrderSubstreamIdToLabels = {
      {0, {kA0}},   {1, {kA1}},   {2, {kA2}},   {3, {kA3}},
      {4, {kA4}},   {5, {kA5}},   {6, {kA6}},   {7, {kA7}},
      {8, {kA8}},   {9, {kA9}},   {10, {kA10}}, {11, {kA11}},
      {12, {kA12}}, {13, {kA13}}, {14, {kA14}}, {15, {kA15}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullThirdOrderAmbisonicsConfig, kFullThirdOrderAudioSubstreamIds,
          kThirdOrderSubstreamIdToLabels, kStereoLayout),
      nullptr);
}

TEST(CreateFromAmbisonicsConfig, SupportsFourthOrderAmbisonics) {
  const SubstreamIdLabelsMap kFourthOrderSubstreamIdToLabels = {
      {0, {kA0}},   {1, {kA1}},   {2, {kA2}},   {3, {kA3}},   {4, {kA4}},
      {5, {kA5}},   {6, {kA6}},   {7, {kA7}},   {8, {kA8}},   {9, {kA9}},
      {10, {kA10}}, {11, {kA11}}, {12, {kA12}}, {13, {kA13}}, {14, {kA14}},
      {15, {kA15}}, {16, {kA16}}, {17, {kA17}}, {18, {kA18}}, {19, {kA19}},
      {20, {kA20}}, {21, {kA21}}, {22, {kA22}}, {23, {kA23}}, {24, {kA24}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullFourthOrderAmbisonicsConfig, kFullFourthOrderAudioSubstreamIds,
          kFourthOrderSubstreamIdToLabels, kStereoLayout),
      nullptr);
}

TEST(CreateFromAmbisonicsConfig, DoesNotSupportBinauralOutput) {
  const SubstreamIdLabelsMap kZerothOrderSubstreamIdToLabels = {{0, {kA0}}};

  EXPECT_EQ(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullZerothOrderAmbisonicsConfig, kFullZerothOrderAudioSubstreamIds,
          kZerothOrderSubstreamIdToLabels, kBinauralLayout),
      nullptr);
}

// =========== Mixed-order ambisonics mono config ===========

TEST(CreateFromAmbisonicsConfig, SupportsMixedFirstOrderAmbisonics) {
  const AmbisonicsConfig kMixedFirstOrderAmbisonicsConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config =
          AmbisonicsMonoConfig{.output_channel_count = 4,
                               .substream_count = 3,
                               .channel_mapping = {0, 255, 1, 2}}};
  const std::vector<DecodedUleb128> kMixedFirstOrderAudioSubstreamIds = {0, 1,
                                                                         2};
  const SubstreamIdLabelsMap kMixedFirstOrderSubstreamIdToLabels = {
      {0, {kA0}}, {1, {kA2}}, {2, {kA3}}};

  EXPECT_NE(
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kMixedFirstOrderAmbisonicsConfig, kMixedFirstOrderAudioSubstreamIds,
          kMixedFirstOrderSubstreamIdToLabels, kStereoLayout),
      nullptr);
}

// =========== Full-order ambisonics projection config ===========

const int16_t kMaxGain = std::numeric_limits<int16_t>::max();
const std::vector<int16_t> kEpsilonIdentityFoa =
    {/*           ACN#: 0, 1, 2, 3 */
     /* Channel 0: */ kMaxGain, 0,        0,        0,
     /* Channel 1: */ 0,        kMaxGain, 0,        0,
     /* Channel 2: */ 0,        0,        kMaxGain, 0,
     /* Channel 3: */ 0,        0,        0,        kMaxGain};

const std::vector<int16_t> kNegativeEpsilonIdentityFoa =
    {/*           ACN#: 0, 1, 2, 3 */
     /* Channel 0: */ kMaxGain * -1,
     0,
     0,
     0,
     /* Channel 1: */ 0,
     kMaxGain * -1,
     0,
     0,
     /* Channel 2: */ 0,
     0,
     kMaxGain * -1,
     0,
     /* Channel 3: */ 0,
     0,
     0,
     kMaxGain * -1};

TEST(CreateFromAmbisonicsConfig, Projection) {
  const AmbisonicsConfig kAmbisonicsProjectionConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config =
          AmbisonicsProjectionConfig{.output_channel_count = 4,
                                     .substream_count = 4,
                                     .coupled_substream_count = 0,
                                     .demixing_matrix = kEpsilonIdentityFoa}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {0, 1, 2, 3};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}};

  EXPECT_NE(AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
                kAmbisonicsProjectionConfig, kFirstOrderAudioSubstreamIds,
                kFirstOrderSubstreamIdToLabels, kStereoLayout),
            nullptr);
}

TEST(CreateFromAmbisonicsConfig,
     SupportsAmbisonicsProjectionConfigWithCoupledSubstreams) {
  const AmbisonicsConfig kAmbisonicsProjectionConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config =
          AmbisonicsProjectionConfig{.output_channel_count = 4,
                                     .substream_count = 2,
                                     .coupled_substream_count = 2,
                                     .demixing_matrix = kEpsilonIdentityFoa}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {0, 1};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {{0, {kA0, kA1}},
                                                               {1, {kA2, kA3}}};

  EXPECT_NE(AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
                kAmbisonicsProjectionConfig, kFirstOrderAudioSubstreamIds,
                kFirstOrderSubstreamIdToLabels, kStereoLayout),
            nullptr);
}

// =========== Mixed-order ambisonics projection config ===========

TEST(CreateFromAmbisonicsConfig, SupportedMixedOrderProjectionConfig) {
  const std::vector<int16_t> kNearIdentityMixedFoa = {
      /*           ACN#: 0, 1, 2, 3 */
      /* Channel 0: */ kMaxGain, 0,        0, 0,
      /* Channel 1: */ 0,        kMaxGain, 0, 0,
      /* Channel 2: */ 0,        0,        0, kMaxGain};

  const AmbisonicsConfig kAmbisonicsProjectionConfig = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config =
          AmbisonicsProjectionConfig{.output_channel_count = 4,
                                     .substream_count = 3,
                                     .coupled_substream_count = 0,
                                     .demixing_matrix = kNearIdentityMixedFoa}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {0, 1, 2};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA3}}};

  EXPECT_NE(AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
                kAmbisonicsProjectionConfig, kFirstOrderAudioSubstreamIds,
                kFirstOrderSubstreamIdToLabels, kStereoLayout),
            nullptr);
}

TEST(RenderFrames, AcnZeroIsSymmetric) {
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}};

  auto renderer =
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kFullFirstOrderAmbisonicsConfig, kFullFirstOrderAudioSubstreamIds,
          kFirstOrderSubstreamIdToLabels, kStereoLayout);

  const LabeledFrame frame = {
      .label_to_samples = {{kA0, {10000}}, {kA1, {0}}, {kA2, {0}}, {kA3, {0}}}};
  std::vector<InternalSampleType> output_samples;
  RenderAndFlushExpectOk(frame, renderer.get(), output_samples);

  EXPECT_EQ(output_samples.size(), 2);
  EXPECT_NEAR(output_samples[0], output_samples[1], 0.11);
}

TEST(RenderFrames, UsesDemixingMatrix) {
  const AmbisonicsConfig kAmbisonicsProjectionConfigIdentity = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config =
          AmbisonicsProjectionConfig{.output_channel_count = 4,
                                     .substream_count = 4,
                                     .coupled_substream_count = 0,
                                     .demixing_matrix = kEpsilonIdentityFoa}};
  const AmbisonicsConfig kAmbisonicsProjectionConfigIdentityInverse = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config = AmbisonicsProjectionConfig{
          .output_channel_count = 4,
          .substream_count = 4,
          .coupled_substream_count = 0,
          .demixing_matrix = kNegativeEpsilonIdentityFoa}};
  const std::vector<DecodedUleb128> kFirstOrderAudioSubstreamIds = {0, 1, 2, 3};
  const SubstreamIdLabelsMap kFirstOrderSubstreamIdToLabels = {
      {0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}};
  const LabeledFrame frame = {
      .label_to_samples = {
          {kA0, {10000}}, {kA1, {5000}}, {kA2, {2500}}, {kA3, {1250}}}};
  // Create a renderer which uses a near-identity matrix (I*epsilon) and a
  // different one that uses (-1*I*epsilon).
  auto renderer_epsilon_identity =
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kAmbisonicsProjectionConfigIdentity, kFirstOrderAudioSubstreamIds,
          kFirstOrderSubstreamIdToLabels, kStereoLayout);
  std::vector<InternalSampleType> output_samples_epsilon_identity;
  RenderAndFlushExpectOk(frame, renderer_epsilon_identity.get(),
                         output_samples_epsilon_identity);
  auto renderer_negative_epsilon_identity =
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          kAmbisonicsProjectionConfigIdentityInverse,
          kFirstOrderAudioSubstreamIds, kFirstOrderSubstreamIdToLabels,
          kStereoLayout);
  std::vector<InternalSampleType> output_samples_negative_epsilon_identity;
  RenderAndFlushExpectOk(frame, renderer_negative_epsilon_identity.get(),
                         output_samples_negative_epsilon_identity);

  // The samples should be multiplicative inverses of each other because of the
  // difference in demixing matrices.
  EXPECT_EQ(output_samples_epsilon_identity.size(), 2);
  EXPECT_EQ(output_samples_negative_epsilon_identity.size(), 2);
  EXPECT_DOUBLE_EQ(output_samples_epsilon_identity[0],
                   -1 * output_samples_negative_epsilon_identity[0]);
  EXPECT_DOUBLE_EQ(output_samples_epsilon_identity[1],
                   -1 * output_samples_negative_epsilon_identity[1]);
}

}  // namespace
}  // namespace iamf_tools
