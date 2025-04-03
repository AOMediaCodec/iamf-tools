/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/obu_sequencer_base.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::_;
using ::testing::Not;
using ::testing::Return;

using absl::MakeConstSpan;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint32_t kSampleRate = 48000;
// Some timestamps consistent with the number of samples per frame.
constexpr InternalTimestamp kFirstTimestamp = kNumSamplesPerFrame * 0;
constexpr InternalTimestamp kSecondTimestamp = kNumSamplesPerFrame * 1;
constexpr InternalTimestamp kThirdTimestamp = kNumSamplesPerFrame * 2;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kSecondAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 1;
constexpr DecodedUleb128 kSecondSubstreamId = 2;
constexpr DecodedUleb128 kFirstMixPresentationId = 100;
constexpr DecodedUleb128 kFirstDemixingParameterId = 998;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr uint32_t kCommonMixGainParameterRate = kSampleRate;

constexpr bool kIncludeTemporalDelimiters = true;
constexpr bool kDoNotIncludeTemporalDelimiters = false;

constexpr bool kDelayDescriptorsUntilTrimAtStartIsKnown = true;
constexpr bool kDoNotDelayDescriptorsUntilTrimAtStartIsKnown = false;

constexpr std::nullopt_t kOriginalSamplesAreIrrelevant = std::nullopt;

constexpr absl::Span<ParameterBlockWithData> kNoParameterBlocks = {};
constexpr absl::Span<ArbitraryObu> kNoArbitraryObus = {};

void InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
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
      .pcm_samples = kOriginalSamplesAreIrrelevant,
      .down_mixing_params = {.in_bitstream = false},
      .audio_element_with_data = &audio_elements.at(audio_element_id)});
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

void InitializeOneFrameIaSequenceWithMixPresentation(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus,
    std::list<AudioFrameWithData>& audio_frames) {
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);

  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, 0, 8, audio_elements,
      audio_frames);
}

void InitializeOneParameterBlockAndOneAudioFrame(
    DemixingParamDefinition& param_definition,
    std::list<ParameterBlockWithData>& parameter_blocks,
    std::list<AudioFrameWithData>& audio_frames,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements) {
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  auto data = std::make_unique<DemixingInfoParameterData>();
  data->dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  data->reserved = 0;
  auto parameter_block = std::make_unique<ParameterBlockObu>(
      ObuHeader(), param_definition.parameter_id_, param_definition);
  ASSERT_THAT(parameter_block->InitializeSubblocks(), IsOk());
  parameter_block->subblocks_[0].param_data = std::move(data);
  parameter_blocks.emplace_back(ParameterBlockWithData{
      .obu = std::move(parameter_block),
      .start_timestamp = kFirstTimestamp,
      .end_timestamp = kSecondTimestamp,
  });
}
void InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus, audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId, kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
}

DemixingParamDefinition CreateDemixingParamDefinition(
    const DecodedUleb128 parameter_id) {
  DemixingParamDefinition demixing_param_definition;
  demixing_param_definition.parameter_id_ = parameter_id;
  demixing_param_definition.parameter_rate_ = 48000;
  demixing_param_definition.param_definition_mode_ = 0;
  demixing_param_definition.duration_ = 8;
  demixing_param_definition.constant_subblock_duration_ = 8;
  demixing_param_definition.reserved_ = 10;

  return demixing_param_definition;
}

void ExpectPushedTemporalUnitMatchesExpectedSequence(
    const TemporalUnitView& temporal_unit,
    const std::list<const ObuBase*>& expected_sequence,
    MockObuSequencer& mock_obu_sequencer) {
  const std::vector<uint8_t> expected_serialized_temporal_unit =
      SerializeObusExpectOk(expected_sequence);
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(
                  _, _, MakeConstSpan(expected_serialized_temporal_unit)));

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(temporal_unit), IsOk());
}

TEST(PushTemporalUnit, SerializesArbitraryObuBeforeParameterBlocksAtTime) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  // Create a temporal unit with an arbitrary OBU before the parameter blocks.
  const std::list<ArbitraryObu> kArbitraryObuBeforeParameterBlocks(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookBeforeParameterBlocksAtTick,
                    kFirstTimestamp)});
  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kArbitraryObuBeforeParameterBlocks);
  ASSERT_THAT(temporal_unit, IsOk());
  const TemporalDelimiterObu temporal_delimiter_obu(ObuHeader{});

  ExpectPushedTemporalUnitMatchesExpectedSequence(
      *temporal_unit,
      {&temporal_delimiter_obu, &kArbitraryObuBeforeParameterBlocks.front(),
       parameter_blocks.front().obu.get(), &audio_frames.front().obu},
      mock_obu_sequencer);
}

TEST(PushTemporalUnit, SerializesArbitraryObuAfterParameterBlocksAtTime) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  // Create a temporal unit with an arbitrary OBU after the parameter blocks.
  const std::list<ArbitraryObu> kArbitraryObuAfterParameterBlocks(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                    kFirstTimestamp)});
  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kArbitraryObuAfterParameterBlocks);
  ASSERT_THAT(temporal_unit, IsOk());

  ExpectPushedTemporalUnitMatchesExpectedSequence(
      *temporal_unit,
      {parameter_blocks.front().obu.get(),
       &kArbitraryObuAfterParameterBlocks.front(), &audio_frames.front().obu},
      mock_obu_sequencer);
}

