/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "iamf/cli/temporal_unit_view.h"

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using ::testing::NotNull;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint32_t kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kSecondAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 1;
constexpr DecodedUleb128 kSecondSubstreamId = 2;
constexpr InternalTimestamp kFirstTimestamp = 0;
constexpr InternalTimestamp kSecondTimestamp = 8;
constexpr InternalTimestamp kFirstAudioFrameStartTimestamp = 0;
constexpr InternalTimestamp kFirstAudioFrameEndTimestamp = 8;
constexpr DecodedUleb128 kFirstParameterId = 998;
constexpr std::nullopt_t kOriginalSamplesAreIrrelevant = std::nullopt;
constexpr std::nullopt_t kNoInsertionTick = std::nullopt;
constexpr bool kInvalidatesBitstream = true;
constexpr bool kDoesNotInvalidateBitstream = false;

constexpr absl::Span<ParameterBlockWithData> kNoParameterBlocks = {};
constexpr absl::Span<AudioFrameWithData> kNoAudioFrames = {};
constexpr absl::Span<ArbitraryObu> kNoArbitraryObus = {};

constexpr absl::Span<const ParameterBlockWithData*> kNoParameterBlockPtrs = {};
constexpr absl::Span<const ArbitraryObu*> kNoArbitraryObuPtrs = {};

void InitializePrerequisiteObusForOneSubstream(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
}

void AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
    uint32_t audio_element_id, uint32_t substream_id,
    InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames) {
  ASSERT_TRUE(audio_elements.contains(audio_element_id));

  audio_frames.emplace_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), substream_id, {}),
      .start_timestamp = start_timestamp,
      .end_timestamp = end_timestamp,
      .encoded_samples = kOriginalSamplesAreIrrelevant,
      .down_mixing_params = {.in_bitstream = false},
      .audio_element_with_data = &audio_elements.at(audio_element_id)});
}

MixGainParamDefinition CreateDemixingParamDefinition(
    const DecodedUleb128 parameter_id) {
  MixGainParamDefinition mix_gain_param_definition;
  mix_gain_param_definition.parameter_id_ = parameter_id;
  mix_gain_param_definition.parameter_rate_ = 48000;
  mix_gain_param_definition.param_definition_mode_ = 0;
  mix_gain_param_definition.duration_ = 8;
  mix_gain_param_definition.constant_subblock_duration_ = 8;

  return mix_gain_param_definition;
}

void AddMixGainParameterBlock(
    const MixGainParamDefinition& param_definition,
    InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
    std::list<ParameterBlockWithData>& parameter_blocks) {
  auto data = std::make_unique<MixGainParameterData>();
  data->animation_type = MixGainParameterData::kAnimateStep;
  data->param_data = AnimationStepInt16{.start_point_value = 1};
  auto parameter_block = ParameterBlockObu::CreateMode0(
      ObuHeader(), param_definition.parameter_id_, param_definition);
  ASSERT_THAT(parameter_block, NotNull());
  parameter_block->subblocks_[0].param_data = std::move(data);
  parameter_blocks.emplace_back(ParameterBlockWithData{
      .obu = std::move(parameter_block),
      .start_timestamp = start_timestamp,
      .end_timestamp = end_timestamp,
  });
}

void InitializeOneFrameIaSequence(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);
}

void InitializePrerequsiteObusForTwoSubstreams(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus, audio_elements);
}

TEST(Create, PopulatesMemberVariablesWithOneAudioFrame) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->audio_frames_.size(), 1);
  EXPECT_TRUE(temporal_unit->parameter_blocks_.empty());
  EXPECT_TRUE(temporal_unit->arbitrary_obus_.empty());
  EXPECT_EQ(temporal_unit->num_untrimmed_samples_, kNumSamplesPerFrame);
}

TEST(Create, PopulatesMemberVariablesWithOneParameterBlock) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  const auto mix_gain_param_definition =
      CreateDemixingParamDefinition(kFirstParameterId);
  AddMixGainParameterBlock(mix_gain_param_definition, kFirstTimestamp,
                           kSecondTimestamp, parameter_blocks);

  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->parameter_blocks_.size(), 1);
}

