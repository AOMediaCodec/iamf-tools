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

#include "iamf/cli/renderer/oar_layout_renderer.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using enum ChannelLabel::Label;

constexpr uint32_t kAudioElementId = 100;
constexpr uint32_t kCodecConfigId = 10;
constexpr DecodedUleb128 kAudioSubstreamId = 91;
constexpr DecodedUleb128 kAudioSubstreamIdsStereo[] = {92};
constexpr DecodedUleb128 kAudioSubstreamIds5_1[] = {93, 94, 95, 96};
constexpr DecodedUleb128 kAudioSubstreamIds5_1_2[] = {97, 98, 99, 100, 101};
constexpr DecodedUleb128 kAudioSubstreamIdsFoa[] = {102, 103, 104, 105};
constexpr DecodedUleb128 kElementMixGainParameterId = 200;
constexpr DecodedUleb128 kOutputMixGainParameterId = 201;
constexpr DecodedUleb128 kDemixingInfoParameterId = 300;
constexpr uint32_t kCommonSampleRate = 48000;
constexpr uint32_t kCommonNumSamplesPerFrame = 64;
constexpr uint8_t kBitDepth16 = 16;
constexpr size_t kTwoChannelsForStereo = 2;

MixGainParamDefinition CreateMixGainParamDefinition(
    DecodedUleb128 parameter_id) {
  return MixGainParamDefinition(MakeOneSubblockParamDefinitionBaseArgs(
      parameter_id, kCommonSampleRate, kCommonNumSamplesPerFrame));
}

Layout CreateStereoLayout() {
  Layout layout;
  layout.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention;
  layout.specific_layout = LoudspeakersSsConventionLayout{
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
      .reserved = 0};
  return layout;
}

class OarLayoutRendererTest : public ::testing::Test {
 protected:
  OarLayoutRendererTest()
      : kLayoutConfig5_1{
            .channel_audio_layer_configs = {ChannelAudioLayerConfig{
                .loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_ch,
                .output_gain_is_present_flag = false,
                .recon_gain_is_present_flag = false,
                .substream_count = 4,
                .coupled_substream_count = 2,
            }}},
        rendered_samples_(kTwoChannelsForStereo) {
    auto codec_config_obu = CodecConfigObu::Create(
        ObuHeader(), kCodecConfigId,
        {.num_samples_per_frame = kCommonNumSamplesPerFrame,
         .decoder_config =
             LpcmDecoderConfig{.sample_format_flags_bitmask_ =
                                   LpcmDecoderConfig::kLpcmLittleEndian,
                               .sample_size_ = kBitDepth16,
                               .sample_rate_ = kCommonSampleRate}});

    codec_config_obus_.emplace(kCodecConfigId, *std::move(codec_config_obu));

    // Add labeled frame for the first audio element.
    id_to_labeled_frame_[kAudioElementId] =
        CreateLabeledFrame(kCommonNumSamplesPerFrame);
  }

  void CreateScalableAudioElementForLayout(
      DecodedUleb128 audio_element_id,
      absl::Span<const DecodedUleb128> audio_substream_ids,
      const ScalableChannelLayoutConfig& layout_config) {
    auto obu = AudioElementObu::CreateForScalableChannelLayout(
        ObuHeader(), audio_element_id, /*reserved=*/0, kCodecConfigId,
        audio_substream_ids, layout_config);
    ASSERT_THAT(obu, IsOk());
    auto audio_element_with_data =
        ObuWithDataGenerator::GenerateAudioElementWithData(codec_config_obus_,
                                                           *obu);
    ASSERT_THAT(audio_element_with_data, IsOk());
    audio_elements_.emplace(audio_element_id,
                            std::move(*audio_element_with_data));
  }

  void CreateScalableAudioElement(DecodedUleb128 audio_element_id) {
    // By default construct a 5.1 audio element.
    CreateScalableAudioElementForLayout(audio_element_id, kAudioSubstreamIds5_1,
                                        kLayoutConfig5_1);
  }