TEST(PushTemporalUnit, SerializesArbitraryObuAfterAudioFramesAtTime) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  // Create a temporal unit with an arbitrary OBU after the audio frames.
  const std::list<ArbitraryObu> kArbitraryObuAfterAudioFrames({ArbitraryObu(
      kObuIaReserved25, ObuHeader(), {},
      ArbitraryObu::kInsertionHookAfterAudioFramesAtTick, kFirstTimestamp)});
  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kArbitraryObuAfterAudioFrames);
  ASSERT_THAT(temporal_unit, IsOk());

  ExpectPushedTemporalUnitMatchesExpectedSequence(
      *temporal_unit,
      {parameter_blocks.front().obu.get(), &audio_frames.front().obu,
       &kArbitraryObuAfterAudioFrames.front()},
      mock_obu_sequencer);
}

TEST(PushTemporalUnit, PassesZeroSamplesForFullyTrimmedAudioFrame) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  // Make a temporal unit with an audio frame that is fully trimmed.
  audio_frames.front().obu.header_.num_samples_to_trim_at_end = 0;
  audio_frames.front().obu.header_.num_samples_to_trim_at_start = 8;
  constexpr uint32_t kNumUntrimmedSamples = 0;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(_, kNumUntrimmedSamples, _));
  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), IsOk());
}

TEST(PushTemporalUnit, PAssesNumberOfUntrimmedSamplesToNumSamples) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  InitializeOneFrameIaSequence(codec_config_obus, audio_elements, audio_frames);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  // Make a temporal unit with an audio frame that is fully trimmed.
  audio_frames.front().obu.header_.num_samples_to_trim_at_end = 2;
  audio_frames.front().obu.header_.num_samples_to_trim_at_start = 1;
  constexpr uint32_t kNumUntrimmedSamples = kNumSamplesPerFrame - 1 - 2;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(_, kNumUntrimmedSamples, _));
  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), IsOk());
}

TEST(WriteTemporalUnit, WritesTemporalDelimiterObuWhenEnabled) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  // Configure with temporal delimiters.
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  const TemporalDelimiterObu kTemporalDelimiterObu(ObuHeader{});
  ExpectPushedTemporalUnitMatchesExpectedSequence(
      *temporal_unit,
      {&kTemporalDelimiterObu, parameter_blocks.front().obu.get(),
       &audio_frames.front().obu},
      mock_obu_sequencer);
}

TEST(WriteTemporalUnit, OmitsTemporalDelimiterObuWhenDisabled) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<ParameterBlockWithData> parameter_blocks;
  std::list<AudioFrameWithData> audio_frames;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoDescriptorArbitraryObus;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  // Configure without temporal delimiters.
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  kNoMixPresentationObus, kNoDescriptorArbitraryObus),
              IsOk());

  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());

  ExpectPushedTemporalUnitMatchesExpectedSequence(
      *temporal_unit,
      {parameter_blocks.front().obu.get(), &audio_frames.front().obu},
      mock_obu_sequencer);
}

class ObuSequencerTest : public ::testing::Test {
 public:
  void InitializeDescriptorObus() {
    ia_sequence_header_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                                    ProfileVersion::kIamfSimpleProfile,
                                    ProfileVersion::kIamfSimpleProfile);
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus_);
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
        codec_config_obus_, audio_elements_);
    AddMixPresentationObuWithAudioElementIds(
        kFirstMixPresentationId, {kFirstAudioElementId},
        kCommonMixGainParameterId, kCommonMixGainParameterRate,
        mix_presentation_obus_);

    ASSERT_TRUE(ia_sequence_header_obu_.has_value());
    ASSERT_TRUE(codec_config_obus_.contains(kCodecConfigId));
    ASSERT_TRUE(audio_elements_.contains(kFirstAudioElementId));
    ASSERT_FALSE(mix_presentation_obus_.empty());
  }

  void InitObusForOneFrameIaSequence() {
    ia_sequence_header_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                                    ProfileVersion::kIamfSimpleProfile,
                                    ProfileVersion::kIamfSimpleProfile);
    param_definition_ =
        CreateDemixingParamDefinition(kFirstDemixingParameterId);
    InitializeOneParameterBlockAndOneAudioFrame(
        param_definition_, parameter_blocks_, audio_frames_, codec_config_obus_,
        audio_elements_);
    AddMixPresentationObuWithAudioElementIds(
        kFirstMixPresentationId, {audio_elements_.begin()->first},
        kCommonMixGainParameterId, kCommonMixGainParameterRate,
        mix_presentation_obus_);
  }

  void ValidateWriteDescriptorObuSequence(
      const std::list<const ObuBase*>& expected_sequence) {
    WriteBitBuffer expected_wb(128);
    for (const auto* expected_obu : expected_sequence) {
      ASSERT_NE(expected_obu, nullptr);
      EXPECT_THAT(expected_obu->ValidateAndWriteObu(expected_wb), IsOk());
    }

    WriteBitBuffer result_wb(128);
    EXPECT_THAT(ObuSequencerBase::WriteDescriptorObus(
                    ia_sequence_header_obu_.value(), codec_config_obus_,
                    audio_elements_, mix_presentation_obus_, arbitrary_obus_,
                    result_wb),
                IsOk());

    EXPECT_EQ(result_wb.bit_buffer(), expected_wb.bit_buffer());
  }

 protected:
  std::optional<IASequenceHeaderObu> ia_sequence_header_obu_;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements_;
  std::list<MixPresentationObu> mix_presentation_obus_;

  DemixingParamDefinition param_definition_;
  std::list<ParameterBlockWithData> parameter_blocks_;
  std::list<AudioFrameWithData> audio_frames_;

  std::list<ArbitraryObu> arbitrary_obus_;
};

