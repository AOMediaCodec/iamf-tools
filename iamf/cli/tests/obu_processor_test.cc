/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/obu_processor.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr DecodedUleb128 kSecondCodecConfigId = 2;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kSecondAudioElementId = 3;
constexpr DecodedUleb128 kThirdAudioElementId = 4;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kSecondSubstreamId = 19;
constexpr DecodedUleb128 kThirdSubstreamId = 20;
constexpr DecodedUleb128 kFourthSubstreamId = 21;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kSecondMixPresentationId = 4;
constexpr DecodedUleb128 kThirdMixPresentationId = 5;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

constexpr DecodedUleb128 kImplicitSubstreamId = 0;

constexpr int kObuTypeBitShift = 3;
constexpr int64_t kBufferCapacity = 1024;

constexpr std::optional<uint8_t> kNoOutputFileBitDepthOverride = std::nullopt;
constexpr uint8_t kOutputBitDepth24 = 24;
constexpr uint8_t kOutputBitDepth32 = 32;

std::vector<uint8_t> InitObuSequence(
    const std::list<const ObuBase*>& input_ia_sequence) {
  WriteBitBuffer expected_wb(0);
  for (const auto* expected_obu : input_ia_sequence) {
    EXPECT_NE(expected_obu, nullptr);
    EXPECT_THAT(expected_obu->ValidateAndWriteObu(expected_wb), IsOk());
  }

  return expected_wb.bit_buffer();
}

std::vector<uint8_t> InitObuSequenceAddSequenceHeader(
    const std::list<const ObuBase*>& input_ia_sequence_without_header) {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(input_ia_sequence_without_header);
  input_ia_sequence.push_front(&ia_sequence_header);
  return InitObuSequence(input_ia_sequence);
}

auto CreateAllWavWriters(const std::string output_filename_string,
                         bool write_wav_header) {
  return [output_filename_string, write_wav_header](
             DecodedUleb128 mix_presentation_id, int /*sub_mix_index*/,
             int /*layout_index*/, const Layout& layout, int num_channels,
             int sample_rate, int bit_depth, size_t max_input_samples_per_frame)
             -> std::unique_ptr<SampleProcessorBase> {
    return WavWriter::Create(output_filename_string, num_channels, sample_rate,
                             bit_depth, max_input_samples_per_frame,
                             write_wav_header);
  };
}

TEST(ProcessDescriptorObus, InvalidWithoutIaSequenceHeader) {
  std::vector<uint8_t> bitstream_without_ia_sequence_header =
      InitObuSequence({});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obu;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(bitstream_without_ia_sequence_header));
  bool insufficient_data = false;
  EXPECT_FALSE(ObuProcessor::ProcessDescriptorObus(
                   /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                   ia_sequence_header, codec_config_obu,
                   audio_elements_with_data, mix_presentation_obus,
                   insufficient_data)
                   .ok());
}

TEST(ProcessDescriptorObus, CollectsCodecConfigsBeforeATemporalUnit) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  AudioFrameObu input_audio_frame(
      ObuHeader(), kFirstSubstreamId, /*audio_frame=*/
      {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  auto two_codec_configs_and_audio_frame = InitObuSequenceAddSequenceHeader(
      {&input_codec_configs.at(kFirstCodecConfigId),
       &input_codec_configs.at(kSecondCodecConfigId), &input_audio_frame});
  IASequenceHeaderObu unused_ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> output_codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(two_codec_configs_and_audio_frame));
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          unused_ia_sequence_header, output_codec_config_obus,
          audio_elements_with_data, mix_presentation_obus, insufficient_data),
      IsOk());

  EXPECT_EQ(output_codec_config_obus.size(), 2);
  EXPECT_TRUE(output_codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_TRUE(output_codec_config_obus.contains(kSecondCodecConfigId));
}

TEST(ProcessDescriptorObus, CollectsCodecConfigsAtEndOfBitstream) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  auto two_codec_configs_at_end_of_bitstream = InitObuSequenceAddSequenceHeader(
      {&input_codec_configs.at(kFirstCodecConfigId),
       &input_codec_configs.at(kSecondCodecConfigId)});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(two_codec_configs_at_end_of_bitstream));
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());
  EXPECT_FALSE(insufficient_data);

  EXPECT_EQ(codec_config_obus.size(), 2);
  EXPECT_TRUE(codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_TRUE(codec_config_obus.contains(kSecondCodecConfigId));
}

TEST(ProcessDescriptorObus,
     DoesNotCollectCodecConfigsAtEndOfBitstreamWithoutIsExhaustiveAndExact) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  auto two_codec_configs_at_end_of_bitstream = InitObuSequenceAddSequenceHeader(
      {&input_codec_configs.at(kFirstCodecConfigId),
       &input_codec_configs.at(kSecondCodecConfigId)});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(two_codec_configs_at_end_of_bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));
  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(codec_config_obus.size(), 0);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(ProcessDescriptorObus, CollectsIaSequenceHeaderWithoutOtherObus) {
  auto only_ia_sequence_header = InitObuSequenceAddSequenceHeader({});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(only_ia_sequence_header));
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());

  EXPECT_EQ(ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(ia_sequence_header.GetAdditionalProfile(),
            ProfileVersion::kIamfBaseProfile);
}

