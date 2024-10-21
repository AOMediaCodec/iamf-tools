/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/rendering_mix_presentation_finalizer.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::_;
using testing::Return;
using enum ChannelLabel::Label;

constexpr bool kValidateLoudness = true;
constexpr bool kDontValidateLoudness = false;
const std::optional<uint8_t> kNoOverrideBitDepth = std::nullopt;

class MockRenderer : public AudioElementRendererBase {
 public:
  MockRenderer(const std::vector<ChannelLabel::Label> ordered_labels,
               size_t num_output_channels)
      : AudioElementRendererBase(ordered_labels, num_output_channels) {}
  MockRenderer() : MockRenderer({}, 0) {}

  MOCK_METHOD(
      absl::Status, RenderSamples,
      (const std::vector<std::vector<InternalSampleType>>& samples_to_render,
       std::vector<InternalSampleType>& rendered_samples),
      (override));
};

class MockRendererFactory : public RendererFactoryBase {
 public:
  MockRendererFactory() : RendererFactoryBase() {}

  MOCK_METHOD(std::unique_ptr<AudioElementRendererBase>,
              CreateRendererForLayout,
              (const std::vector<DecodedUleb128>& audio_substream_ids,
               const SubstreamIdLabelsMap& substream_id_to_labels,
               AudioElementObu::AudioElementType audio_element_type,
               const AudioElementObu::AudioElementConfig& audio_element_config,
               const RenderingConfig& rendering_config,
               const Layout& /*loudness_layout*/),
              (const, override));
};

/*!\brief A simple factory which always returns `nullptr`. */
class AlwaysNullRendererFactory : public RendererFactoryBase {
 public:
  /*!\brief Destructor. */
  ~AlwaysNullRendererFactory() override = default;

  std::unique_ptr<AudioElementRendererBase> CreateRendererForLayout(
      const std::vector<DecodedUleb128>& /*audio_substream_ids*/,
      const SubstreamIdLabelsMap& /*substream_id_to_labels*/,
      AudioElementObu::AudioElementType /*audio_element_type*/,
      const AudioElementObu::AudioElementConfig& /*audio_element_config*/,
      const RenderingConfig& /*rendering_config*/,
      const Layout& /*loudness_layout*/) const override {
    return nullptr;
  }
};

/*!\brief A simple factory which always returns `nullptr`. */
class AlwaysNullLoudnessCalculatorFactory
    : public LoudnessCalculatorFactoryBase {
 public:
  /*!\brief Destructor. */
  ~AlwaysNullLoudnessCalculatorFactory() override = default;

  std::unique_ptr<LoudnessCalculatorBase> CreateLoudnessCalculator(
      const MixPresentationLayout& /*layout*/, int32_t /*rendered_sample_rate*/,
      int32_t /*rendered_bit_depth*/) const override {
    return nullptr;
  }
};

class MockLoudnessCalculator : public LoudnessCalculatorBase {
 public:
  MockLoudnessCalculator() : LoudnessCalculatorBase() {}

  MOCK_METHOD(absl::Status, AccumulateLoudnessForSamples,
              (const std::vector<int32_t>& rendered_samples), (override));

  MOCK_METHOD(absl::StatusOr<LoudnessInfo>, QueryLoudness, (),
              (const, override));
};

class MockLoudnessCalculatorFactory : public LoudnessCalculatorFactoryBase {
 public:
  MockLoudnessCalculatorFactory() : LoudnessCalculatorFactoryBase() {}

  MOCK_METHOD(std::unique_ptr<LoudnessCalculatorBase>, CreateLoudnessCalculator,
              (const MixPresentationLayout& layout,
               int32_t rendered_sample_rate, int32_t rendered_bit_depth),
              (const, override));
};

TEST(Constructor, DoesNotCrashWithMockFactories) {
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth, kValidateLoudness,
      std::make_unique<MockRendererFactory>(),
      std::make_unique<MockLoudnessCalculatorFactory>());
}

TEST(Constructor, DoesNotCrashWhenRendererFactoryIsNullptr) {
  std::unique_ptr<RendererFactoryBase> null_renderer_factory = nullptr;

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth, kValidateLoudness,
      std::move(null_renderer_factory),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());
}

