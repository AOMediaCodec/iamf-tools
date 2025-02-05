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
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/codec_config_obu_metadata_builder.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::IsEmpty;
using ::testing::Not;
using testing::Return;
using enum ChannelLabel::Label;

using absl::StatusCode::kFailedPrecondition;

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
constexpr int kNumchannelsForMono = 1;
constexpr uint32_t kBitDepth = 16;
constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kCommonParameterRate = kSampleRate;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint8_t kCodecConfigBitDepth = 16;
constexpr uint8_t kNoTrimFromEnd = 0;
constexpr std::array<DecodedUleb128, 1> kMonoSubstreamIds = {0};
constexpr std::array<DecodedUleb128, 1> kStereoSubstreamIds = {1};

constexpr std::array<ChannelLabel::Label, 2> kStereoLabels = {kL2, kR2};

typedef ::google::protobuf::RepeatedPtrField<
    iamf_tools_cli_proto::CodecConfigObuMetadata>
    CodecConfigObuMetadatas;

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
      const MixPresentationLayout& /*layout*/,
      uint32_t /*num_samples_per_frame*/, int32_t /*rendered_sample_rate*/,
      int32_t /*rendered_bit_depth*/) const override {
    return nullptr;
  }
};

std::string GetFirstSubmixFirstLayoutExpectedPath() {
  return absl::StrCat(GetAndCreateOutputDirectory(""), "_id_",
                      kMixPresentationId, kSuffixAfterMixPresentationId);
}

class FinalizerTest : public ::testing::Test {
 public:
  void InitPrerequisiteObusForMonoInput(DecodedUleb128 audio_element_id) {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_configs_);
    AddScalableAudioElementWithSubstreamIds(
        IamfInputLayout::kMono, audio_element_id, kCodecConfigId,
        kMonoSubstreamIds, codec_configs_, audio_elements_);
  }

  void InitPrerequisiteObusForStereoInput(DecodedUleb128 audio_element_id) {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_configs_);
    AddScalableAudioElementWithSubstreamIds(
        IamfInputLayout::kStereo, audio_element_id, kCodecConfigId,
        kStereoSubstreamIds, codec_configs_, audio_elements_);
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

  RenderingMixPresentationFinalizer CreateFinalizerExpectOk() {
    auto finalizer = RenderingMixPresentationFinalizer::Create(
        renderer_factory_.get(), loudness_calculator_factory_.get(),
        audio_elements_, sample_processor_factory_, obus_to_finalize_);
    EXPECT_THAT(finalizer, IsOk());
    return *std::move(finalizer);
  }

  void ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout() {
    sample_processor_factory_ =
        [output_directory = output_directory_,
         output_wav_file_bit_depth_override =
             output_wav_file_bit_depth_override_](
            DecodedUleb128 mix_presentation_id, int sub_mix_index,
            int layout_index, const Layout&, int num_channels, int sample_rate,
            int bit_depth,
            size_t num_samples_per_frame) -> std::unique_ptr<WavWriter> {
      if (sub_mix_index != 0 || layout_index != 0) {
        return nullptr;
      }
      // Obey the override bit depth. But if it is not set, just match the input
      // audio.
      const uint8_t wav_file_bit_depth =
          output_wav_file_bit_depth_override.value_or(bit_depth);
      const auto wav_path =
          absl::StrCat(output_directory.string(), "_id_", mix_presentation_id,
                       kSuffixAfterMixPresentationId);
      return WavWriter::Create(wav_path, num_channels, sample_rate,
                               wav_file_bit_depth, num_samples_per_frame);
    };
  }

  void IterativeRenderingExpectOk(
      RenderingMixPresentationFinalizer& finalizer,
      const std::list<ParameterBlockWithData>& parameter_blocks) {
    int64_t start_timestamp = 0;
    for (const auto& id_to_labeled_frame : ordered_labeled_frames_) {
      ASSERT_TRUE(id_to_labeled_frame.contains(kAudioElementId));
      EXPECT_THAT(finalizer.PushTemporalUnit(
                      id_to_labeled_frame, start_timestamp,
                      id_to_labeled_frame.at(kAudioElementId).end_timestamp,
                      parameter_blocks),
                  IsOk());
    }

    EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());
    auto finalized_obus =
        finalizer.GetFinalizedMixPresentationObus(validate_loudness_);
    ASSERT_THAT(finalized_obus, IsOk());
    finalized_obus_ = *std::move(finalized_obus);
  }

 protected:
  // Prerequisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  std::list<MixPresentationObu> obus_to_finalize_;
  std::list<ParameterBlockWithData> parameter_blocks_;

  // Finalizer create settings. Default to simplistic inputs that disable
  // most features.
  std::filesystem::path output_directory_ = GetAndCreateOutputDirectory("");
  std::optional<uint8_t> output_wav_file_bit_depth_override_ =
      kNoOverrideBitDepth;
  bool validate_loudness_ = kDontValidateLoudness;
  std::unique_ptr<RendererFactoryBase> renderer_factory_;
  std::unique_ptr<LoudnessCalculatorFactoryBase> loudness_calculator_factory_;
  // Custom `Finalize` arguments.
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory_ =
          RenderingMixPresentationFinalizer::ProduceNoSampleProcessors;

  std::vector<IdLabeledFrameMap> ordered_labeled_frames_;

  std::list<MixPresentationObu> finalized_obus_;
};