TEST_F(ObuSequencerTest, OrdersByAParticularObuType) {
  InitializeDescriptorObus();
  // The IAMF spec REQUIRES descriptor OBUs to be ordered by `obu_type` in a
  // particular order (i.e. IA Sequence Header, Codec Config Audio Element, Mix
  // Presentation).
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, ArbitraryObuAfterIaSequenceHeader) {
  InitializeDescriptorObus();

  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterIaSequenceHeader));

  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(),
      &arbitrary_obus_.back(),
      &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back(),
  };

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, ArbitraryObuAfterCodecConfigs) {
  InitializeDescriptorObus();

  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterCodecConfigs));

  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(),
      &codec_config_obus_.at(kCodecConfigId),
      &arbitrary_obus_.back(),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back(),
  };

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, ArbitraryObuAfterAudioElements) {
  InitializeDescriptorObus();

  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterAudioElements));

  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(),
      &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &arbitrary_obus_.back(),
      &mix_presentation_obus_.back(),
  };

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, ArbitraryObuAfterMixPresentations) {
  InitializeDescriptorObus();

  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterMixPresentations));

  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(),
      &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back(),
      &arbitrary_obus_.back(),
  };

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

// This behavior helps ensure that "after descriptors" are not written in the
// "IACB" box in MP4.
TEST_F(ObuSequencerTest, DoesNotWriteArbitraryObuAfterDescriptors) {
  InitializeDescriptorObus();

  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterDescriptors));

  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back(),
      // &arbitrary_obus_.back(),
  };

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, CodecConfigAreAscendingOrderByDefault) {
  InitializeDescriptorObus();

  // Initialize a second Codec Config OBU.
  const DecodedUleb128 kSecondCodecConfigId = 101;
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus_);

  // IAMF makes no recommendation for the ordering between multiple descriptor
  // OBUs of the same type. By default `WriteDescriptorObus` orders them in
  // ascending order.
  ASSERT_LT(kCodecConfigId, kSecondCodecConfigId);
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &codec_config_obus_.at(kSecondCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, AudioElementAreAscendingOrderByDefault) {
  InitializeDescriptorObus();

  // Initialize a second Audio Element OBU.
  const DecodedUleb128 kSecondAudioElementId = 101;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus_, audio_elements_);

  // IAMF makes no recommendation for the ordering between multiple descriptor
  // OBUs of the same type. By default `WriteDescriptorObus` orders them in
  // ascending order.
  ASSERT_LT(kFirstAudioElementId, kSecondAudioElementId);
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &audio_elements_.at(kSecondAudioElementId).obu,
      &mix_presentation_obus_.back()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, MixPresentationsMaintainOriginalOrder) {
  InitializeDescriptorObus();
  mix_presentation_obus_.clear();

  // Prefix descriptor OBUs.
  std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(),
      &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
  };
  // Initialize three Mix Presentation OBUs, regardless of their IDs we
  // expect them to be serialized in the same order as the input list.
  constexpr DecodedUleb128 kFirstMixPresentationId = 100;
  constexpr DecodedUleb128 kSecondMixPresentationId = 99;
  constexpr DecodedUleb128 kThirdMixPresentationId = 101;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus_);
  expected_sequence.push_back(&mix_presentation_obus_.back());
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus_);
  expected_sequence.push_back(&mix_presentation_obus_.back());
  AddMixPresentationObuWithAudioElementIds(
      kThirdMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus_);
  expected_sequence.push_back(&mix_presentation_obus_.back());

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST(WriteDescriptorObus,
     InvalidWhenMixPresentationDoesNotComplyWithIaSequenceHeader) {
  IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);

  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(ObuSequencerBase::WriteDescriptorObus(
                   ia_sequence_header_obu, codec_config_obus, audio_elements,
                   mix_presentation_obus, /*arbitrary_obus=*/{}, unused_wb)
                   .ok());
}

TEST(WriteDescriptorObus,
     ValidWhenMixPresentationCompliesWithIaSequenceHeader) {
  IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(ObuSequencerBase::WriteDescriptorObus(
                  ia_sequence_header_obu, codec_config_obus, audio_elements,
                  mix_presentation_obus, /*arbitrary_obus=*/{}, unused_wb),
              IsOk());
}

TEST(PushDescriptorObus, SucceedsWithIaSequenceHeaderOnly) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
}

