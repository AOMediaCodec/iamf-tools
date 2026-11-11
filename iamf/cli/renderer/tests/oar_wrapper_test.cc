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

#include "iamf/cli/renderer/oar_wrapper.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"
#include "iamf/obu/polar_position_data.h"
#include "iamf/obu/rendering_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using enum DemixingInfoParameterData::DMixPMode;

constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kNumSamplesPerFrame = 512;
constexpr uint32_t kMonoChannels = 1;
constexpr uint32_t kStereoChannels = 2;
constexpr uint32_t kAudioElementId = 100;
constexpr uint32_t kNonExistentAudioElementId = 999;
constexpr uint32_t kElementMixGainId = 31;
constexpr uint32_t kOutputMixGainId = 761;
constexpr auto kNoDemixingInfo = std::nullopt;
constexpr auto kNoMixGainParameterData = std::nullopt;

Layout GetMonoLayout() {
  return Layout{
      .layout_type = Layout::LayoutType::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersSsConventionLayout{
          .sound_system =
              LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0}};
}

Layout GetStereoLayout() {
  return Layout{
      .layout_type = Layout::LayoutType::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersSsConventionLayout{
          .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};
}

Layout GetBinauralLayout() {
  return Layout{
      .layout_type = Layout::LayoutType::kLayoutTypeBinaural,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0}};
}

absl::Status AddChannelBasedElement(OarWrapper& wrapper, uint32_t element_id,
                                    bool add_demixing_info = false) {
  ScalableChannelLayoutConfig config;
  ChannelAudioLayerConfig layer_config;
  layer_config.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo;
  layer_config.output_gain_is_present_flag = false;
  layer_config.recon_gain_is_present_flag = false;
  layer_config.substream_count = 1;
  layer_config.coupled_substream_count = 1;
  config.channel_audio_layer_configs.push_back(layer_config);

  ObuHeader header;
  header.obu_type = kObuIaAudioElement;
  std::vector<DecodedUleb128> substream_ids = {0};
  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      header, element_id, 0, 0, absl::MakeConstSpan(substream_ids), config);
  if (!obu.ok()) {
    return obu.status();
  }

  std::optional<DemixingParamDefinition> demixing_param_definition =
      std::nullopt;
  if (add_demixing_info) {
    DemixingParamDefinition param_definition({});
    param_definition.default_demixing_info_parameter_data_ =
        DefaultDemixingInfoParameterData(DemixingInfoParameterData::kDMixPMode2,
                                         0, 3, 0);
    demixing_param_definition = param_definition;
  }

  RenderingConfig rendering_config;
  rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeStereo;
  rendering_config.binaural_filter_profile =
      RenderingConfig::kBinauralFilterProfileReverberant;

  return wrapper.AddAudioElement(
      obu->GetAudioElementId(), obu->GetAudioElementType(), obu->config_,
      demixing_param_definition, rendering_config, kElementMixGainId);
}

absl::Status AddSceneBasedElement(OarWrapper& wrapper, uint32_t element_id) {
  ObuHeader header;
  header.obu_type = kObuIaAudioElement;
  std::vector<uint8_t> channel_mapping = {0};
  auto mono_config =
      AmbisonicsMonoConfig::Create(/*substream_count=*/1, channel_mapping);
  if (!mono_config.ok()) {
    return mono_config.status();
  }

  AmbisonicsConfig config;
  config.ambisonics_config = *mono_config;

  std::vector<DecodedUleb128> substream_ids = {0};
  auto obu = AudioElementObu::CreateForAmbisonics(
      header, element_id, 0, 0, absl::MakeConstSpan(substream_ids), config);
  if (!obu.ok()) {
    return obu.status();
  }

  RenderingConfig rendering_config;
  rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeBinauralHeadLocked;
  rendering_config.binaural_filter_profile =
      RenderingConfig::kBinauralFilterProfileAmbient;

  return wrapper.AddAudioElement(
      obu->GetAudioElementId(), obu->GetAudioElementType(), obu->config_,
      kNoDemixingInfo, rendering_config, kElementMixGainId);
}

