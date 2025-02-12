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
#include <memory>
#include <utility>
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
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 21;
constexpr DecodedUleb128 kAudioElementId = 100;
constexpr DecodedUleb128 kSecondAudioElementId = 101;
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
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  EXPECT_EQ(output_buffer.size(), kExpectedSize);
}

TEST(WritePcmFrameToBuffer, WritesBigEndian) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = true;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  const std::vector<uint8_t> kExpectedBytes = {
      0x7f, 0x00, 0x12, 0x7e, 0x00, 0x34, 0x7f, 0x00, 0x56, 0x7e, 0x00, 0x78};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, WritesLittleEndian) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  const std::vector<uint8_t> kExpectedBytes = {
      0x12, 0x00, 0x7f, 0x34, 0x00, 0x7e, 0x56, 0x00, 0x7f, 0x78, 0x00, 0x7e};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, TrimsSamples) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 1;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer),
              IsOk());

  const std::vector<uint8_t> kExpectedBytes = {0x56, 0x00, 0x7f,
                                               0x78, 0x00, 0x7e};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, RequiresBitDepthIsMultipleOfEight) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 23;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;

  EXPECT_FALSE(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                     kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
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

TEST_F(GetCommonSampleRateAndBitDepthTest, DifferentBitDepthResampleTo16Bits) {
  bit_depths_ = {24, 32};
  expected_bit_depth_ = 16;
  expected_requires_resampling_ = true;

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

TEST(ValidateAndGetCommonTrim, ValidForEmptyAudioFrames) {
  constexpr uint32_t kNumSamplesPerFrame = 0;
  const std::list<AudioFrameWithData> kNoAudioFrames = {};

  uint32_t num_samples_to_trim_at_end = 99;
  uint32_t num_samples_to_trim_at_start = 99;
  EXPECT_THAT(ValidateAndGetCommonTrim(kNumSamplesPerFrame, kNoAudioFrames,
                                       num_samples_to_trim_at_end,
                                       num_samples_to_trim_at_start),
              IsOk());
  EXPECT_EQ(num_samples_to_trim_at_end, 0);
  EXPECT_EQ(num_samples_to_trim_at_start, 0);
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

TEST(ValidateAndGetCommonTrim,
     AccumulatesSamplesToTrimAtStartForFullyTrimmedFrames) {
  std::list<AudioFrameWithData> audio_frames;
  const uint32_t kFirstFrameSamplesToTrimAtStart = kFourSamplesPerFrame;
  const uint32_t kSecondFrameSamplesToTrimAtStart = 1;
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kZeroSamplesToTrimAtEnd,
                             kFirstFrameSamplesToTrimAtStart, audio_frames);
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kZeroSamplesToTrimAtEnd,
                             kSecondFrameSamplesToTrimAtStart, audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_THAT(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                       num_samples_to_trim_at_end,
                                       num_samples_to_trim_at_start),
              IsOk());
  EXPECT_EQ(num_samples_to_trim_at_end, kZeroSamplesToTrimAtEnd);
  EXPECT_EQ(num_samples_to_trim_at_start,
            kFirstFrameSamplesToTrimAtStart + kSecondFrameSamplesToTrimAtStart);
}

TEST(ValidateAndGetCommonTrim, FindsCommonTrimBetweenMultipleSubstreams) {
  const DecodedUleb128 kCommonTrimFromStart = 2;
  const DecodedUleb128 kCommonTrimFromEnd = 1;
  const DecodedUleb128 kSecondSubstreamId = 2;
  std::list<AudioFrameWithData> audio_frames;
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kCommonTrimFromEnd, kCommonTrimFromStart,
                             audio_frames);
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kSecondSubstreamId,
                             kCommonTrimFromEnd, kCommonTrimFromStart,
                             audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_TRUE(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                       num_samples_to_trim_at_end,
                                       num_samples_to_trim_at_start)
                  .ok());
  EXPECT_EQ(num_samples_to_trim_at_end, kCommonTrimFromEnd);
  EXPECT_EQ(num_samples_to_trim_at_start, kCommonTrimFromStart);
}

