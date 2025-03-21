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

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
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
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
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
constexpr uint32_t kFrameSize = 1024;
constexpr uint32_t kBitDepth = 16;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

constexpr DecodedUleb128 kImplicitSubstreamId = 0;

constexpr int kObuTypeBitShift = 3;
constexpr int64_t kBufferCapacity = 1024;

constexpr std::optional<uint8_t> kNoOutputFileBitDepthOverride = std::nullopt;
constexpr std::array<uint8_t, 16> kArbitraryAudioFrame = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

std::vector<uint8_t> AddSequenceHeaderAndSerializeObusExpectOk(
    const std::list<const ObuBase*>& input_ia_sequence_without_header) {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(input_ia_sequence_without_header);
  input_ia_sequence.push_front(&ia_sequence_header);
  return SerializeObusExpectOk(input_ia_sequence);
}

auto CreateAllWavWriters(const std::string output_filename_string,
                         bool write_wav_header) {
  return [output_filename_string, write_wav_header](
             DecodedUleb128 /*mix_presentation_id*/, int /*sub_mix_index*/,
             int /*layout_index*/, const Layout& /*layout*/, int num_channels,
             int sample_rate, int bit_depth, size_t max_input_samples_per_frame)
             -> std::unique_ptr<SampleProcessorBase> {
    return WavWriter::Create(output_filename_string, num_channels, sample_rate,
                             bit_depth, max_input_samples_per_frame,
                             write_wav_header);
  };
}

TEST(ProcessDescriptorObus, FailsWithEmptyBitstream) {
  const std::vector<uint8_t> bitstream_without_ia_sequence_header =
      SerializeObusExpectOk({});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obu;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(bitstream_without_ia_sequence_header));
  bool insufficient_data;
  EXPECT_FALSE(ObuProcessor::ProcessDescriptorObus(
                   /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                   ia_sequence_header, codec_config_obu,
                   audio_elements_with_data, mix_presentation_obus,
                   insufficient_data)
                   .ok());
  // There's no data (and `is_exhaustive_and_exact` is false), so we need more
  // data to proceed.
  EXPECT_TRUE(insufficient_data);
}