TEST(ProcessDescriptorObus, DescriptorObusMustStartWithIaSequenceHeader) {
  const IASequenceHeaderObu input_ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);

  IASequenceHeaderObu unused_ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> unused_codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      unused_audio_elements_with_data;
  std::list<MixPresentationObu> unused_mix_presentation_obus;

  // Descriptor OBUs must start with IA Sequence Header.
  auto ia_sequence_header_then_codec_config =
      InitObuSequence({&input_ia_sequence_header,
                       &input_codec_configs.at(kFirstCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(ia_sequence_header_then_codec_config));
  bool insufficient_data = false;
  EXPECT_THAT(ObuProcessor::ProcessDescriptorObus(
                  /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                  unused_ia_sequence_header, unused_codec_config_obus,
                  unused_audio_elements_with_data, unused_mix_presentation_obus,
                  insufficient_data),
              IsOk());
  EXPECT_FALSE(insufficient_data);
  // The reverse order is not valid according to
  // https://aomediacodec.github.io/iamf/#standalone-descriptor-obus
  auto codec_config_then_ia_sequence_header =
      InitObuSequence({&input_codec_configs.at(kFirstCodecConfigId),
                       &input_ia_sequence_header});

  read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(codec_config_then_ia_sequence_header));
  EXPECT_FALSE(ObuProcessor::ProcessDescriptorObus(
                   /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                   unused_ia_sequence_header, unused_codec_config_obus,
                   unused_audio_elements_with_data,
                   unused_mix_presentation_obus, insufficient_data)
                   .ok());
}

TEST(ProcessDescriptorObus, SucceedsWithSuccessiveRedundantSequenceHeaders) {
  const IASequenceHeaderObu input_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  auto bitstream =
      InitObuSequenceAddSequenceHeader({&input_redundant_ia_sequence_header});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, ConsumesUpToNextNonRedundantSequenceHeader) {
  const IASequenceHeaderObu input_non_redundant_ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  auto buffer = InitObuSequence({&input_non_redundant_ia_sequence_header});
  const int64_t first_ia_sequence_size = buffer.size();

  // Add a second non-redundant sequence header.
  const auto second_non_redundant_ia_sequence =
      InitObuSequence({&input_non_redundant_ia_sequence_header});
  buffer.insert(buffer.end(), second_non_redundant_ia_sequence.begin(),
                second_non_redundant_ia_sequence.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());
  EXPECT_FALSE(insufficient_data);

  // Expect the reader position to be right next to the end of the first IA
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), first_ia_sequence_size * 8);
}

TEST(ProcessDescriptorObus, CollectsIaSequenceHeaderWithCodecConfigs) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  const DecodedUleb128 kFirstCodecConfigId = 123;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  const DecodedUleb128 kSecondCodecConfigId = 124;
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  auto ia_sequence_header_with_codec_configs = InitObuSequenceAddSequenceHeader(
      {&input_codec_configs.at(kFirstCodecConfigId),
       &input_codec_configs.at(kSecondCodecConfigId)});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(ia_sequence_header_with_codec_configs));
  bool insufficient_data = false;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());

  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(codec_config_obus.size(), 2);
  EXPECT_TRUE(codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_TRUE(codec_config_obus.contains(kSecondCodecConfigId));
}

// Returns a bitstream with all the descriptor obus for a zeroth order
// ambisonics stream.
std::vector<uint8_t> InitAllDescriptorsForZerothOrderAmbisonics() {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      input_codec_configs, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);

  return InitObuSequenceAddSequenceHeader(
      {&input_codec_configs.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
}

// Descriptor obus only, is_exhaustive_and_exact = true.
TEST(ProcessDescriptorObus, SucceedsWithoutTemporalUnitFollowing) {
  auto zeroth_order_ambisonics_descriptor_obus =
      InitAllDescriptorsForZerothOrderAmbisonics();

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(zeroth_order_ambisonics_descriptor_obus));
  bool insufficient_data = false;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());

  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(codec_config_obus.size(), 1);
  EXPECT_TRUE(codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_EQ(audio_elements_with_data.size(), 1);
  EXPECT_TRUE(audio_elements_with_data.contains(kFirstAudioElementId));
  EXPECT_EQ(mix_presentation_obus.size(), 1);
  EXPECT_EQ(mix_presentation_obus.front().GetMixPresentationId(),
            kFirstMixPresentationId);
}

// Descriptor obus only, is_exhaustive_and_exact = false.
TEST(ProcessDescriptorObus,
     RejectsWithoutTemporalUnitFollowingAndNotExhaustive) {
  auto zeroth_order_ambisonics_descriptor_obus =
      InitAllDescriptorsForZerothOrderAmbisonics();

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(zeroth_order_ambisonics_descriptor_obus));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data = false;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(codec_config_obus.size(), 0);
  EXPECT_EQ(audio_elements_with_data.size(), 0);
  EXPECT_EQ(mix_presentation_obus.size(), 0);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + temporal unit header following, is_exhaustive_and_exact =