absl::Status AddObjectBasedElement(OarWrapper& wrapper, uint32_t element_id,
                                   uint32_t num_objects) {
  ObuHeader header;
  header.obu_type = kObuIaAudioElement;
  std::vector<uint8_t> ext_bytes;
  auto config = ObjectsConfig::Create(num_objects, ext_bytes);
  if (!config.ok()) {
    return config.status();
  }

  auto obu =
      AudioElementObu::CreateForObjects(header, element_id, 0, 0, 0, *config);
  if (!obu.ok()) {
    return obu.status();
  }

  RenderingConfig rendering_config;
  rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked;
  rendering_config.binaural_filter_profile =
      RenderingConfig::kBinauralFilterProfileDirect;

  return wrapper.AddAudioElement(
      obu->GetAudioElementId(), obu->GetAudioElementType(), obu->config_,
      kNoDemixingInfo, rendering_config, kElementMixGainId);
}

// ---- Creation related ----
TEST(OarWrapperTest, CreateSucceedsWithValidParameters) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_EQ(wrapper->GetSamplingRate(), kSampleRate);
  EXPECT_EQ(wrapper->GetNumSamplesPerFrame(), kNumSamplesPerFrame);
  EXPECT_EQ(wrapper->GetNumOutputChannels(), kStereoChannels);
}

TEST(OarWrapperTest, CreateFailsWithInconsistentBinauralLayout) {
  Layout layout;
  layout.layout_type = Layout::LayoutType::kLayoutTypeBinaural;
  layout.specific_layout = LoudspeakersSsConventionLayout{
      .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0};

  auto wrapper = OarWrapper::Create(layout, kSampleRate, kStereoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  EXPECT_EQ(wrapper, nullptr);
}

TEST(OarWrapperTest, CreateFailsWithInconsistentSoundSystemLayout) {
  Layout layout;
  layout.layout_type = Layout::LayoutType::kLayoutTypeLoudspeakersSsConvention;
  layout.specific_layout = LoudspeakersReservedOrBinauralLayout{.reserved = 0};

  auto wrapper = OarWrapper::Create(layout, kSampleRate, kStereoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  EXPECT_EQ(wrapper, nullptr);
}

TEST(OarWrapperTest, CreateFailsWithUnsupportedLayout) {
  Layout layout;
  layout.layout_type = Layout::LayoutType::kLayoutTypeReserved0;

  auto wrapper = OarWrapper::Create(layout, kSampleRate, kStereoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  EXPECT_EQ(wrapper, nullptr);
}

TEST(OarWrapperTest, CreateFailsWithMismatchingOutputChannels) {
  // GetStereoLayout() specifies 2 channels, but we request 3 output channels.
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels + 1,
                         kNumSamplesPerFrame, kOutputMixGainId);
  EXPECT_EQ(wrapper, nullptr);
}

// TODO(b/576563728): Update the tests about frame size limitations.
TEST(OarWrapperTest, CreateBinauralFailsWhenFrameSizeIsBelowMinimum) {
  auto wrapper = OarWrapper::Create(
      GetBinauralLayout(), kSampleRate, kStereoChannels,
      OarWrapper::kBinauralMinFrameSize - 1, kOutputMixGainId);
  EXPECT_EQ(wrapper, nullptr);
}

TEST(OarWrapperTest, CreateBinauralFailsWhenFrameSizeIsAboveMaximum) {
  auto wrapper = OarWrapper::Create(
      GetBinauralLayout(), kSampleRate, kStereoChannels,
      OarWrapper::kBinauralMaxFrameSize + 1, kOutputMixGainId);
  EXPECT_EQ(wrapper, nullptr);
}

TEST(OarWrapperTest, CreateBinauralSucceedsAtBoundaryFrameSizes) {
  auto min_wrapper =
      OarWrapper::Create(GetBinauralLayout(), kSampleRate, kStereoChannels,
                         OarWrapper::kBinauralMinFrameSize, kOutputMixGainId);
  EXPECT_NE(min_wrapper, nullptr);

  auto max_wrapper =
      OarWrapper::Create(GetBinauralLayout(), kSampleRate, kStereoChannels,
                         OarWrapper::kBinauralMaxFrameSize, kOutputMixGainId);
  EXPECT_NE(max_wrapper, nullptr);
}

TEST(OarWrapperTest, CreateNonBinauralSucceedsWhenFrameSizeIsOutOfBounds) {
  // The frame size limits only apply when rendering to binaural.
  auto min_wrapper = OarWrapper::Create(
      GetStereoLayout(), kSampleRate, kStereoChannels,
      OarWrapper::kBinauralMinFrameSize - 1, kOutputMixGainId);
  EXPECT_NE(min_wrapper, nullptr);

  auto max_wrapper = OarWrapper::Create(
      GetStereoLayout(), kSampleRate, kStereoChannels,
      OarWrapper::kBinauralMaxFrameSize + 1, kOutputMixGainId);
  EXPECT_NE(max_wrapper, nullptr);
}

// ---- Audio element related ----
TEST(OarWrapperTest, AddAudioElementFailsIfDuplicated) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), IsOk());
  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), Not(IsOk()));
}