// =Tests that the create function does not crash with various modes disabled.=

TEST_F(FinalizerTest, CreateDoesNotCrashWithMockFactories) {
  renderer_factory_ = std::make_unique<MockRendererFactory>();
  loudness_calculator_factory_ =
      std::make_unique<MockLoudnessCalculatorFactory>();

  CreateFinalizerExpectOk();
}

TEST_F(FinalizerTest, CreateDoesNotCrashWhenRendererFactoryIsNullptr) {
  renderer_factory_ = nullptr;

  CreateFinalizerExpectOk();
}

TEST_F(FinalizerTest,
       CreateDoesNotCrashWhenLoudnessCalculatorFactoryIsNullptr) {
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  loudness_calculator_factory_ = nullptr;

  CreateFinalizerExpectOk();
}

TEST_F(FinalizerTest, CreateFailsWitMismatchingNumSamplesPerFrame) {
  // The first audio element references an LPCM codec config.
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  CodecConfigObuMetadatas metadata;
  metadata.Add(CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
      kCodecConfigId, 960));
  constexpr uint32_t kSecondCodecConfigId = kCodecConfigId + 1;
  metadata.Add(CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
      kSecondCodecConfigId, 1920));
  CodecConfigGenerator generator(metadata);
  ASSERT_THAT(generator.Generate(codec_configs_), IsOk());

  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kMono, kAudioElementId, kCodecConfigId,
      kMonoSubstreamIds, codec_configs_, audio_elements_);
  // The second audio element references a codec Config with a different
  // number of samples per frame.
  constexpr DecodedUleb128 kStereoAudioElementId = kAudioElementId + 1;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, kStereoAudioElementId, kSecondCodecConfigId,
      kStereoSubstreamIds, codec_configs_, audio_elements_);
  // Mixing these is invalid because there must be only one codec config in IAMF
  // v1.1.0.
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId, kStereoAudioElementId},
      /*common_parameter_id=*/999, kCommonParameterRate, obus_to_finalize_);

  EXPECT_FALSE(RenderingMixPresentationFinalizer::Create(
                   renderer_factory_.get(), loudness_calculator_factory_.get(),
                   audio_elements_, sample_processor_factory_,
                   obus_to_finalize_)
                   .ok());
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

  auto finalizer = CreateFinalizerExpectOk();
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

  CreateFinalizerExpectOk();
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

  CreateFinalizerExpectOk();
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

  auto finalizer = CreateFinalizerExpectOk();
  IterativeRenderingExpectOk(finalizer, parameter_blocks_);
}