// true
TEST(ProcessDescriptorObusTest,
     RejectDescriptorObusWithTemporalUnitFollowingAndIsExhaustiveAndExact) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = InitObuSequence({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data = false;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  // We failed with sufficient data.
  EXPECT_FALSE(insufficient_data);

  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + temporal unit header following, is_exhaustive_and_exact =
// false.
TEST(ProcessDescriptorObusTest, SucceedsWithTemporalUnitFollowing) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  const int64_t descriptors_size = bitstream.size();

  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = InitObuSequence({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = false;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());

  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(codec_config_obus.size(), 1);
  EXPECT_TRUE(codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_EQ(audio_elements_with_data.size(), 1);
  EXPECT_TRUE(audio_elements_with_data.contains(kFirstAudioElementId));
  EXPECT_EQ(mix_presentation_obus.size(), 1);
  EXPECT_EQ(mix_presentation_obus.front().GetMixPresentationId(),
            kFirstMixPresentationId);

  // Expect the reader position to be right next to the end of the descriptors.
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), descriptors_size * 8);
}

// Descriptor obus + non_temporal_unit_header following but not enough data to
// read last obu.
TEST(ProcessDescriptorObusTest,
     RejectDescriptorObusWithNonTemporalUnitHeaderFollowingAndNotEnoughData) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  std::vector<uint8_t> extra_descriptor_obu_header_bytes = {
      kObuIaCodecConfig << kObuTypeBitShift,
      // `obu_size`. -> Non-zero size, but we have no bytes following.
      0x7f};

  bitstream.insert(bitstream.end(), extra_descriptor_obu_header_bytes.begin(),
                   extra_descriptor_obu_header_bytes.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data = false;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + partial header following.
TEST(ProcessDescriptorObus, RejectsDescriptorObusWithPartialHeaderFollowing) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  std::vector<uint8_t> partial_header_obu = {0x80};
  bitstream.insert(bitstream.end(), partial_header_obu.begin(),
                   partial_header_obu.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data = false;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(codec_config_obus.size(), 0);
  EXPECT_EQ(audio_elements_with_data.size(), 0);
  EXPECT_EQ(mix_presentation_obus.size(), 0);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(ProcessTemporalUnitObus, OkAndProducesNoObusIfEmpty) {
  auto empty_temporal_unit = InitObuSequence({});
  auto empty_read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(empty_temporal_unit));
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigs =
      {};
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kNoAudioElementsWithData = {};
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(kNoAudioElementsWithData,
                                              /*param_definitions=*/{}),
              IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {};
  ParametersManager parameters_manager(kNoAudioElementsWithData);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          kNoAudioElementsWithData, kNoCodecConfigs,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *empty_read_bit_buffer,
          global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());

  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_FALSE(continue_processing);
}

TEST(ProcessTemporalUnitObus, ConsumesAllTemporalUnits) {
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});

  auto one_temporal_unit = InitObuSequence({&audio_frame_obu});

  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(one_temporal_unit));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());

  // Reaching the end of the stream.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
}

TEST(ProcessTemporalUnitObus, ReadsAllTemporalUnitsBeforeNewIaSequence) {
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  const IASequenceHeaderObu non_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = false}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);

  auto one_temporal_unit_before_non_redundant_descriptor_obu =
      InitObuSequence({&audio_frame_obu, &non_redundant_ia_sequence_header});

  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(
          one_temporal_unit_before_non_redundant_descriptor_obu));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Process again, this time a new IA sequence is encountred, empty OBUs
  // are returned, and `continue_processing` is set to false.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_FALSE(continue_processing);

  // NOT reaching the end of the stream because we haven't consumed the
  // next IA sequence header.
  EXPECT_TRUE(read_bit_buffer->IsDataAvailable());
}

TEST(ProcessTemporalUnitObus,
     ConsumesAllTemporalUnitsAndRedundantDescriptorObus) {
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  const IASequenceHeaderObu redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  auto& redundant_codec_config = codec_config_obus.at(kFirstCodecConfigId);
  redundant_codec_config.header_.obu_redundant_copy = true;

  auto one_temporal_unit_before_redundant_descriptor_obu =
      InitObuSequence({&audio_frame_obu, &redundant_ia_sequence_header,
                       &redundant_codec_config});

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(one_temporal_unit_before_redundant_descriptor_obu));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Process again, this time the redundant IA sequence header is read and
  // outputs are empty.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Process again, this time the redundant Codec Config is read and outputs
  // are empty.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());

  // Reaching the end of the stream.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
}

TEST(ProcessTemporalUnitObus,
     FailsOnNonRedundantAndNonIaSequenceHeaderDescriptorObu) {
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  auto& non_redundant_codec_config = codec_config_obus.at(kFirstCodecConfigId);
  non_redundant_codec_config.header_.obu_redundant_copy = false;

  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});

  auto temporal_unit_with_non_redundant_codec_config_obu =
      InitObuSequence({&audio_frame_obu, &non_redundant_codec_config});

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(temporal_unit_with_non_redundant_codec_config_obu));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Process again, this time the non-redundant Codec Config OBU is read and
  // the function fails.
  EXPECT_FALSE(ObuProcessor::ProcessTemporalUnitObu(
                   audio_elements_with_data, codec_config_obus,
                   substream_id_to_audio_element, parameters_manager,
                   parameter_id_to_metadata, *read_bit_buffer,
                   global_timing_module, audio_frame_with_data,
                   parameter_block_with_data, temporal_delimiter,
                   continue_processing)
                   .ok());
}

TEST(ProcessTemporalUnitObus, ConsumesAllTemporalUnitsAndReservedObus) {
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  ArbitraryObu reserved_obu_before_audio_frame(
      kObuIaReserved25, ObuHeader(), /*payload=*/{0, 99},
      ArbitraryObu::kInsertionHookAfterDescriptors);
  ArbitraryObu reserved_obu_after_audio_frame(
      kObuIaReserved29, ObuHeader{.obu_redundant_copy = true},
      /*payload=*/{0, 99}, ArbitraryObu::kInsertionHookAfterDescriptors);

  auto temporal_unit_with_reserved_obus =
      InitObuSequence({&reserved_obu_before_audio_frame, &audio_frame_obu,
                       &reserved_obu_after_audio_frame});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(temporal_unit_with_reserved_obus));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;

  // First call: reading and discarding the reserved OBU.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Second call: reading the Audio Frame OBU.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Third call: reading and discarding the reserved OBU.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Reaching the end of the stream.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
}

