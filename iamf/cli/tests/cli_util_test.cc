/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/cli_util.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr DecodedUleb128 kCodecConfigId = 21;
constexpr DecodedUleb128 kAudioElementId = 100;
constexpr DecodedUleb128 kMixPresentationId = 100;
constexpr DecodedUleb128 kParameterId = 99999;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kFirstSubstreamId = 31;
constexpr DecodedUleb128 kSecondSubstreamId = 32;
constexpr std::array<DecodedUleb128, 1> kZerothOrderAmbisonicsSubstreamId{
    kFirstSubstreamId};

TEST(WritePcmFrameToBuffer, ResizesOutputBuffer) {
  const size_t kExpectedSize = 12;  // 3 bytes per sample * 4 samples.
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f000000, 0x7e000000},
                                                      {0x7f000000, 0x7e000000}};
  const uint8_t kBitDepth = 24;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  EXPECT_EQ(output_buffer.size(), kExpectedSize);
}

TEST(WritePcmFrameToBuffer, WritesBigEndian) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7f005600},
                                                      {0x7e003400, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const bool kBigEndian = true;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  const std::vector<uint8_t> kExpectedBytes = {
      0x7f, 0x00, 0x12, 0x7e, 0x00, 0x34, 0x7f, 0x00, 0x56, 0x7e, 0x00, 0x78};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, WritesLittleEndian) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7f005600},
                                                      {0x7e003400, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  const std::vector<uint8_t> kExpectedBytes = {
      0x12, 0x00, 0x7f, 0x34, 0x00, 0x7e, 0x56, 0x00, 0x7f, 0x78, 0x00, 0x7e};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, RequiresBitDepthIsMultipleOfEight) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 23;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;

  EXPECT_FALSE(WritePcmFrameToBuffer(frame_to_write, kBitDepth, kBigEndian,
                                     output_buffer)
                   .ok());
}

class GetCommonSampleRateAndBitDepthTest : public ::testing::Test {
 public:
  GetCommonSampleRateAndBitDepthTest()
      : sample_rates_({48000}),
        bit_depths_({16}),
        expected_status_code_(absl::StatusCode::kOk),
        expected_sample_rate_(48000),
        expected_bit_depth_(16),
        expected_requires_resampling_(false) {}
  void Test() {
    uint32_t common_sample_rate;
    uint8_t common_bit_depth;
    bool requires_resampling;
    EXPECT_EQ(GetCommonSampleRateAndBitDepth(
                  sample_rates_, bit_depths_, common_sample_rate,
                  common_bit_depth, requires_resampling)
                  .code(),
              expected_status_code_);

    if (expected_status_code_ == absl::StatusCode::kOk) {
      EXPECT_EQ(common_sample_rate, expected_sample_rate_);
      EXPECT_EQ(common_bit_depth, expected_bit_depth_);
      EXPECT_EQ(requires_resampling, expected_requires_resampling_);
    }
  }

  absl::flat_hash_set<uint32_t> sample_rates_;
  absl::flat_hash_set<uint8_t> bit_depths_;
  absl::StatusCode expected_status_code_;
  uint32_t expected_sample_rate_;
  uint8_t expected_bit_depth_;
  bool expected_requires_resampling_;
};

TEST_F(GetCommonSampleRateAndBitDepthTest, DefaultUnique) { Test(); }

TEST_F(GetCommonSampleRateAndBitDepthTest, InvalidSampleRatesArg) {
  sample_rates_ = {};
  expected_status_code_ = absl::StatusCode::kInvalidArgument;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, InvalidBitDepthsArg) {
  bit_depths_ = {};
  expected_status_code_ = absl::StatusCode::kInvalidArgument;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest,
       DifferentSampleRatesResampleTo48Khz) {
  sample_rates_ = {16000, 96000};
  expected_sample_rate_ = 48000;
  expected_requires_resampling_ = true;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest,
       DifferentBitDepthResultsInCommonBitDepth16) {
  bit_depths_ = {24, 32};
  expected_bit_depth_ = 16;
  // The resampling flag is only set when the sample rate needs to change.
  expected_requires_resampling_ = false;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, SampleRatesAndBitDepthsVary) {
  bit_depths_ = {24, 32};
  expected_bit_depth_ = 16;

  sample_rates_ = {16000, 96000};
  expected_sample_rate_ = 48000;

  expected_requires_resampling_ = true;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, LargeCommonSampleRatesAndBitDepths) {
  sample_rates_ = {192000};
  expected_sample_rate_ = 192000;
  bit_depths_ = {32};
  expected_bit_depth_ = 32;

  Test();
}