  void CreateSceneBasedAudioElement(DecodedUleb128 audio_element_id) {
    const auto ambisonics_config = AmbisonicsMonoConfig::Create(
        /*channel_count=*/4,
        /*channel_mapping=*/std::vector<uint8_t>{0, 1, 2, 3});
    ASSERT_THAT(ambisonics_config, IsOk());
    auto config = AmbisonicsConfig{
        .ambisonics_config = *ambisonics_config,
    };

    auto obu = AudioElementObu::CreateForAmbisonics(
        ObuHeader(), audio_element_id, /*reserved=*/0, kCodecConfigId,
        kAudioSubstreamIdsFoa, config);
    ASSERT_THAT(obu, IsOk());
    auto audio_element_with_data =
        ObuWithDataGenerator::GenerateAudioElementWithData(codec_config_obus_,
                                                           *obu);
    ASSERT_THAT(audio_element_with_data, IsOk());
    audio_elements_.emplace(audio_element_id,
                            std::move(*audio_element_with_data));
  }

  void CreateObjectBasedAudioElement(DecodedUleb128 audio_element_id,
                                     uint8_t num_objects) {
    auto config = ObjectsConfig::Create(num_objects, {});
    ASSERT_THAT(config, IsOk());
    auto obu = AudioElementObu::CreateForObjects(ObuHeader(), audio_element_id,
                                                 /*reserved=*/0, kCodecConfigId,
                                                 kAudioSubstreamId, *config);
    ASSERT_THAT(obu, IsOk());
    auto audio_element_with_data =
        ObuWithDataGenerator::GenerateAudioElementWithData(codec_config_obus_,
                                                           *obu);
    ASSERT_THAT(audio_element_with_data, IsOk());
    audio_elements_.emplace(audio_element_id,
                            std::move(*audio_element_with_data));
  }

  void AddDemixingInfoForAudioElement(DecodedUleb128 parameter_id,
                                      DecodedUleb128 audio_element_id) {
    const DemixingParamDefinition param_definition(
        MakeOneSubblockParamDefinitionBaseArgs(parameter_id, kCommonSampleRate,
                                               kCommonNumSamplesPerFrame));
    audio_elements_.at(audio_element_id)
        .obu.audio_element_params_.emplace_back(
            AudioElementParam{param_definition});
    const auto* added_param_definition = std::get_if<DemixingParamDefinition>(
        &audio_elements_.at(audio_element_id)
             .obu.audio_element_params_.back()
             .param_definition);
    ASSERT_NE(added_param_definition, nullptr);

    auto parameter_block =
        ParameterBlockObu::CreateMode0(ObuHeader(), *added_param_definition);
    ASSERT_NE(parameter_block, nullptr);

    auto data = std::make_unique<DemixingInfoParameterData>();
    data->dmixp_mode = DemixingInfoParameterData::kDMixPMode1;

    parameter_block->subblocks_.clear();
    parameter_block->subblocks_.push_back(std::move(data));
    ASSERT_EQ(parameter_block->GetNumSubblocks(), 1);

    parameter_blocks_.emplace_back(
        ParameterBlockWithData{.obu = std::move(parameter_block),
                               .start_timestamp = 0,
                               .end_timestamp = kCommonNumSamplesPerFrame});
    id_to_parameter_block_[kDemixingInfoParameterId] =
        &parameter_blocks_.back();
  }

  void SetDefaultPolarPositionForAudioElement(DecodedUleb128 audio_element_id,
                                              int16_t azimuth, int8_t elevation,
                                              uint8_t distance) {
    PolarParamDefinition polar_param_definition({});
    polar_param_definition.default_azimuth_ = azimuth;
    polar_param_definition.default_elevation_ = elevation;
    polar_param_definition.default_distance_ = distance;
    RenderingConfig& rendering_config =
        id_to_rendering_config_[audio_element_id];
    rendering_config.rendering_config_param_definitions.push_back(
        RenderingConfigParamDefinition::Create(polar_param_definition, {}));
  }