TEST(Constructor, DoesNotCrashWhenLoudnessFactoryIsNullptr) {
  std::unique_ptr<LoudnessCalculatorFactoryBase>
      null_loudness_calculator_factory = nullptr;

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth, kValidateLoudness,
      std::make_unique<AlwaysNullRendererFactory>(),
      std::move(null_loudness_calculator_factory));
}

constexpr uint32_t kMixPresentationId = 42;
constexpr uint32_t kCodecConfigId = 42;
constexpr uint32_t kAudioElementId = 42;
constexpr uint32_t kBitDepth = 16;
constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kCommonParameterRate = kSampleRate;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint8_t kCodecConfigBitDepth = 16;

void InitPrerequisiteObusForStereoInput(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_configs,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  const std::vector<DecodedUleb128> kStereoSubstreamIds = {0};

  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_configs);
  AddScalableAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                          kStereoSubstreamIds, codec_configs,
                                          audio_elements);

  // Fill in the first layer correctly for stereo input.
  auto& first_layer = std::get<ScalableChannelLayoutConfig>(
                          audio_elements.at(kAudioElementId).obu.config_)
                          .channel_audio_layer_configs.front();
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo;
  first_layer.substream_count = 1;
  first_layer.coupled_substream_count = 1;
}

void InitPrerequisiteObusForMonoInput(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_configs,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  const std::vector<DecodedUleb128> kMonoSubstreamIds = {0};

  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_configs);
  AddScalableAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                          kMonoSubstreamIds, codec_configs,
                                          audio_elements);

  // Fill in the first layer correctly for mono input.
  auto& first_layer = std::get<ScalableChannelLayoutConfig>(
                          audio_elements.at(kAudioElementId).obu.config_)
                          .channel_audio_layer_configs.front();
  first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono;
  first_layer.substream_count = 1;
  first_layer.coupled_substream_count = 0;
}

void InitPrerequisiteObusForStereoOutput(
    std::list<MixPresentationObu>& obus_to_finalize) {
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId},
      /*common_parameter_id=*/999, kCommonParameterRate, obus_to_finalize);
}

void InitPrerequisiteObusForMonoOutput(
    std::list<MixPresentationObu>& obus_to_finalize) {
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId},
      /*common_parameter_id=*/999, kCommonParameterRate, obus_to_finalize);
  obus_to_finalize.back().sub_mixes_[0].layouts[0].loudness_layout = {
      .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersSsConventionLayout{
          .sound_system =
              LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0}};
}

std::unique_ptr<WavWriter> ProduceNoWavWriters(DecodedUleb128, int, int,
                                               const Layout&,
                                               const std::filesystem::path&,
                                               int, int, int) {
  return nullptr;
}

std::filesystem::path GetFirstSubmixFirstLayoutExpectedPath() {
  return absl::StrCat(GetAndCreateOutputDirectory(""), "_id_",
                      kMixPresentationId, "_first_submix_first_layout.wav");
}

std::unique_ptr<WavWriter> ProduceFirstSubMixFirstLayoutWavWriter(
    DecodedUleb128 mix_presentation_id, int sub_mix_index, int layout_index,
    const Layout&, const std::filesystem::path& prefix, int num_channels,
    int sample_rate, int bit_depth) {
  if (sub_mix_index != 0 || layout_index != 0) {
    return nullptr;
  }

  const auto wav_path =
      absl::StrCat(prefix.string(), "_id_", mix_presentation_id,
                   "_first_submix_first_layout.wav");
  return WavWriter::Create(wav_path, num_channels, sample_rate, bit_depth);
}

// =========== Tests that work is delegated to the renderer factory. ===========

TEST(Finalize, ForwardsArgumentsToRendererFactory) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForStereoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForStereoOutput(obus_to_finalize);
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId] = {{0,
                                        {.label_to_samples = {
                                             {kL2, {0, 1}},
                                             {kR2, {2, 3}},
                                         }}}};

  // We expect arguments to be forwarded from the OBUs to the renderer factory.
  auto mock_renderer_factory = absl::WrapUnique(new MockRendererFactory());
  const auto& forwarded_audio_element = audio_elements.at(kAudioElementId);
  const auto& forwarded_sub_mix = obus_to_finalize.front().sub_mixes_[0];
  const auto& forwarded_rendering_config =
      forwarded_sub_mix.audio_elements[0].rendering_config;
  const auto& forwarded_layout = forwarded_sub_mix.layouts[0].loudness_layout;
  EXPECT_CALL(
      *mock_renderer_factory,
      CreateRendererForLayout(forwarded_audio_element.obu.audio_substream_ids_,
                              forwarded_audio_element.substream_id_to_labels,
                              forwarded_audio_element.obu.GetAudioElementType(),
                              forwarded_audio_element.obu.config_,
                              forwarded_rendering_config, forwarded_layout));
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::move(mock_renderer_factory),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());

  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceFirstSubMixFirstLayoutWavWriter,
                                 obus_to_finalize),
              IsOk());
}

