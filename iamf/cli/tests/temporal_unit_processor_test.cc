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
#include <list>
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
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/momentary_loudness.h"
#include "iamf/obu/momentary_loudness_parameter_data.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::SizeIs;

using OutputTemporalUnit = TemporalUnitProcessor::OutputTemporalUnit;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kSecondAudioElementId = 3;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kSecondSubstreamId = 19;
constexpr DecodedUleb128 kThirdSubstreamId = 20;
constexpr DecodedUleb128 kFourthSubstreamId = 21;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

constexpr int kObuTypeBitShift = 3;
constexpr int64_t kBufferCapacity = 1024;

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

TEST(ProcessTemporalUnit, DoesNotConsumeOneAudioFrameAsTemporalUnit) {
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
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, ConsumesMultipleTemporalUnitsWithTemporalDelimiters) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const TemporalDelimiterObu temporal_delimiter_obu{ObuHeader()};
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({&temporal_delimiter_obu, &audio_frame_obu_1,
                             &temporal_delimiter_obu, &audio_frame_obu_2});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit_1;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_1,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_1.has_value());
  EXPECT_THAT(output_temporal_unit_1->output_audio_frames, SizeIs(1));

  std::optional<OutputTemporalUnit> output_temporal_unit_2;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_2,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_2.has_value());
  EXPECT_THAT(output_temporal_unit_2->output_audio_frames, SizeIs(1));
}

TEST(ProcessTemporalUnit, ConsumesOneTemporalUnitsWithNextTemporalDelimiter) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const TemporalDelimiterObu temporal_delimiter_obu{ObuHeader()};
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  const auto bitstream = SerializeObusExpectOk(
      {&temporal_delimiter_obu, &audio_frame_obu, &temporal_delimiter_obu});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_THAT(output_temporal_unit->output_audio_frames, SizeIs(1));
}

TEST(ProcessTemporalUnit,
     ConsumesMultipleTemporalUnitsWithoutTemporalDelimiters) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({&audio_frame_obu_1, &audio_frame_obu_2});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit_1;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_1,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_1.has_value());
  EXPECT_THAT(output_temporal_unit_1->output_audio_frames, SizeIs(1));

  std::optional<OutputTemporalUnit> output_temporal_unit_2;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_2,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_2.has_value());
  EXPECT_THAT(output_temporal_unit_2->output_audio_frames, SizeIs(1));
}

TEST(ProcessTemporalUnit, ConsumesOnlyOneTemporalUnitFromTwoAudioFrames) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({&audio_frame_obu_1, &audio_frame_obu_2});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit_1;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit_1,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_1.has_value());
  EXPECT_THAT(output_temporal_unit_1->output_audio_frames, SizeIs(1));

  std::optional<OutputTemporalUnit> output_temporal_unit_2;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit_2,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit_2.has_value());
}

TEST(ProcessTemporalUnit,
     ConsumesOnlyOneTemporalUnitFromTwoAudioFramesAndIncompleteObuAtEnd) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  auto bitstream =
      SerializeObusExpectOk({&audio_frame_obu_1, &audio_frame_obu_2});
  const std::vector<uint8_t> extra_audio_frame_obu_header_bytes = {
      kObuIaAudioFrameId0 << kObuTypeBitShift,
      // `obu_size`. -> Non-zero size, but we have no bytes following.
      0x7f};
  bitstream.insert(bitstream.end(), extra_audio_frame_obu_header_bytes.begin(),
                   extra_audio_frame_obu_header_bytes.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit_1;
  bool continue_processing = false;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit_1,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_1.has_value());
  EXPECT_THAT(output_temporal_unit_1->output_audio_frames, SizeIs(1));

  std::optional<OutputTemporalUnit> output_temporal_unit_2;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit_2,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit_2.has_value());
}