  void SetDefaultDualPolarPositionForAudioElement(
      DecodedUleb128 audio_element_id, int16_t first_azimuth,
      int8_t first_elevation, uint8_t first_distance, int16_t second_azimuth,
      int8_t second_elevation, uint8_t second_distance) {
    DualPolarParamDefinition dual_polar_param_definition({});
    dual_polar_param_definition.default_first_azimuth_ = first_azimuth;
    dual_polar_param_definition.default_first_elevation_ = first_elevation;
    dual_polar_param_definition.default_first_distance_ = first_distance;
    dual_polar_param_definition.default_second_azimuth_ = second_azimuth;
    dual_polar_param_definition.default_second_elevation_ = second_elevation;
    dual_polar_param_definition.default_second_distance_ = second_distance;
    RenderingConfig& rendering_config =
        id_to_rendering_config_[audio_element_id];
    rendering_config.rendering_config_param_definitions.push_back(
        RenderingConfigParamDefinition::Create(dual_polar_param_definition,
                                               {}));
  }

  void SetDefaultCart8PositionForAudioElement(DecodedUleb128 audio_element_id,
                                              int8_t x, int8_t y, int8_t z) {
    Cart8ParamDefinition cart8_param_definition({});
    cart8_param_definition.default_x_ = x;
    cart8_param_definition.default_y_ = y;
    cart8_param_definition.default_z_ = z;
    RenderingConfig& rendering_config =
        id_to_rendering_config_[audio_element_id];
    rendering_config.rendering_config_param_definitions.push_back(
        RenderingConfigParamDefinition::Create(cart8_param_definition, {}));
  }

  void SetDefaultDualCart8PositionForAudioElement(
      DecodedUleb128 audio_element_id, int8_t first_x, int8_t first_y,
      int8_t first_z, int8_t second_x, int8_t second_y, int8_t second_z) {
    DualCart8ParamDefinition dual_cart8_param_definition({});
    dual_cart8_param_definition.default_first_x_ = first_x;
    dual_cart8_param_definition.default_first_y_ = first_y;
    dual_cart8_param_definition.default_first_z_ = first_z;
    dual_cart8_param_definition.default_second_x_ = second_x;
    dual_cart8_param_definition.default_second_y_ = second_y;
    dual_cart8_param_definition.default_second_z_ = second_z;
    RenderingConfig& rendering_config =
        id_to_rendering_config_[audio_element_id];
    rendering_config.rendering_config_param_definitions.push_back(
        RenderingConfigParamDefinition::Create(dual_cart8_param_definition,
                                               {}));
  }

  std::unique_ptr<OarLayoutRenderer> CreateRenderer(
      const std::vector<DecodedUleb128>& audio_element_ids) {
    std::vector<const AudioElementWithData*> audio_elements_in_sub_mix;
    std::vector<SubMixAudioElement> sub_mix_audio_elements;
    for (const auto id : audio_element_ids) {
      audio_elements_in_sub_mix.push_back(&audio_elements_.at(id));
      sub_mix_audio_elements.push_back(
          {.audio_element_id = id,
           .rendering_config = id_to_rendering_config_[id],
           .element_mix_gain =
               CreateMixGainParamDefinition(kElementMixGainParameterId)});
    }
    MixGainParamDefinition output_mix_gain =
        CreateMixGainParamDefinition(kOutputMixGainParameterId);
    Layout layout = CreateStereoLayout();
    int32_t num_channels = 2;
    TrimmingSettings trimming_settings = {.trim_beginning = true,
                                          .trim_end = true};

    return OarLayoutRenderer::Create(
        audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
        layout, num_channels, kCommonSampleRate, kCommonNumSamplesPerFrame,
        trimming_settings);
  }

  LabeledFrame CreateLabeledFrame(size_t num_samples) {
    LabeledFrame frame;
    frame.samples_to_trim_at_start = 0;
    frame.samples_to_trim_at_end = 0;
    // For stereo.
    frame.label_to_samples[kL2] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kR2] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    // For 5.1.x.
    frame.label_to_samples[kL5] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kR5] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kLs5] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kRs5] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kCentre] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kLFE] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kLtf2] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kRtf2] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    // For ambisonics.
    frame.label_to_samples[kA0] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kA1] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kA2] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kA3] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    // For objects.
    frame.label_to_samples[kObjectChannel0] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    frame.label_to_samples[kObjectChannel1] =
        std::vector<InternalSampleType>(num_samples, 0.0);
    return frame;
  }

  const ScalableChannelLayoutConfig kLayoutConfig5_1;
  DescriptorObus::CodecConfigsById codec_config_obus_;
  DescriptorObus::AudioElementsById audio_elements_;
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      id_to_parameter_block_;
  absl::flat_hash_map<DecodedUleb128, RenderingConfig> id_to_rendering_config_;

  IdLabeledFrameMap id_to_labeled_frame_;
  std::vector<ParameterBlockWithData> parameter_blocks_;
  std::vector<std::vector<InternalSampleType>> rendered_samples_;
  std::vector<absl::Span<const InternalSampleType>> valid_rendered_samples_;
};