TEST(OarWrapperTest, AddAndRemoveAudioElementsSucceeds) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  EXPECT_THAT(AddChannelBasedElement(*wrapper, 100), IsOk());
  EXPECT_THAT(AddSceneBasedElement(*wrapper, 200), IsOk());
  EXPECT_THAT(AddObjectBasedElement(*wrapper, 300, 2), IsOk());

  EXPECT_EQ(wrapper->GetNumAudioElements(), 3);
  EXPECT_EQ(wrapper->GetNumAudioElementChannels(100), 2);

  EXPECT_THAT(wrapper->RemoveAudioElement(100), IsOk());
  EXPECT_THAT(wrapper->RemoveAudioElement(200), IsOk());
  EXPECT_THAT(wrapper->RemoveAudioElement(300), IsOk());
  EXPECT_EQ(wrapper->GetNumAudioElements(), 0);
}

// ---- Audio input related ----
TEST(OarWrapperTest, AddInputSucceeds) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), IsOk());

  std::vector<float> input_buffer(kNumSamplesPerFrame * kStereoChannels, 0.0f);
  EXPECT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer),
                        kStereoChannels),
      IsOk());
}

TEST(OarWrapperTest, AddInputFailsWhenBufferIsIncorrectSize) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  constexpr uint32_t kElementId = 101;
  constexpr uint32_t kOneChannel = 1;

  // Expected size is kNumSamplesPerFrame * kOneChannel = 512
  std::vector<float> bad_sized_buffer(kNumSamplesPerFrame - 10, 0.0f);

  EXPECT_FALSE(wrapper
                   ->AddInput(kElementId, absl::MakeConstSpan(bad_sized_buffer),
                              kOneChannel)
                   .ok());
}

TEST(OarWrapperTest, AddInputFailsIfAudioElementIdNotFound) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Here we omit adding an audio element, so adding input fails.
  std::vector<float> input_buffer(kNumSamplesPerFrame * kStereoChannels, 0.0f);
  EXPECT_THAT(
      wrapper->AddInput(kNonExistentAudioElementId,
                        absl::MakeConstSpan(input_buffer), kStereoChannels),
      Not(IsOk()));
}

// ---- Mix gains related ----
TEST(OarWrapperTest, UpdateMixGainsSucceeds) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), IsOk());

  EXPECT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 1.0f,
                                    kNoMixGainParameterData),
      IsOk());
  EXPECT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 1.0f,
                                           kNoMixGainParameterData),
              IsOk());

  MixGainParameterData param_data(
      AnimatedParameterData<int16_t>::MakeLinear(256, 1024));
  EXPECT_THAT(wrapper->UpdateElementMixGain(
                  kAudioElementId, kNumSamplesPerFrame, 1.0f, param_data),
              IsOk());
}

TEST(OarWrapperTest, UpdateElementMixGainFailsIfAudioElementIdNotFound) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Here we omit adding an audio element, updating element mix gain fails.
  EXPECT_THAT(wrapper->UpdateElementMixGain(kNonExistentAudioElementId,
                                            kNumSamplesPerFrame, 1.0f,
                                            kNoMixGainParameterData),
              Not(IsOk()));
}