TEST(Create, OrderingByAscendingParameterId) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  constexpr DecodedUleb128 kHighParameterId = 999;
  constexpr DecodedUleb128 kLowParameterId = 998;
  const auto high_id_param_definition =
      CreateDemixingParamDefinition(kHighParameterId);
  AddMixGainParameterBlock(high_id_param_definition, kFirstTimestamp,
                           kSecondTimestamp, parameter_blocks);
  const ParameterBlockWithData* high_id_parameter_block =
      &parameter_blocks.back();
  const auto low_id_param_definition =
      CreateDemixingParamDefinition(kLowParameterId);
  AddMixGainParameterBlock(low_id_param_definition, kFirstTimestamp,
                           kSecondTimestamp, parameter_blocks);
  const ParameterBlockWithData* low_id_parameter_block =
      &parameter_blocks.back();
  const std::vector<const ParameterBlockWithData*> kExpectedOrder = {
      low_id_parameter_block, high_id_parameter_block};

  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->parameter_blocks_, kExpectedOrder);
}

TEST(CompareAudioElementIdAudioSubstreamId,
     OrdersByAudioElementIdThenSubstreamId) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  constexpr uint32_t kFirstAudioElementId = 1;
  constexpr uint32_t kSecondAudioElementId = 10;
  constexpr uint32_t kFirstSubstreamId = 500;
  constexpr uint32_t kSecondSubstreamId = 250;
  constexpr uint32_t kThirdSubstreamId = 750;
  constexpr uint32_t kFourthSubstreamId = 999;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId,
      {kSecondSubstreamId, kThirdSubstreamId, kFourthSubstreamId},
      codec_config_obus, audio_elements);

  // Add the audio frames in a non-canonical order.
  std::list<AudioFrameWithData> audio_frames;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kSecondAudioElementId, kThirdSubstreamId, kFirstAudioFrameStartTimestamp,
      kFirstAudioFrameEndTimestamp, audio_elements, audio_frames);
  const AudioFrameWithData* third_audio_frame_after_sort = &audio_frames.back();
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kSecondAudioElementId, kFourthSubstreamId, kFirstAudioFrameStartTimestamp,
      kFirstAudioFrameEndTimestamp, audio_elements, audio_frames);
  const AudioFrameWithData* fourth_audio_frame_after_sort =
      &audio_frames.back();
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kFirstAudioFrameStartTimestamp,
      kFirstAudioFrameEndTimestamp, audio_elements, audio_frames);
  const AudioFrameWithData* first_audio_frame_after_sort = &audio_frames.back();
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kSecondAudioElementId, kSecondSubstreamId, kFirstAudioFrameStartTimestamp,
      kFirstAudioFrameEndTimestamp, audio_elements, audio_frames);
  const AudioFrameWithData* second_audio_frame_after_sort =
      &audio_frames.back();

  // The view will be based a "canonical" (but not necessarily IAMF-required)
  // order.
  const std::vector<const AudioFrameWithData*> kExpectedOrder = {
      first_audio_frame_after_sort, second_audio_frame_after_sort,
      third_audio_frame_after_sort, fourth_audio_frame_after_sort};

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->audio_frames_, kExpectedOrder);
}

TEST(Create, MaintainsArbitraryObusInInputOrder) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  std::list<ArbitraryObu> arbitrary_obus;
  const auto& first_abitrary_obu = arbitrary_obus.emplace_back(ArbitraryObu(
      kObuIaReserved25, ObuHeader(), {},
      ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick, kFirstTimestamp));
  const auto& second_abitrary_obu = arbitrary_obus.emplace_back(ArbitraryObu(
      kObuIaReserved25, ObuHeader(), {},
      ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick, kFirstTimestamp));
  const std::vector<const ArbitraryObu*> arbitrary_obus_ptrs = {
      &first_abitrary_obu, &second_abitrary_obu};

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, arbitrary_obus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->arbitrary_obus_, arbitrary_obus_ptrs);
}

TEST(Create, SetsStartTimestamp) {
  constexpr InternalTimestamp kExpectedStartTimestamp = 123456789;
  constexpr InternalTimestamp kEndTimestamp = kExpectedStartTimestamp + 8;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  audio_frames.front().start_timestamp = kExpectedStartTimestamp;
  audio_frames.front().end_timestamp = kEndTimestamp;

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->start_timestamp_, kExpectedStartTimestamp);
}

TEST(Create, SetsEndTimestamp) {
  constexpr InternalTimestamp kStartTimestamp = 123456789;
  constexpr InternalTimestamp kExpectedEndTimestamp = kStartTimestamp + 8;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  audio_frames.front().start_timestamp = kStartTimestamp;
  audio_frames.front().end_timestamp = kExpectedEndTimestamp;

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->end_timestamp_, kExpectedEndTimestamp);
}