TEST_F(OarLayoutRendererTest, CreateSucceedsWithValidInput) {
  CreateScalableAudioElement(kAudioElementId);
  const AudioElementWithData* audio_element_with_data =
      &audio_elements_.at(kAudioElementId);

  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix = {
      audio_element_with_data};

  SubMixAudioElement sub_mix_audio_element = {
      .audio_element_id = kAudioElementId,
      .element_mix_gain =
          CreateMixGainParamDefinition(kElementMixGainParameterId)};
  std::vector<SubMixAudioElement> sub_mix_audio_elements = {
      sub_mix_audio_element};

  MixGainParamDefinition output_mix_gain =
      CreateMixGainParamDefinition(kOutputMixGainParameterId);
  Layout layout = CreateStereoLayout();
  int32_t num_channels = 2;
  TrimmingSettings trimming_settings = {.trim_beginning = false,
                                        .trim_end = false};

  auto renderer = OarLayoutRenderer::Create(
      audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
      layout, num_channels, kCommonSampleRate, kCommonNumSamplesPerFrame,
      trimming_settings);

  EXPECT_NE(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest, CreateFailsWithNoAudioElement) {
  auto renderer = CreateRenderer(/*audio_element_ids=*/{});
  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest, CreateFailsWithMultipleDemixingParameters) {
  CreateScalableAudioElement(kAudioElementId);
  AudioElementWithData& audio_element_with_data =
      audio_elements_.at(kAudioElementId);

  // Add multiple demixing parameters to trigger failure.
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId + 1, kAudioElementId);

  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix = {
      &audio_element_with_data};

  SubMixAudioElement sub_mix_audio_element = {
      .audio_element_id = kAudioElementId,
      .element_mix_gain =
          CreateMixGainParamDefinition(kElementMixGainParameterId)};
  std::vector<SubMixAudioElement> sub_mix_audio_elements = {
      sub_mix_audio_element};

  MixGainParamDefinition output_mix_gain =
      CreateMixGainParamDefinition(kOutputMixGainParameterId);
  Layout layout = CreateStereoLayout();
  int32_t num_channels = 2;
  TrimmingSettings trimming_settings = {.trim_beginning = false,
                                        .trim_end = false};

  auto renderer = OarLayoutRenderer::Create(
      audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
      layout, num_channels, kCommonSampleRate, kCommonNumSamplesPerFrame,
      trimming_settings);

  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest, CreateFailsWithInconsistentAudioElementType) {
  CreateScalableAudioElement(kAudioElementId);
  AudioElementWithData& audio_element_with_data =
      audio_elements_.at(kAudioElementId);

  // Sabotage the audio element by changing its `config_` field to an ambisonic
  // config, which is inconsistent with its scalable type.
  audio_element_with_data.obu.config_ =
      AmbisonicsConfig{.ambisonics_config = *AmbisonicsMonoConfig::Create(
                           /*channel_count=*/1,
                           /*channel_mapping=*/std::vector<uint8_t>{0})};
  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix = {
      &audio_element_with_data};

  SubMixAudioElement sub_mix_audio_element = {
      .audio_element_id = kAudioElementId,
      .element_mix_gain =
          CreateMixGainParamDefinition(kElementMixGainParameterId)};
  std::vector<SubMixAudioElement> sub_mix_audio_elements = {
      sub_mix_audio_element};

  MixGainParamDefinition output_mix_gain =
      CreateMixGainParamDefinition(kOutputMixGainParameterId);
  Layout layout = CreateStereoLayout();
  int32_t num_channels = 2;
  TrimmingSettings trimming_settings = {.trim_beginning = false,
                                        .trim_end = false};

  auto renderer = OarLayoutRenderer::Create(
      audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
      layout, num_channels, kCommonSampleRate, kCommonNumSamplesPerFrame,
      trimming_settings);

  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest,
       CreateFailsWithObjectBasedAudioElementWithoutDefaultPositions) {
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/1);

  // Here we omit calling `SetDefaultPolarPositionForAudioElement()`, so there
  // is no default positions for the object-based audio element.
  auto renderer = CreateRenderer({kAudioElementId});
  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest,
       CreateReturnsNullWhenNonObjectElementHasPositionParam) {
  CreateScalableAudioElement(kAudioElementId);
  SetDefaultPolarPositionForAudioElement(kAudioElementId, /*azimuth=*/0,
                                         /*elevation=*/0, /*distance=*/127);

  auto renderer = CreateRenderer({kAudioElementId});
  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest,
       CreateFailsWithMismatchingNumberOfObjectsAndDefaultPositions) {
  // One-object based audio element requires a single default position.
  // Providing a dual polar param definition fails.
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/1);
  SetDefaultDualPolarPositionForAudioElement(kAudioElementId,
                                             /*first_azimuth=*/200,
                                             /*first_elevation=*/-10,
                                             /*first_distance=*/50,
                                             /*second_azimuth=*/-400,
                                             /*second_elevation=*/20,
                                             /*second_distance=*/100);
  {
    auto renderer = CreateRenderer({kAudioElementId});
    EXPECT_EQ(renderer, nullptr);
  }
  audio_elements_.clear();
  id_to_rendering_config_.clear();

  // Two-object based audio element requires two default positions.
  // Providing a (non-dual) polar param definition fails.
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/2);
  SetDefaultPolarPositionForAudioElement(kAudioElementId,
                                         /*azimuth=*/200,
                                         /*elevation=*/-10,
                                         /*distance=*/50);
  {
    auto renderer = CreateRenderer({kAudioElementId});
    EXPECT_EQ(renderer, nullptr);
  }
}