// ---- Head rotation/head tracking related ----
TEST(OarWrapperTest, SetHeadRotationSucceeds) {
  auto wrapper =
      OarWrapper::Create(GetBinauralLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  ASSERT_THAT(AddSceneBasedElement(*wrapper, kAudioElementId), IsOk());
  ASSERT_THAT(wrapper->EnableHeadTracking(true), IsOk());
  EXPECT_THAT(
      wrapper->SetHeadRotation(1.0f, 0.0f, 0.0f, 0.0f, kNumSamplesPerFrame),
      IsOk());
}

TEST(OarWrapperTest, SetHeadTrackingSucceedsForBinauralLayout) {
  auto wrapper =
      OarWrapper::Create(GetBinauralLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  EXPECT_THAT(wrapper->EnableHeadTracking(true), IsOk());
  EXPECT_THAT(wrapper->EnableHeadTracking(false), IsOk());
}

TEST(OarWrapperTest, SetHeadTrackingFailsForSterioLayout) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // For stereo, setting head tracking is not supported.
  EXPECT_THAT(wrapper->EnableHeadTracking(false), Not(IsOk()));
  EXPECT_THAT(wrapper->EnableHeadTracking(true), Not(IsOk()));
}

// ---- Object positions related ----
TEST(SetDefaultObjectPositions, RendersDefaultSingleObjectPositions) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 1), IsOk());
  PolarParamDefinition polar_def({});
  polar_def.default_azimuth_ = 90;
  polar_def.default_elevation_ = 0;
  polar_def.default_distance_ = 127;
  Cart8ParamDefinition cart8_def({});
  cart8_def.default_x_ = -127;
  Cart16ParamDefinition cart16_def({});
  cart16_def.default_x_ = 32767;
  std::vector<float> input_buffer(kNumSamplesPerFrame, 1.0f);
  std::vector<float> polar_output(kNumSamplesPerFrame * kStereoChannels, 0.0f);
  std::vector<float> cart8_output(kNumSamplesPerFrame * kStereoChannels, 0.0f);
  std::vector<float> cart16_output(kNumSamplesPerFrame * kStereoChannels, 0.0f);

  ASSERT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer), 1),
      IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, polar_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(polar_output)), IsOk());

  ASSERT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer), 1),
      IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, cart8_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(cart8_output)), IsOk());

  ASSERT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer), 1),
      IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, cart16_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(cart16_output)), IsOk());

  constexpr size_t kLastLeftSample = kNumSamplesPerFrame - 1;
  constexpr size_t kLastRightSample = 2 * kNumSamplesPerFrame - 1;
  EXPECT_GT(polar_output[kLastLeftSample], polar_output[kLastRightSample]);
  EXPECT_GT(cart8_output[kLastLeftSample], cart8_output[kLastRightSample]);
  EXPECT_GT(cart16_output[kLastRightSample], cart16_output[kLastLeftSample]);
}

TEST(SetDefaultObjectPositions, ClampsOutOfRangePolarCoordinatesPerIamfSpec) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 1), IsOk());
  PolarParamDefinition polar_def({});
  // -270 degrees clamps to -180 degrees (directly behind on the median plane)
  // rather than wrapping modulo 360 to +90 degrees (left).
  polar_def.default_azimuth_ = -270;
  polar_def.default_elevation_ = 100;
  polar_def.default_distance_ = 200;
  std::vector<float> input_buffer(kNumSamplesPerFrame, 1.0f);
  std::vector<float> polar_output(kNumSamplesPerFrame * kStereoChannels, 0.0f);

  ASSERT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer), 1),
      IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, polar_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(polar_output)), IsOk());

  constexpr size_t kLastLeftSample = kNumSamplesPerFrame - 1;
  constexpr size_t kLastRightSample = 2 * kNumSamplesPerFrame - 1;
  EXPECT_GT(polar_output[kLastLeftSample], 0.0f);
  EXPECT_FLOAT_EQ(polar_output[kLastLeftSample],
                  polar_output[kLastRightSample]);
}