TEST(Create, SetsNumSamplesToTrimAtStart) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  const uint32_t kExpectedNumSamplesToTrimAtStart = 4;
  audio_frames.front().obu.header_ = ObuHeader{
      .num_samples_to_trim_at_start = kExpectedNumSamplesToTrimAtStart};

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->num_samples_to_trim_at_start_,
            kExpectedNumSamplesToTrimAtStart);
}

TEST(Create, SetsNumUntrimmedSamplesToZeroForFullyTrimmedAudioFrame) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  // Ok. Fully trimmed frames are allowed. They are common in codecs like
  // AAC-LC.
  audio_frames.front().obu.header_ =
      ObuHeader{.num_samples_to_trim_at_end = kNumSamplesPerFrame,
                .num_samples_to_trim_at_start = 0};

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->num_untrimmed_samples_, 0);
}

TEST(Create, SetsNumUntrimmedSamples) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  audio_frames.front().obu.header_ = ObuHeader{
      .num_samples_to_trim_at_end = 2, .num_samples_to_trim_at_start = 4};
  // There are 8 samples in the frame, but a total of 5 (2+4) are trimmed. We
  // expect the number of untrimmed samples to be 2.
  const uint32_t kExpectedNumUntrimmedSamples = 2;

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_EQ(temporal_unit->num_untrimmed_samples_,
            kExpectedNumUntrimmedSamples);
}

TEST(Create, FailsWithNoAudioFramesAndNoArbitraryObus) {
  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, kNoAudioFrames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, SucceedsWithNoAudioFramesIfArbitraryObusArePresent) {
  // To support files in the test suite, we allow arbitrary OBUs to be present
  // in the absence of an audio frame. As long as one of the arbitrary OBUs
  // invalidates the bitstream.
  constexpr InternalTimestamp kInsertionTick = 123456789;
  const std::vector<ArbitraryObu> arbitrary_obus = {
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                   kInsertionTick, kInvalidatesBitstream)};

  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, kNoAudioFrames, arbitrary_obus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_TRUE(temporal_unit->audio_frames_.empty());
  EXPECT_TRUE(temporal_unit->parameter_blocks_.empty());
  EXPECT_EQ(arbitrary_obus.size(), temporal_unit->arbitrary_obus_.size());
  EXPECT_EQ(temporal_unit->start_timestamp_, kInsertionTick);
  EXPECT_EQ(temporal_unit->end_timestamp_, kInsertionTick);
  EXPECT_EQ(temporal_unit->num_samples_to_trim_at_start_, 0);
  EXPECT_EQ(temporal_unit->num_untrimmed_samples_, 0);
}

TEST(Create, FailsWithNoAudioFramesIfNoArbitraryInvalidesTheBitstream) {
  constexpr InternalTimestamp kInsertionTick = 123456789;
  const std::vector<ArbitraryObu> arbitrary_obus = {
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                   kInsertionTick, kDoesNotInvalidateBitstream)};

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, kNoAudioFrames,
                                       arbitrary_obus),
              Not(IsOk()));
}

TEST(Create, FailsWithNoAudioFramesAndArbitraryObusWithNoInsertionTick) {
  const std::vector<ArbitraryObu> arbitrary_obus = {
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                   kNoInsertionTick, kInvalidatesBitstream)};

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, kNoAudioFrames,
                                       arbitrary_obus),
              Not(IsOk()));
}

TEST(Create,
     FailsWithNoAudioFramesAndArbitraryObusHaveMismatchingInsertionTicks) {
  const std::vector<ArbitraryObu> arbitrary_obus = {
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                   kFirstTimestamp, kInvalidatesBitstream),
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                   kNoInsertionTick, kInvalidatesBitstream)};

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, kNoAudioFrames,
                                       arbitrary_obus),
              Not(IsOk()));
}

TEST(CreateFromPointers, FailsIfAudioFramesContainNullPtrs) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  InitializePrerequisiteObusForOneSubstream(codec_config_obus, audio_elements);
  constexpr std::array<const AudioFrameWithData*, 1> kNullAudioFramePtr = {
      nullptr};

  EXPECT_THAT(TemporalUnitView::CreateFromPointers(
                  kNoParameterBlockPtrs,
                  absl::MakeConstSpan(kNullAudioFramePtr), kNoArbitraryObuPtrs),
              Not(IsOk()));
}