TEST(ProcessDescriptorObus, CollectsCodecConfigsBeforeATemporalUnit) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  AudioFrameObu input_audio_frame(
      ObuHeader(), kFirstSubstreamId, /*audio_frame=*/
      {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  const auto two_codec_configs_and_audio_frame =
      AddSequenceHeaderAndSerializeObusExpectOk(
          {&input_codec_configs.at(kFirstCodecConfigId),
           &input_codec_configs.at(kSecondCodecConfigId), &input_audio_frame});
  IASequenceHeaderObu unused_ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> output_codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(two_codec_configs_and_audio_frame));
  bool insufficient_data;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          unused_ia_sequence_header, output_codec_config_obus,
          audio_elements_with_data, mix_presentation_obus, insufficient_data),
      IsOk());

  EXPECT_EQ(output_codec_config_obus.size(), 2);
  EXPECT_TRUE(output_codec_config_obus.contains(kFirstCodecConfigId));
  EXPECT_TRUE(output_codec_config_obus.contains(kSecondCodecConfigId));
  // `insufficient_data` is false because we have successfully read all provided
  // descriptor obus AND `is_exhaustive_and_exact` is true, meaning that the
  // caller has indicated that there are no future Descriptor OBUs coming.
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, CollectsCodecConfigsAtEndOfBitstream) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  const auto two_codec_configs_at_end_of_bitstream =
      AddSequenceHeaderAndSerializeObusExpectOk(
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
  bool insufficient_data;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      IsOk());
  // `is_exhaustive_and_exact` is true so it could not be a more-data situation.
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
  const auto two_codec_configs_at_end_of_bitstream =
      AddSequenceHeaderAndSerializeObusExpectOk(
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
  bool insufficient_data;
  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));
  // `is_exhaustive_and_exact` is false so we won't know it's the end of the
  // bitstream until we see a temporal unit.  Need more data to know we're done.
  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(codec_config_obus.size(), 0);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(ProcessDescriptorObus, CollectsIaSequenceHeaderWithoutOtherObus) {
  const auto only_ia_sequence_header =
      AddSequenceHeaderAndSerializeObusExpectOk({});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(only_ia_sequence_header));
  bool insufficient_data;
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
  EXPECT_FALSE(insufficient_data);
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
  const auto ia_sequence_header_then_codec_config =
      SerializeObusExpectOk({&input_ia_sequence_header,
                             &input_codec_configs.at(kFirstCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity,
      absl::MakeConstSpan(ia_sequence_header_then_codec_config));
  bool insufficient_data;
  EXPECT_THAT(ObuProcessor::ProcessDescriptorObus(
                  /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                  unused_ia_sequence_header, unused_codec_config_obus,
                  unused_audio_elements_with_data, unused_mix_presentation_obus,
                  insufficient_data),
              IsOk());
  EXPECT_FALSE(insufficient_data);
  // The reverse order is not valid according to
  // https://aomediacodec.github.io/iamf/#standalone-descriptor-obus
  const auto codec_config_then_ia_sequence_header =
      SerializeObusExpectOk({&input_codec_configs.at(kFirstCodecConfigId),
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
  // `insufficient_data` is false as the error was due to an invalid ordering of
  // OBUs, rather than not having enough data.
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, SucceedsWithSuccessiveRedundantSequenceHeaders) {
  const IASequenceHeaderObu input_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&input_redundant_ia_sequence_header});
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
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
  auto buffer =
      SerializeObusExpectOk({&input_non_redundant_ia_sequence_header});
  const int64_t first_ia_sequence_size = buffer.size();

  // Add a second non-redundant sequence header.
  const auto second_non_redundant_ia_sequence =
      SerializeObusExpectOk({&input_non_redundant_ia_sequence_header});
  buffer.insert(buffer.end(), second_non_redundant_ia_sequence.begin(),
                second_non_redundant_ia_sequence.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
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
  const auto ia_sequence_header_with_codec_configs =
      AddSequenceHeaderAndSerializeObusExpectOk(
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
  bool insufficient_data;
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

  return AddSequenceHeaderAndSerializeObusExpectOk(
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
  bool insufficient_data;

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
  bool insufficient_data;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  // We've received a valid bitstream so far but not complete.
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
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
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
  bool insufficient_data;

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
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  std::list<MixPresentationObu> mix_presentation_obus;

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

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
  bool insufficient_data;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  // We've received a valid bitstream so far but not complete.
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
  bool insufficient_data;

  EXPECT_THAT(
      ObuProcessor::ProcessDescriptorObus(
          /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
          ia_sequence_header, codec_config_obus, audio_elements_with_data,
          mix_presentation_obus, insufficient_data),
      Not(IsOk()));

  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(codec_config_obus.size(), 0);
  EXPECT_EQ(audio_elements_with_data.size(), 0);
  EXPECT_EQ(mix_presentation_obus.size(), 0);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(ProcessTemporalUnitObus, OkAndProducesNoObusIfEmpty) {
  const auto empty_temporal_unit = SerializeObusExpectOk({});
  auto empty_read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(empty_temporal_unit));
  const absl::flat_hash_map<DecodedUleb128, CodecConfigObu> kNoCodecConfigs =
      {};
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kNoAudioElementsWithData = {};
  auto global_timing_module =
      GlobalTimingModule::Create(kNoAudioElementsWithData,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;

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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *empty_read_bit_buffer, *global_timing_module, audio_frame_with_data,
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

  const auto one_temporal_unit = SerializeObusExpectOk({&audio_frame_obu});

  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(one_temporal_unit));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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

  const auto one_temporal_unit_before_non_redundant_descriptor_obu =
      SerializeObusExpectOk(
          {&audio_frame_obu, &non_redundant_ia_sequence_header});

  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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

  const auto one_temporal_unit_before_redundant_descriptor_obu =
      SerializeObusExpectOk({&audio_frame_obu, &redundant_ia_sequence_header,
                             &redundant_codec_config});

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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

  const auto temporal_unit_with_non_redundant_codec_config_obu =
      SerializeObusExpectOk({&audio_frame_obu, &non_redundant_codec_config});

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Process again, this time the non-redundant Codec Config OBU is read and
  // the function fails.
  EXPECT_FALSE(ObuProcessor::ProcessTemporalUnitObu(
                   audio_elements_with_data, codec_config_obus,
                   substream_id_to_audio_element, param_definitions,
                   parameters_manager, *read_bit_buffer, *global_timing_module,
                   audio_frame_with_data, parameter_block_with_data,
                   temporal_delimiter, continue_processing)
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

  const auto temporal_unit_with_reserved_obus =
      SerializeObusExpectOk({&reserved_obu_before_audio_frame, &audio_frame_obu,
                             &reserved_obu_after_audio_frame});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Second call: reading the Audio Frame OBU.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_TRUE(continue_processing);

  // Third call: reading and discarding the reserved OBU.
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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
  const auto multiple_audio_substreams = SerializeObusExpectOk(
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
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element},
          {kSecondSubstreamId, first_audio_element},
          {kImplicitSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(multiple_audio_substreams));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;

  // Call three times, each outputing an audio frame.
  for (int i = 0; i < 3; i++) {
    EXPECT_THAT(ObuProcessor::ProcessTemporalUnitObu(
                    audio_elements_with_data, codec_config_obus,
                    substream_id_to_audio_element, param_definitions,
                    parameters_manager, *read_bit_buffer, *global_timing_module,
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
  const auto audio_substream_with_two_frames =
      SerializeObusExpectOk({&audio_frame_obus[0], &audio_frame_obus[1]});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(audio_substream_with_two_frames));

  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;

  // Call two times, each outputing an audio frame.
  for (int i = 0; i < 2; i++) {
    EXPECT_THAT(ObuProcessor::ProcessTemporalUnitObu(
                    audio_elements_with_data, codec_config_obus,
                    substream_id_to_audio_element, param_definitions,
                    parameters_manager, *read_bit_buffer, *global_timing_module,
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

  const auto two_temporal_units_with_delimiter_obu =
      SerializeObusExpectOk({&audio_frame_obus[0], &temporal_delimiter_obu,
                             &audio_frame_obus[1], &temporal_delimiter_obu});
  // Set up inputs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  const auto* first_audio_element =
      &audio_elements_with_data.at(kFirstAudioElementId);
  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId, first_audio_element}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
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
    EXPECT_THAT(ObuProcessor::ProcessTemporalUnitObu(
                    audio_elements_with_data, codec_config_obus,
                    substream_id_to_audio_element, param_definitions,
                    parameters_manager, *read_bit_buffer, *global_timing_module,
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
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kParameterBlockId, param_definition);
  ParameterBlockObu parameter_block_obu(ObuHeader(), kParameterBlockId,
                                        param_definition);
  EXPECT_THAT(parameter_block_obu.InitializeSubblocks(), IsOk());
  parameter_block_obu.subblocks_[0].param_data =
      std::make_unique<MixGainParameterData>(
          MixGainParameterData::kAnimateStep,
          AnimationStepInt16{.start_point_value = 99});

  // Initialize the sequence with a single parameter block.
  const auto one_parameter_block_obu =
      SerializeObusExpectOk({&parameter_block_obu});
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data, param_definitions);
  ASSERT_THAT(global_timing_module, NotNull());
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
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
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

TEST(ProcessTemporalUnitObus,
     ConsumesAllTemporalUnitsWithAnIncompleteHeaderAtEnd) {
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);

  auto one_temporal_unit = SerializeObusExpectOk({&audio_frame_obu});

  // Set up inputs with descriptors, one audio frame, and one incomplete header.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  // Add a single byte to the end of the temporal unit to represent an
  // incomplete header (A header requires at least 2 bytes).
  one_temporal_unit.push_back(0);
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(one_temporal_unit));

  // Confirm that the first temporal unit is processed successfully.
  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());

  // Confirm that the second temporal unit it is incomplete.
  auto start_position = read_bit_buffer->Tell();
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_FALSE(continue_processing);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(ProcessTemporalUnitObus,
     ConsumesAllTemporalUnitsWithAnIncompleteObuAtEnd) {
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);

  auto ia_sequence = SerializeObusExpectOk({&audio_frame_obu});

  // Set up inputs with descriptors, one audio frame, and one incomplete obu
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_with_data,
                                 /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());

  const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element = {
          {kFirstSubstreamId,
           &audio_elements_with_data.at(kFirstAudioElementId)}};
  ParametersManager parameters_manager(audio_elements_with_data);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  std::vector<uint8_t> extra_audio_frame_obu_header_bytes = {
      kObuIaAudioFrameId0 << kObuTypeBitShift,
      // `obu_size`. -> Non-zero size, but we have no bytes following.
      0x7f};
  ia_sequence.insert(ia_sequence.end(),
                     extra_audio_frame_obu_header_bytes.begin(),
                     extra_audio_frame_obu_header_bytes.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(ia_sequence));

  // Confirm that the first temporal unit is processed successfully.
  bool continue_processing = true;
  std::optional<AudioFrameWithData> audio_frame_with_data;
  std::optional<ParameterBlockWithData> parameter_block_with_data;
  std::optional<TemporalDelimiterObu> temporal_delimiter;
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_TRUE(audio_frame_with_data.has_value());

  // Confirm that the second temporal unit it is incomplete.
  auto start_position = read_bit_buffer->Tell();
  EXPECT_THAT(
      ObuProcessor::ProcessTemporalUnitObu(
          audio_elements_with_data, codec_config_obus,
          substream_id_to_audio_element, param_definitions, parameters_manager,
          *read_bit_buffer, *global_timing_module, audio_frame_with_data,
          parameter_block_with_data, temporal_delimiter, continue_processing),
      IsOk());
  EXPECT_FALSE(audio_frame_with_data.has_value());
  EXPECT_FALSE(parameter_block_with_data.has_value());
  EXPECT_FALSE(temporal_delimiter.has_value());
  EXPECT_FALSE(continue_processing);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

using OutputTemporalUnit = ObuProcessor::OutputTemporalUnit;

TEST(ProcessTemporalUnit, ConsumesOneAudioFrameAsTemporalUnit) {
  // Set up inputs with a single audio frame.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);
  auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  // Call `ProcessTemporalUnit()` with `eos_is_end_of_sequence` set to true.
  // This means that we can assume that the end of the stream implies the end of
  // the temporal unit.
  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
}

TEST(ProcessTemporalUnit, DoesNotConsumeOneAudioFrameAsTemporalUnit) {
  // Set up inputs with a single audio frame.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, ConsumesMultipleTemporalUnitsWithTemporalDelimiters) {
  // Set up inputs with two audio frames and temporal delimiters.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId, kArbitraryAudioFrame));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId, kArbitraryAudioFrame));
  const auto two_temporal_units_with_delimiter_obu =
      SerializeObusExpectOk({&audio_frame_obus[0], &temporal_delimiter_obu,
                             &audio_frame_obus[1], &temporal_delimiter_obu});
  bitstream.insert(bitstream.end(),
                   two_temporal_units_with_delimiter_obu.begin(),
                   two_temporal_units_with_delimiter_obu.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  // The first temporal unit is consumed; it should only contain the first
  // audio frame.
  EXPECT_TRUE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);

  output_temporal_unit.reset();
  continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());
  // Seeing a temporal delimiter at the end of the stream implies that the
  // stream is incomplete.
  EXPECT_TRUE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
}

TEST(ProcessTemporalUnit,
     ConsumesMultipleTemporalUnitsWithoutTemporalDelimiters) {
  // Set up inputs with two audio frames. Two audio frames are known to be in a
  // separate temporal unit if they have the same substream ID. Their underlying
  // timestamps are different.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId, kArbitraryAudioFrame));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId, kArbitraryAudioFrame));
  const auto two_temporal_units =
      SerializeObusExpectOk({&audio_frame_obus[0], &audio_frame_obus[1]});
  bitstream.insert(bitstream.end(), two_temporal_units.begin(),
                   two_temporal_units.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  // The first temporal unit is consumed; it should only contain the first
  // audio frame.
  EXPECT_TRUE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);

  output_temporal_unit.reset();
  continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
}

TEST(ProcessTemporalUnit, ConsumesOnlyOneTemporalUnitFromTwoAudioFrames) {
  // eos_is_end_of_sequence is false. Only one temporal unit is consumed because
  // we don't know that the second temporal unit is finished.

  // Set up inputs with two audio frames. Two audio
  // frames are known to be in a separate temporal unit if they have the same
  // substream ID. Their underlying timestamps are different.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  const auto two_temporal_units =
      SerializeObusExpectOk({&audio_frame_obus[0], &audio_frame_obus[1]});
  bitstream.insert(bitstream.end(), two_temporal_units.begin(),
                   two_temporal_units.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  // The first temporal unit is consumed; it should only contain the first
  // audio frame.
  EXPECT_TRUE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);

  output_temporal_unit.reset();
  continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit,
     ConsumesOnlyOneTemporalUnitFromTwoAudioFramesAndIncompleteObuAtEnd) {
  // eos_is_end_of_sequence is false. Only one temporal unit is consumed because
  // we don't know that the second temporal unit is finished.

  // Set up inputs with two audio frames. Two audio
  // frames are known to be in a separate temporal unit if they have the same
  // substream ID. Their underlying timestamps are different.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  auto two_temporal_units =
      SerializeObusExpectOk({&audio_frame_obus[0], &audio_frame_obus[1]});
  std::vector<uint8_t> extra_audio_frame_obu_header_bytes = {
      kObuIaAudioFrameId0 << kObuTypeBitShift,
      // `obu_size`. -> Non-zero size, but we have no bytes following.
      0x7f};
  two_temporal_units.insert(two_temporal_units.end(),
                            extra_audio_frame_obu_header_bytes.begin(),
                            extra_audio_frame_obu_header_bytes.end());
  bitstream.insert(bitstream.end(), two_temporal_units.begin(),
                   two_temporal_units.end());
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = false;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  // The first temporal unit is consumed; it should only contain the first
  // audio frame.
  EXPECT_TRUE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);

  output_temporal_unit.reset();
  continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  // The second temporal unit is not consumed since we don't know that it is
  // complete.
  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, ConsumesMultipleTemporalUnitsChunkedArbitrarily) {
  // Set up inputs with two audio frames. Two audio frames are known to be in a
  // separate temporal unit if they have the same substream ID. Their underlying
  // timestamps are different.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer = StreamBasedReadBitBuffer::Create(kBufferCapacity);
  // Push descriptors.
  ASSERT_THAT(read_bit_buffer->PushBytes(bitstream), IsOk());
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId,
                    /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8}));
  const auto two_temporal_units =
      SerializeObusExpectOk({&audio_frame_obus[0], &audio_frame_obus[1]});

  // Split the temporal units into three chunks.
  const int64_t chunk_size = two_temporal_units.size() / 3;
  std::vector<uint8_t> chunk_1(two_temporal_units.begin(),
                               two_temporal_units.begin() + chunk_size);
  std::vector<uint8_t> chunk_2(two_temporal_units.begin() + chunk_size,
                               two_temporal_units.begin() + 2 * chunk_size);
  std::vector<uint8_t> chunk_3(two_temporal_units.begin() + 2 * chunk_size,
                               two_temporal_units.end());

  // Chunk 1.
  ASSERT_THAT(read_bit_buffer->PushBytes(chunk_1), IsOk());
  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  // Chunk 1 is not enough to finish reading the first audio frame, so the
  // first temporal unit is not finished.
  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());

  // Chunk 2.
  ASSERT_THAT(read_bit_buffer->PushBytes(chunk_2), IsOk());
  continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  // Chunk 2 is enough to finish reading the first audio frame, but not the
  // second. Since we haven't finished reading the second audio frame, we cannot
  // know that the first temporal unit is complete. Therefore we still do not
  // have a temporal unit.
  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());

  // Chunk 3.
  ASSERT_THAT(read_bit_buffer->PushBytes(chunk_3), IsOk());
  continue_processing = true;
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/false, output_temporal_unit,
                  continue_processing),
              IsOk());

  // Chunk 3 is enough to finish reading the second audio frame, so the first
  // temporal unit is now complete. But we don't know that the second temporal
  // unit is complete since more data could be coming behind it.
  EXPECT_TRUE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);

  // To get the second temporal unit, we make one final call with
  // `eos_is_end_of_sequence` set to true. At this point, the bitstream is
  // exhausted, but we can get the second temporal unit that we previously
  // processed since we now know that the sequence is complete.
  continue_processing = true;
  output_temporal_unit.reset();
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  EXPECT_FALSE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
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
                                kArbitraryAudioFrame);
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
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
      SerializeObusExpectOk({&input_non_redundant_ia_sequence_header});
  auto non_trivial_ia_sequence = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
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
                                kArbitraryAudioFrame);
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  const int64_t first_ia_sequence_size = bitstream.size();
  const IASequenceHeaderObu non_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = false}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const auto start_of_second_ia_sequence =
      SerializeObusExpectOk({&non_redundant_ia_sequence_header});
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

