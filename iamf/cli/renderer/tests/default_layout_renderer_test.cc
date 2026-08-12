/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/renderer/default_layout_renderer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelLabel::Label;
using ::testing::_;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;

constexpr uint32_t kCodecConfigId = 42;
constexpr uint32_t kAudioElementId = 67;
constexpr uint32_t kParameterId = 13;
constexpr uint32_t kSampleRate = 48000;
constexpr int32_t kNumChannelsForStereo = 2;
constexpr uint32_t kNumSamplesPerFrame = 8;
MixGainParamDefinition GetDefaultMixGainParamDefinition() {
  return MixGainParamDefinition({
      .parameter_id = kParameterId,
      .parameter_rate = kSampleRate,
  });
}

class MockRenderer : public AudioElementRendererBase {
 public:
  MockRenderer(absl::Span<const ChannelLabel::Label> ordered_labels,
               size_t num_output_channels)
      : AudioElementRendererBase(ordered_labels, kNumSamplesPerFrame,
                                 num_output_channels, TrimmingSettings{}),
        kAllZeroRenderedSamples(
            num_output_channels,
            std::vector<InternalSampleType>(kNumSamplesPerFrame, 0.0)) {
    ON_CALL(*this, RenderSamples(_))
        .WillByDefault(::testing::InvokeWithoutArgs(
            this, &MockRenderer::ResetRenderedSamples));
  }

  MOCK_METHOD(absl::Status, RenderSamples,
              (absl::Span<const absl::Span<const InternalSampleType>>
                   samples_to_render),
              (override));

 private:
  absl::Status ResetRenderedSamples() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    rendered_samples_ = kAllZeroRenderedSamples;
    return absl::OkStatus();
  }

  const std::vector<std::vector<InternalSampleType>> kAllZeroRenderedSamples;
};

class MockRendererFactory : public RendererFactoryBase {
 public:
  MOCK_METHOD(std::unique_ptr<AudioElementRendererBase>,
              CreateRendererForLayout,
              (const std::vector<DecodedUleb128>&, const SubstreamIdLabelsMap&,
               AudioElementObu::AudioElementType,
               const AudioElementObu::AudioElementConfig&,
               const RenderingConfig&, const Layout&, size_t, size_t),
              (const, override));
};

class DefaultLayoutRendererTest : public ::testing::Test {
 public:
  DefaultLayoutRendererTest()
      : output_mix_gain_(GetDefaultMixGainParamDefinition()),
        layout_(
            {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
             .specific_layout =
                 LoudspeakersSsConventionLayout{
                     .sound_system =
                         LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}}),
        rendered_samples_(
            kNumChannelsForStereo,
            std::vector<InternalSampleType>(kNumSamplesPerFrame)) {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_configs_);
    AddScalableAudioElementWithSubstreamIds(
        IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {0},
        codec_configs_, audio_elements_);

    audio_elements_in_sub_mix_.push_back(&audio_elements_.at(kAudioElementId));
    SubMixAudioElement sub_mix_audio_element = {
        .audio_element_id = kAudioElementId,
        .element_mix_gain = GetDefaultMixGainParamDefinition(),
    };
    sub_mix_audio_elements_.push_back(sub_mix_audio_element);
  }

  DescriptorObus::CodecConfigsById codec_configs_;
  DescriptorObus::AudioElementsById audio_elements_;
  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix_;
  std::vector<SubMixAudioElement> sub_mix_audio_elements_;
  MixGainParamDefinition output_mix_gain_;
  Layout layout_;
  MockRendererFactory mock_factory_;

  // Auxiliary data and buffers for rendering.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      id_to_parameter_block_;
  std::vector<std::vector<InternalSampleType>> rendered_samples_;
  std::vector<absl::Span<const InternalSampleType>> valid_rendered_samples_;
};

TEST_F(DefaultLayoutRendererTest, CreateSuccess) {
  EXPECT_CALL(mock_factory_, CreateRendererForLayout(_, _, _, _, _, _, _, _))
      .WillOnce([]() {
        std::vector<ChannelLabel::Label> labels = {kL2, kR2};
        return std::make_unique<MockRenderer>(labels, kNumChannelsForStereo);
      });

  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      mock_factory_);

  EXPECT_THAT(renderer, NotNull());
}

TEST_F(DefaultLayoutRendererTest, CreateFailsWhenFactoryReturnsNull) {
  EXPECT_CALL(mock_factory_, CreateRendererForLayout(_, _, _, _, _, _, _, _))
      .WillOnce(Return(nullptr));

  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      mock_factory_);

  EXPECT_THAT(renderer, IsNull());
}

using DefaultLayoutRendererDeathTest = DefaultLayoutRendererTest;