TEST(SetDefaultObjectPositions, RendersDefaultDualObjectPolarPositions) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 2), IsOk());
  DualPolarParamDefinition dual_polar_def({});
  dual_polar_def.default_first_azimuth_ = 90;
  dual_polar_def.default_first_distance_ = 127;
  dual_polar_def.default_second_azimuth_ = -90;
  dual_polar_def.default_second_distance_ = 127;
  std::vector<float> first_object_input(2 * kNumSamplesPerFrame, 0.0f);
  std::fill_n(first_object_input.begin(), kNumSamplesPerFrame, 1.0f);
  std::vector<float> second_object_input(2 * kNumSamplesPerFrame, 0.0f);
  std::fill_n(second_object_input.begin() + kNumSamplesPerFrame,
              kNumSamplesPerFrame, 1.0f);
  std::vector<float> first_object_output(kNumSamplesPerFrame * kStereoChannels,
                                         0.0f);
  std::vector<float> second_object_output(kNumSamplesPerFrame * kStereoChannels,
                                          0.0f);

  ASSERT_THAT(wrapper->AddInput(kAudioElementId,
                                absl::MakeConstSpan(first_object_input), 2),
              IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, dual_polar_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(first_object_output)), IsOk());

  ASSERT_THAT(wrapper->AddInput(kAudioElementId,
                                absl::MakeConstSpan(second_object_input), 2),
              IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, dual_polar_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(second_object_output)), IsOk());

  constexpr size_t kLastLeftSample = kNumSamplesPerFrame - 1;
  constexpr size_t kLastRightSample = 2 * kNumSamplesPerFrame - 1;
  EXPECT_GT(first_object_output[kLastLeftSample],
            first_object_output[kLastRightSample]);
  EXPECT_GT(second_object_output[kLastRightSample],
            second_object_output[kLastLeftSample]);
}

TEST(SetDefaultObjectPositions, RendersDefaultDualCartesianObjectPositions) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 2), IsOk());
  DualCart8ParamDefinition dual_cart8_def({});
  dual_cart8_def.default_first_x_ = -128;
  dual_cart8_def.default_second_x_ = 127;
  DualCart16ParamDefinition dual_cart16_def({});
  dual_cart16_def.default_first_x_ = -32768;
  dual_cart16_def.default_second_x_ = 32767;
  std::vector<float> first_object_input(2 * kNumSamplesPerFrame, 0.0f);
  std::fill_n(first_object_input.begin(), kNumSamplesPerFrame, 1.0f);
  std::vector<float> second_object_input(2 * kNumSamplesPerFrame, 0.0f);
  std::fill_n(second_object_input.begin() + kNumSamplesPerFrame,
              kNumSamplesPerFrame, 1.0f);
  std::vector<float> first_object_output(kNumSamplesPerFrame * kStereoChannels,
                                         0.0f);
  std::vector<float> second_object_output(kNumSamplesPerFrame * kStereoChannels,
                                          0.0f);

  ASSERT_THAT(wrapper->AddInput(kAudioElementId,
                                absl::MakeConstSpan(first_object_input), 2),
              IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, dual_cart8_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(first_object_output)), IsOk());

  ASSERT_THAT(wrapper->AddInput(kAudioElementId,
                                absl::MakeConstSpan(second_object_input), 2),
              IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, dual_cart16_def),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(second_object_output)), IsOk());

  constexpr size_t kLastLeftSample = kNumSamplesPerFrame - 1;
  constexpr size_t kLastRightSample = 2 * kNumSamplesPerFrame - 1;
  EXPECT_GT(first_object_output[kLastLeftSample],
            first_object_output[kLastRightSample]);
  EXPECT_GT(second_object_output[kLastRightSample],
            second_object_output[kLastLeftSample]);
}

TEST(SetDefaultObjectPositions, RejectsNonExistentOrNonObjectElement) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), IsOk());
  PolarParamDefinition polar_def({});

  EXPECT_THAT(wrapper->SetDefaultObjectPositions(
                  kNonExistentAudioElementId, kNumSamplesPerFrame, polar_def),
              Not(IsOk()));
  EXPECT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, polar_def),
              Not(IsOk()));
}

TEST(SetDefaultObjectPositions, RejectsMismatchedNumObjects) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 1), IsOk());
  DualPolarParamDefinition dual_polar_def({});

  EXPECT_THAT(wrapper->SetDefaultObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, dual_polar_def),
              Not(IsOk()));
}