TEST(ProcessTemporalUnit, ConsumesMultipleTemporalUnitsChunkedArbitrarily) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer = StreamBasedReadBitBuffer::Create(kBufferCapacity);
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto two_temporal_units =
      SerializeObusExpectOk({&audio_frame_obu_1, &audio_frame_obu_2});
  const int64_t chunk_size = two_temporal_units.size() / 3;
  const std::vector<uint8_t> chunk_1(two_temporal_units.begin(),
                                     two_temporal_units.begin() + chunk_size);
  const std::vector<uint8_t> chunk_2(
      two_temporal_units.begin() + chunk_size,
      two_temporal_units.begin() + 2 * chunk_size);
  const std::vector<uint8_t> chunk_3(
      two_temporal_units.begin() + 2 * chunk_size, two_temporal_units.end());

  // Chunk 1: insufficient to finish reading the first audio frame.
  ASSERT_THAT(read_bit_buffer->PushBytes(chunk_1), IsOk());
  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());
  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());

  // Chunk 2: finishes reading the first audio frame, but not the second.
  ASSERT_THAT(read_bit_buffer->PushBytes(chunk_2), IsOk());
  continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());
  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());

  // Chunk 3: finishes reading the second audio frame, completing the first TU.
  ASSERT_THAT(read_bit_buffer->PushBytes(chunk_3), IsOk());
  continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());
  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_THAT(output_temporal_unit->output_audio_frames, SizeIs(1));

  // Final call with `eos_is_end_of_sequence` set to true flushes the second TU.
  output_temporal_unit.reset();
  continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());
  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_THAT(output_temporal_unit->output_audio_frames, SizeIs(1));
}

MetadataObu CreateTestMetadataObu(uint8_t country_code = 0xb5,
                                  ObuHeader header = ObuHeader()) {
  return MetadataObu::Create(
      header, MetadataITUTT35{.itu_t_t35_country_code = country_code,
                              .itu_t_t35_payload_bytes = {0x01, 0x02, 0x03}});
}

std::unique_ptr<ParameterBlockObu> CreateTestMixGainParameterBlock(
    const ParamDefinition& param_definition) {
  constexpr DecodedUleb128 kDuration = 8;
  const auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(
      kDuration, /*constant_subblock_duration=*/kDuration);
  EXPECT_THAT(schedule, IsOk());
  auto parameter_block =
      ParameterBlockObu::CreateMode1(ObuHeader(), param_definition, *schedule);
  EXPECT_THAT(parameter_block, NotNull());
  parameter_block->subblocks_[0] = std::make_unique<MixGainParameterData>(
      AnimatedParameterData<int16_t>::MakeStep(0));
  return parameter_block;
}

void ExpectProcessTemporalUnitFailsWithObus(
    const std::list<const ObuBase*>& temporal_unit_obus) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const auto bitstream = SerializeObusExpectOk(temporal_unit_obus);
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
              Not(IsOk()));
}

TEST(ProcessTemporalUnit, CollectsMetadataObusBeforeAndAfterParameterBlock) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const auto metadata_obu_1 = CreateTestMetadataObu(0xb5);
  const auto metadata_obu_2 = CreateTestMetadataObu(0xb6);
  const MixGainParamDefinition param_definition(
      MakeScheduleInParameterBlockBaseArgs(kCommonMixGainParameterId,
                                           kCommonParameterRate));
  const auto parameter_block =
      CreateTestMixGainParameterBlock(param_definition);
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({&metadata_obu_1, parameter_block.get(),
                             &metadata_obu_2, &audio_frame_obu});
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
  EXPECT_THAT(output_temporal_unit->output_parameter_blocks, SizeIs(1));
  ASSERT_THAT(output_temporal_unit->output_metadata_obus, SizeIs(2));
  EXPECT_EQ(std::get<MetadataITUTT35>(
                output_temporal_unit->output_metadata_obus.front()
                    .GetMetadataVariant())
                .itu_t_t35_country_code,
            0xb5);
  EXPECT_EQ(std::get<MetadataITUTT35>(
                output_temporal_unit->output_metadata_obus.back()
                    .GetMetadataVariant())
                .itu_t_t35_country_code,
            0xb6);
}

