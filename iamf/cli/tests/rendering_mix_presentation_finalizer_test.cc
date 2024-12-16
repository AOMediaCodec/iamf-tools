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

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::_;
using testing::Return;
using enum ChannelLabel::Label;

constexpr int64_t kStartTime = 0;
constexpr int32_t kEndTime = 10;
constexpr bool kValidateLoudness = true;
constexpr bool kDontValidateLoudness = false;
const std::optional<uint8_t> kNoOverrideBitDepth = std::nullopt;
constexpr absl::string_view kSuffixAfterMixPresentationId =
    "_first_submix_first_layout.wav";

constexpr uint32_t kMixPresentationId = 42;
constexpr uint32_t kCodecConfigId = 42;
constexpr uint32_t kAudioElementId = 42;
constexpr uint32_t kBitDepth = 16;
constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kCommonParameterRate = kSampleRate;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint8_t kCodecConfigBitDepth = 16;
constexpr uint8_t kNoTrimFromEnd = 0;
constexpr std::array<ChannelLabel::Label, 2> kStereoLabels = {kL2, kR2};

class MockRenderer : public AudioElementRendererBase {
 public:
  MockRenderer(absl::Span<const ChannelLabel::Label> ordered_labels,
               size_t num_output_channels)
      : AudioElementRendererBase(ordered_labels,
                                 static_cast<size_t>(kNumSamplesPerFrame),
                                 num_output_channels) {}
  MockRenderer() : MockRenderer({}, 0) {}

  MOCK_METHOD(
      absl::Status, RenderSamples,
      (absl::Span<const std::vector<InternalSampleType>> samples_to_render,
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
               const Layout& loudness_layout, size_t num_samples_per_frame),
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
      const Layout& /*loudness_layout*/,
      size_t /*num_samples_per_frame*/) const override {
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

std::string GetFirstSubmixFirstLayoutExpectedPath() {
  return absl::StrCat(GetAndCreateOutputDirectory(""), "_id_",
                      kMixPresentationId, kSuffixAfterMixPresentationId);
}

std::unique_ptr<WavWriter> ProduceNoWavWriters(DecodedUleb128, int, int,
                                               const Layout&,
                                               const std::filesystem::path&,
                                               int, int, int) {
  return nullptr;
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
                   kSuffixAfterMixPresentationId);
  return WavWriter::Create(wav_path, num_channels, sample_rate, bit_depth);
}

class FinalizerTest : public ::testing::Test {
 public:
  void InitPrerequisiteObusForMonoInput(DecodedUleb128 audio_element_id) {
    const std::vector<DecodedUleb128> kMonoSubstreamIds = {0};

    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_configs_);
    AddScalableAudioElementWithSubstreamIds(audio_element_id, kCodecConfigId,
                                            kMonoSubstreamIds, codec_configs_,
                                            audio_elements_);

    // Fill in the first layer correctly for mono input.
    auto& first_layer = std::get<ScalableChannelLayoutConfig>(
                            audio_elements_.at(audio_element_id).obu.config_)
                            .channel_audio_layer_configs.front();
    first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono;
    first_layer.substream_count = 1;
    first_layer.coupled_substream_count = 0;
  }

  void InitPrerequisiteObusForStereoInput(DecodedUleb128 audio_element_id) {
    const std::vector<DecodedUleb128> kStereoSubstreamIds = {0};

    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_configs_);
    AddScalableAudioElementWithSubstreamIds(audio_element_id, kCodecConfigId,
                                            kStereoSubstreamIds, codec_configs_,
                                            audio_elements_);