TEST_F(OarLayoutRendererTest,
       CreateFailsWithDuplicateObjectBasedAudioElementId) {
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/1);
  SetDefaultPolarPositionForAudioElement(kAudioElementId, /*azimuth=*/0,
                                         /*elevation=*/0, /*distance=*/127);

  auto renderer = CreateRenderer({kAudioElementId, kAudioElementId});
  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest,
       CreateFailsWithMismatchedSubMixAudioElementsSize) {
  CreateScalableAudioElement(kAudioElementId);
  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix = {
      &audio_elements_.at(kAudioElementId)};
  std::vector<SubMixAudioElement> sub_mix_audio_elements;
  MixGainParamDefinition output_mix_gain =
      CreateMixGainParamDefinition(kOutputMixGainParameterId);
  Layout layout = CreateStereoLayout();
  constexpr int32_t kNumChannels = 2;
  TrimmingSettings trimming_settings = {.trim_beginning = false,
                                        .trim_end = false};

  auto renderer = OarLayoutRenderer::Create(
      audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
      layout, kNumChannels, kCommonSampleRate, kCommonNumSamplesPerFrame,
      trimming_settings);
  EXPECT_EQ(renderer, nullptr);
}

TEST_F(OarLayoutRendererTest, RenderSucceeds5_1ToStereo) {
  CreateScalableAudioElement(kAudioElementId);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceedsStereoToStereo) {
  ScalableChannelLayoutConfig kStereoLayout = {
      .channel_audio_layer_configs = {ChannelAudioLayerConfig{
          .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
          .output_gain_is_present_flag = false,
          .recon_gain_is_present_flag = false,
          .substream_count = 1,
          .coupled_substream_count = 1,
      }}};

  CreateScalableAudioElementForLayout(kAudioElementId, kAudioSubstreamIdsStereo,
                                      kStereoLayout);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceeds5_1_2ToStereo) {
  ScalableChannelLayoutConfig kStereoLayout = {
      .channel_audio_layer_configs = {ChannelAudioLayerConfig{
          .loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_2_ch,
          .output_gain_is_present_flag = false,
          .recon_gain_is_present_flag = false,
          .substream_count = 5,
          .coupled_substream_count = 3,
      }}};

  CreateScalableAudioElementForLayout(kAudioElementId, kAudioSubstreamIds5_1_2,
                                      kStereoLayout);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWithNoDemixingInfo) {
  CreateScalableAudioElement(kAudioElementId);

  // Omitted `AddDemixingInfoForAudioElement(...)` here, so the audio
  // element does not carry demixing info, which is OK.

  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest,
       RenderSucceedsWithDemixingInfoButNoParameterBlock) {
  CreateScalableAudioElement(kAudioElementId);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  // Empty parameter block map, which is OK because the default demixing info
  // will be used.
  id_to_parameter_block_.clear();

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWithBinauralLayout) {
  CreateScalableAudioElement(kAudioElementId);
  const AudioElementWithData* audio_element_with_data =
      &audio_elements_.at(kAudioElementId);

  std::vector<const AudioElementWithData*> audio_elements_in_sub_mix = {
      audio_element_with_data};

  SubMixAudioElement sub_mix_audio_element = {
      .audio_element_id = kAudioElementId,
      .element_mix_gain =
          CreateMixGainParamDefinition(kElementMixGainParameterId)};
  std::vector<SubMixAudioElement> sub_mix_audio_elements = {
      sub_mix_audio_element};

  MixGainParamDefinition output_mix_gain =
      CreateMixGainParamDefinition(kOutputMixGainParameterId);
  Layout layout;
  layout.layout_type = Layout::kLayoutTypeBinaural;
  layout.specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0};
  int32_t num_channels = 2;
  TrimmingSettings trimming_settings = {.trim_beginning = true,
                                        .trim_end = true};

  auto renderer = OarLayoutRenderer::Create(
      audio_elements_in_sub_mix, sub_mix_audio_elements, output_mix_gain,
      layout, num_channels, kCommonSampleRate, kCommonNumSamplesPerFrame,
      trimming_settings);

  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWithFewerSamplesThanFrameSize) {
  constexpr size_t kFewerSamples = kCommonNumSamplesPerFrame / 2;
  id_to_labeled_frame_[kAudioElementId] = CreateLabeledFrame(kFewerSamples);

  CreateScalableAudioElement(kAudioElementId);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
  EXPECT_EQ(valid_rendered_samples_.size(), kTwoChannelsForStereo);
  for (const auto& channel_span : valid_rendered_samples_) {
    EXPECT_EQ(channel_span.size(), kFewerSamples);
  }
}