TEST(ProcessTemporalUnitObusTest, ProcessMultipleAudioSubstreams) {
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(AudioFrameObu(
      ObuHeader(), kFirstSubstreamId, /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kSecondSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kImplicitSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8, 9}));
  auto multiple_audio_substreams = InitObuSequence(
      {&audio_frame_obus[0], &audio_frame_obus[1], &audio_frame_obus[2]});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId, kImplicitSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());

  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element},
          {kSecondSubstreamId, first_audio_element},
          {kImplicitSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(multiple_audio_substreams));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;

  // Call three times, each outputing an audio frame.
  for (int i = 0; i < 3; i++) {
    EXPECT_THAT(
        ObuProcessor::ProcessTemporalUnitObu(
            audio_elements_with_data, codec_config_obus,
            substream_id_to_audio_element, parameters_manager,
            parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
            audio_frame_with_data, parameter_block_with_data,
            temporal_delimiter, continue_processing),
        IsOk());
    EXPECT_TRUE(audio_frame_with_data.has_value());
    EXPECT_FALSE(parameter_block_with_data.has_value());
    EXPECT_FALSE(temporal_delimiter.has_value());
    EXPECT_TRUE(continue_processing);
  }
}

TEST(ProcessTemporalUnitObusTest, ProcessesSubstreamWithMultipleFrames) {
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(AudioFrameObu(
      ObuHeader(), kFirstSubstreamId, /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}));
  auto audio_substream_with_two_frames =
      InitObuSequence({&audio_frame_obus[0], &audio_frame_obus[1]});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());

  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(audio_substream_with_two_frames));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;

  // Call two times, each outputing an audio frame.
  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(
        ObuProcessor::ProcessTemporalUnitObu(
            audio_elements_with_data, codec_config_obus,
            substream_id_to_audio_element, parameters_manager,
            parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
            audio_frame_with_data, parameter_block_with_data,
            temporal_delimiter, continue_processing),
        IsOk());
    EXPECT_TRUE(audio_frame_with_data.has_value());
    EXPECT_FALSE(parameter_block_with_data.has_value());
    EXPECT_FALSE(temporal_delimiter.has_value());
    EXPECT_TRUE(continue_processing);
  }
}

TEST(ProcessTemporalUnitObusTest, ProcessesTemporalDelimiterObu) {
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));

  auto two_temporal_units_with_delimiter_obu =
      InitObuSequence({&audio_frame_obus[0], &temporal_delimiter_obu,
                       &audio_frame_obus[1], &temporal_delimiter_obu});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              /*param_definitions=*/{}),
              IsOk());

  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(two_temporal_units_with_delimiter_obu));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;

  // Call four times, outputing two audio frames and two temporal delimiters.
  const std::vector<bool> expecting_audio_frame = {true, false, true, false};
  const std::vector<bool> expecting_temporal_delimiter = {false, true, false,
                                                          true};
  for (int i = 0; i < 4; i++) {
    EXPECT_THAT(
        ObuProcessor::ProcessTemporalUnitObu(
            audio_elements_with_data, codec_config_obus,
            substream_id_to_audio_element, parameters_manager,
            parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
            audio_frame_with_data, parameter_block_with_data,
            temporal_delimiter, continue_processing),
        IsOk());
    EXPECT_EQ(audio_frame_with_data.has_value(), expecting_audio_frame[i]);
    EXPECT_FALSE(parameter_block_with_data.has_value());
    EXPECT_EQ(temporal_delimiter.has_value(), expecting_temporal_delimiter[i]);
    EXPECT_TRUE(continue_processing);
  }
}

TEST(ProcessTemporalUnitObusTest,
     FillsMetadataAndTimestampsForParameterBlocks) {
  constexpr DecodedUleb128 kParameterBlockId = 1;
  constexpr DecodedUleb128 kParameterBlockDuration = 10;
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);

  // Param definition.
  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kParameterBlockId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = kParameterBlockDuration;
  param_definition.constant_subblock_duration_ = kParameterBlockDuration;
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  param_definitions.emplace(kParameterBlockId, &param_definition);
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  parameter_id_to_metadata[kParameterBlockId] = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition = param_definition,
  };
  ParameterBlockObu parameter_block_obu(
      ObuHeader(), kParameterBlockId,
      parameter_id_to_metadata[kParameterBlockId]);
  EXPECT_THAT(parameter_block_obu.InitializeSubblocks(), IsOk());
  parameter_block_obu.subblocks_[0].param_data =
      std::make_unique<MixGainParameterData>(
          MixGainParameterData::kAnimateStep,
          AnimationStepInt16{.start_point_value = 99});

  // Initialize the sequence with a single parameter block.
  auto one_parameter_block_obu = InitObuSequence({&parameter_block_obu});
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(global_timing_module.Initialize(audio_elements_with_data,
                                              param_definitions),
              IsOk());

  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(one_parameter_block_obu));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, parameters_manager,
          parameter_id_to_metadata, *read_bit_buffer, global_timing_module,
          audio_frame_with_data, parameter_block_with_data, temporal_delimiter,
          continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_TRUE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  const int32_t kObuRelativeTime = 0;
  float unused_mix_gain;
  EXPECT_THAT(parameter_block_with_data->obu->GetLinearMixGain(kObuRelativeTime,
                                                               unused_mix_gain),
              IsOk());
  EXPECT_EQ(parameter_block_with_data->start_timestamp, 0);
  EXPECT_EQ(parameter_block_with_data->end_timestamp, kParameterBlockDuration);
}