    // Fill in the first layer correctly for stereo input.
    auto& first_layer = std::get<ScalableChannelLayoutConfig>(
                            audio_elements_.at(audio_element_id).obu.config_)
                            .channel_audio_layer_configs.front();
    first_layer.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo;
    first_layer.substream_count = 1;
    first_layer.coupled_substream_count = 1;
  }

  void AddMixPresentationObuForMonoOutput(DecodedUleb128 mix_presentation_id) {
    AddMixPresentationObuWithAudioElementIds(
        mix_presentation_id, {kAudioElementId},
        /*common_parameter_id=*/999, kCommonParameterRate, obus_to_finalize_);
    obus_to_finalize_.back().sub_mixes_[0].layouts[0].loudness_layout = {
        .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
        .specific_layout = LoudspeakersSsConventionLayout{
            .sound_system =
                LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0}};
  }

  void AddMixPresentationObuForStereoOutput(
      DecodedUleb128 mix_presentation_id) {
    AddMixPresentationObuWithAudioElementIds(
        mix_presentation_id, {kAudioElementId},
        /*common_parameter_id=*/999, kCommonParameterRate, obus_to_finalize_);
  }

  void AddLabeledFrame(DecodedUleb128 audio_element_id,
                       const LabelSamplesMap& label_to_samples,
                       int32_t end_timestamp,
                       uint32_t samples_to_trim_at_end = 0,
                       uint32_t samples_to_trim_at_start = 0) {
    IdLabeledFrameMap id_to_labeled_frame;
    id_to_labeled_frame[audio_element_id] = {
        .end_timestamp = end_timestamp,
        .samples_to_trim_at_end = samples_to_trim_at_end,
        .samples_to_trim_at_start = samples_to_trim_at_start,
        .label_to_samples = label_to_samples};
    ordered_labeled_frames_.push_back(id_to_labeled_frame);
  }

  void PrepareObusForOneSamplePassThroughMono() {
    InitPrerequisiteObusForMonoInput(kAudioElementId);
    AddMixPresentationObuForMonoOutput(kMixPresentationId);
    const LabelSamplesMap kLabelToSamples = {{kMono, {0, 1}}};
    AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  }

  RenderingMixPresentationFinalizer GetFinalizer() {
    return RenderingMixPresentationFinalizer(
        output_directory_, output_wav_file_bit_depth_override_,
        validate_loudness_, std::move(renderer_factory_),
        std::move(loudness_calculator_factory_));
  }

  void IterativeRenderingExpectOk(
      RenderingMixPresentationFinalizer& finalizer,
      const std::list<ParameterBlockWithData>::const_iterator&
          parameter_blocks_start,
      const std::list<ParameterBlockWithData>::const_iterator&
          parameter_blocks_end) {
    EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                     obus_to_finalize_),
                IsOk());
    int64_t start_timestamp = 0;
    for (const auto& id_to_labeled_frame : ordered_labeled_frames_) {
      ASSERT_TRUE(id_to_labeled_frame.contains(kAudioElementId));
      EXPECT_THAT(
          finalizer.PushTemporalUnit(
              id_to_labeled_frame, start_timestamp,
              id_to_labeled_frame.at(kAudioElementId).end_timestamp,
              parameter_blocks_start, parameter_blocks_end, obus_to_finalize_),
          IsOk());
    }

    EXPECT_THAT(finalizer.Finalize(validate_loudness_, obus_to_finalize_),
                IsOk());
  }

 protected:
  // Prerequisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  std::list<MixPresentationObu> obus_to_finalize_;
  std::list<ParameterBlockWithData> parameter_blocks_;

  // Finalizer constructor settings. Default to simplistic inputs that disable
  // most features.
  std::filesystem::path output_directory_ = GetAndCreateOutputDirectory("");
  std::optional<uint8_t> output_wav_file_bit_depth_override_ =
      kNoOverrideBitDepth;
  bool validate_loudness_ = kDontValidateLoudness;
  std::unique_ptr<RendererFactoryBase> renderer_factory_;
  std::unique_ptr<LoudnessCalculatorFactoryBase> loudness_calculator_factory_;
  // Custom `Finalize` arguments.
  RenderingMixPresentationFinalizer::WavWriterFactory wav_writer_factory_ =
      ProduceNoWavWriters;

  std::vector<IdLabeledFrameMap> ordered_labeled_frames_;
};

// === Tests that the constructor does not crash with various modes disabled ===

TEST_F(FinalizerTest, ConstructorDoesNotCrashWithMockFactories) {
  renderer_factory_ = std::make_unique<MockRendererFactory>();
  loudness_calculator_factory_ =
      std::make_unique<MockLoudnessCalculatorFactory>();

  GetFinalizer();
}

TEST_F(FinalizerTest, ConstructorDoesNotCrashWhenRendererFactoryIsNullptr) {
  renderer_factory_ = nullptr;

  GetFinalizer();
}

TEST_F(FinalizerTest,
       ConstructorDoesNotCrashWhenLoudnessCalculatorFactoryIsNullptr) {
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  loudness_calculator_factory_ = nullptr;

  GetFinalizer();
}