TEST(Create, Succeeds) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(obu_processor->audio_elements_.size(), 1);
  EXPECT_EQ(obu_processor->codec_config_obus_.size(), 1);
  EXPECT_EQ(obu_processor->mix_presentations_.size(), 1);
}

TEST(Create, SucceedsForTrivialIaSequence) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  auto buffer = SerializeObusExpectOk({&kIaSequenceHeader});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, FailsOnNullReadBitBuffer) {
  bool insufficient_data;

  auto obu_processor = ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                                            nullptr, insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, FailsOnInsufficientData) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
}

TEST(GetOutputSampleRate, ReturnsSampleRateBasedOnCodecConfigObu) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  const auto buffer = SerializeObusExpectOk(
      {&kIaSequenceHeader, &codec_config_obus.at(kFirstCodecConfigId)});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputSampleRate(), IsOkAndHolds(kSampleRate));
}

TEST(GetOutputSampleRate, FailsForTrivialIaSequence) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const auto buffer = SerializeObusExpectOk({&kIaSequenceHeader});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputSampleRate(), Not(IsOk()));
}

TEST(GetOutputSampleRate, FailsForMultipleCodecConfigObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus);
  const auto buffer = SerializeObusExpectOk(
      {&kIaSequenceHeader, &codec_config_obus.at(kFirstCodecConfigId),
       &codec_config_obus.at(kSecondCodecConfigId)});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputSampleRate(), Not(IsOk()));
}