// TODO(b/377772983): Test rejecting processing temporal units with mismatching
//                    durations from parameter blocks and audio frames.
// TODO(b/377772983): Test rejecting processing temporal units where the
//                    required descriptors (audio elements, codec configs, etc.)
//                    are not present.

constexpr bool kWriteWavHeader = true;
constexpr bool kDontWriteWavHeader = false;
constexpr Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};

TEST(CollectObusFromIaSequence, ConsumesIaSequenceAndCollectsAllObus) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = InitObuSequence({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  const int64_t ia_sequence_size = bitstream.size();

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, ia_sequence_header,
                                        codec_config_obus, audio_elements,
                                        mix_presentation_obus, audio_frames,
                                        parameter_blocks),
              IsOk());
  EXPECT_EQ(read_bit_buffer->Tell(), ia_sequence_size * 8);

  // Reaching the end of the stream.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
  EXPECT_TRUE(codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_TRUE(audio_elements.contains(kFirstAudioElementId));
  EXPECT_FALSE(mix_presentation_obus.empty());
  EXPECT_EQ(mix_presentation_obus.front().GetMixPresentationId(),
            kFirstMixPresentationId);
  EXPECT_FALSE(audio_frames.empty());
  EXPECT_EQ(audio_frames.front().obu.GetSubstreamId(), kFirstSubstreamId);
  EXPECT_TRUE(parameter_blocks.empty());
}

TEST(CollectObusFromIaSequence, ConsumesTrivialIaSequence) {
  const IASequenceHeaderObu input_non_redundant_ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const auto trivial_ia_sequence =
      InitObuSequence({&input_non_redundant_ia_sequence_header});
  auto non_trivial_ia_sequence = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = InitObuSequence({&audio_frame_obu});
  non_trivial_ia_sequence.insert(non_trivial_ia_sequence.end(),
                                 temporal_unit_obus.begin(),
                                 temporal_unit_obus.end());
  std::vector<uint8_t> two_ia_sequences(trivial_ia_sequence);
  const int64_t trivial_ia_sequence_size = trivial_ia_sequence.size();

  two_ia_sequences.insert(two_ia_sequences.end(),
                          non_trivial_ia_sequence.begin(),
                          non_trivial_ia_sequence.end());
  const int64_t two_ia_sequences_size = two_ia_sequences.size();

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(two_ia_sequences));
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, ia_sequence_header,
                                        codec_config_obus, audio_elements,
                                        mix_presentation_obus, audio_frames,
                                        parameter_blocks),
              IsOk());
  EXPECT_EQ(read_bit_buffer->Tell(), trivial_ia_sequence_size * 8);

  // The first IA sequence is trivial and should be consumed.
  EXPECT_TRUE(codec_config_obus.empty());
  EXPECT_TRUE(audio_elements.empty());
  EXPECT_TRUE(mix_presentation_obus.empty());
  EXPECT_TRUE(audio_frames.empty());
  EXPECT_TRUE(parameter_blocks.empty());

  // A second call retrieves the next IA sequence, which has an audio frame.
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, ia_sequence_header,
                                        codec_config_obus, audio_elements,
                                        mix_presentation_obus, audio_frames,
                                        parameter_blocks),
              IsOk());
  EXPECT_FALSE(audio_frames.empty());
  EXPECT_EQ(read_bit_buffer->Tell(), two_ia_sequences_size * 8);
}

TEST(CollectObusFromIaSequence, ConsumesUpToNextIaSequence) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = InitObuSequence({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  const int64_t first_ia_sequence_size = bitstream.size();
  const IASequenceHeaderObu non_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = false}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  auto start_of_second_ia_sequence =
      InitObuSequence({&non_redundant_ia_sequence_header});
  bitstream.insert(bitstream.end(), start_of_second_ia_sequence.begin(),
                   start_of_second_ia_sequence.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, ia_sequence_header,
                                        codec_config_obus, audio_elements,
                                        mix_presentation_obus, audio_frames,
                                        parameter_blocks),
              IsOk());

  // Expect the reader position to be right next to the end of the first IA
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), first_ia_sequence_size * 8);
}

TEST(NonStatic, CreateSucceeds) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = false;

  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(obu_processor->audio_elements_.size(), 1);
  EXPECT_EQ(obu_processor->codec_config_obus_.size(), 1);
  EXPECT_EQ(obu_processor->mix_presentations_.size(), 1);
}

TEST(NonStatic, CreateFailsOnNullReadBitBuffer) {
  bool insufficient_data = false;

  auto obu_processor = ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                                            nullptr, insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  EXPECT_FALSE(insufficient_data);
}

TEST(NonStatic, CreateFailsOnInsufficientData) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = true;

  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  EXPECT_TRUE(insufficient_data);
}

TEST(NonStatic, ProcessTemporalUnitObu) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = InitObuSequence({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = false;

  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnitObu(
                  audio_frame_with_data, parameter_block_with_data,
                  temporal_delimiter, continue_processing),
              IsOk());

  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);
}

// TODO(b/381068413): Add more tests for the new iterative API.
void RenderUsingObuProcessorExpectOk(
    absl::string_view output_filename, bool write_wav_header,
    const std::optional<uint8_t> output_file_bit_depth_override,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    std::vector<uint8_t>& bitstream_of_descriptors) {
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream_of_descriptors));
  bool insufficient_data = false;

  const std::string output_filename_string(output_filename);
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      CreateAllWavWriters(output_filename_string, write_wav_header),
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);
  absl::Span<const std::vector<int32_t>> output_rendered_pcm_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, audio_frames, parameter_blocks,
                  output_rendered_pcm_samples),
              IsOk());
  EXPECT_TRUE(output_rendered_pcm_samples.empty());
}