TEST(Finalize, ForwardsOrderedSamplesToRenderer) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForStereoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForStereoOutput(obus_to_finalize);
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId] = {{0,
                                        {.label_to_samples = {
                                             {kL2, {0, 1}},
                                             {kR2, {2, 3}},
                                         }}}};

  // We expect arguments to be forwarded from the OBUs to the renderer.
  auto mock_renderer = absl::WrapUnique(new MockRenderer({kL2, kR2}, 2));
  std::vector<InternalSampleType> rendered_samples;
  const std::vector<std::vector<InternalSampleType>>
      kExpectedTimeChannelOrderedSamples = {{0, 2}, {1, 3}};
  EXPECT_CALL(*mock_renderer,
              RenderSamples(kExpectedTimeChannelOrderedSamples, _));
  auto mock_renderer_factory = absl::WrapUnique(new MockRendererFactory());
  ASSERT_NE(mock_renderer_factory, nullptr);
  EXPECT_CALL(*mock_renderer_factory, CreateRendererForLayout(_, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_renderer)));
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::move(mock_renderer_factory),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());

  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceFirstSubMixFirstLayoutWavWriter,
                                 obus_to_finalize),
              IsOk());
}

TEST(Finalize, CreatesWavFileWhenRenderingIsSupported) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForStereoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForStereoOutput(obus_to_finalize);
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId] = {{0,
                                        {.label_to_samples = {
                                             {kL2, {0, 1}},
                                             {kR2, {2, 3}},
                                         }}}};

  auto mock_renderer = absl::WrapUnique(new MockRenderer());
  std::vector<InternalSampleType> rendered_samples;
  EXPECT_CALL(*mock_renderer, RenderSamples(_, _));
  auto mock_renderer_factory = absl::WrapUnique(new MockRendererFactory());
  ASSERT_NE(mock_renderer_factory, nullptr);
  EXPECT_CALL(*mock_renderer_factory, CreateRendererForLayout(_, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_renderer)));
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::move(mock_renderer_factory),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());

  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceFirstSubMixFirstLayoutWavWriter,
                                 obus_to_finalize),
              IsOk());

  EXPECT_TRUE(std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
}

TEST(Finalize, DoesNotCreateFilesWhenRenderingFactoryIsNullptr) {
  const std::filesystem::path output_directory =
      GetAndCreateOutputDirectory("");
  std::unique_ptr<RendererFactoryBase> null_renderer_factory = nullptr;
  RenderingMixPresentationFinalizer finalizer(
      output_directory, kNoOverrideBitDepth, kDontValidateLoudness,
      std::move(null_renderer_factory),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForStereoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId] = {{0,
                                        {.label_to_samples = {
                                             {kL2, {0, 1}},
                                             {kR2, {2, 3}},
                                         }}}};

  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceFirstSubMixFirstLayoutWavWriter,
                                 obus_to_finalize),
              IsOk());

  EXPECT_TRUE(std::filesystem::is_empty(output_directory));
}

TEST(Finalize, DoesNotCreateFilesWhenRenderingFactoryReturnsNullptr) {
  const std::filesystem::path output_directory =
      GetAndCreateOutputDirectory("");
  RenderingMixPresentationFinalizer finalizer(
      output_directory, kNoOverrideBitDepth, kDontValidateLoudness,
      std::make_unique<AlwaysNullRendererFactory>(),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForStereoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId] = {{0,
                                        {.label_to_samples = {
                                             {kL2, {0, 1}},
                                             {kR2, {2, 3}},
                                         }}}};

  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceFirstSubMixFirstLayoutWavWriter,
                                 obus_to_finalize),
              IsOk());

  EXPECT_TRUE(std::filesystem::is_empty(output_directory));
}