const DecodedUleb128 kFourSamplesPerFrame = 4;
const uint32_t kZeroSamplesToTrimAtEnd = 0;
const uint32_t kZeroSamplesToTrimAtStart = 0;
void AddAudioFrameWithIdAndTrim(int32_t num_samples_per_frame,
                                DecodedUleb128 audio_frame_id,
                                uint32_t num_samples_to_trim_at_end,
                                uint32_t num_samples_to_trim_at_start,
                                std::list<AudioFrameWithData>& audio_frames) {
  const std::vector<uint8_t> kEmptyAudioFrameData({});

  audio_frames.emplace_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = num_samples_to_trim_at_end,
              .num_samples_to_trim_at_start = num_samples_to_trim_at_start},
          audio_frame_id, kEmptyAudioFrameData),
      .start_timestamp = 0,
      .end_timestamp = num_samples_per_frame,
      .audio_element_with_data = nullptr});
}

TEST(CollectAndValidateParamDefinitions,
     ReturnsOneUniqueParamDefinitionWhenTheyAreIdentical) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements = {};

  // Create a mix presentation OBU. It will have a `element_mix_gain` and
  // `output_mix_gain` which common settings.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      mix_presentation_obus);
  // Assert that the new mix presentation OBU has identical param definitions.
  ASSERT_EQ(mix_presentation_obus.back()
                .sub_mixes_[0]
                .audio_elements[0]
                .element_mix_gain,
            mix_presentation_obus.back().sub_mixes_[0].output_mix_gain);

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> result;
  EXPECT_THAT(CollectAndValidateParamDefinitions(audio_elements,
                                                 mix_presentation_obus, result),
              IsOk());
  // Validate there is one unique param definition.
  EXPECT_EQ(result.size(), 1);
}

TEST(CollectAndValidateParamDefinitions,
     IsInvalidWhenParamDefinitionsAreNotEquivalent) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements = {};

  // Create a mix presentation OBU. It will have a `element_mix_gain` and
  // `output_mix_gain` which common settings.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      mix_presentation_obus);
  auto& output_mix_gain =
      mix_presentation_obus.back().sub_mixes_[0].output_mix_gain;
  output_mix_gain.default_mix_gain_ = 1;
  // Assert that the new mix presentation OBU has different param definitions.
  ASSERT_NE(mix_presentation_obus.back()
                .sub_mixes_[0]
                .audio_elements[0]
                .element_mix_gain,
            output_mix_gain);

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> result;
  EXPECT_FALSE(CollectAndValidateParamDefinitions(audio_elements,
                                                  mix_presentation_obus, result)
                   .ok());
}

TEST(CollectAndValidateParamDefinitions,
     DoesNotCollectParamDefinitionsFromExtensionParamDefinitions) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, input_codec_configs);
  const std::list<MixPresentationObu> kNoMixPresentationObus = {};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, kZerothOrderAmbisonicsSubstreamId,
      input_codec_configs, audio_elements);

  // Add an extension param definition to the audio element. It is not possible
  // to determine the ID to store it or to use further processing.
  auto& audio_element = audio_elements.at(kAudioElementId);
  audio_element.obu.InitializeParams(1);
  audio_element.obu.audio_element_params_.emplace_back(
      AudioElementParam{ExtendedParamDefinition(
          ParamDefinition::kParameterDefinitionReservedStart)});

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> result;
  EXPECT_THAT(CollectAndValidateParamDefinitions(
                  audio_elements, kNoMixPresentationObus, result),
              IsOk());
  EXPECT_TRUE(result.empty());
}