TEST(ProcessTemporalUnit, CollectsMetadataObusAcrossMultipleTemporalUnits) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const auto metadata_obu_1 = CreateTestMetadataObu(0xb5);
  const auto metadata_obu_2 = CreateTestMetadataObu(0xb6);
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto bitstream =
      SerializeObusExpectOk({&metadata_obu_1, &audio_frame_obu_1,
                             &metadata_obu_2, &audio_frame_obu_2});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit_1;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_1,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_1.has_value());
  EXPECT_THAT(output_temporal_unit_1->output_audio_frames, SizeIs(1));
  ASSERT_THAT(output_temporal_unit_1->output_metadata_obus, SizeIs(1));
  EXPECT_EQ(std::get<MetadataITUTT35>(
                output_temporal_unit_1->output_metadata_obus.front()
                    .GetMetadataVariant())
                .itu_t_t35_country_code,
            0xb5);

  std::optional<OutputTemporalUnit> output_temporal_unit_2;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_2,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_2.has_value());
  EXPECT_THAT(output_temporal_unit_2->output_audio_frames, SizeIs(1));
  ASSERT_THAT(output_temporal_unit_2->output_metadata_obus, SizeIs(1));
  EXPECT_EQ(std::get<MetadataITUTT35>(
                output_temporal_unit_2->output_metadata_obus.front()
                    .GetMetadataVariant())
                .itu_t_t35_country_code,
            0xb6);
}

TEST(ProcessTemporalUnit,
     CollectsMetadataObusAcrossMultipleDelimitedTemporalUnits) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const TemporalDelimiterObu temporal_delimiter_obu{ObuHeader()};
  const auto metadata_obu_1 = CreateTestMetadataObu(0xb5);
  const auto metadata_obu_2 = CreateTestMetadataObu(0xb6);
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto bitstream = SerializeObusExpectOk(
      {&temporal_delimiter_obu, &metadata_obu_1, &audio_frame_obu_1,
       &temporal_delimiter_obu, &metadata_obu_2, &audio_frame_obu_2});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto processor = TemporalUnitProcessor::Create(
      descriptors.audio_elements, descriptors.mix_presentation_obus,
      read_bit_buffer.get());
  ASSERT_THAT(processor, IsOk());

  std::optional<OutputTemporalUnit> output_temporal_unit_1;
  bool continue_processing = true;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_1,
                  continue_processing),
              IsOk());

  EXPECT_TRUE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_1.has_value());
  EXPECT_THAT(output_temporal_unit_1->output_audio_frames, SizeIs(1));
  ASSERT_THAT(output_temporal_unit_1->output_metadata_obus, SizeIs(1));
  EXPECT_EQ(std::get<MetadataITUTT35>(
                output_temporal_unit_1->output_metadata_obus.front()
                    .GetMetadataVariant())
                .itu_t_t35_country_code,
            0xb5);

  std::optional<OutputTemporalUnit> output_temporal_unit_2;
  EXPECT_THAT(processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit_2,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  ASSERT_TRUE(output_temporal_unit_2.has_value());
  EXPECT_THAT(output_temporal_unit_2->output_audio_frames, SizeIs(1));
  ASSERT_THAT(output_temporal_unit_2->output_metadata_obus, SizeIs(1));
  EXPECT_EQ(std::get<MetadataITUTT35>(
                output_temporal_unit_2->output_metadata_obus.front()
                    .GetMetadataVariant())
                .itu_t_t35_country_code,
            0xb6);
}

TEST(ProcessTemporalUnit, IgnoresRedundantAndUnsupportedMetadataObus) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const auto redundant_metadata_obu =
      CreateTestMetadataObu(0xb5, ObuHeader{.obu_redundant_copy = true});
  const ArbitraryObu unsupported_metadata_obu(
      kObuIaMetadata, ObuHeader(),
      /*payload=*/{/*metadata_type=*/3, 0x01, 0x02},
      ArbitraryObu::kInsertionHookAfterDescriptors);
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  const auto bitstream = SerializeObusExpectOk(
      {&redundant_metadata_obu, &unsupported_metadata_obu, &audio_frame_obu});
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
  EXPECT_THAT(output_temporal_unit->output_metadata_obus, IsEmpty());
}

