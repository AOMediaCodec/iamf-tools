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

#include "iamf/cli/layout_renderer_factory.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

constexpr uint32_t kCodecConfigId = 42;
constexpr uint32_t kAudioElementId = 67;
constexpr uint32_t kParameterId = 13;
constexpr uint32_t kSampleRate = 48000;
constexpr int32_t kNumChannelsForStereo = 2;
constexpr uint32_t kNumSamplesPerFrame = 8;

class LayoutRendererFactoryTest : public ::testing::Test {
 public:
  LayoutRendererFactoryTest()
      : kDefaultMixGainParamDefinition(MixGainParamDefinition({
            .parameter_id = kParameterId,
            .parameter_rate = kSampleRate,
        })),
        factory_(TrimmingSettings{}),
        output_mix_gain_(kDefaultMixGainParamDefinition),
        layout_(
            {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
             .specific_layout = LoudspeakersSsConventionLayout{
                 .sound_system =
                     LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}}) {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_configs_);
    AddScalableAudioElementWithSubstreamIds(
        IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId, {0},
        codec_configs_, audio_elements_);

    audio_elements_in_sub_mix_.push_back(&audio_elements_.at(kAudioElementId));
    SubMixAudioElement sub_mix_audio_element = {
        .audio_element_id = kAudioElementId,
        .element_mix_gain = kDefaultMixGainParamDefinition,
    };
    sub_mix_audio_elements_.push_back(sub_mix_audio_element);
  }

  const MixGainParamDefinition kDefaultMixGainParamDefinition;
  LayoutRendererFactory factory_;
  DescriptorObus::CodecConfigsById codec_configs_;
  DescriptorObus::AudioElementsById audio_elements_;
  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix_;
  std::vector<SubMixAudioElement> sub_mix_audio_elements_;
  MixGainParamDefinition output_mix_gain_;
  Layout layout_;
};

TEST_F(LayoutRendererFactoryTest, CreateRendererSucceeds) {
  EXPECT_NE(factory_.CreateRenderer(audio_elements_in_sub_mix_,
                                    sub_mix_audio_elements_, output_mix_gain_,
                                    layout_, kNumChannelsForStereo, kSampleRate,
                                    kNumSamplesPerFrame),
            nullptr);
}

TEST_F(LayoutRendererFactoryTest, CreateRendererReturnsNullForInvalidConfig) {
  audio_elements_.at(kAudioElementId).obu.config_ =
      ScalableChannelLayoutConfig{.channel_audio_layer_configs = {}};

  EXPECT_EQ(factory_.CreateRenderer(audio_elements_in_sub_mix_,
                                    sub_mix_audio_elements_, output_mix_gain_,
                                    layout_, kNumChannelsForStereo, kSampleRate,
                                    kNumSamplesPerFrame),
            nullptr);
}

}  // namespace
}  // namespace iamf_tools