TEST_F(FinalizerTest, CreatesWavFileWhenRenderingIsSupported) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  auto mock_renderer = std::make_unique<MockRenderer>();
  EXPECT_CALL(*mock_renderer, RenderSamples(_, _));
  auto mock_renderer_factory = std::make_unique<MockRendererFactory>();
  EXPECT_CALL(*mock_renderer_factory,
              CreateRendererForLayout(_, _, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_renderer)));
  renderer_factory_ = std::move(mock_renderer_factory);
  std::list<ParameterBlockWithData> parameter_blocks;

  auto finalizer = CreateFinalizerExpectOk();
  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

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

  auto finalizer = CreateFinalizerExpectOk();
  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

  EXPECT_TRUE(std::filesystem::is_empty(output_directory));
}

TEST_F(FinalizerTest, DoesNotCreateFilesWhenRenderingFactoryReturnsNullptr) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kL2, {0}}, {kR2, {2}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  const std::filesystem::path output_directory =
      GetAndCreateOutputDirectory("");
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  std::list<ParameterBlockWithData> parameter_blocks;

  auto finalizer = CreateFinalizerExpectOk();
  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

  EXPECT_TRUE(std::filesystem::is_empty(output_directory));
}

// =========== Tests on output rendered wav file properties ===========

TEST_F(FinalizerTest, UsesCodecConfigBitDepthWhenOverrideIsNotSet) {
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, {0, 1}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  renderer_factory_ = std::make_unique<RendererFactory>();
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  std::list<ParameterBlockWithData> parameter_blocks;
  auto finalizer = CreateFinalizerExpectOk();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

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
  output_wav_file_bit_depth_override_ = 32;
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  std::list<ParameterBlockWithData> parameter_blocks;
  auto finalizer = CreateFinalizerExpectOk();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

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
  auto finalizer = CreateFinalizerExpectOk();

  EXPECT_FALSE(
      finalizer
          .PushTemporalUnit(
              ordered_labeled_frames_[0], kStartTime,
              ordered_labeled_frames_[0].at(kAudioElementId).end_timestamp,
              parameter_blocks)
          .ok());
}

TEST_F(FinalizerTest, WavFileHasExpectedProperties) {
  const std::vector<InternalSampleType> kFourSamples = {1, 2, 3, 4};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, kFourSamples}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  renderer_factory_ = std::make_unique<RendererFactory>();
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  std::list<ParameterBlockWithData> parameter_blocks;
  auto finalizer = CreateFinalizerExpectOk();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

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
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  std::list<ParameterBlockWithData> parameter_blocks;
  auto finalizer = CreateFinalizerExpectOk();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

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
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  std::list<ParameterBlockWithData> parameter_blocks;
  auto finalizer = CreateFinalizerExpectOk();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

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

  // A factory can be used to omit generating wav files.
  renderer_factory_ = std::make_unique<RendererFactory>();
  sample_processor_factory_ =
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors;
  auto finalizer_without_post_processors = CreateFinalizerExpectOk();
  EXPECT_THAT(finalizer_without_post_processors.FinalizePushingTemporalUnits(),
              IsOk());
  EXPECT_FALSE(
      std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));

  // Or a factory can be used to create wav files.
  renderer_factory_ = std::make_unique<RendererFactory>();
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  auto finalizer_with_wav_writers = CreateFinalizerExpectOk();
  EXPECT_THAT(finalizer_with_wav_writers.FinalizePushingTemporalUnits(),
              IsOk());
  EXPECT_TRUE(std::filesystem::exists(GetFirstSubmixFirstLayoutExpectedPath()));
}

TEST_F(FinalizerTest, ForwardsArgumentsToSampleProcessorFactory) {
  PrepareObusForOneSamplePassThroughMono();
  // Rendering needs to be initialized to create wav files.
  renderer_factory_ = std::make_unique<RendererFactory>();
  // We expect arguments to be forwarded from the OBUs to the wav writer
  // factory.
  constexpr int kFirstSubmixIndex = 0;
  constexpr int kFirstLayoutIndex = 0;
  const auto& forwarded_layout =
      obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness_layout;
  const int32_t forwarded_sample_rate = static_cast<int32_t>(
      codec_configs_.at(kCodecConfigId).GetOutputSampleRate());
  const int32_t forwarded_bit_depth = static_cast<int32_t>(
      codec_configs_.at(kCodecConfigId).GetBitDepthToMeasureLoudness());
  const uint32_t forwarded_num_samples_per_frame =
      codec_configs_.at(kCodecConfigId).GetNumSamplesPerFrame();

  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(mock_sample_processor_factory,
              Call(kMixPresentationId, kFirstSubmixIndex, kFirstLayoutIndex,
                   forwarded_layout, kNumchannelsForMono, forwarded_sample_rate,
                   forwarded_bit_depth, forwarded_num_samples_per_frame));
  sample_processor_factory_ = mock_sample_processor_factory.AsStdFunction();

  auto finalizer = CreateFinalizerExpectOk();
}