TEST(ValidateAndGetCommonTrim, InvalidWhenSubstreamsHaveNoCommonTrim) {
  const DecodedUleb128 kFirstSubstreamTrim = 0;
  const DecodedUleb128 kMismatchingSecondSubstreamTrim = 1;
  const DecodedUleb128 kSecondSubstreamId = 2;
  std::list<AudioFrameWithData> audio_frames;
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kZeroSamplesToTrimAtEnd, kFirstSubstreamTrim,
                             audio_frames);
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kSecondSubstreamId,
                             kZeroSamplesToTrimAtEnd,
                             kMismatchingSecondSubstreamTrim, audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_FALSE(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                        num_samples_to_trim_at_end,
                                        num_samples_to_trim_at_start)
                   .ok());
}

TEST(ValidateAndGetCommonTrim,
     InvalidWithConsecutivePartialFramesTrimmedFromStart) {
  const DecodedUleb128 kPartiallyTrimmedFrameSamplesToTrimAtStart = 1;
  std::list<AudioFrameWithData> audio_frames;
  AddAudioFrameWithIdAndTrim(
      kFourSamplesPerFrame, kFirstSubstreamId, kZeroSamplesToTrimAtEnd,
      kPartiallyTrimmedFrameSamplesToTrimAtStart, audio_frames);
  AddAudioFrameWithIdAndTrim(
      kFourSamplesPerFrame, kFirstSubstreamId, kZeroSamplesToTrimAtEnd,
      kPartiallyTrimmedFrameSamplesToTrimAtStart, audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_FALSE(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                        num_samples_to_trim_at_end,
                                        num_samples_to_trim_at_start)
                   .ok());
}

TEST(ValidateAndGetCommonTrim,
     InvalidWhenFramesOccurAfterSamplesTrimmedFromEnd) {
  const uint32_t kFirstFramePartialTrimFromEnd = 1;
  std::list<AudioFrameWithData> audio_frames;
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kFirstFramePartialTrimFromEnd,
                             kZeroSamplesToTrimAtStart, audio_frames);
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kZeroSamplesToTrimAtEnd, kZeroSamplesToTrimAtStart,
                             audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_FALSE(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                        num_samples_to_trim_at_end,
                                        num_samples_to_trim_at_start)
                   .ok());
}

TEST(ValidateAndGetCommonTrim,
     InvalidWhenCumulativeTrimIsGreaterThanNumSamplesPerFrame) {
  std::list<AudioFrameWithData> audio_frames;
  const uint32_t kNumSamplesToTrimAtEnd = kFourSamplesPerFrame - 1;
  const uint32_t kNumSamplesToTrimAtStart = 2;
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kNumSamplesToTrimAtEnd, kNumSamplesToTrimAtStart,
                             audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_FALSE(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                        num_samples_to_trim_at_end,
                                        num_samples_to_trim_at_start)
                   .ok());
}

TEST(ValidateAndGetCommonTrim, InvalidWithFullyTrimmedSamplesFromEnd) {
  const uint32_t kFullyTrimmedSamplesFromEnd = kFourSamplesPerFrame;
  std::list<AudioFrameWithData> audio_frames;
  AddAudioFrameWithIdAndTrim(kFourSamplesPerFrame, kFirstSubstreamId,
                             kFullyTrimmedSamplesFromEnd,
                             kZeroSamplesToTrimAtStart, audio_frames);

  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
  EXPECT_FALSE(ValidateAndGetCommonTrim(kFourSamplesPerFrame, audio_frames,
                                        num_samples_to_trim_at_end,
                                        num_samples_to_trim_at_start)
                   .ok());
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

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> result;
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

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> result;
  EXPECT_FALSE(CollectAndValidateParamDefinitions(audio_elements,
                                                  mix_presentation_obus, result)
                   .ok());
}