void RenderOneSampleFoaToStereoWavExpectOk(
    absl::string_view output_filename, bool write_wav_header,
    std::optional<uint8_t> output_file_bit_depth_override) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const auto* common_audio_element_with_data =
      &audio_elements_with_data.at(kFirstAudioElementId);
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu =
          AudioFrameObu(ObuHeader(), kFirstSubstreamId, /*audio_frame=*/{0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data = common_audio_element_with_data,
  });
  // Create a single parameter block consistent with the mix presentation OBU.
  PerIdParameterMetadata common_mix_gain_parameter_metadata = {
      .param_definition_type = ParamDefinition::kParameterDefinitionMixGain,
      .param_definition =
          mix_presentation_obus.front().sub_mixes_[0].output_mix_gain};
  std::list<ParameterBlockWithData> parameter_blocks_with_data = {};
  auto parameter_block = std::make_unique<ParameterBlockObu>(
      ObuHeader(), kCommonMixGainParameterId,
      common_mix_gain_parameter_metadata);
  EXPECT_THAT(parameter_block->InitializeSubblocks(1, 1, 1), IsOk());
  parameter_block->subblocks_[0].param_data =
      std::make_unique<MixGainParameterData>(
          MixGainParameterData::kAnimateStep,
          AnimationStepInt16{.start_point_value = 99});
  parameter_blocks_with_data.push_back(ParameterBlockWithData{
      .obu = std::move(parameter_block),
      .start_timestamp = 0,
      .end_timestamp = 1,
  });

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  RenderUsingObuProcessorExpectOk(
      output_filename, write_wav_header, output_file_bit_depth_override,
      audio_frames_with_data, parameter_blocks_with_data, bitstream);
}
TEST(RenderAudioFramesWithDataAndMeasureLoudness, RenderingNothingReturnsOk) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId, kThirdSubstreamId,
       kFourthSubstreamId},
      codec_config_obus, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};
  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  RenderUsingObuProcessorExpectOk("unused_filename", kDontWriteWavHeader,
                                  kNoOutputFileBitDepthOverride,
                                  empty_audio_frames_with_data,
                                  empty_parameter_blocks_with_data, bitstream);
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness, RendersFoaToStereoWav) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId, kThirdSubstreamId,
       kFourthSubstreamId},
      codec_config_obus, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const std::list<ParameterBlockWithData> parameter_blocks_with_data = {};
  const auto* common_audio_element_with_data =
      &audio_elements_with_data.at(kFirstAudioElementId);
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu =
          AudioFrameObu(ObuHeader(), kFirstSubstreamId, /*audio_frame=*/{0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data = common_audio_element_with_data,
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kSecondSubstreamId,
                           /*audio_frame=*/{0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data = common_audio_element_with_data,
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu =
          AudioFrameObu(ObuHeader(), kThirdSubstreamId, /*audio_frame=*/{0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data = common_audio_element_with_data,
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kFourthSubstreamId,
                           /*audio_frame=*/{0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data = common_audio_element_with_data,
  });

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  RenderUsingObuProcessorExpectOk(
      output_filename, kWriteWavHeader, kNoOutputFileBitDepthOverride,
      audio_frames_with_data, parameter_blocks_with_data, bitstream);

  const auto wav_reader = CreateWavReaderExpectOk(output_filename);
  EXPECT_EQ(wav_reader.num_channels(), 2);
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     SupportsMixGainParameterBlocks) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");

  RenderOneSampleFoaToStereoWavExpectOk(output_filename, kWriteWavHeader,
                                        kNoOutputFileBitDepthOverride);

  const auto wav_reader = CreateWavReaderExpectOk(output_filename);
  EXPECT_EQ(wav_reader.num_channels(), 2);
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness, CanWritePcmOrWav) {
  const auto output_wav_filename = GetAndCleanupOutputFileName(".wav");
  RenderOneSampleFoaToStereoWavExpectOk(output_wav_filename, kWriteWavHeader,
                                        kNoOutputFileBitDepthOverride);

  const auto wav_reader = CreateWavReaderExpectOk(output_wav_filename);
  EXPECT_EQ(wav_reader.remaining_samples(), 2);

  const auto output_pcm_filename = GetAndCleanupOutputFileName(".pcm");
  RenderOneSampleFoaToStereoWavExpectOk(
      output_pcm_filename, kDontWriteWavHeader, kNoOutputFileBitDepthOverride);

  EXPECT_TRUE(std::filesystem::exists(output_pcm_filename));
  // PCM file size excludes the header. We expect each sample to be 2 bytes.
  std::error_code error_code;
  EXPECT_EQ(std::filesystem::file_size(output_pcm_filename, error_code), 4);
  EXPECT_FALSE(error_code);
}

void AddOneLayerStereoAudioElement(
    DecodedUleb128 codec_config_id, DecodedUleb128 audio_element_id,
    uint32_t substream_id,
    const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kStereo, audio_element_id, codec_config_id,
      {substream_id}, codec_config_obus, audio_elements);
}

TEST(RenderTemporalUnitAndMeasureLoudness, RendersPassthroughStereoToPcm) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kFirstAudioElementId,
                                kFirstSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const std::list<ParameterBlockWithData> kNoParameterBlocks = {};

  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                           /*audio_frame=*/
                           {// First left sample.
                            0x11, 0x33,
                            // First right sample.
                            0x22, 0x44,
                            // Second left sample.
                            0x55, 0x77,
                            // Second right sample.
                            0x66, 0x08,
                            // Third left sample.
                            0x99, 0x0a,
                            // Third right sample.
                            0xbb, 0x0d}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kFirstAudioElementId),
  });

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));

  bool insufficient_data = false;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);
  absl::Span<const std::vector<int32_t>> output_rendered_pcm_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, audio_frames_with_data, kNoParameterBlocks,
                  output_rendered_pcm_samples),
              IsOk());

  // Outer vector is for each tick, inner vector is for each channel.
  std::vector<std::vector<int32_t>> expected_pcm_samples = {
      {0x33110000, 0x44220000},
      {0x77550000, 0x08660000},
      {0x0a990000, 0x0dbb0000},
  };
  EXPECT_EQ(output_rendered_pcm_samples, expected_pcm_samples);
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     RendersPassthroughStereoToWav) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kFirstAudioElementId,
                                kFirstSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const std::list<ParameterBlockWithData> kNoParameterBlocks = {};

  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                           /*audio_frame=*/
                           {// First left sample.
                            0x11, 0x33,
                            // First right sample.
                            0x22, 0x44,
                            // Second left sample.
                            0x55, 0x77,
                            // Second right sample.
                            0x66, 0x08,
                            // Third left sample.
                            0x99, 0x0a,
                            // Third right sample.
                            0xbb, 0x0d}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kFirstAudioElementId),
  });

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});

  RenderUsingObuProcessorExpectOk(
      output_filename, kWriteWavHeader, kNoOutputFileBitDepthOverride,
      audio_frames_with_data, kNoParameterBlocks, bitstream);

  auto wav_reader = CreateWavReaderExpectOk(output_filename, 4);
  EXPECT_EQ(wav_reader.num_channels(), 2);
  EXPECT_EQ(wav_reader.ReadFrame(), 6);
  // Validate left channel.
  EXPECT_EQ(wav_reader.buffers_[0][0], int32_t{0x33110000});
  EXPECT_EQ(wav_reader.buffers_[1][0], int32_t{0x77550000});
  EXPECT_EQ(wav_reader.buffers_[2][0], int32_t{0x0a990000});
  // Validate right channel.
  EXPECT_EQ(wav_reader.buffers_[0][1], int32_t{0x44220000});
  EXPECT_EQ(wav_reader.buffers_[1][1], int32_t{0x08660000});
  EXPECT_EQ(wav_reader.buffers_[2][1], int32_t{0x0dbb0000});
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     RendersPassthroughStereoToWav_2) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kFirstAudioElementId,
                                kFirstSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  const std::list<ParameterBlockWithData> kNoParameterBlocks = {};

  // Render using `ObuProcessor`, which closes the output WAV file upon
  // going out of scope.
  {
    auto bitstream = InitObuSequenceAddSequenceHeader(
        {&codec_config_obus.at(kFirstCodecConfigId),
         &audio_elements_with_data.at(kFirstAudioElementId).obu,
         &mix_presentation_obus.front()});
    auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
        kBufferCapacity, absl::MakeConstSpan(bitstream));
    bool insufficient_data = false;

    const std::string output_filename_string(output_filename);
    auto obu_processor = ObuProcessor::CreateForRendering(
        kStereoLayout,
        CreateAllWavWriters(output_filename_string, kWriteWavHeader),
        /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
        insufficient_data);
    ASSERT_THAT(obu_processor, NotNull());
    ASSERT_FALSE(insufficient_data);

    for (int i = 0; i < 100; ++i) {
      std::list<AudioFrameWithData> audio_frames_with_data;
      audio_frames_with_data.push_back(AudioFrameWithData{
          .obu = AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                               /*audio_frame=*/
                               std::vector<uint8_t>(8, i)),
          .start_timestamp = i,
          .end_timestamp = i + 1,
          .audio_element_with_data =
              &audio_elements_with_data.at(kFirstAudioElementId),
      });
      absl::Span<const std::vector<int32_t>> unused_output_rendered_pcm_samples;
      EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                      /*timestamp=*/i, audio_frames_with_data,
                      kNoParameterBlocks, unused_output_rendered_pcm_samples),
                  IsOk());
    }
  }

  auto wav_reader = CreateWavReaderExpectOk(output_filename, 2);
  EXPECT_EQ(wav_reader.num_channels(), 2);

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(wav_reader.ReadFrame(), 4);
    const int32_t expected_sample = (i << 16) | (i << 24);
    EXPECT_THAT(wav_reader.buffers_[0],
                std::vector<int32_t>(2, expected_sample));
    EXPECT_THAT(wav_reader.buffers_[1],
                std::vector<int32_t>(2, expected_sample));
  }
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     SelectsFirstMixPresentationWhenSupported) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kFirstAudioElementId,
                                kFirstSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kSecondAudioElementId,
                                kSecondSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                           /*audio_frame=*/{1, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kFirstAudioElementId),
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kSecondSubstreamId,
                           /*audio_frame=*/{7, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kSecondAudioElementId),
  });

  std::list<MixPresentationObu> mix_presentation_obus;
  constexpr int32_t kExpectedFirstSampleForFirstMixPresentation = 1 << 16;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, {kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &audio_elements_with_data.at(kSecondAudioElementId).obu,
       &mix_presentation_obus.front(), &mix_presentation_obus.back()});
  RenderUsingObuProcessorExpectOk(
      output_filename, kWriteWavHeader, kNoOutputFileBitDepthOverride,
      audio_frames_with_data, kNoParameterBlocks, bitstream);

  auto wav_reader = CreateWavReaderExpectOk(output_filename);
  EXPECT_EQ(wav_reader.ReadFrame(), 2);
  EXPECT_EQ(wav_reader.buffers_[0][0],
            kExpectedFirstSampleForFirstMixPresentation);
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     DoesNotSupportBaseEnhancedProfile) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kFirstAudioElementId,
                                kFirstSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kSecondAudioElementId,
                                kSecondSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kThirdAudioElementId,
                                kThirdSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                           /*audio_frame=*/{0, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kFirstAudioElementId),
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kSecondSubstreamId,
                           /*audio_frame=*/{0, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kSecondAudioElementId),
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kThirdSubstreamId,
                           /*audio_frame=*/{0, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kThirdAudioElementId),
  });

  // The only mix presentation is not suitable for simple or base profile.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId,
      {kFirstAudioElementId, kSecondAudioElementId, kThirdAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &audio_elements_with_data.at(kSecondAudioElementId).obu,
       &audio_elements_with_data.at(kThirdAudioElementId).obu,
       &mix_presentation_obus.front()});

  // Expect that the `ObuProcessor` rejects the rendering request.
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = false;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  EXPECT_FALSE(insufficient_data);
  EXPECT_THAT(obu_processor, IsNull());
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     SelectsFirstSupportedMixPresentation) {
  const auto output_filename = GetAndCleanupOutputFileName(".wav");
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kFirstAudioElementId,
                                kFirstSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kSecondAudioElementId,
                                kSecondSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  AddOneLayerStereoAudioElement(kFirstCodecConfigId, kThirdAudioElementId,
                                kThirdSubstreamId, codec_config_obus,
                                audio_elements_with_data);
  std::list<AudioFrameWithData> audio_frames_with_data;
  const std::list<ParameterBlockWithData> kNoParameterBlocks;
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                           /*audio_frame=*/{10, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kFirstAudioElementId),
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kSecondSubstreamId,
                           /*audio_frame=*/{20, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kSecondAudioElementId),
  });
  audio_frames_with_data.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kThirdSubstreamId,
                           /*audio_frame=*/{40, 0, 0, 0}),
      .start_timestamp = 0,
      .end_timestamp = 1,
      .audio_element_with_data =
          &audio_elements_with_data.at(kThirdAudioElementId),
  });
  // The first mix presentation is not suitable for simple or base profile.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId,
      {kFirstAudioElementId, kSecondAudioElementId, kThirdAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  // The second is suitable.
  constexpr int32_t kExpectedFirstSampleForFirstSupportedMixPresentation =
      30 << 16;
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, {kFirstAudioElementId, kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  // The third is also suitable, but the will not be selected.
  AddMixPresentationObuWithAudioElementIds(
      kThirdMixPresentationId, {kFirstAudioElementId, kThirdAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);

  auto mix_presentation_obus_iter = mix_presentation_obus.begin();
  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &audio_elements_with_data.at(kSecondAudioElementId).obu,
       &audio_elements_with_data.at(kThirdAudioElementId).obu,
       &(*mix_presentation_obus_iter++), &(*mix_presentation_obus_iter++),
       &(*mix_presentation_obus_iter++)});
  RenderUsingObuProcessorExpectOk(
      output_filename, kWriteWavHeader, kNoOutputFileBitDepthOverride,
      audio_frames_with_data, kNoParameterBlocks, bitstream);

  auto wav_reader = CreateWavReaderExpectOk(output_filename);
  EXPECT_EQ(wav_reader.ReadFrame(), 2);
  EXPECT_EQ(wav_reader.buffers_[0][0],
            kExpectedFirstSampleForFirstSupportedMixPresentation);
}