// =========== Tests on output rendered wav file properties ===========

absl::Status FinalizeMonoStreamWithOneFrame(
    const LabeledFrame& labeled_frame,
    RenderingMixPresentationFinalizer& finalizer) {
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId][0] = labeled_frame;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForMonoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);

  return finalizer.Finalize(audio_elements, stream_to_render, {},
                            ProduceFirstSubMixFirstLayoutWavWriter,
                            obus_to_finalize);
}

TEST(Finalize, UsesCodecConfigBitDepthWhenOverrideIsNotSet) {
  const LabeledFrame kLabeledFrame = {.label_to_samples = {{kMono, {0, 1}}}};

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  EXPECT_THAT(FinalizeMonoStreamWithOneFrame(kLabeledFrame, finalizer), IsOk());

  const auto wav_reader = CreateWavReaderExpectOk(
      GetFirstSubmixFirstLayoutExpectedPath().string(), kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.bit_depth(), kCodecConfigBitDepth);
}

TEST(Finalize, OverridesBitDepthWhenRequested) {
  const uint8_t kRequestedOverrideBitDepth = 32;
  const LabeledFrame kLabeledFrame = {.label_to_samples = {{kMono, {0, 1}}}};

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kRequestedOverrideBitDepth,
      kDontValidateLoudness, std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  EXPECT_THAT(FinalizeMonoStreamWithOneFrame(kLabeledFrame, finalizer), IsOk());
  const auto wav_reader = CreateWavReaderExpectOk(
      GetFirstSubmixFirstLayoutExpectedPath().string(), kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.bit_depth(), kRequestedOverrideBitDepth);
}

TEST(Finalize, InvalidWhenFrameIsLargerThanNumSamplesPerFrame) {
  const LabeledFrame kInvalidFrameWithTooManySamples = {
      .label_to_samples = {{kMono, std::vector<InternalSampleType>(
                                       kNumSamplesPerFrame + 1, 0)}}};

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  EXPECT_FALSE(
      FinalizeMonoStreamWithOneFrame(kInvalidFrameWithTooManySamples, finalizer)
          .ok());
}

TEST(Finalize, WavFileHasExpectedProperties) {
  const int kNumSamples = 4;
  const LabeledFrame kFrameWithFourSamples = {
      .label_to_samples = {
          {kMono, std::vector<InternalSampleType>(kNumSamples, 0)}}};

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  EXPECT_THAT(FinalizeMonoStreamWithOneFrame(kFrameWithFourSamples, finalizer),
              IsOk());
  const auto wav_reader = CreateWavReaderExpectOk(
      GetFirstSubmixFirstLayoutExpectedPath().string(), kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamples);
  EXPECT_EQ(wav_reader.sample_rate_hz(), kSampleRate);
  EXPECT_EQ(wav_reader.num_channels(), 1);
  EXPECT_EQ(wav_reader.bit_depth(), kBitDepth);
}

TEST(Finalize, SamplesAreTrimmedFromWavFile) {
  const LabeledFrame kFrameWithOneSampleAfterTrimming = {
      .samples_to_trim_at_end = 1,
      .samples_to_trim_at_start = 2,
      .label_to_samples = {{kMono, std::vector<InternalSampleType>(4, 0)}}};
  const int kExpectedNumSamples = 1;

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  EXPECT_THAT(FinalizeMonoStreamWithOneFrame(kFrameWithOneSampleAfterTrimming,
                                             finalizer),
              IsOk());
  const auto wav_reader = CreateWavReaderExpectOk(
      GetFirstSubmixFirstLayoutExpectedPath().string(), kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.remaining_samples(), kExpectedNumSamples);
}

TEST(Finalize, SupportsFullyTrimmedFrames) {
  const LabeledFrame kFrameWithZeroSamplesAfterTrimming = {
      .samples_to_trim_at_start = 4,
      .label_to_samples = {{kMono, std::vector<InternalSampleType>(4, 0)}}};
  const int kExpectedNumSamples = 0;

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth,
      kDontValidateLoudness, std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  EXPECT_THAT(FinalizeMonoStreamWithOneFrame(kFrameWithZeroSamplesAfterTrimming,
                                             finalizer),
              IsOk());
  const auto wav_reader = CreateWavReaderExpectOk(
      GetFirstSubmixFirstLayoutExpectedPath().string(), kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.remaining_samples(), kExpectedNumSamples);
}