TEST(ProcessTemporalUnit, RejectsMetadataObuWithIamfTags) {
  const auto iamf_tags_metadata_obu = MetadataObu::Create(
      ObuHeader(),
      MetadataIamfTags{.tags = {{.tag_name = "title", .tag_value = "song"}}});
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);
  ExpectProcessTemporalUnitFailsWithObus(
      {&iamf_tags_metadata_obu, &audio_frame_obu});
}

TEST(ProcessTemporalUnit, RejectsMetadataObuBeforeTemporalDelimiter) {
  const auto metadata_obu = CreateTestMetadataObu();
  const TemporalDelimiterObu temporal_delimiter_obu{ObuHeader()};
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);

  // Metadata OBU before the opening Temporal Delimiter OBU.
  ExpectProcessTemporalUnitFailsWithObus(
      {&metadata_obu, &temporal_delimiter_obu, &audio_frame_obu_1});
  // Metadata OBU between an Audio Frame OBU and the next Temporal Delimiter
  // OBU.
  ExpectProcessTemporalUnitFailsWithObus(
      {&temporal_delimiter_obu, &audio_frame_obu_1, &metadata_obu,
       &temporal_delimiter_obu, &audio_frame_obu_2});
}

TEST(ProcessTemporalUnit, RejectsMetadataObuNotFollowedByAudioFrames) {
  const auto metadata_obu = CreateTestMetadataObu();
  const TemporalDelimiterObu temporal_delimiter_obu{ObuHeader()};
  const MixGainParamDefinition param_definition(
      MakeScheduleInParameterBlockBaseArgs(kCommonMixGainParameterId,
                                           kCommonParameterRate));
  const auto parameter_block =
      CreateTestMixGainParameterBlock(param_definition);
  const AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                      kArbitraryAudioFrame);

  // Temporal unit with only a Metadata OBU.
  ExpectProcessTemporalUnitFailsWithObus({&metadata_obu});
  // Temporal unit with only a Metadata OBU and a Parameter Block OBU.
  ExpectProcessTemporalUnitFailsWithObus(
      {&metadata_obu, parameter_block.get()});
  // Delimited temporal unit with only a Metadata OBU before the next Temporal
  // Delimiter OBU.
  ExpectProcessTemporalUnitFailsWithObus(
      {&temporal_delimiter_obu, &metadata_obu, &temporal_delimiter_obu,
       &audio_frame_obu});
  // Trailing Metadata OBU after an Audio Frame OBU without a Temporal
  // Delimiter OBU.
  ExpectProcessTemporalUnitFailsWithObus({&audio_frame_obu, &metadata_obu});
  // Trailing Metadata OBU after an Audio Frame OBU in a delimited Temporal
  // Unit.
  ExpectProcessTemporalUnitFailsWithObus(
      {&temporal_delimiter_obu, &audio_frame_obu, &metadata_obu});
}

TEST(ProcessTemporalUnit,
     RejectsMetadataObuBetweenAudioFramesOfSameTemporalUnit) {
  DescriptorObus descriptors;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, descriptors.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId, kThirdSubstreamId,
       kFourthSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      descriptors.mix_presentation_obus);
  const AudioFrameObu audio_frame_obu_1(ObuHeader(), kFirstSubstreamId,
                                        kArbitraryAudioFrame);
  const auto metadata_obu = CreateTestMetadataObu();
  const AudioFrameObu audio_frame_obu_2(ObuHeader(), kSecondSubstreamId,
                                        kArbitraryAudioFrame);
  const auto bitstream = SerializeObusExpectOk(
      {&audio_frame_obu_1, &metadata_obu, &audio_frame_obu_2});
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
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