TEST(PickAndPlace, SucceedsWithIaSequenceHeaderOnly) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushDescriptorObus, FailsWhenCalledTwice) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PickAndPlace, FailsWhenCalledTwice) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PushDescriptorObus, ForwardsPropertiesToPushSerializedDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // Several properties should match values derived from the descriptor OBUs.
  const auto& codec_config_obu = codec_config_obus.begin()->second;
  const uint32_t kExpectedCommonSamplesPerFrame =
      codec_config_obu.GetNumSamplesPerFrame();
  const uint32_t kExpectedCommonSampleRate =
      codec_config_obu.GetOutputSampleRate();
  const uint8_t kExpectedCommonBitDepth =
      codec_config_obu.GetBitDepthToMeasureLoudness();
  const std::optional<int64_t> kOmitFirstPts = std::nullopt;
  const int kExpectedNumChannels = 2;
  const std::vector<uint8_t> descriptor_obus = {1, 2, 3};
  EXPECT_CALL(
      mock_obu_sequencer,
      PushSerializedDescriptorObus(
          kExpectedCommonSamplesPerFrame, kExpectedCommonSampleRate,
          kExpectedCommonBitDepth, kOmitFirstPts, kExpectedNumChannels, _));

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());
}

TEST(PickAndPlace, ForwardsPropertiesToPushSerializedDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // Several properties should match values derived from the descriptor OBUs.
  const auto& codec_config_obu = codec_config_obus.begin()->second;
  const uint32_t kExpectedCommonSamplesPerFrame =
      codec_config_obu.GetNumSamplesPerFrame();
  const uint32_t kExpectedCommonSampleRate =
      codec_config_obu.GetOutputSampleRate();
  const uint8_t kExpectedCommonBitDepth =
      codec_config_obu.GetBitDepthToMeasureLoudness();
  const std::optional<int64_t> kOmitFirstPts = std::nullopt;
  const int kExpectedNumChannels = 2;
  const std::vector<uint8_t> descriptor_obus = {1, 2, 3};
  EXPECT_CALL(
      mock_obu_sequencer,
      PushSerializedDescriptorObus(
          kExpectedCommonSamplesPerFrame, kExpectedCommonSampleRate,
          kExpectedCommonBitDepth, kOmitFirstPts, kExpectedNumChannels, _));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushDescriptorObus,
     WhenDescriptorsAreNotDelayedDescriptorsAreForwardedImmediately) {
  // Configure the OBU sequencer to not delay descriptors. This means the
  // properties can be forwarded right away.
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // The properties themselves are arbitrary, but "reasonable" defaults. This is
  // to ensure certain OBU sequencers can have a file with reasonable
  // properties, even if the IA Sequence is trivial.
  const uint32_t kExpectedCommonSamplesPerFrame = 1024;
  const uint32_t kExpectedCommonSampleRate = 48000;
  const uint8_t kExpectedCommonBitDepth = 16;
  const std::optional<int64_t> kFirstUntrimmedTimestamp = std::nullopt;
  const int kExpectedNumChannels = 2;
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(
                  kExpectedCommonSamplesPerFrame, kExpectedCommonSampleRate,
                  kExpectedCommonBitDepth, kFirstUntrimmedTimestamp,
                  kExpectedNumChannels, _));

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
}

TEST(
    PushDescriptorObus,
    WhenDescriptorsAreDelayedPropertiesAreForwardedAfterCloseForTrivialIaSequences) {
  // Configure the OBU sequencer to delay descriptors until the first untrimmed
  // sample is known. We can't detect it is a trivial IA Sequence, until the
  // sequencer is closed.
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());

  // The properties themselves are arbitrary, but "reasonable" defaults. This is
  // to ensure certain OBU sequencers can have a file with reasonable
  // properties, even if the IA Sequence is trivial.
  const uint32_t kExpectedCommonSamplesPerFrame = 1024;
  const uint32_t kExpectedCommonSampleRate = 48000;
  const uint8_t kExpectedCommonBitDepth = 16;
  const std::optional<int64_t> kFirstUntrimmedTimestamp = 0;
  const int kExpectedNumChannels = 2;
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(
                  kExpectedCommonSamplesPerFrame, kExpectedCommonSampleRate,
                  kExpectedCommonBitDepth, kFirstUntrimmedTimestamp,
                  kExpectedNumChannels, _));
  // Finally at close time, we detect that there are no audio frames. Therefore
  // we can make up a fake first timestamp.
  EXPECT_THAT(mock_obu_sequencer.Close(), IsOk());
}

TEST(PickAndPlace, ForwardsDefaultPropertiesForTrivialIaSequences) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);

  // The properties themselves are arbitrary, but "reasonable" defaults. This is
  // to ensure certain OBU sequencers can have a file with reasonable
  // properties, even if the IA Sequence is trivial.
  const uint32_t kExpectedCommonSamplesPerFrame = 1024;
  const uint32_t kExpectedCommonSampleRate = 48000;
  const uint8_t kExpectedCommonBitDepth = 16;
  const std::optional<int64_t> kFirstUntrimmedTimestamp = 0;
  const int kExpectedNumChannels = 2;
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(
                  kExpectedCommonSamplesPerFrame, kExpectedCommonSampleRate,
                  kExpectedCommonBitDepth, kFirstUntrimmedTimestamp,
                  kExpectedNumChannels, _));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushDescriptorObus, ForwardsSerializedDescriptorObusToPushDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // The spec prescribes an order among different types of descriptor OBUs.
  const auto descriptor_obus = SerializeObusExpectOk(std::list<const ObuBase*>{
      &kIaSequenceHeader, &codec_config_obus.begin()->second,
      &audio_elements.begin()->second.obu, &mix_presentation_obus.front()});
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(_, _, _, _, _,
                                           MakeConstSpan(descriptor_obus)));

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());
}