// =========== Tests for finalized OBUs ===========

const LoudnessInfo kExpectedMinimumLoudnessInfo = {
    .info_type = 0,
    .integrated_loudness = std::numeric_limits<int16_t>::min(),
    .digital_peak = std::numeric_limits<int16_t>::min(),
};

TEST(Finalize, CreatesWavFilesBasedOnFactoryFunction) {
  const LabeledFrame kLabeledFrame = {.label_to_samples = {{kMono, {0, 1}}}};
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth, kValidateLoudness,
      std::make_unique<RendererFactory>(),
      std::unique_ptr<AlwaysNullLoudnessCalculatorFactory>());
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId][0] = kLabeledFrame;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForMonoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);
  obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness =
      kExpectedMinimumLoudnessInfo;

  // A factory can be used to omit generating the wav file.
  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceNoWavWriters, obus_to_finalize),
              IsOk());
  EXPECT_FALSE(
      std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
  // Or a factory can be used to create it.
  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceFirstSubMixFirstLayoutWavWriter,
                                 obus_to_finalize),
              IsOk());
  EXPECT_TRUE(std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
}

TEST(Finalize, ForwardsArgumentsToLoudnessCalculatorFactory) {
  const LabeledFrame kLabeledFrame = {.label_to_samples = {{kMono, {0, 1}}}};
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId][0] = kLabeledFrame;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForMonoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);

  // We expect arguments to be forwarded from the OBUs to the loudness
  // calculator factory.
  auto mock_loudness_calculator_factory =
      absl::WrapUnique(new MockLoudnessCalculatorFactory());
  const auto& forwarded_layout =
      obus_to_finalize.front().sub_mixes_[0].layouts[0];
  const int32_t forwarded_sample_rate = static_cast<int32_t>(
      codec_configs.at(kCodecConfigId).GetOutputSampleRate());
  const int32_t forwarded_bit_depth_to_measure_loudness = static_cast<int32_t>(
      codec_configs.at(kCodecConfigId).GetBitDepthToMeasureLoudness());
  EXPECT_CALL(
      *mock_loudness_calculator_factory,
      CreateLoudnessCalculator(forwarded_layout, forwarded_sample_rate,
                               forwarded_bit_depth_to_measure_loudness));
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), std::nullopt, kDontValidateLoudness,
      std::make_unique<RendererFactory>(),
      std::move(mock_loudness_calculator_factory));

  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceNoWavWriters, obus_to_finalize),
              IsOk());
}

const LoudnessInfo kArbitraryLoudnessInfo = {
    .info_type = LoudnessInfo::kTruePeak,
    .integrated_loudness = 123,
    .digital_peak = 456,
    .true_peak = 789,
};

TEST(Finalize, DelegatestoLoudnessCalculator) {
  const LoudnessInfo kMockCalculatedLoudness = kArbitraryLoudnessInfo;
  const LoudnessInfo kMismatchingUserLoudness = kExpectedMinimumLoudnessInfo;
  const std::vector<int32_t> kExpectedPassthroughSamples = {0, 1};
  const std::vector<InternalSampleType> kInputSamples = {0, 1};
  const LabeledFrame kLabeledFrame = {
      .label_to_samples = {{kMono, kInputSamples}}};
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId][0] = kLabeledFrame;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForMonoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);

  // We expect arguments to be forwarded from the OBUs to the loudness
  // calculator factory.
  auto mock_loudness_calculator_factory =
      absl::WrapUnique(new MockLoudnessCalculatorFactory());
  auto mock_loudness_calculator =
      absl::WrapUnique(new MockLoudnessCalculator());
  // We expect the loudness calculator to be called with the rendered samples.
  EXPECT_CALL(*mock_loudness_calculator,
              AccumulateLoudnessForSamples(kExpectedPassthroughSamples))
      .WillOnce(Return(absl::OkStatus()));
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));

  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), std::nullopt, kDontValidateLoudness,
      std::make_unique<RendererFactory>(),
      std::move(mock_loudness_calculator_factory));
  obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness =
      kExpectedMinimumLoudnessInfo;
  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceNoWavWriters, obus_to_finalize),
              IsOk());

  // Data was copied based on `QueryLoudness()`.
  EXPECT_EQ(obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness,
            kArbitraryLoudnessInfo);
}