// =========== Tests that work is delegated to the renderer factory. ===========
TEST_F(FinalizerTest, ForwardsAudioElementToRenderer) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);

  // We expect audio-element related arguments to be forwarded from the OBUs to
  // the renderer factory.
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  const auto& forwarded_audio_element = audio_elements_.at(kAudioElementId);
  EXPECT_CALL(
      *mock_renderer_factory,
      CreateRendererForLayout(
          forwarded_audio_element.obu.audio_substream_ids_,
          forwarded_audio_element.substream_id_to_labels,
          forwarded_audio_element.obu.GetAudioElementType(),
          forwarded_audio_element.obu.config_, _, _,
          forwarded_audio_element.codec_config->GetNumSamplesPerFrame()));
  renderer_factory_ = std::move(mock_renderer_factory);
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
}

TEST_F(FinalizerTest, ForwardsRenderingConfigToRenderer) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);

  // We expect arguments to be forwarded from the OBUs to the renderer factory.
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  const auto& forwarded_sub_mix = obus_to_finalize_.front().sub_mixes_[0];
  const auto& forwarded_rendering_config =
      forwarded_sub_mix.audio_elements[0].rendering_config;
  EXPECT_CALL(
      *mock_renderer_factory,
      CreateRendererForLayout(_, _, _, _, forwarded_rendering_config, _, _));
  renderer_factory_ = std::move(mock_renderer_factory);
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
}

TEST_F(FinalizerTest, ForwardsLayoutToRenderer) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);

  // We expect arguments to be forwarded from the OBUs to the renderer factory.
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  const auto& forwarded_sub_mix = obus_to_finalize_.front().sub_mixes_[0];
  const auto& forwarded_layout = forwarded_sub_mix.layouts[0].loudness_layout;
  EXPECT_CALL(*mock_renderer_factory,
              CreateRendererForLayout(_, _, _, _, _, forwarded_layout, _));
  renderer_factory_ = std::move(mock_renderer_factory);
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
}

TEST_F(FinalizerTest, ForwardsOrderedSamplesToRenderer) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0, 1}}, {kR2, {2, 3}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);

  // We expect arguments to be forwarded from the OBUs to the renderer.
  auto mock_renderer = std::make_unique<MockRenderer>(kStereoLabels, 2);
  std::vector<InternalSampleType> rendered_samples;
  const std::vector<std::vector<InternalSampleType>>
      kExpectedTimeChannelOrderedSamples = {{0, 2}, {1, 3}};
  EXPECT_CALL(*mock_renderer,
              RenderSamples(
                  absl::MakeConstSpan(kExpectedTimeChannelOrderedSamples), _));
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  ASSERT_NE(mock_renderer_factory, nullptr);
  EXPECT_CALL(*mock_renderer_factory,
              CreateRendererForLayout(_, _, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_renderer)));
  renderer_factory_ = std::move(mock_renderer_factory);
  std::list<ParameterBlockWithData> parameter_blocks;

  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());
}

TEST_F(FinalizerTest, CreatesWavFileWhenRenderingIsSupported) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;

  auto mock_renderer = std::make_unique<MockRenderer>();
  EXPECT_CALL(*mock_renderer, RenderSamples(_, _));
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  EXPECT_CALL(*mock_renderer_factory,
              CreateRendererForLayout(_, _, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_renderer)));
  renderer_factory_ = std::move(mock_renderer_factory);
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();
  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  EXPECT_TRUE(std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
}

TEST_F(FinalizerTest, DoesNotCreateFilesWhenRenderingFactoryIsNullptr) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  const std::filesystem::path output_directory =
      GetAndCreateOutputDirectory("");
  renderer_factory_ = nullptr;
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  EXPECT_TRUE(std::filesystem::is_empty(output_directory));
}

TEST_F(FinalizerTest, DoesNotCreateFilesWhenRenderingFactoryReturnsNullptr) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  const std::filesystem::path output_directory =
      GetAndCreateOutputDirectory("");
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  EXPECT_TRUE(std::filesystem::is_empty(output_directory));
}

// =========== Tests on output rendered wav file properties ===========

TEST_F(FinalizerTest, UsesCodecConfigBitDepthWhenOverrideIsNotSet) {
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, {0, 1}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  renderer_factory_ = std::make_unique<RendererFactory>();
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  const auto wav_reader =
      CreateWavReaderExpectOk(GetFirstSubmixFirstLayoutExpectedPath());
  EXPECT_EQ(wav_reader.bit_depth(), kCodecConfigBitDepth);
}