TEST(PickAndPlace, ForwardsSerializedDescriptorObusToPushDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeDescriptorObusForOneMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // The spec prescribes an order among different types of descriptor OBUs.
  const auto descriptor_obus = SerializeObusExpectOk(std::list<const ObuBase*>{
      &kIaSequenceHeader, &codec_config_obus.begin()->second,
      &audio_elements.begin()->second.obu, &mix_presentation_obus.front()});
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(_, _, _, _, _,
                                           MakeConstSpan(descriptor_obus)));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushDescriptorObus, ForwardsArbitraryObusToPushSerializedDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kArbitraryObuAfterIaSequenceHeader(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookAfterIaSequenceHeader)});
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // Custom arbitrary OBUs can be placed according to their hook.
  const auto descriptor_obus = SerializeObusExpectOk(std::list<const ObuBase*>{
      &kIaSequenceHeader, &kArbitraryObuAfterIaSequenceHeader.front()});
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(_, _, _, _, _,
                                           MakeConstSpan(descriptor_obus)));

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kArbitraryObuAfterIaSequenceHeader),
              IsOk());
}

TEST(PickAndPlace, ForwardsArbitraryObusToPushSerializedDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kArbitraryObuAfterIaSequenceHeader(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookAfterIaSequenceHeader)});
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // Custom arbitrary OBUs can be placed according to their hook.
  const auto descriptor_obus = SerializeObusExpectOk(std::list<const ObuBase*>{
      &kIaSequenceHeader, &kArbitraryObuAfterIaSequenceHeader.front()});
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(_, _, _, _, _,
                                           MakeConstSpan(descriptor_obus)));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kArbitraryObuAfterIaSequenceHeader),
              IsOk());
}

TEST(PushTemporalUnit, ForwardsPropertiesToPushAllTemporalUnits) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  audio_frames.front().obu.header_.num_samples_to_trim_at_start = 2;
  audio_frames.front().obu.header_.num_samples_to_trim_at_end = 1;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  // We expect eight samples per frame, less the trimmed samples.
  constexpr int kExpectedTimestamp = 0;
  constexpr int kExpectedNumSamples = 5;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());

  EXPECT_CALL(
      mock_obu_sequencer,
      PushSerializedTemporalUnit(kExpectedTimestamp, kExpectedNumSamples, _));

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), IsOk());
}

TEST(PickAndPlace, ForwardsPropertiesToPushAllTemporalUnits) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  audio_frames.front().obu.header_.num_samples_to_trim_at_start = 2;
  audio_frames.front().obu.header_.num_samples_to_trim_at_end = 1;
  // We expect eight samples per frame, less the trimmed samples.
  constexpr int kExpectedTimestamp = 0;
  constexpr int kExpectedNumSamples = 5;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_CALL(
      mock_obu_sequencer,
      PushSerializedTemporalUnit(kExpectedTimestamp, kExpectedNumSamples, _));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, audio_frames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PickAndPlace, OrdersTemporalUnitsByTimestamp) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> two_audio_frames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus,
      two_audio_frames);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kSecondTimestamp,
      kThirdTimestamp, audio_elements, two_audio_frames);
  // Ok, it is strange, to have audio frames in the wrong order. But the
  // sequencer handles this and arranges as per the timestamp.
  std::swap(two_audio_frames.front(), two_audio_frames.back());
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);

  // The cumulative number of samples to trim at the start of the IA Sequence
  // for the initial audio frane(s).
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(kFirstTimestamp, _, _));
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(kSecondTimestamp, _, _));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, two_audio_frames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushTemporalUnit,
     ForwardsNumUntrimmedSamplesToPushSerializedTemporalUnitWhenConfigured) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  std::list<AudioFrameWithData> first_audio_frame;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus,
      first_audio_frame);
  first_audio_frame.back().obu.header_.num_samples_to_trim_at_start = 8;
  const auto first_temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, first_audio_frame, kNoArbitraryObus);
  ASSERT_THAT(first_temporal_unit, IsOk());
  std::list<AudioFrameWithData> second_audio_frame;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kSecondTimestamp,
      kThirdTimestamp, audio_elements, second_audio_frame);
  second_audio_frame.back().obu.header_.num_samples_to_trim_at_start = 3;
  const auto second_temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, second_audio_frame, kNoArbitraryObus);
  ASSERT_THAT(second_temporal_unit, IsOk());
  // The first frame is fully trimmed. The second frame is partially trimmed.
  constexpr std::optional<int64_t> kExpectedFirstUntrimmedTimestamp = 11;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);
  // Neither the initial descriptors, nor the first temporal unit have enough
  // information to determine the first untrimmed timestamp.
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());
  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*first_temporal_unit),
              IsOk());

  // But by the second temporal unit, we can see the cumulative number of
  // samples to trim at the start for this IA Sequence.
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(
                  _, _, _, kExpectedFirstUntrimmedTimestamp, _, _));
  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*second_temporal_unit),
              IsOk());
}