TEST(CollectAndValidateParamDefinitions,
     IsInvalidWhenMixGainParamDefinitionIsPresentInAudioElement) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, input_codec_configs);
  const std::list<MixPresentationObu> kNoMixPresentationObus = {};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, kZerothOrderAmbisonicsSubstreamId,
      input_codec_configs, audio_elements);
  auto& audio_element = audio_elements.at(kAudioElementId);
  audio_element.obu.InitializeParams(1);
  audio_element.obu.audio_element_params_[0] = AudioElementParam{
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition = std::make_unique<MixGainParamDefinition>()};

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> result;
  EXPECT_FALSE(CollectAndValidateParamDefinitions(
                   audio_elements, kNoMixPresentationObus, result)
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
  audio_element.obu.audio_element_params_[0] = AudioElementParam{
      .param_definition_type =
          ParamDefinition::kParameterDefinitionReservedStart,
      .param_definition = std::make_unique<ExtendedParamDefinition>(
          ParamDefinition::kParameterDefinitionReservedStart)};

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> result;
  EXPECT_THAT(CollectAndValidateParamDefinitions(
                  audio_elements, kNoMixPresentationObus, result),
              IsOk());
  EXPECT_TRUE(result.empty());
}

TEST(GenerateParamIdToMetadataMapTest, MixGainParamDefinition) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data = {};
  auto param_definition = MixGainParamDefinition();
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = kParameterRate;
  param_definition.param_definition_mode_ = 1;
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  param_definitions[kParameterId] = &param_definition;
  auto param_id_to_metadata_map =
      GenerateParamIdToMetadataMap(param_definitions, audio_elements_with_data);
  EXPECT_THAT(param_id_to_metadata_map, IsOk());
  EXPECT_EQ(param_id_to_metadata_map->size(), 1);
  auto iter = param_id_to_metadata_map->find(kParameterId);
  EXPECT_NE(iter, param_id_to_metadata_map->end());
  EXPECT_EQ(iter->second.param_definition_type,
            ParamDefinition::kParameterDefinitionMixGain);
  EXPECT_EQ(iter->second.param_definition, param_definition);
}

TEST(GenerateParamIdToMetadataMapTest, ReconGainParamDefinition) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, input_codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AudioElementObu obu(ObuHeader(), kAudioElementId,
                      AudioElementObu::kAudioElementChannelBased, 0,
                      kCodecConfigId);
  obu.audio_substream_ids_ = {kFirstSubstreamId, kSecondSubstreamId};
  obu.num_substreams_ = 2;
  obu.InitializeParams(0);
  EXPECT_THAT(obu.InitializeScalableChannelLayout(2, 0), IsOk());

  auto& two_layer_stereo_config =
      std::get<ScalableChannelLayoutConfig>(obu.config_);
  two_layer_stereo_config.channel_audio_layer_configs.clear();
  const ChannelAudioLayerConfig mono_layer = {
      .loudspeaker_layout =
          ChannelAudioLayerConfig::LoudspeakerLayout::kLayoutMono,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = true,
      .substream_count = 1,
      .coupled_substream_count = 0};
  two_layer_stereo_config.channel_audio_layer_configs.push_back(mono_layer);
  const ChannelAudioLayerConfig stereo_layer = {
      .loudspeaker_layout =
          ChannelAudioLayerConfig::LoudspeakerLayout::kLayoutStereo,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = true,
      .substream_count = 1,
      .coupled_substream_count = 0};
  two_layer_stereo_config.channel_audio_layer_configs.push_back(stereo_layer);
  SubstreamIdLabelsMap substream_id_labels_map;
  LabelGainMap label_gain_map;
  std::vector<ChannelNumbers> channel_numbers;
  ASSERT_THAT(ObuWithDataGenerator::FinalizeScalableChannelLayoutConfig(
                  obu.audio_substream_ids_, two_layer_stereo_config,
                  substream_id_labels_map, label_gain_map, channel_numbers),
              IsOk());

  auto iter = input_codec_configs.find(kCodecConfigId);
  audio_elements_with_data.insert(
      {kAudioElementId, AudioElementWithData{std::move(obu), &iter->second,
                                             substream_id_labels_map,
                                             label_gain_map, channel_numbers}});

  auto param_definition = ReconGainParamDefinition(kAudioElementId);
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = kParameterRate;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = 1;
  param_definition.constant_subblock_duration_ = 0;
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  param_definitions[kParameterId] = &param_definition;
  auto param_id_to_metadata_map =
      GenerateParamIdToMetadataMap(param_definitions, audio_elements_with_data);
  EXPECT_THAT(param_id_to_metadata_map, IsOk());
  EXPECT_EQ(param_id_to_metadata_map->size(), 1);
  auto param_iter = param_id_to_metadata_map->find(kParameterId);
  EXPECT_NE(param_iter, param_id_to_metadata_map->end());
  EXPECT_EQ(param_iter->second.param_definition_type,
            ParamDefinition::kParameterDefinitionReconGain);
  EXPECT_EQ(param_iter->second.param_definition, param_definition);
  EXPECT_EQ(param_iter->second.audio_element_id, kAudioElementId);
  EXPECT_EQ(param_iter->second.num_layers, 2);
  constexpr ChannelNumbers expected_channel_numbers_mono_layer = {.surround =
                                                                      1};
  constexpr ChannelNumbers expected_channel_numbers_stereo_layer = {.surround =
                                                                        2};
  EXPECT_EQ(param_iter->second.channel_numbers_for_layers[0],
            expected_channel_numbers_mono_layer);
  EXPECT_EQ(param_iter->second.channel_numbers_for_layers[1],
            expected_channel_numbers_stereo_layer);
  EXPECT_EQ(param_iter->second.recon_gain_is_present_flags[0], true);
  EXPECT_EQ(param_iter->second.recon_gain_is_present_flags[1], true);
}