TEST(GetOutputFrameSize, ReturnsSampleRateBasedOnCodecConfigObu) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfig(kFirstCodecConfigId, kFrameSize, kBitDepth, kSampleRate,
                     codec_config_obus);
  const auto buffer = SerializeObusExpectOk(
      {&kIaSequenceHeader, &codec_config_obus.at(kFirstCodecConfigId)});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputSampleRate(), IsOkAndHolds(kSampleRate));
}

TEST(GetOutputFrameSize, FailsForTrivialIaSequence) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const auto buffer = SerializeObusExpectOk({&kIaSequenceHeader});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputFrameSize(), Not(IsOk()));
}

TEST(GetOutputFrameSize, FailsForMultipleCodecConfigObus) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus);
  const auto buffer = SerializeObusExpectOk(
      {&kIaSequenceHeader, &codec_config_obus.at(kFirstCodecConfigId),
       &codec_config_obus.at(kSecondCodecConfigId)});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputFrameSize(), Not(IsOk()));
}

TEST(NonStatic, ProcessTemporalUnitObu) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

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
    const std::vector<uint8_t>& bitstream_of_descriptors) {
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream_of_descriptors));
  bool insufficient_data;

  const std::string output_filename_string(output_filename);
  Layout unused_output_layout;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      CreateAllWavWriters(output_filename_string, write_wav_header),
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      unused_output_layout, insufficient_data);
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
  std::list<ParameterBlockWithData> parameter_blocks_with_data = {};
  auto parameter_block = std::make_unique<ParameterBlockObu>(
      ObuHeader(), kCommonMixGainParameterId,
      mix_presentation_obus.front().sub_mixes_[0].output_mix_gain);
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
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
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));

  Layout unused_output_layout;
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      unused_output_layout, insufficient_data);
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
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
    const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
        {&codec_config_obus.at(kFirstCodecConfigId),
         &audio_elements_with_data.at(kFirstAudioElementId).obu,
         &mix_presentation_obus.front()});
    auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
        kBufferCapacity, absl::MakeConstSpan(bitstream));

    Layout unused_output_layout;
    bool insufficient_data;
    const std::string output_filename_string(output_filename);
    auto obu_processor = ObuProcessor::CreateForRendering(
        kStereoLayout,
        CreateAllWavWriters(output_filename_string, kWriteWavHeader),
        /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
        unused_output_layout, insufficient_data);
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

  auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &audio_elements_with_data.at(kSecondAudioElementId).obu,
       &audio_elements_with_data.at(kThirdAudioElementId).obu,
       &mix_presentation_obus.front()});

  // Expect that the `ObuProcessor` rejects the rendering request.
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  Layout unused_output_layout;
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kStereoLayout,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      unused_output_layout, insufficient_data);
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
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

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

  Layout unused_output_layout;
  EXPECT_THAT(ObuProcessor::CreateForRendering(
                  kStereoLayout, sample_processor_factory,
                  /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
                  unused_output_layout, insufficient_data),
              NotNull());
}