TEST_F(FinalizerTest, OverridesBitDepthWhenRequested) {
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, {0, 1}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  renderer_factory_ = std::make_unique<RendererFactory>();
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  output_wav_file_bit_depth_override_ = 32;
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  const auto wav_reader =
      CreateWavReaderExpectOk(GetFirstSubmixFirstLayoutExpectedPath());

  EXPECT_EQ(wav_reader.bit_depth(), 32);
}

TEST_F(FinalizerTest, InvalidWhenFrameIsLargerThanNumSamplesPerFrame) {
  const LabelSamplesMap kInvalidLabelToSamplesWithTooManySamples = {
      {kMono, std::vector<InternalSampleType>(kNumSamplesPerFrame + 1, 0)}};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  AddLabeledFrame(kAudioElementId, kInvalidLabelToSamplesWithTooManySamples,
                  kEndTime);
  renderer_factory_ = std::make_unique<RendererFactory>();
  std::list<ParameterBlockWithData> parameter_blocks;

  RenderingMixPresentationFinalizer finalizer = GetFinalizer();
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_FALSE(
      finalizer
          .PushTemporalUnit(
              ordered_labeled_frames_[0], kStartTime,
              ordered_labeled_frames_[0].at(kAudioElementId).end_timestamp,
              parameter_blocks.begin(), parameter_blocks.end(),
              obus_to_finalize_)
          .ok());
}

TEST_F(FinalizerTest, WavFileHasExpectedProperties) {
  const std::vector<InternalSampleType> kFourSamples = {1, 2, 3, 4};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, kFourSamples}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  renderer_factory_ = std::make_unique<RendererFactory>();
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  const auto wav_reader =
      CreateWavReaderExpectOk(GetFirstSubmixFirstLayoutExpectedPath());
  EXPECT_EQ(wav_reader.remaining_samples(), kFourSamples.size());
  EXPECT_EQ(wav_reader.sample_rate_hz(), kSampleRate);
  EXPECT_EQ(wav_reader.num_channels(), 1);
  EXPECT_EQ(wav_reader.bit_depth(), kBitDepth);
}

TEST_F(FinalizerTest, SamplesAreTrimmedFromWavFile) {
  constexpr int kNumSamplesToTrimFromStart = 2;
  constexpr int kNumSamplesToTrimFromEnd = 1;
  constexpr int kExpectedNumSamples = 1;
  const std::vector<InternalSampleType> kFourSamples = {1, 2, 3, 4};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, kFourSamples}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime,
                  kNumSamplesToTrimFromStart, kNumSamplesToTrimFromEnd);
  renderer_factory_ = std::make_unique<RendererFactory>();
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  const auto wav_reader =
      CreateWavReaderExpectOk(GetFirstSubmixFirstLayoutExpectedPath());
  EXPECT_EQ(wav_reader.remaining_samples(), kExpectedNumSamples);
}

TEST_F(FinalizerTest, SupportsFullyTrimmedFrames) {
  // Sometimes at the start of a stream frames could be fully trimmed due to
  // codec delay.
  constexpr int kNumSamplesToTrimFromStart = 4;
  constexpr int kExpectedZeroSamplesAfterTrimming = 0;
  const std::vector<InternalSampleType> kFourSamples = {1, 2, 3, 4};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, kFourSamples}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime,
                  kNumSamplesToTrimFromStart, kNoTrimFromEnd);
  renderer_factory_ = std::make_unique<RendererFactory>();
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_.begin(),
                             parameter_blocks_.end());

  const auto wav_reader =
      CreateWavReaderExpectOk(GetFirstSubmixFirstLayoutExpectedPath());
  EXPECT_EQ(wav_reader.remaining_samples(), kExpectedZeroSamplesAfterTrimming);
}

// =========== Tests for finalized OBUs ===========

const LoudnessInfo kExpectedMinimumLoudnessInfo = {
    .info_type = 0,
    .integrated_loudness = std::numeric_limits<int16_t>::min(),
    .digital_peak = std::numeric_limits<int16_t>::min(),
};

const LoudnessInfo kArbitraryLoudnessInfo = {
    .info_type = LoudnessInfo::kTruePeak,
    .integrated_loudness = 123,
    .digital_peak = 456,
    .true_peak = 789,
};