TEST_F(FinalizerTest, PushTemporalUnitDelegatesToSampleProcessor) {
  // Post-processing is only possible if rendering is enabled.
  renderer_factory_ = std::make_unique<RendererFactory>();
  const std::vector<std::vector<int32_t>> kExpectedPassthroughSamples = {
      {0}, {std::numeric_limits<int32_t>::max()}};
  const std::vector<InternalSampleType> kInputSamples = {0, 1.0};
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  const LabelSamplesMap kLabelToSamples = {{kMono, {0, 1}}};
  AddLabeledFrame(kAudioElementId, kLabelToSamples, kEndTime);
  constexpr auto kNoOutputSamples = 0;
  auto mock_sample_processor = std::make_unique<MockSampleProcessor>(
      codec_configs_.at(kCodecConfigId).GetNumSamplesPerFrame(),
      kNumchannelsForMono, kNoOutputSamples);
  // We expect the post-processor to be called with the rendered samples.
  EXPECT_CALL(
      *mock_sample_processor,
      PushFrameDerived(absl::MakeConstSpan(kExpectedPassthroughSamples)));
  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(mock_sample_processor_factory, Call(_, _, _, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_sample_processor)));
  sample_processor_factory_ = mock_sample_processor_factory.AsStdFunction();

  auto finalizer = CreateFinalizerExpectOk();

  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks_),
      IsOk());
}

TEST_F(FinalizerTest,
       FinalizePushingTemporalUnitsDelegatesToSampleProcessorFlush) {
  // Post-processing is only possible if rendering is enabled.
  renderer_factory_ = std::make_unique<RendererFactory>();
  InitPrerequisiteObusForMonoInput(kAudioElementId);
  AddMixPresentationObuForMonoOutput(kMixPresentationId);
  constexpr auto kNoOutputSamples = 0;
  auto mock_sample_processor = std::make_unique<MockSampleProcessor>(
      codec_configs_.at(kCodecConfigId).GetNumSamplesPerFrame(),
      kNumchannelsForMono, kNoOutputSamples);
  // We expect sample processors to be flushed when FinalizePushingTemporalUnits
  // is called.
  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(*mock_sample_processor, FlushDerived());
  EXPECT_CALL(mock_sample_processor_factory, Call(_, _, _, _, _, _, _, _))
      .WillOnce(Return(std::move(mock_sample_processor)));
  sample_processor_factory_ = mock_sample_processor_factory.AsStdFunction();

  auto finalizer = CreateFinalizerExpectOk();

  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());
}

TEST_F(FinalizerTest, ForwardsArgumentsToLoudnessCalculatorFactory) {
  PrepareObusForOneSamplePassThroughMono();
  // We expect arguments to be forwarded from the OBUs to the loudness
  // calculator factory.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  const auto& forwarded_layout =
      obus_to_finalize_.front().sub_mixes_[0].layouts[0];
  const uint32_t forwarded_num_samples_per_frame =
      codec_configs_.at(kCodecConfigId).GetNumSamplesPerFrame();
  const int32_t forwarded_sample_rate = static_cast<int32_t>(
      codec_configs_.at(kCodecConfigId).GetOutputSampleRate());
  const int32_t forwarded_bit_depth_to_measure_loudness = static_cast<int32_t>(
      codec_configs_.at(kCodecConfigId).GetBitDepthToMeasureLoudness());
  EXPECT_CALL(
      *mock_loudness_calculator_factory,
      CreateLoudnessCalculator(
          forwarded_layout, forwarded_num_samples_per_frame,
          forwarded_sample_rate, forwarded_bit_depth_to_measure_loudness));
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);

  auto finalizer = CreateFinalizerExpectOk();
}