using testing::_;

constexpr Layout k5_1_Layout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0}};

TEST(CreateForRendering, ForwardsChosenLayoutToSampleProcessorFactory) {
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
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      sound_system_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, sound_system_layouts,
      mix_presentation_obus);

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  // We expect to use the second layout, since this is the only one that matches
  // the desired layout.
  constexpr int kSubmixIndex = 0;
  constexpr int kLayoutIndex = 1;
  const auto& forwarded_layout =
      mix_presentation_obus.front().sub_mixes_[0].layouts[1].loudness_layout;

  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(mock_sample_processor_factory,
              Call(kFirstMixPresentationId, kSubmixIndex, kLayoutIndex,
                   forwarded_layout, /*num_channels=*/6, _, _, _));
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory = mock_sample_processor_factory.AsStdFunction();

  Layout output_layout;
  EXPECT_THAT(ObuProcessor::CreateForRendering(
                  k5_1_Layout, sample_processor_factory,
                  /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
                  output_layout, insufficient_data),
              NotNull());
  EXPECT_EQ(output_layout, k5_1_Layout);
}

TEST(CreateForRendering, ForwardsDefaultLayoutToSampleProcessorFactory) {
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
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      sound_system_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, sound_system_layouts,
      mix_presentation_obus);

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  // We expect to use the first layout as default, since the desired layout is
  // not available in the mix presentation.
  constexpr int kSubmixIndex = 0;
  constexpr int kLayoutIndex = 0;
  const auto& forwarded_layout =
      mix_presentation_obus.front().sub_mixes_[0].layouts[0].loudness_layout;

  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(mock_sample_processor_factory,
              Call(kFirstMixPresentationId, kSubmixIndex, kLayoutIndex,
                   forwarded_layout, /*num_channels=*/2, _, _, _));
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory = mock_sample_processor_factory.AsStdFunction();

  Layout unused_output_layout;
  EXPECT_THAT(ObuProcessor::CreateForRendering(
                  k5_1_Layout, sample_processor_factory,
                  /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
                  unused_output_layout, insufficient_data),
              NotNull());
}