TEST_F(FinalizerTest, CreatesWavFilesBasedOnFactoryFunction) {
  PrepareObusForOneSamplePassThroughMono();
  renderer_factory_ = std::make_unique<RendererFactory>();
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  // A factory can be used to omit generating the wav file.
  wav_writer_factory_ = ProduceNoWavWriters;
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_FALSE(
      std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
  // Or a factory can be used to create it.
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_TRUE(std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
}

TEST_F(FinalizerTest, ForwardsArgumentsToLoudnessCalculatorFactory) {
  PrepareObusForOneSamplePassThroughMono();
  // We expect arguments to be forwarded from the OBUs to the loudness
  // calculator factory.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  const auto& forwarded_layout =
      obus_to_finalize_.front().sub_mixes_[0].layouts[0];
  const int32_t forwarded_sample_rate = static_cast<int32_t>(
      codec_configs_.at(kCodecConfigId).GetOutputSampleRate());
  const int32_t forwarded_bit_depth_to_measure_loudness = static_cast<int32_t>(
      codec_configs_.at(kCodecConfigId).GetBitDepthToMeasureLoudness());
  EXPECT_CALL(
      *mock_loudness_calculator_factory,
      CreateLoudnessCalculator(forwarded_layout, forwarded_sample_rate,
                               forwarded_bit_depth_to_measure_loudness));
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
}

TEST_F(FinalizerTest, DelegatestoLoudnessCalculator) {
  const LoudnessInfo kMockCalculatedLoudness = kArbitraryLoudnessInfo;
  const LoudnessInfo kMismatchingUserLoudness = kExpectedMinimumLoudnessInfo;
  const std::vector<int32_t> kExpectedPassthroughSamples = {
      0, std::numeric_limits<int32_t>::max()};
  const std::vector<InternalSampleType> kInputSamples = {0, 1.0};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, {0, 1}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  // We expect arguments to be forwarded from the OBUs to the loudness
  // calculator factory.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  // We expect the loudness calculator to be called with the rendered samples.
  EXPECT_CALL(*mock_loudness_calculator,
              AccumulateLoudnessForSamples(kExpectedPassthroughSamples))
      .WillOnce(Return(absl::OkStatus()));
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kMismatchingUserLoudness;
  IterativeRenderingExpectOk(finalizer, parameter_blocks.begin(),
                             parameter_blocks.end());

  // Data was copied based on `QueryLoudness()`.
  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kArbitraryLoudnessInfo);
}

TEST_F(FinalizerTest, ValidatesUserLoudnessWhenRequested) {
  const LoudnessInfo kMockCalculatedLoudness = kArbitraryLoudnessInfo;
  const LoudnessInfo kMismatchingUserLoudness = kExpectedMinimumLoudnessInfo;
  PrepareObusForOneSamplePassThroughMono();

  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  EXPECT_CALL(*mock_loudness_calculator, AccumulateLoudnessForSamples(_))
      .WillOnce(Return(absl::OkStatus()));
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kMockCalculatedLoudness));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));

  // The user provided loudness does not match what the mock "measured".
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kMismatchingUserLoudness;
  validate_loudness_ = kValidateLoudness;
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  std::list<ParameterBlockWithData> parameter_blocks;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks.begin(),
                                 parameter_blocks.end(), obus_to_finalize_),
      IsOk());

  EXPECT_FALSE(finalizer.Finalize(validate_loudness_, obus_to_finalize_).ok());
}

//============== Various modes fallback to preserving loudness. ==============

void FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const std::vector<IdLabeledFrameMap>& ordered_labeled_frames_,
    RenderingMixPresentationFinalizer& finalizer,
    std::list<MixPresentationObu>& obus_to_finalize) {
  obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness =
      kArbitraryLoudnessInfo;
  std::list<ParameterBlockWithData> parameter_blocks;
  EXPECT_THAT(finalizer.Initialize(audio_elements, ProduceNoWavWriters,
                                   obus_to_finalize),
              IsOk());
  int64_t start_timestamp = 0;
  for (const auto& id_to_labeled_frame : ordered_labeled_frames_) {
    ASSERT_TRUE(id_to_labeled_frame.contains(kAudioElementId));
    EXPECT_THAT(
        finalizer.PushTemporalUnit(
            id_to_labeled_frame, start_timestamp,
            id_to_labeled_frame.at(kAudioElementId).end_timestamp,
            parameter_blocks.begin(), parameter_blocks.end(), obus_to_finalize),
        IsOk());
  }

  EXPECT_THAT(finalizer.Finalize(/*validate_loudness=*/true, obus_to_finalize),
              IsOk());

  const auto& loudness =
      obus_to_finalize.front().sub_mixes_[0].layouts[0].loudness;
  EXPECT_EQ(loudness, kArbitraryLoudnessInfo);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenRenderFactoryIsNullptr) {
  PrepareObusForOneSamplePassThroughMono();
  renderer_factory_ = nullptr;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      audio_elements_, ordered_labeled_frames_, finalizer, obus_to_finalize_);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenRenderingIsNotSupported) {
  PrepareObusForOneSamplePassThroughMono();
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  loudness_calculator_factory_ =
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>();
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      audio_elements_, ordered_labeled_frames_, finalizer, obus_to_finalize_);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenLoudnessFactoryIsNullPtr) {
  PrepareObusForOneSamplePassThroughMono();
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = nullptr;
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      audio_elements_, ordered_labeled_frames_, finalizer, obus_to_finalize_);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenLoudnessFactoryReturnsNullPtr) {
  PrepareObusForOneSamplePassThroughMono();
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ =
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>();
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      audio_elements_, ordered_labeled_frames_, finalizer, obus_to_finalize_);
}