TEST(GenerateParamIdToMetadataMapTest,
     RejectReconGainParamDefinitionNotInAudioElement) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kCodecConfigId, input_codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kMono, kAudioElementId, kCodecConfigId,
      {kFirstSubstreamId}, input_codec_configs, audio_elements_with_data);

  auto param_definition = ReconGainParamDefinition(kSecondAudioElementId);
  param_definition.parameter_id_ = kParameterId;
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  param_definitions[kParameterId] = &param_definition;
  auto param_id_to_metadata_map =
      GenerateParamIdToMetadataMap(param_definitions, audio_elements_with_data);
  EXPECT_FALSE(param_id_to_metadata_map.ok());
}

TEST(IsStereoLayout, ReturnsTrueForStereoLayout) {
  Layout playback_layout = {
      .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersSsConventionLayout{
          .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          .reserved = 0}};
  EXPECT_TRUE(IsStereoLayout(playback_layout));
}

TEST(IsStereoLayout, ReturnsFalseForNonStereoLayout) {
  Layout playback_layout = {.layout_type = Layout::kLayoutTypeBinaural};
  EXPECT_FALSE(IsStereoLayout(playback_layout));
}

TEST(IsStereoLayout, ReturnsFalseForInvalidLayout) {
  Layout playback_layout = {
      .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersReservedOrBinauralLayout{}};
  EXPECT_FALSE(IsStereoLayout(playback_layout));
}

TEST(GetIndicesForLayout, SuccessWithStereoLayout) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements = {};
  // Create a mix presentation OBU; by default, it's created with a stereo
  // layout in the first submix.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      mix_presentation_obus);
  Layout playback_layout = {
      .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
      .specific_layout = LoudspeakersSsConventionLayout{
          .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          .reserved = 0}};
  // Set to non-default values to ensure they are returned correctly.
  int submix_index = 2;
  int layout_index = 2;
  auto layout_info =
      GetIndicesForLayout(mix_presentation_obus.back().sub_mixes_,
                          playback_layout, submix_index, layout_index);
  EXPECT_THAT(layout_info, IsOk());
  EXPECT_EQ(submix_index, 0);
  EXPECT_EQ(layout_index, 0);
}

TEST(GetIndicesForLayout, FailsWithMismatchedLayout) {
  // Initialize prerequisites.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements = {};
  // Create a mix presentation OBU; by default, it's created with a stereo
  // layout in the first submix.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, {kAudioElementId}, kParameterId, kParameterRate,
      mix_presentation_obus);
  Layout playback_layout = {.layout_type = Layout::kLayoutTypeBinaural};
  int submix_index;
  int layout_index;
  auto layout_info =
      GetIndicesForLayout(mix_presentation_obus.back().sub_mixes_,
                          playback_layout, submix_index, layout_index);
  EXPECT_THAT(layout_info, testing::Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