TEST(UpdatePolarObjectPositions, RendersAnimatedStepSinglePolarObjectPosition) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 1), IsOk());
  auto polar_left_data = PolarPositionData::Create(
      AnimationType::kStep,
      PolarPosition{
          .azimuth = AnimatedParameterData<int16_t>::MakeStep(90),
          .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
          .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
      });
  ASSERT_THAT(polar_left_data, IsOk());
  std::vector<float> input_buffer(kNumSamplesPerFrame, 1.0f);
  std::vector<float> polar_output(kNumSamplesPerFrame * kStereoChannels, 0.0f);

  ASSERT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer), 1),
      IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->UpdatePolarObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, *polar_left_data),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(polar_output)), IsOk());

  constexpr size_t kLastLeftSample = kNumSamplesPerFrame - 1;
  constexpr size_t kLastRightSample = 2 * kNumSamplesPerFrame - 1;
  EXPECT_GT(polar_output[kLastLeftSample], polar_output[kLastRightSample]);
}

TEST(UpdatePolarObjectPositions, RendersAnimatedStepDualPolarObjectPositions) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 2), IsOk());
  auto dual_polar_data = PolarPositionData::CreateDual(
      AnimationType::kStep,
      PolarPosition{
          .azimuth = AnimatedParameterData<int16_t>::MakeStep(90),
          .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
          .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
      },
      PolarPosition{
          .azimuth = AnimatedParameterData<int16_t>::MakeStep(-90),
          .elevation = AnimatedParameterData<int8_t>::MakeStep(0),
          .distance = AnimatedParameterData<uint8_t>::MakeStep(127),
      });
  ASSERT_THAT(dual_polar_data, IsOk());
  std::vector<float> first_object_input(2 * kNumSamplesPerFrame, 0.0f);
  std::fill_n(first_object_input.begin(), kNumSamplesPerFrame, 1.0f);
  std::vector<float> second_object_input(2 * kNumSamplesPerFrame, 0.0f);
  std::fill_n(second_object_input.begin() + kNumSamplesPerFrame,
              kNumSamplesPerFrame, 1.0f);
  std::vector<float> first_object_output(kNumSamplesPerFrame * kStereoChannels,
                                         0.0f);
  std::vector<float> second_object_output(kNumSamplesPerFrame * kStereoChannels,
                                          0.0f);

  ASSERT_THAT(wrapper->AddInput(kAudioElementId,
                                absl::MakeConstSpan(first_object_input), 2),
              IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->UpdatePolarObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, *dual_polar_data),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(first_object_output)), IsOk());

  ASSERT_THAT(wrapper->AddInput(kAudioElementId,
                                absl::MakeConstSpan(second_object_input), 2),
              IsOk());
  ASSERT_THAT(
      wrapper->UpdateElementMixGain(kAudioElementId, kNumSamplesPerFrame, 0.0f,
                                    kNoMixGainParameterData),
      IsOk());
  ASSERT_THAT(wrapper->UpdateOutputMixGain(kNumSamplesPerFrame, 0.0f,
                                           kNoMixGainParameterData),
              IsOk());
  ASSERT_THAT(wrapper->UpdatePolarObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, *dual_polar_data),
              IsOk());
  ASSERT_THAT(wrapper->Render(absl::MakeSpan(second_object_output)), IsOk());

  constexpr size_t kLastLeftSample = kNumSamplesPerFrame - 1;
  constexpr size_t kLastRightSample = 2 * kNumSamplesPerFrame - 1;
  EXPECT_GT(first_object_output[kLastLeftSample],
            first_object_output[kLastRightSample]);
  EXPECT_GT(second_object_output[kLastRightSample],
            second_object_output[kLastLeftSample]);
}