TEST_F(FinalizerTest, InitializeSucceedsWithValidInput) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  renderer_factory_ = std::make_unique<RendererFactory>();

  RenderingMixPresentationFinalizer finalizer = GetFinalizer();
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
}

TEST_F(FinalizerTest, FinalizeFailsIfCalledTwice) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  renderer_factory_ = std::make_unique<RendererFactory>();

  RenderingMixPresentationFinalizer finalizer = GetFinalizer();
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_THAT(finalizer.Finalize(validate_loudness_, obus_to_finalize_),
              IsOk());
  EXPECT_FALSE(finalizer.Finalize(validate_loudness_, obus_to_finalize_).ok());
}

// =========== Tests for PushTemporalUnit ===========
// TODO(b/380110994): Add more tests for PushTemporalUnit. Check that rendered
// output is written to wav file appropriately.
TEST_F(FinalizerTest, PushTemporalUnitSucceedsWithValidInput) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, /*end_timestamp=*/10);

  PerIdParameterMetadata common_mix_gain_parameter_metadata = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition =
          obus_to_finalize_.front().sub_mixes_[0].output_mix_gain};
  std::list<ParameterBlockWithData> parameter_blocks;

  ASSERT_EQ(ordered_labeled_frames_.size(), 1);
  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  renderer_factory_ = std::make_unique<RendererFactory>();
  RenderingMixPresentationFinalizer finalizer = GetFinalizer();
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks.begin(),
                                 parameter_blocks.end(), obus_to_finalize_),
      IsOk());
}

TEST_F(FinalizerTest, FullIterativeRenderingSucceedsWithValidInput) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, /*end_timestamp=*/10);

  PerIdParameterMetadata common_mix_gain_parameter_metadata = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition =
          obus_to_finalize_.front().sub_mixes_[0].output_mix_gain};
  std::list<ParameterBlockWithData> parameter_blocks;

  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  renderer_factory_ = std::make_unique<RendererFactory>();

  // Prepare a mock loudness calculator that will return arbitrary loudness
  // information.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  validate_loudness_ = false;

  RenderingMixPresentationFinalizer finalizer = GetFinalizer();

  IterativeRenderingExpectOk(finalizer, parameter_blocks.begin(),
                             parameter_blocks.end());

  // Then we expect the loudness to be populated with the computed loudness.
  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kArbitraryLoudnessInfo);
}

TEST_F(FinalizerTest, InvalidComputedLoudnessFails) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, /*end_timestamp=*/10);

  PerIdParameterMetadata common_mix_gain_parameter_metadata = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition =
          obus_to_finalize_.front().sub_mixes_[0].output_mix_gain};
  std::list<ParameterBlockWithData> parameter_blocks;

  wav_writer_factory_ = ProduceFirstSubMixFirstLayoutWavWriter;
  renderer_factory_ = std::make_unique<RendererFactory>();

  // Prepare a mock loudness calculator that will return arbitrary loudness
  // information.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);

  RenderingMixPresentationFinalizer finalizer = GetFinalizer();
  EXPECT_THAT(finalizer.Initialize(audio_elements_, wav_writer_factory_,
                                   obus_to_finalize_),
              IsOk());
  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks.begin(),
                                 parameter_blocks.end(), obus_to_finalize_),
      IsOk());
  // Do validate that computed loudness matches the user provided loudness -
  // since kArbitraryLoudnessInfo is the `computed` loudness, it won't.
  EXPECT_FALSE(
      finalizer.Finalize(/*validate_loudness=*/true, obus_to_finalize_).ok());
}

}  // namespace
}  // namespace iamf_tools