TEST(CreateForRendering,
     ForwardsChosenLayoutToSampleProcessorFactoryWithMultipleMixPresentations) {
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
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      sound_system_layouts_first_mix_presentation = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystem10_2_7_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      sound_system_layouts_first_mix_presentation, mix_presentation_obus);
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      sound_system_layouts_second_mix_presentation = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kSecondMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      sound_system_layouts_second_mix_presentation, mix_presentation_obus);

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front(),
       &*(std::next(mix_presentation_obus.begin()))});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      kBufferCapacity, absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  // We expect to use the second layout in the second mix presentation, since
  // this is the only one that matches the desired layout.
  constexpr int kSubmixIndex = 0;
  constexpr int kLayoutIndex = 1;
  const auto& forwarded_layout = (std::next(mix_presentation_obus.begin()))
                                     ->sub_mixes_[0]
                                     .layouts[1]
                                     .loudness_layout;

  MockSampleProcessorFactory mock_sample_processor_factory;
  EXPECT_CALL(mock_sample_processor_factory,
              Call(kSecondMixPresentationId, kSubmixIndex, kLayoutIndex,
                   forwarded_layout, /*num_channels=*/6, _, _, _));
  RenderingMixPresentationFinalizer::SampleProcessorFactory
      sample_processor_factory = mock_sample_processor_factory.AsStdFunction();

  Layout output_layout;
  EXPECT_THAT(ObuProcessor::CreateForRendering(
                  k5_1_Layout, sample_processor_factory,
                  /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
                  output_layout, insufficient_data),
              NotNull());
  EXPECT_EQ(output_layout, k5_1_Layout);
}

TEST(CreateForRendering, NullReadBitBufferRejected) {
  MockSampleProcessorFactory mock_sample_processor_factory;
  auto sample_processor_factory = mock_sample_processor_factory.AsStdFunction();
  ReadBitBuffer* read_bit_buffer_nullptr = nullptr;
  bool insufficient_data;

  Layout unused_output_layout;
  EXPECT_THAT(ObuProcessor::CreateForRendering(
                  kStereoLayout, sample_processor_factory,
                  /*is_exhaustive_and_exact=*/true, read_bit_buffer_nullptr,
                  unused_output_layout, insufficient_data),
              IsNull());
  EXPECT_FALSE(insufficient_data);
}

}  // namespace
}  // namespace iamf_tools