TEST_F(FinalizerTest, DelegatestoLoudnessCalculator) {
  const LoudnessInfo kMockCalculatedLoudness = kArbitraryLoudnessInfo;
  const LoudnessInfo kMismatchingUserLoudness = kExpectedMinimumLoudnessInfo;
  const std::vector<std::vector<int32_t>> kExpectedPassthroughSamples = {
      {0}, {std::numeric_limits<int32_t>::max()}};
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
              AccumulateLoudnessForSamples(
                  absl::MakeConstSpan(kExpectedPassthroughSamples)))
      .WillOnce(Return(absl::OkStatus()));
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  auto finalizer = CreateFinalizerExpectOk();

  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kMismatchingUserLoudness;
  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

  // Data was copied based on `QueryLoudness()`.
  EXPECT_EQ(finalized_obus_.front().sub_mixes_[0].layouts[0].loudness,
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
              CreateLoudnessCalculator(_, _, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));

  // The user provided loudness does not match what the mock "measured".
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kMismatchingUserLoudness;
  validate_loudness_ = kValidateLoudness;
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  std::list<ParameterBlockWithData> parameter_blocks;
  auto finalizer = CreateFinalizerExpectOk();

  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks),
      IsOk());

  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());
  EXPECT_FALSE(
      finalizer.GetFinalizedMixPresentationObus(validate_loudness_).ok());
}

//============== Various modes fallback to preserving loudness. ==============

void FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
    const std::vector<IdLabeledFrameMap>& ordered_labeled_frames_,
    const LoudnessInfo& kExpectedLoudness,
    RenderingMixPresentationFinalizer& finalizer) {
  std::list<ParameterBlockWithData> parameter_blocks;
  int64_t start_timestamp = 0;
  for (const auto& id_to_labeled_frame : ordered_labeled_frames_) {
    ASSERT_TRUE(id_to_labeled_frame.contains(kAudioElementId));
    EXPECT_THAT(finalizer.PushTemporalUnit(
                    id_to_labeled_frame, start_timestamp,
                    id_to_labeled_frame.at(kAudioElementId).end_timestamp,
                    parameter_blocks),
                IsOk());
  }
  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());

  const auto finalized_obus =
      finalizer.GetFinalizedMixPresentationObus(kDontValidateLoudness);
  ASSERT_THAT(finalized_obus, IsOkAndHolds(Not(IsEmpty())));

  EXPECT_EQ(finalized_obus->front().sub_mixes_[0].layouts[0].loudness,
            kExpectedLoudness);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenRenderFactoryIsNullptr) {
  PrepareObusForOneSamplePassThroughMono();
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kArbitraryLoudnessInfo;
  renderer_factory_ = nullptr;
  auto finalizer = CreateFinalizerExpectOk();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      ordered_labeled_frames_, kArbitraryLoudnessInfo, finalizer);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenRenderingIsNotSupported) {
  PrepareObusForOneSamplePassThroughMono();
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kArbitraryLoudnessInfo;
  renderer_factory_ = std::make_unique<AlwaysNullRendererFactory>();
  loudness_calculator_factory_ =
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>();
  auto finalizer = CreateFinalizerExpectOk();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      ordered_labeled_frames_, kArbitraryLoudnessInfo, finalizer);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenLoudnessFactoryIsNullPtr) {
  PrepareObusForOneSamplePassThroughMono();
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kArbitraryLoudnessInfo;
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ = nullptr;
  auto finalizer = CreateFinalizerExpectOk();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      ordered_labeled_frames_, kArbitraryLoudnessInfo, finalizer);
}