TEST_F(DefaultLayoutRendererDeathTest,
       CreateDiesWhenSubMixAudioElementsSizeMismatch) {
  // Make the sizes of `audio_elements_in_sub_mix_` and
  // `sub_mix_audio_elements_` unequal by adding an extra element to
  // `sub_mix_audio_elements_`.
  sub_mix_audio_elements_.push_back({.audio_element_id = kAudioElementId + 1});

  // Expect a death due to the size mismatch.
  EXPECT_DEATH(
      {
        DefaultLayoutRenderer::Create(
            audio_elements_in_sub_mix_, sub_mix_audio_elements_,
            output_mix_gain_, layout_, kNumChannelsForStereo, kSampleRate,
            kNumSamplesPerFrame, mock_factory_);
      },
      "ValidateContainerSizeEqual.*sub_mix_audio_elements");
}

// =========== Tests that work is delegated to the renderer factory. ===========
TEST_F(DefaultLayoutRendererTest, ForwardsAudioElementToRendererFactory) {
  // We expect audio-element related arguments to be forwarded from the OBUs to
  // the renderer factory.
  const auto& forwarded_audio_element = audio_elements_.at(kAudioElementId);
  EXPECT_CALL(
      mock_factory_,
      CreateRendererForLayout(forwarded_audio_element.obu.audio_substream_ids_,
                              forwarded_audio_element.substream_id_to_labels,
                              forwarded_audio_element.obu.GetAudioElementType(),
                              forwarded_audio_element.obu.config_, _, _,
                              kNumSamplesPerFrame, kSampleRate));
  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      mock_factory_);

  EXPECT_THAT(renderer, IsNull());
}

TEST_F(DefaultLayoutRendererTest, ForwardsRenderingConfigToRendererFactory) {
  // We expect arguments to be forwarded from the OBUs to the renderer factory.
  auto& forwarded_rendering_config =
      sub_mix_audio_elements_[0].rendering_config;
  forwarded_rendering_config.rendering_config_extension_bytes = {
      't', 'e', 's', 't', 'b', 'y', 't', 'e'};
  EXPECT_CALL(
      mock_factory_,
      CreateRendererForLayout(_, _, _, _, forwarded_rendering_config, _, _, _));

  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      mock_factory_);

  EXPECT_THAT(renderer, IsNull());
}

TEST_F(DefaultLayoutRendererTest, RenderSuccess) {
  EXPECT_CALL(mock_factory_, CreateRendererForLayout(_, _, _, _, _, _, _, _))
      .WillOnce([]() {
        std::vector<ChannelLabel::Label> labels = {kL2, kR2};
        return std::make_unique<MockRenderer>(labels, kNumChannelsForStereo);
      });

  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      mock_factory_);

  ASSERT_THAT(renderer, NotNull());

  IdLabeledFrameMap id_to_labeled_frame;
  id_to_labeled_frame[kAudioElementId] = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = 0,
      .label_to_samples = {
          {kL2, std::vector<InternalSampleType>(kNumSamplesPerFrame, 0.0)},
          {kR2, std::vector<InternalSampleType>(kNumSamplesPerFrame, 0.0)}}};
  EXPECT_THAT(renderer->Render(id_to_labeled_frame, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(DefaultLayoutRendererTest, RenderFailsWhenFrameHasTooManySamples) {
  EXPECT_CALL(mock_factory_, CreateRendererForLayout(_, _, _, _, _, _, _, _))
      .WillOnce([]() {
        std::vector<ChannelLabel::Label> labels = {kL2, kR2};
        return std::make_unique<MockRenderer>(labels, kNumChannelsForStereo);
      });

  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      mock_factory_);

  ASSERT_THAT(renderer, NotNull());

  IdLabeledFrameMap id_to_labeled_frame;
  id_to_labeled_frame[kAudioElementId] = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = 0,
      .label_to_samples = {{kL2, std::vector<InternalSampleType>(9, 0.0)},
                           {kR2, std::vector<InternalSampleType>(9, 0.0)}}};
  EXPECT_THAT(renderer->Render(id_to_labeled_frame, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

TEST_F(DefaultLayoutRendererTest, RenderSucceedsWithFullyTrimmedFrame) {
  // Use a real renderer factory here instead of a mock one to get the trimming
  // behavior right.
  RendererFactory renderer_factory;
  auto renderer = DefaultLayoutRenderer::Create(
      audio_elements_in_sub_mix_, sub_mix_audio_elements_, output_mix_gain_,
      layout_, kNumChannelsForStereo, kSampleRate, kNumSamplesPerFrame,
      renderer_factory);

  ASSERT_THAT(renderer, NotNull());

  IdLabeledFrameMap id_to_labeled_frame;
  id_to_labeled_frame[kAudioElementId] = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = kNumSamplesPerFrame,
      .label_to_samples = {
          {kL2, std::vector<InternalSampleType>(kNumSamplesPerFrame, 0.0)},
          {kR2, std::vector<InternalSampleType>(kNumSamplesPerFrame, 0.0)}}};
  EXPECT_THAT(renderer->Render(id_to_labeled_frame, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
  ASSERT_EQ(valid_rendered_samples_.size(), kNumChannelsForStereo);
  EXPECT_TRUE(valid_rendered_samples_[0].empty());
  EXPECT_TRUE(valid_rendered_samples_[1].empty());
}

}  // namespace
}  // namespace iamf_tools