TEST(PickAndPlace,
     ForwardsNumUntrimmedSamplesToPushAllTemporalUnitsWhenConfigured) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  audio_frames.back().obu.header_.num_samples_to_trim_at_start = 8;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kSecondTimestamp,
      kThirdTimestamp, audio_elements, audio_frames);
  audio_frames.back().obu.header_.num_samples_to_trim_at_start = 3;
  // The first frame is fully trimmed. The second frame is partially trimmed.
  constexpr std::optional<int64_t> kExpectedFirstUntrimmedTimestamp = 11;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);

  // The cumulative number of samples to trim at the start of the IA Sequence
  // for the initial audio frane(s).
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(
                  _, _, _, kExpectedFirstUntrimmedTimestamp, _, _));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, audio_frames, kNoParameterBlocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushDescriptorObus, ReturnsErrorWhenResamplingWouldBeRequired) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  // Theoretically, a future profile may support multiple codec config OBUs with
  // different sample rates. The underlying code is written to only support IAMF
  // v1.1.0 profiles, which all only support a single codec config OBU.
  constexpr uint32_t kCodecConfigId = 1;
  constexpr uint32_t kSecondCodecConfigId = 2;
  constexpr uint32_t kSampleRate = 48000;
  constexpr uint32_t kSecondSampleRate = 44100;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSecondSampleRate,
                                        codec_config_obus);
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PickAndPlace, ReturnsErrorWhenResamplingWouldBeRequired) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  // Theoretically, a future profile may support multiple codec config OBUs with
  // different sample rates. The underlying code is written to only support IAMF
  // v1.1.0 profiles, which all only support a single codec config OBU.
  constexpr uint32_t kCodecConfigId = 1;
  constexpr uint32_t kSecondCodecConfigId = 2;
  constexpr uint32_t kSampleRate = 48000;
  constexpr uint32_t kSecondSampleRate = 44100;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSecondSampleRate,
                                        codec_config_obus);
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PushTemporalUnit,
     ReturnsErrorWhenSamplesAreTrimmedFromStartAfterFirstUntrimmedSample) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  std::list<AudioFrameWithData> first_audio_frame;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus,
      first_audio_frame);
  first_audio_frame.back().obu.header_.num_samples_to_trim_at_start = 0;
  const auto first_temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, first_audio_frame, kNoArbitraryObus);
  ASSERT_THAT(first_temporal_unit, IsOk());
  // Corrupt the data by adding a second frame with samples trimmed from the
  // start, after the first frame had no trimmed samples.
  std::list<AudioFrameWithData> second_audio_frame;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kSecondTimestamp,
      kThirdTimestamp, audio_elements, second_audio_frame);
  second_audio_frame.back().obu.header_.num_samples_to_trim_at_start = 1;
  const auto second_temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, second_audio_frame, kNoArbitraryObus);
  ASSERT_THAT(second_temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());
  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*first_temporal_unit),
              IsOk());

  // The second temporal unit is corrupt, because it has samples trimmed from
  // the start after the first temporal unit had no trimmed samples.
  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*second_temporal_unit),
              Not(IsOk()));
}

TEST(PickAndPlace,
     ReturnsErrorWhenSamplesAreTrimmedFromStartAfterFirstUntrimmedSample) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  audio_frames.back().obu.header_.num_samples_to_trim_at_start = 0;
  // Corrupt the data by adding a second frame with samples trimmed from the
  // start, after the first frame had no trimmed samples.
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kSecondTimestamp,
      kThirdTimestamp, audio_elements, audio_frames);
  audio_frames.back().obu.header_.num_samples_to_trim_at_start = 1;
  MockObuSequencer mock_obu_sequencer(*LebGenerator::Create(),
                                      kDoNotIncludeTemporalDelimiters,
                                      kDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, audio_frames, kNoParameterBlocks,
                  kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PushTemporalUnit, ForwardsObusToPushSerializedTemporalUnit) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  std::list<ParameterBlockWithData> parameter_blocks;
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  const std::list<ArbitraryObu> kNoArbitraryObus;
  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());

  // The spec prescribes an order among different types of OBUs.
  const std::vector<uint8_t> serialized_temporal_unit =
      SerializeObusExpectOk(std::list<const ObuBase*>{
          parameter_blocks.front().obu.get(), &audio_frames.front().obu});
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(
                  _, _, MakeConstSpan(serialized_temporal_unit)));

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), IsOk());
}

TEST(PickAndPlace, ForwardsObusToPushSerializedTemporalUnit) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  DemixingParamDefinition param_definition =
      CreateDemixingParamDefinition(kFirstDemixingParameterId);
  std::list<ParameterBlockWithData> parameter_blocks;
  InitializeOneParameterBlockAndOneAudioFrame(
      param_definition, parameter_blocks, audio_frames, codec_config_obus,
      audio_elements);
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // The spec prescribes an order among different types of OBUs.
  const std::vector<uint8_t> serialized_temporal_unit =
      SerializeObusExpectOk(std::list<const ObuBase*>{
          parameter_blocks.front().obu.get(), &audio_frames.front().obu});
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedTemporalUnit(
                  _, _, MakeConstSpan(serialized_temporal_unit)));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, audio_frames, parameter_blocks,
                  kNoArbitraryObus),
              IsOk());
}