TEST(UpdatePolarObjectPositions, RejectsInvalidElementOrMismatchedNumObjects) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  ASSERT_THAT(AddObjectBasedElement(*wrapper, kAudioElementId, 1), IsOk());
  auto single_polar_data =
      PolarPositionData::Create(AnimationType::kStep, PolarPosition{});
  ASSERT_THAT(single_polar_data, IsOk());
  auto dual_polar_data = PolarPositionData::CreateDual(
      AnimationType::kStep, PolarPosition{}, PolarPosition{});
  ASSERT_THAT(dual_polar_data, IsOk());

  EXPECT_THAT(
      wrapper->UpdatePolarObjectPositions(
          kNonExistentAudioElementId, kNumSamplesPerFrame, *single_polar_data),
      Not(IsOk()));
  EXPECT_THAT(wrapper->UpdatePolarObjectPositions(
                  kAudioElementId, kNumSamplesPerFrame, *dual_polar_data),
              Not(IsOk()));
}

// ---- Down mix mode related ----
TEST(OarWrapperTest, UpdateDownMixModeSucceedsForStereroToMono) {
  // Input audio element is stereo, so we request a mono output layout to
  // make sure the down-mixing is enabled.
  auto wrapper = OarWrapper::Create(GetMonoLayout(), kSampleRate, kMonoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Add a demixing info param definition so there is a down-mix mode to be
  // updated.
  const bool kAddDemixingInfo = true;
  EXPECT_THAT(
      AddChannelBasedElement(*wrapper, kAudioElementId, kAddDemixingInfo),
      IsOk());

  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode1),
              IsOk());
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode2),
              IsOk());
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode3),
              IsOk());
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode1_n),
              IsOk());
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode2_n),
              IsOk());
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode3_n),
              IsOk());
}

TEST(OarWrapperTest, UpdateDownMixModeFailsForReservedDownmixModes) {
  auto wrapper = OarWrapper::Create(GetMonoLayout(), kSampleRate, kMonoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Add a demixing info param definition so there is a down-mix mode to be
  // updated.
  const bool kAddDemixingInfo = true;
  EXPECT_THAT(
      AddChannelBasedElement(*wrapper, kAudioElementId, kAddDemixingInfo),
      IsOk());

  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPModeReserved1),
              Not(IsOk()));

  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPModeReserved2),
              Not(IsOk()));
}

TEST(OarWrapperTest, UpdateDownMixModeFailsWithoutDemixingInfo) {
  // Input audio element is stereo, so we request a mono output layout to
  // make sure the down-mixing is enabled.
  auto wrapper = OarWrapper::Create(GetMonoLayout(), kSampleRate, kMonoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Even if the down-mixer is present, if there is no demixing info param
  // definition, there is nothing to "update".
  const bool kDoNotAddDemixingInfo = false;
  EXPECT_THAT(
      AddChannelBasedElement(*wrapper, kAudioElementId, kDoNotAddDemixingInfo),
      IsOk());

  // Updating down-mix mode fails because the demixing info is not defined.
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode1),
              Not(IsOk()));
}

TEST(OarWrapperTest, UpdateDownMixModeFailsStereoToStereo) {
  // Input and output audio element are stereo, so no down-mixer is present.
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), IsOk());

  // Updating down-mix mode fails because there is no down-mixer.
  EXPECT_THAT(wrapper->UpdateDownMixMode(kAudioElementId, kNumSamplesPerFrame,
                                         kDMixPMode1),
              Not(IsOk()));
}

TEST(OarWrapperTest, UpdateDownMixModeFailsIfAudioElementIdNotFound) {
  auto wrapper = OarWrapper::Create(GetMonoLayout(), kSampleRate, kMonoChannels,
                                    kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Here we omit adding an audio element, so updating a down-mix mode fails.
  EXPECT_THAT(wrapper->UpdateDownMixMode(kNonExistentAudioElementId,
                                         kNumSamplesPerFrame, kDMixPMode1),
              Not(IsOk()));
}

// ---- Rendering related ----
TEST(OarWrapperTest, RenderSucceeds) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);
  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId), IsOk());

  std::vector<float> input_buffer(kNumSamplesPerFrame * kStereoChannels, 0.0f);
  EXPECT_THAT(
      wrapper->AddInput(kAudioElementId, absl::MakeConstSpan(input_buffer),
                        kStereoChannels),
      IsOk());

  std::vector<float> output_buffer(kNumSamplesPerFrame * kStereoChannels, 0.0f);
  EXPECT_THAT(wrapper->Render(absl::MakeSpan(output_buffer)), IsOk());
}