TEST(CreateFromPointers, FailsIfParameterBlocksContainNullPtrs) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  const std::vector<const AudioFrameWithData*> audio_frames_ptrs = {
      &audio_frames.back()};
  const std::array<const ParameterBlockWithData*, 1> kNullParameterBlockPtr = {
      nullptr};

  EXPECT_THAT(
      TemporalUnitView::CreateFromPointers(
          kNullParameterBlockPtr, audio_frames_ptrs, kNoArbitraryObuPtrs),
      Not(IsOk()));
}

TEST(CreateFromPointers, FailsIfArbitraryObusContainNullPtrs) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  const std::vector<const AudioFrameWithData*> audio_frames_ptrs = {
      &audio_frames.back()};
  const std::array<const ArbitraryObu*, 1> kNullArbitraryObuPtr = {nullptr};

  EXPECT_THAT(
      TemporalUnitView::CreateFromPointers(
          kNoParameterBlockPtrs, audio_frames_ptrs, kNullArbitraryObuPtr),
      Not(IsOk()));
}

TEST(Create, ReturnsErrorIfAudioElementWithDataIsNullptr) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializePrerequisiteObusForOneSubstream(codec_config_obus, audio_elements);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);
  // Corrupt the audio frame by disassociating the audio element.
  audio_frames.back().audio_element_with_data = nullptr;

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfCodecConfigIsNullptr) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializePrerequisiteObusForOneSubstream(codec_config_obus, audio_elements);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);
  // Corrupt the audio element by disassociating the codec config.
  audio_elements.at(kFirstAudioElementId).codec_config = nullptr;

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfTrimmingIsImplausible) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  // Corrupt the audio frame. Trim cannot be greater than the total number of
  // samples in the frame.
  audio_frames.front().obu.header_ =
      ObuHeader{.num_samples_to_trim_at_end = kNumSamplesPerFrame,
                .num_samples_to_trim_at_start = 1};

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfSubstreamIdsAreRepeated) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  constexpr DecodedUleb128 kRepeatedSubstreamId = kFirstSubstreamId;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kRepeatedSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfTrimmingIsInconsistent) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializePrerequsiteObusForTwoSubstreams(codec_config_obus, audio_elements);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);
  audio_frames.back().obu.header_ = ObuHeader{
      .num_samples_to_trim_at_end = 1, .num_samples_to_trim_at_start = 1};
  // Add a new frame. It has trimming information inconsistent with the first
  // frame.
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kSecondSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);
  audio_frames.back().obu.header_ = ObuHeader{
      .num_samples_to_trim_at_end = 2, .num_samples_to_trim_at_start = 1};

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfAudioFrameTimestampsAreInconsistent) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  InitializePrerequsiteObusForTwoSubstreams(codec_config_obus, audio_elements);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kFirstTimestamp,
      kSecondTimestamp, audio_elements, audio_frames);
  constexpr InternalTimestamp kInconsistentTimestamp = kSecondTimestamp + 1;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kSecondSubstreamId, kFirstTimestamp,
      kInconsistentTimestamp, audio_elements, audio_frames);

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfParameterBlockTimestampsAreInconsistent) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  const auto mix_gain_param_definition =
      CreateDemixingParamDefinition(kFirstParameterId);
  constexpr InternalTimestamp kInconsistentTimestamp = kSecondTimestamp + 1;
  AddMixGainParameterBlock(mix_gain_param_definition, kFirstTimestamp,
                           kInconsistentTimestamp, parameter_blocks);

  EXPECT_THAT(TemporalUnitView::Create(parameter_blocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfParameterBlockIdsAreRepeated) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  const auto mix_gain_param_definition =
      CreateDemixingParamDefinition(kFirstParameterId);
  AddMixGainParameterBlock(mix_gain_param_definition, kFirstTimestamp,
                           kSecondTimestamp, parameter_blocks);
  AddMixGainParameterBlock(mix_gain_param_definition, kFirstTimestamp,
                           kSecondTimestamp, parameter_blocks);

  EXPECT_THAT(TemporalUnitView::Create(parameter_blocks, audio_frames,
                                       kNoArbitraryObus),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorIfArbitraryObuTimestampsAreInconsistent) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ArbitraryObu> arbitrary_obus;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  InternalTimestamp kInconsistentTimestamp = kFirstTimestamp + 1;
  arbitrary_obus.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                   kInconsistentTimestamp));

  EXPECT_THAT(TemporalUnitView::Create(kNoParameterBlocks, audio_frames,
                                       arbitrary_obus),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
