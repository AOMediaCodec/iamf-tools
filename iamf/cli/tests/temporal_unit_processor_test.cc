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

#include "iamf/cli/temporal_unit_processor.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/momentary_loudness.h"
#include "iamf/obu/momentary_loudness_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;

using OutputTemporalUnit = TemporalUnitProcessor::OutputTemporalUnit;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kSecondAudioElementId = 3;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kSecondSubstreamId = 19;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

constexpr std::array<uint8_t, 16> kArbitraryAudioFrame = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

DescriptorObus InitDescriptorsForZerothOrderAmbisonics() {
  DescriptorObus descriptors;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, descriptors.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      descriptors.mix_presentation_obus);
  return descriptors;
}

TEST(Create, SucceedsWithValidDescriptors) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));

  EXPECT_THAT(TemporalUnitProcessor::Create(descriptors.audio_elements,
                                            descriptors.mix_presentation_obus,
                                            read_bit_buffer.get()),
              IsOk());
}

TEST(Create, FailsWithDuplicateSubstreamIds) {
  auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));

  EXPECT_THAT(TemporalUnitProcessor::Create(descriptors.audio_elements,
                                            descriptors.mix_presentation_obus,
                                            read_bit_buffer.get()),
              Not(IsOk()));
}

TEST(Create, FailsWithZeroParameterRate) {
  DescriptorObus descriptors;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, descriptors.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  constexpr DecodedUleb128 kInvalidZeroParameterRate = 0;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kInvalidZeroParameterRate,
      descriptors.mix_presentation_obus);
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));

  EXPECT_THAT(TemporalUnitProcessor::Create(descriptors.audio_elements,
                                            descriptors.mix_presentation_obus,
                                            read_bit_buffer.get()),
              Not(IsOk()));
}

TEST(ProcessTemporalUnit, DoesNotCreateTemporalUnitWithNoData) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, DoesNotCreateTemporalUnitWithOnlyATemporalDelimiter) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const TemporalDelimiterObu temporal_delimiter_obu{ObuHeader()};
  const auto bitstream = SerializeObusExpectOk({&temporal_delimiter_obu});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, ConsumesOneAudioFrameAsTemporalUnit) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  const auto bitstream = SerializeObusExpectOk({&audio_frame_obu});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_THAT(output_temporal_unit->output_audio_frames, SizeIs(1));
}

TEST(ProcessTemporalUnit, SkipsStrayParameterBlocks) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  constexpr DecodedUleb128 kStrayParameterBlockId =
      std::numeric_limits<DecodedUleb128>::max();
  constexpr DecodedUleb128 kParameterRate = 1;
  constexpr DecodedUleb128 kParameterBlockDuration = 10;
  const MixGainParamDefinition param_definition(
      MakeOneSubblockParamDefinitionBaseArgs(
          kStrayParameterBlockId, kParameterRate, kParameterBlockDuration));
  auto parameter_block =
      ParameterBlockObu::CreateMode0(ObuHeader(), param_definition);
  ASSERT_THAT(parameter_block, NotNull());
  parameter_block->subblocks_[0] = std::make_unique<MixGainParameterData>(
      AnimatedParameterData<int16_t>::MakeStep(99));
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({parameter_block.get(), &audio_frame_obu});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_TRUE(output_temporal_unit->output_parameter_blocks.empty());
  EXPECT_THAT(output_temporal_unit->output_audio_frames, SizeIs(1));
}

TEST(ProcessTemporalUnit, ProcessesMomentaryLoudnessParameterBlock) {
  auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  constexpr DecodedUleb128 kMomentaryLoudnessParameterId = 1000;
  constexpr DecodedUleb128 kParameterBlockDuration = 8;
  const MomentaryLoudnessParamDefinition param_definition(
      MakeOneSubblockParamDefinitionBaseArgs(kMomentaryLoudnessParameterId,
                                             kCommonParameterRate,
                                             kParameterBlockDuration));
  auto momentary_loudness = MomentaryLoudness::Create(
      param_definition, /*bin_width_minus_one=*/0, /*first_bin_center=*/0,
      /*reserved_momentary_loudness=*/0, /*count=*/{1, 2});
  ASSERT_THAT(momentary_loudness, IsOk());
  descriptors.mix_presentation_obus.front()
      .sub_mixes_[0]
      .layouts[0]
      .loudness.momentary_loudness = *momentary_loudness;

  auto parameter_block =
      ParameterBlockObu::CreateMode0(ObuHeader(), param_definition);
  ASSERT_THAT(parameter_block, NotNull());
  auto momentary_loudness_data = MomentaryLoudnessParameterData::Create(
      /*momentary_loudness=*/42, /*reserved=*/0);
  ASSERT_THAT(momentary_loudness_data, IsOk());
  parameter_block->subblocks_[0] =
      std::make_unique<MomentaryLoudnessParameterData>(
          *momentary_loudness_data);
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({parameter_block.get(), &audio_frame_obu});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit.has_value());
  ASSERT_THAT(output_temporal_unit->output_parameter_blocks, SizeIs(1));
  const auto& output_parameter_block =
      *output_temporal_unit->output_parameter_blocks.front().obu;
  EXPECT_EQ(output_parameter_block.parameter_id_,
            kMomentaryLoudnessParameterId);
  ASSERT_THAT(output_parameter_block.subblocks_, SizeIs(1));
  EXPECT_EQ(*static_cast<const MomentaryLoudnessParameterData*>(
                output_parameter_block.subblocks_[0].get()),
            *momentary_loudness_data);
}

TEST(ProcessTemporalUnit, SkipsStrayAudioFrames) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  // `kSecondSubstreamId` does not belong to any audio element in `descriptors`.
  const AudioFrameObu stray_audio_frame_obu(ObuHeader(), kSecondSubstreamId,
                                            kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({&audio_frame_obu, &stray_audio_frame_obu});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_THAT(output_temporal_unit->output_audio_frames, SizeIs(1));
  EXPECT_EQ(
      output_temporal_unit->output_audio_frames.front().obu.GetSubstreamId(),
      kFirstSubstreamId);
}

}  // namespace
}  // namespace iamf_tools