TEST_F(OarLayoutRendererTest, RenderFailsWithMoreSamplesThanFrameSize) {
  constexpr size_t kMoreSamples = kCommonNumSamplesPerFrame * 2;
  id_to_labeled_frame_[kAudioElementId] = CreateLabeledFrame(kMoreSamples);

  CreateScalableAudioElement(kAudioElementId);
  AddDemixingInfoForAudioElement(kDemixingInfoParameterId, kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

TEST_F(OarLayoutRendererTest, RenderFailsWhenLabeledFrameIsMissing) {
  CreateScalableAudioElement(kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  // Empty labeled frame map, which fails because there is no input audio
  // samples.
  id_to_labeled_frame_.clear();

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

TEST_F(OarLayoutRendererTest, RenderFailsWhenTrimmingIsDifferent) {
  constexpr uint32_t kAudioElementId2 = 101;
  CreateScalableAudioElement(kAudioElementId);
  CreateScalableAudioElement(kAudioElementId2);
  auto renderer = CreateRenderer({kAudioElementId, kAudioElementId2});
  ASSERT_NE(renderer, nullptr);

  constexpr uint32_t kTrimmingFiveSamples = 5;

  // Adds a labeled frame for a second audio element.
  id_to_labeled_frame_[kAudioElementId2] =
      CreateLabeledFrame(kCommonNumSamplesPerFrame);

  // Sabotage the trimming information so that different audio elements are
  // trimmed differently, which is not allowed.
  id_to_labeled_frame_[kAudioElementId].samples_to_trim_at_start =
      kTrimmingFiveSamples;
  id_to_labeled_frame_[kAudioElementId2].samples_to_trim_at_start =
      kTrimmingFiveSamples + 1;

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

TEST_F(OarLayoutRendererTest, RenderFailsWhenInputAudioSamplesOutOfRange) {
  CreateScalableAudioElement(kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  // This value is out of the range of audio samples that OAR accepts.
  id_to_labeled_frame_[kAudioElementId].label_to_samples[kL5][0] =
      std::numeric_limits<double>::max();

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWithSceneBasedAudioElement) {
  CreateSceneBasedAudioElement(kAudioElementId);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWithOneObjectBasedAudioElement) {
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/1);
  SetDefaultPolarPositionForAudioElement(kAudioElementId,
                                         /*azimuth=*/200,
                                         /*elevation=*/-10,
                                         /*distance=*/50);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWithTwoObjectBasedAudioElement) {
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/2);
  SetDefaultDualPolarPositionForAudioElement(kAudioElementId,
                                             /*first_azimuth=*/200,
                                             /*first_elevation=*/-10,
                                             /*first_distance=*/50,
                                             /*second_azimuth=*/-400,
                                             /*second_elevation=*/20,
                                             /*second_distance=*/100);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderRendersSingleObjectWithDefaultPositions) {
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/1);
  id_to_labeled_frame_[kAudioElementId].label_to_samples[kObjectChannel0] =
      std::vector<InternalSampleType>(kCommonNumSamplesPerFrame, 1.0);
  SetDefaultPolarPositionForAudioElement(kAudioElementId, /*azimuth=*/90,
                                         /*elevation=*/0, /*distance=*/127);
  auto polar_renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(polar_renderer, nullptr);

  id_to_rendering_config_.clear();
  SetDefaultCart8PositionForAudioElement(kAudioElementId, /*x=*/127, /*y=*/0,
                                         /*z=*/0);
  auto cart8_renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(cart8_renderer, nullptr);

  std::vector<std::vector<InternalSampleType>> polar_samples(
      kTwoChannelsForStereo);
  std::vector<absl::Span<const InternalSampleType>> polar_valid_samples;
  std::vector<std::vector<InternalSampleType>> cart8_samples(
      kTwoChannelsForStereo);
  std::vector<absl::Span<const InternalSampleType>> cart8_valid_samples;

  ASSERT_THAT(
      polar_renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                             polar_samples, polar_valid_samples),
      IsOk());
  ASSERT_THAT(
      cart8_renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                             cart8_samples, cart8_valid_samples),
      IsOk());

  constexpr size_t kLastSample = kCommonNumSamplesPerFrame - 1;
  EXPECT_GT(polar_valid_samples[0][kLastSample],
            polar_valid_samples[1][kLastSample]);
  EXPECT_GT(cart8_valid_samples[1][kLastSample],
            cart8_valid_samples[0][kLastSample]);
}

TEST_F(OarLayoutRendererTest,
       RenderRendersDualObjectsWithDefaultCartesianPositions) {
  CreateObjectBasedAudioElement(kAudioElementId, /*num_objects=*/2);
  id_to_labeled_frame_[kAudioElementId].label_to_samples[kObjectChannel0] =
      std::vector<InternalSampleType>(kCommonNumSamplesPerFrame, 1.0);
  id_to_labeled_frame_[kAudioElementId].label_to_samples[kObjectChannel1] =
      std::vector<InternalSampleType>(kCommonNumSamplesPerFrame, 0.0);
  SetDefaultDualCart8PositionForAudioElement(kAudioElementId, /*first_x=*/-128,
                                             /*first_y=*/0, /*first_z=*/0,
                                             /*second_x=*/127, /*second_y=*/0,
                                             /*second_z=*/0);
  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  ASSERT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());

  constexpr size_t kLastSample = kCommonNumSamplesPerFrame - 1;
  EXPECT_GT(valid_rendered_samples_[0][kLastSample],
            valid_rendered_samples_[1][kLastSample]);
}