TEST(OarWrapperTest, RenderFailsWhenOutputBufferIsIncorrectSize) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  // Expected size is kStereoChannels * kNumSamplesPerFrame = 1024
  std::vector<float> bad_output_buffer(
      kStereoChannels * kNumSamplesPerFrame - 1, 0.0f);

  EXPECT_FALSE(wrapper->Render(absl::MakeSpan(bad_output_buffer)).ok());
}

// ---- Misc. setters and getters ----
TEST(OarWrapperTest, GlobalConfigurationsSucceed) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  EXPECT_THAT(wrapper->EnableLoudnessProcessor(true), IsOk());
  EXPECT_THAT(wrapper->EnableLoudnessProcessor(false), IsOk());
  EXPECT_THAT(wrapper->SetLoudness(-24.0f, -24.0f), IsOk());
  EXPECT_THAT(wrapper->EnableLimiter(true), IsOk());
  EXPECT_THAT(wrapper->EnableLimiter(false), IsOk());
}

TEST(OarWrapperTest, AddAudioElementFailsWithDuplicateDemixingInfo) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId,
                                     /*add_demixing_info=*/true),
              IsOk());
  EXPECT_THAT(AddChannelBasedElement(*wrapper, kAudioElementId,
                                     /*add_demixing_info=*/true),
              Not(IsOk()));
}

TEST(OarWrapperTest,
     AddAudioElementFailsWithUnsupportedHeadphonesRenderingMode) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  ScalableChannelLayoutConfig config;
  ChannelAudioLayerConfig layer_config;
  layer_config.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo;
  layer_config.output_gain_is_present_flag = false;
  layer_config.recon_gain_is_present_flag = false;
  layer_config.substream_count = 1;
  layer_config.coupled_substream_count = 1;
  config.channel_audio_layer_configs.push_back(layer_config);

  ObuHeader header;
  header.obu_type = kObuIaAudioElement;
  std::vector<DecodedUleb128> substream_ids = {0};
  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      header, kAudioElementId, 0, 0, absl::MakeConstSpan(substream_ids),
      config);
  ASSERT_THAT(obu, IsOk());

  RenderingConfig rendering_config;
  const uint8_t kIllegalRenderingMode = 99;
  rendering_config.headphones_rendering_mode =
      static_cast<RenderingConfig::HeadphonesRenderingMode>(
          kIllegalRenderingMode);
  rendering_config.binaural_filter_profile =
      RenderingConfig::kBinauralFilterProfileReverberant;

  EXPECT_THAT(wrapper->AddAudioElement(obu->GetAudioElementId(),
                                       obu->GetAudioElementType(), obu->config_,
                                       kNoDemixingInfo, rendering_config,
                                       kElementMixGainId),
              Not(IsOk()));
}

TEST(OarWrapperTest, AddAudioElementFailsWithUnsupportedAudioElementType) {
  auto wrapper =
      OarWrapper::Create(GetStereoLayout(), kSampleRate, kStereoChannels,
                         kNumSamplesPerFrame, kOutputMixGainId);
  ASSERT_NE(wrapper, nullptr);

  ObuHeader header;
  header.obu_type = kObuIaAudioElement;
  std::vector<DecodedUleb128> substream_ids = {0};
  std::vector<uint8_t> config_bytes = {0};
  auto obu = AudioElementObu::CreateForExtension(
      header, kAudioElementId, AudioElementObu::kAudioElementBeginReserved, 0,
      0, absl::MakeConstSpan(substream_ids), absl::MakeConstSpan(config_bytes));
  ASSERT_THAT(obu, IsOk());

  RenderingConfig rendering_config;
  rendering_config.headphones_rendering_mode =
      RenderingConfig::kHeadphonesRenderingModeStereo;
  rendering_config.binaural_filter_profile =
      RenderingConfig::kBinauralFilterProfileReverberant;

  EXPECT_THAT(wrapper->AddAudioElement(obu->GetAudioElementId(),
                                       obu->GetAudioElementType(), obu->config_,
                                       kNoDemixingInfo, rendering_config,
                                       kElementMixGainId),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