TEST(PushTemporalUnit, ForwardsArbitraryObusToPushSerializedTemporalUnit) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kArbitraryObuBeforeFirstAudioFrame(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookAfterAudioFramesAtTick,
                    kFirstTimestamp)});
  const auto first_temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kArbitraryObuBeforeFirstAudioFrame);
  ASSERT_THAT(first_temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kArbitraryObuBeforeFirstAudioFrame),
              IsOk());

  // Custom arbitrary OBUs can be placed according to their hook.
  const std::vector<uint8_t> serialized_audio_frame = SerializeObusExpectOk(
      std::list<const ObuBase*>{&audio_frames.front().obu,
                                &kArbitraryObuBeforeFirstAudioFrame.front()});
  EXPECT_CALL(
      mock_obu_sequencer,
      PushSerializedTemporalUnit(_, _, MakeConstSpan(serialized_audio_frame)));

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*first_temporal_unit),
              IsOk());
}

TEST(PickAndPlace, ForwardsArbitraryObusToPushSerializedTemporalUnit) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kArbitraryObuBeforeFirstAudioFrame(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookAfterAudioFramesAtTick,
                    kFirstTimestamp)});
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // Custom arbitrary OBUs can be placed according to their hook.
  const std::vector<uint8_t> serialized_audio_frame = SerializeObusExpectOk(
      std::list<const ObuBase*>{&audio_frames.front().obu,
                                &kArbitraryObuBeforeFirstAudioFrame.front()});
  EXPECT_CALL(
      mock_obu_sequencer,
      PushSerializedTemporalUnit(_, _, MakeConstSpan(serialized_audio_frame)));

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, audio_frames, kNoParameterBlocks,
                  kArbitraryObuBeforeFirstAudioFrame),
              IsOk());
}

TEST(Close, CallsCloseDerived) {
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // `CloseDerived` is called when done, which allows concrete implementation to
  // finalize and optionally close their output streams.
  EXPECT_CALL(mock_obu_sequencer, CloseDerived());

  EXPECT_THAT(mock_obu_sequencer.Close(), IsOk());
}

TEST(Close, FailsWhenCalledTwice) {
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.Close(), IsOk());

  EXPECT_THAT(mock_obu_sequencer.Close(), Not(IsOk()));
}

TEST(PickAndPlace, CallsCloseDerivedWhenDone) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kArbitraryObuAfterIaSequenceHeader(
      {ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                    ArbitraryObu::kInsertionHookAfterIaSequenceHeader)});
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // `CloseDerived` is called when done, which allows concrete implementation to
  // finalize and optionally close their output streams.
  EXPECT_CALL(mock_obu_sequencer, CloseDerived());

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoAudioFrames, kNoParameterBlocks,
                  kArbitraryObuAfterIaSequenceHeader),
              IsOk());
}

TEST(Abort, CallsAbortDerived) {
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // `CloseDerived` is called when done, which allows concrete implementation to
  // finalize and optionally close their output streams.
  EXPECT_CALL(mock_obu_sequencer, AbortDerived());

  mock_obu_sequencer.Abort();
}

TEST(PushDescriptorObus, CallsAbortDerivedWhenPushDescriptorObusFails) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // If `PushSerializedDescriptorObus` fails, `Abort` is called. This allows
  // concrete implementation to clean up and remove the file in one place.
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(_, _, _, _, _, _))
      .WillOnce(Return(absl::InternalError("")));
  EXPECT_CALL(mock_obu_sequencer, AbortDerived()).Times(1);

  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PickAndPlace, CallsAbortDerivedWhenPushDescriptorObusFails) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);
  const std::list<AudioFrameWithData> kNoAudioFrames;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // If `PushSerializedDescriptorObus` fails, `Abort` is called. This allows
  // concrete implementation to clean up and remove the file in one place.
  EXPECT_CALL(mock_obu_sequencer,
              PushSerializedDescriptorObus(_, _, _, _, _, _))
      .WillOnce(Return(absl::InternalError("")));
  EXPECT_CALL(mock_obu_sequencer, AbortDerived()).Times(1);

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoAudioFrames, kNoParameterBlocks,
                  kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PushTemporalUnit, CallsAbortDerivedWhenPushAllTemporalUnitsFails) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, kNoArbitraryObus),
              IsOk());

  // If `PushSerializedTemporalUnit` fails, `AbortDerived` is called. This
  // allows concrete implementation to clean up and remove the file in one
  // place.
  EXPECT_CALL(mock_obu_sequencer, PushSerializedTemporalUnit(_, _, _))
      .WillOnce(Return(absl::InternalError("")));
  EXPECT_CALL(mock_obu_sequencer, AbortDerived()).Times(1);

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), Not(IsOk()));
}