TEST_F(OarLayoutRendererTest, RenderSucceedsWhenTrimmingIsConsistent) {
  constexpr uint32_t kAudioElementId2 = 101;
  CreateScalableAudioElement(kAudioElementId);
  CreateScalableAudioElement(kAudioElementId2);
  auto renderer = CreateRenderer({kAudioElementId, kAudioElementId2});
  ASSERT_NE(renderer, nullptr);

  // Adds a labeled frame for a second audio element.
  id_to_labeled_frame_[kAudioElementId2] =
      CreateLabeledFrame(kCommonNumSamplesPerFrame);
  constexpr uint32_t kTrimmingFiveSamples = 5;
  id_to_labeled_frame_[kAudioElementId].samples_to_trim_at_start =
      kTrimmingFiveSamples;
  id_to_labeled_frame_[kAudioElementId2].samples_to_trim_at_start =
      kTrimmingFiveSamples;
  id_to_labeled_frame_[kAudioElementId].samples_to_trim_at_end =
      kTrimmingFiveSamples;
  id_to_labeled_frame_[kAudioElementId2].samples_to_trim_at_end =
      kTrimmingFiveSamples;

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              IsOk());
}

TEST_F(OarLayoutRendererTest, RenderFailsWhenTrimmingAtEndIsDifferent) {
  constexpr uint32_t kAudioElementId2 = 101;
  CreateScalableAudioElement(kAudioElementId);
  CreateScalableAudioElement(kAudioElementId2);
  auto renderer = CreateRenderer({kAudioElementId, kAudioElementId2});
  ASSERT_NE(renderer, nullptr);

  // Adds a labeled frame for a second audio element.
  id_to_labeled_frame_[kAudioElementId2] =
      CreateLabeledFrame(kCommonNumSamplesPerFrame);
  constexpr uint32_t kTrimmingFiveSamples = 5;
  id_to_labeled_frame_[kAudioElementId].samples_to_trim_at_end =
      kTrimmingFiveSamples;
  id_to_labeled_frame_[kAudioElementId2].samples_to_trim_at_end =
      kTrimmingFiveSamples + 1;

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

TEST_F(OarLayoutRendererTest, RenderFailsWithMultipleSubblocksInDemixingInfo) {
  CreateScalableAudioElement(kAudioElementId);

  // Make a special demixing param definition with two subblocks, each having
  // a half-frame duration, and add it to the audio element.
  const DemixingParamDefinition two_subblocks_param_definition(
      MakeConstantSubblocksParamDefinitionBaseArgs(
          kDemixingInfoParameterId, kCommonSampleRate,
          kCommonNumSamplesPerFrame, kCommonNumSamplesPerFrame / 2));
  audio_elements_.at(kAudioElementId)
      .obu.audio_element_params_.emplace_back(
          AudioElementParam{two_subblocks_param_definition});

  auto renderer = CreateRenderer({kAudioElementId});
  ASSERT_NE(renderer, nullptr);

  // Replaces the parameter block of ID `kDemixingInfoParameterId`
  // with a two-subblock one.
  auto two_subblock_parameter_block = ParameterBlockObu::CreateMode0(
      ObuHeader(), two_subblocks_param_definition);
  ASSERT_NE(two_subblock_parameter_block, nullptr);

  auto data1 = std::make_unique<DemixingInfoParameterData>();
  data1->dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  auto data2 = std::make_unique<DemixingInfoParameterData>();
  data2->dmixp_mode = DemixingInfoParameterData::kDMixPMode2;

  two_subblock_parameter_block->subblocks_.clear();
  two_subblock_parameter_block->subblocks_.push_back(std::move(data1));
  two_subblock_parameter_block->subblocks_.push_back(std::move(data2));
  ASSERT_EQ(two_subblock_parameter_block->GetNumSubblocks(), 2);
  ParameterBlockWithData two_subblocks_parameter_block_with_data = {
      .obu = std::move(two_subblock_parameter_block),
      .start_timestamp = 0,
      .end_timestamp = kCommonNumSamplesPerFrame};
  id_to_parameter_block_[kDemixingInfoParameterId] =
      &two_subblocks_parameter_block_with_data;

  EXPECT_THAT(renderer->Render(id_to_labeled_frame_, id_to_parameter_block_,
                               rendered_samples_, valid_rendered_samples_),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