TEST(Finalize, ValidatesUserLoudnessWhenRequested) {
  const LoudnessInfo kMockCalculatedLoudness = kArbitraryLoudnessInfo;
  const LoudnessInfo kMismatchingUserLoudness = kExpectedMinimumLoudnessInfo;
  const LabeledFrame kLabeledFrame = {.label_to_samples = {{kMono, {0, 1}}}};
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId][0] = kLabeledFrame;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForMonoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForMonoOutput(obus_to_finalize);

  auto mock_loudness_calculator_factory =
      absl::WrapUnique(new MockLoudnessCalculatorFactory());
  auto mock_loudness_calculator =
      absl::WrapUnique(new MockLoudnessCalculator());
  EXPECT_CALL(*mock_loudness_calculator, AccumulateLoudnessForSamples(_))
      .WillOnce(Return(absl::OkStatus()));
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kMockCalculatedLoudness));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));

  // The user provided loudness does not match what the mock "measured".
  obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness =
      kMismatchingUserLoudness;
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), std::nullopt, kValidateLoudness,
      std::make_unique<RendererFactory>(),
      std::move(mock_loudness_calculator_factory));

  EXPECT_FALSE(finalizer
                   .Finalize(audio_elements, stream_to_render, {},
                             ProduceNoWavWriters, obus_to_finalize)
                   .ok());
}

//============== Various modes fallback to preserving loudness. ==============

void FinalizeOneFrameWithFactoriesAndExpectUserLoudnessIsPreserved(
    std::unique_ptr<RendererFactoryBase> renderer_factory,
    std::unique_ptr<LoudnessCalculatorFactoryBase>
        loudness_calculator_factory) {
  RenderingMixPresentationFinalizer finalizer(
      GetAndCreateOutputDirectory(""), kNoOverrideBitDepth, kValidateLoudness,
      std::move(renderer_factory), std::move(loudness_calculator_factory));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> obus_to_finalize;
  InitPrerequisiteObusForStereoInput(codec_configs, audio_elements);
  InitPrerequisiteObusForStereoOutput(obus_to_finalize);
  IdTimeLabeledFrameMap stream_to_render;
  stream_to_render[kAudioElementId] = {{0,
                                        {.label_to_samples = {
                                             {kL2, {0, 1}},
                                             {kR2, {2, 3}},
                                         }}}};
  obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness =
      kArbitraryLoudnessInfo;
  EXPECT_THAT(finalizer.Finalize(audio_elements, stream_to_render, {},
                                 ProduceNoWavWriters, obus_to_finalize),
              IsOk());

  const auto& loudness =
      obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness;
  EXPECT_EQ(loudness, kArbitraryLoudnessInfo);
}

TEST(Finalize, PreservesUserLoudnessWhenRenderFactoryIsNullptr) {
  std::unique_ptr<RendererFactoryBase> null_renderer_factory = nullptr;

  FinalizeOneFrameWithFactoriesAndExpectUserLoudnessIsPreserved(
      std::move(null_renderer_factory),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());
}

TEST(Finalize, PreservesUserLoudnessWhenRenderingIsNotSupported) {
  FinalizeOneFrameWithFactoriesAndExpectUserLoudnessIsPreserved(
      std::make_unique<AlwaysNullRendererFactory>(),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());
}

TEST(Finalize, PreservesUserLoudnessWhenLoudnessFactoryIsNullPtr) {
  std::unique_ptr<LoudnessCalculatorFactoryBase> null_loudness_factory =
      nullptr;
  FinalizeOneFrameWithFactoriesAndExpectUserLoudnessIsPreserved(
      std::make_unique<RendererFactory>(), std::move(null_loudness_factory));
}

TEST(Finalize, PreservesUserLoudnessWhenLoudnessFactoryReturnsNullPtr) {
  FinalizeOneFrameWithFactoriesAndExpectUserLoudnessIsPreserved(
      std::make_unique<RendererFactory>(),
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>());
}

}  // namespace
}  // namespace iamf_tools