TEST_F(FinalizerTest, PreservesUserLoudnessWhenLoudnessFactoryReturnsNullPtr) {
  PrepareObusForOneSamplePassThroughMono();
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness =
      kArbitraryLoudnessInfo;
  renderer_factory_ = std::make_unique<RendererFactory>();
  loudness_calculator_factory_ =
      std::make_unique<AlwaysNullLoudnessCalculatorFactory>();
  auto finalizer = CreateFinalizerExpectOk();

  FinalizeOneFrameAndExpectUserLoudnessIsPreserved(
      ordered_labeled_frames_, kArbitraryLoudnessInfo, finalizer);
}

TEST_F(FinalizerTest, CreateSucceedsWithValidInput) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  renderer_factory_ = std::make_unique<RendererFactory>();

  auto finalizer = CreateFinalizerExpectOk();
}

TEST_F(FinalizerTest,
       FinalizePushingTemporalUnitsReturnsFailedPreconditionAfterFirstCall) {
  auto finalizer = CreateFinalizerExpectOk();
  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());

  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(),
              StatusIs(kFailedPrecondition));
}

TEST_F(FinalizerTest,
       GetFinalizedMixPresentationObusFailsBeforeFinalizePushingTemporalUnits) {
  auto finalizer = CreateFinalizerExpectOk();

  EXPECT_FALSE(
      finalizer.GetFinalizedMixPresentationObus(kDontValidateLoudness).ok());
}

TEST_F(FinalizerTest, GetFinalizedMixPresentationObusMayBeCalledMultipleTimes) {
  InitPrerequisiteObusForStereoInput(kAudioElementId);
  AddMixPresentationObuForStereoOutput(kMixPresentationId);
  auto finalizer = CreateFinalizerExpectOk();
  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());

  const auto finalized_obus =
      finalizer.GetFinalizedMixPresentationObus(kDontValidateLoudness);
  EXPECT_THAT(finalized_obus, IsOk());
  // Subsequent calls are permitted, but they should not change the result.
  EXPECT_EQ(finalized_obus,
            finalizer.GetFinalizedMixPresentationObus(kDontValidateLoudness));
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
  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  renderer_factory_ = std::make_unique<RendererFactory>();
  auto finalizer = CreateFinalizerExpectOk();
  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks),
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

  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  renderer_factory_ = std::make_unique<RendererFactory>();

  // Prepare a mock loudness calculator that will return arbitrary loudness
  // information.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);
  validate_loudness_ = false;
  auto finalizer = CreateFinalizerExpectOk();

  IterativeRenderingExpectOk(finalizer, parameter_blocks_);

  // Then we expect the loudness to be populated with the computed loudness.
  EXPECT_EQ(finalized_obus_.front().sub_mixes_[0].layouts[0].loudness,
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

  ConfigureWavWriterFactoryToProduceFirstSubMixFirstLayout();
  renderer_factory_ = std::make_unique<RendererFactory>();

  // Prepare a mock loudness calculator that will return arbitrary loudness
  // information.
  auto mock_loudness_calculator_factory =
      std::make_unique<MockLoudnessCalculatorFactory>();
  auto mock_loudness_calculator = std::make_unique<MockLoudnessCalculator>();
  ON_CALL(*mock_loudness_calculator, QueryLoudness())
      .WillByDefault(Return(kArbitraryLoudnessInfo));
  EXPECT_CALL(*mock_loudness_calculator_factory,
              CreateLoudnessCalculator(_, _, _, _))
      .WillOnce(Return(std::move(mock_loudness_calculator)));
  loudness_calculator_factory_ = std::move(mock_loudness_calculator_factory);

  auto finalizer = CreateFinalizerExpectOk();
  EXPECT_THAT(
      finalizer.PushTemporalUnit(ordered_labeled_frames_[0],
                                 /*start_timestamp=*/0,
                                 /*end_timestamp=*/10, parameter_blocks),
      IsOk());
  EXPECT_THAT(finalizer.FinalizePushingTemporalUnits(), IsOk());
  // Do validate that computed loudness matches the user provided loudness -
  // since kArbitraryLoudnessInfo is the `computed` loudness, it won't.
  validate_loudness_ = true;
  EXPECT_FALSE(
      finalizer.GetFinalizedMixPresentationObus(validate_loudness_).ok());
}

}  // namespace
}  // namespace iamf_tools