TEST(CollectAndValidateParamDefinitions, ReconGainParamDefinition) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, input_codec_configs);
  const std::list<MixPresentationObu> kNoMixPresentationObus = {};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;

  const ScalableChannelLayoutConfig kTwoLayerStereoConfig = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
           .output_gain_is_present_flag = false,
           .recon_gain_is_present_flag = true,
           .substream_count = 1,
           .coupled_substream_count = 0},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .output_gain_is_present_flag = false,
           .recon_gain_is_present_flag = true,
           .substream_count = 1,
           .coupled_substream_count = 0},
      }};

  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kAudioElementId, 0, kCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId}, kTwoLayerStereoConfig);
  ASSERT_THAT(obu, IsOk());
  obu->InitializeParams(1);
  AddReconGainParamDefinition(kParameterId, kParameterRate, /*duration=*/1,
                              *obu);
  SubstreamIdLabelsMap substream_id_labels_map;
  LabelGainMap label_gain_map;
  std::vector<ChannelNumbers> channel_numbers;
  ASSERT_THAT(ObuWithDataGenerator::FinalizeScalableChannelLayoutConfig(
                  obu->audio_substream_ids_, kTwoLayerStereoConfig,
                  substream_id_labels_map, label_gain_map, channel_numbers),
              IsOk());

  auto iter = input_codec_configs.find(kCodecConfigId);
  audio_elements_with_data.insert(
      {kAudioElementId, AudioElementWithData{*std::move(obu), &iter->second,
                                             substream_id_labels_map,
                                             label_gain_map, channel_numbers}});

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> result;
  EXPECT_THAT(CollectAndValidateParamDefinitions(
                  audio_elements_with_data, kNoMixPresentationObus, result),
              IsOk());
  EXPECT_EQ(result.size(), 1);
  auto param_definition_iter = result.find(kParameterId);
  ASSERT_NE(param_definition_iter, result.end());
  auto recon_gain_param_definition =
      std::get_if<ReconGainParamDefinition>(&param_definition_iter->second);
  ASSERT_NE(recon_gain_param_definition, nullptr);

  // Fields in `ReconGainParamDefinition`.
  EXPECT_EQ(recon_gain_param_definition->parameter_id_, kParameterId);
  EXPECT_EQ(recon_gain_param_definition->parameter_rate_, kParameterRate);
  EXPECT_EQ(recon_gain_param_definition->param_definition_mode_, 0);
  EXPECT_EQ(recon_gain_param_definition->duration_, 1);
  EXPECT_EQ(recon_gain_param_definition->constant_subblock_duration_, 1);
  EXPECT_EQ(recon_gain_param_definition->audio_element_id_, kAudioElementId);

  // Auxiliary data.
  EXPECT_EQ(recon_gain_param_definition->aux_data_.size(), 2);
  EXPECT_EQ(
      recon_gain_param_definition->aux_data_[0].recon_gain_is_present_flag,
      true);
  EXPECT_EQ(
      recon_gain_param_definition->aux_data_[1].recon_gain_is_present_flag,
      true);
  constexpr ChannelNumbers kExpectedChannelNumbersMonoLayer = {
      .surround = 1, .lfe = 0, .height = 0, .bottom = 0};
  constexpr ChannelNumbers kExpectedChannelNumbersStereoLayer = {
      .surround = 2, .lfe = 0, .height = 0, .bottom = 0};
  EXPECT_EQ(recon_gain_param_definition->aux_data_[0].channel_numbers_for_layer,
            kExpectedChannelNumbersMonoLayer);
  EXPECT_EQ(recon_gain_param_definition->aux_data_[1].channel_numbers_for_layer,
            kExpectedChannelNumbersStereoLayer);
}

TEST(CollectAndValidateParamDefinitions,
     InvalidWhenReconGainParamDefinitionIsPresentButChannelConfigIsMissing) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, input_codec_configs);
  const std::list<MixPresentationObu> kNoMixPresentationObus = {};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {kFirstSubstreamId}, input_codec_configs,
      audio_elements);
  auto& obu = audio_elements.at(kAudioElementId).obu;
  // Normally the spec does not allow ambisonics to have recon gain param
  // definitions.
  obu.InitializeParams(1);
  AddReconGainParamDefinition(kParameterId, kParameterRate, /*duration=*/1,
                              obu);

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> result;
  EXPECT_THAT(CollectAndValidateParamDefinitions(
                  audio_elements, kNoMixPresentationObus, result),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