TEST(PickAndPlace, CallsAbortDerivedWhenPushAllTemporalUnitsFails) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  // If `PushSerializedTemporalUnit` fails, `AbortDerived` is called. This
  // allows concrete implementation to clean up and remove the file in one
  // place.
  EXPECT_CALL(mock_obu_sequencer, PushSerializedTemporalUnit(_, _, _))
      .WillOnce(Return(absl::InternalError("")));
  EXPECT_CALL(mock_obu_sequencer, AbortDerived()).Times(1);

  EXPECT_THAT(mock_obu_sequencer.PickAndPlace(
                  kIaSequenceHeader, codec_config_obus, audio_elements,
                  mix_presentation_obus, audio_frames, kNoParameterBlocks,
                  kNoArbitraryObus),
              Not(IsOk()));
}

TEST(PushTemporalUnit, FailsWhenBeforePushDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  // Omitted call to `PushDescriptorObus`. We can't accept temporal units yet.

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), Not(IsOk()));
}

TEST(PushTemporalUnit, FailsWhenCalledAfterClose) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.Close(), IsOk());

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), Not(IsOk()));
}

TEST(PushTemporalUnit, FailsWhenCalledAfterAbort) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  InitializeOneFrameIaSequenceWithMixPresentation(
      codec_config_obus, audio_elements, mix_presentation_obus, audio_frames);
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  mock_obu_sequencer.Abort();

  EXPECT_THAT(mock_obu_sequencer.PushTemporalUnit(*temporal_unit), Not(IsOk()));
}

TEST(UpdateDescriptorObusAndClose,
     ForwardsDescriptorObusToPushFinalizedDescriptorObus) {
  const IASequenceHeaderObu kOriginalIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const IASequenceHeaderObu kUpdatedIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfBaseProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kOriginalIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
  const auto expected_finalized_descriptor_obus = SerializeObusExpectOk(
      std::list<const ObuBase*>{&kUpdatedIaSequenceHeader});

  // Several properties should match values derived from the descriptor
  // OBUs.
  EXPECT_CALL(mock_obu_sequencer, PushFinalizedDescriptorObus(MakeConstSpan(
                                      expected_finalized_descriptor_obus)));

  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kUpdatedIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
}

TEST(UpdateDescriptorObusAndClose, FailsBeforePushDescriptorObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);

  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              Not(IsOk()));
}

TEST(UpdateDescriptorObusAndClose, SubsequentCloseCallsFails) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kIaSequenceHeader, kNoCodecConfigObus, kNoAudioElements,
                  kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());

  EXPECT_THAT(mock_obu_sequencer.Close(), Not(IsOk()));
}

TEST(UpdateDescriptorObusAndClose, CallsCloseDerived) {
  const IASequenceHeaderObu kOriginalIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const IASequenceHeaderObu kUpdatedIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfBaseProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kOriginalIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
  EXPECT_CALL(mock_obu_sequencer, CloseDerived()).Times(1);

  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kUpdatedIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
}

TEST(UpdateDescriptorObusAndClose,
     CallsAbortDerivedWhenPushFinalizedDescriptorObusFails) {
  const IASequenceHeaderObu kOriginalIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const IASequenceHeaderObu kUpdatedIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfBaseProfile, ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kOriginalIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());
  ON_CALL(mock_obu_sequencer, PushFinalizedDescriptorObus(_))
      .WillByDefault(Return(absl::InternalError("")));
  EXPECT_CALL(mock_obu_sequencer, AbortDerived()).Times(1);

  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kUpdatedIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              Not(IsOk()));
}

TEST(UpdateDescriptorObusAndClose, FailsWhenSerializedSizeChanges) {
  const IASequenceHeaderObu kOriginalIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const IASequenceHeaderObu kResizedIaSequenceHeader(
      ObuHeader{.obu_extension_flag = true, .extension_header_size = 0},
      IASequenceHeaderObu::kIaCode, ProfileVersion::kIamfBaseProfile,
      ProfileVersion::kIamfBaseProfile);
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigObus;
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kOriginalIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());

  // Derived classes may assume the descriptor OBUs are the same size, to
  // permit writes in place. We could lift this restriction, but it's not
  // clear it's worth the effort.
  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kResizedIaSequenceHeader, kNoCodecConfigObus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              Not(IsOk()));
}

TEST(UpdateDescriptorObusAndClose, FailsWhenCodecConfigPropertiesChange) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu>
      original_codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000,
                                        original_codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu>
      modified_codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 44100,
                                        modified_codec_config_obus);
  const absl::flat_hash_map<uint32_t, AudioElementWithData> kNoAudioElements;
  const std::list<MixPresentationObu> kNoMixPresentationObus;
  const std::list<ArbitraryObu> kNoArbitraryObus;
  MockObuSequencer mock_obu_sequencer(
      *LebGenerator::Create(), kDoNotIncludeTemporalDelimiters,
      kDoNotDelayDescriptorsUntilTrimAtStartIsKnown);
  EXPECT_THAT(mock_obu_sequencer.PushDescriptorObus(
                  kIaSequenceHeader, original_codec_config_obus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              IsOk());

  EXPECT_THAT(mock_obu_sequencer.UpdateDescriptorObusAndClose(
                  kIaSequenceHeader, modified_codec_config_obus,
                  kNoAudioElements, kNoMixPresentationObus, kNoArbitraryObus),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