TEST(CreateForRendering, ForwardsArgumentsToSampleProcessorFactory) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      {kFirstSubstreamId, kSecondSubstreamId, kThirdSubstreamId,
       kFourthSubstreamId},
      codec_config_obus, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  auto bitstream = InitObuSequenceAddSequenceHeader(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data = false;

  // We expect arguments to be forwarded from the OBUs to the sample processor
  // factory.
  constexpr int kFirstSubmixIndex = 0;
  constexpr int kFirstLayoutIndex = 0;
  const auto& forwarded_layout =
      mix_presentation_obus.front().sub_mixes_[0].layouts[0].loudness_layout;
  const int32_t forwarded_sample_rate = static_cast<int32_t>(
      codec_config_obus.at(kFirstCodecConfigId).GetOutputSampleRate());
  const int32_t forwarded_bit_depth = static_cast<int32_t>(
      codec_config_obus.at(kFirstCodecConfigId).GetBitDepthToMeasureLoudness());
  const uint32_t forwarded_num_samples_per_frame =
      codec_config_obus.at(kFirstCodecConfigId).GetNumSamplesPerFrame();

  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(
      mock_sample_processor_factory,
      Call(kFirstMixPresentationId, kFirstSubmixIndex, kFirstLayoutIndex,
           forwarded_layout, /*num_channels=*/2, forwarded_sample_rate,
           forwarded_bit_depth, forwarded_num_samples_per_frame));
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory = mock_sample_processor_factory.AsStdFunction();

  EXPECT_THAT(ObuProcessor::CreateForRendering(
                  kStereoLayout, sample_processor_factory,
                  /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
                  insufficient_data),
              NotNull());
}

}  // namespace
}  // namespace iamf_tools
