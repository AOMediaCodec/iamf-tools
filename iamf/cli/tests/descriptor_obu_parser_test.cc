/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/descriptor_obu_parser.h"

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Key;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr DecodedUleb128 kSecondCodecConfigId = 2;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;
constexpr int kObuTypeBitShift = 3;

std::vector<uint8_t> AddSequenceHeaderAndSerializeObusExpectOk(
    const std::list<const ObuBase*>& input_ia_sequence_without_header) {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(input_ia_sequence_without_header);
  input_ia_sequence.push_front(&ia_sequence_header);
  return SerializeObusExpectOk(input_ia_sequence);
}

TEST(ProcessDescriptorObus, FailsWithEmptyBitstream) {
  DescriptorObuParser parser;
  const std::vector<uint8_t> bitstream_without_ia_sequence_header =
      SerializeObusExpectOk({});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(bitstream_without_ia_sequence_header));
  bool insufficient_data;
  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                       insufficient_data)
                   .ok());
  // There's no data (and `is_exhaustive_and_exact` is false), so we need more
  // data to proceed.
  EXPECT_TRUE(insufficient_data);
}

TEST(ProcessDescriptorObus, CollectsCodecConfigsBeforeATemporalUnit) {
  DescriptorObuParser parser;
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

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(two_codec_configs_and_audio_frame));
  bool insufficient_data;
  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/false, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());

  EXPECT_THAT(parsed_obus->codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId),
                                           Key(kSecondCodecConfigId))));
  // `insufficient_data` is false because we have successfully read all provided
  // descriptor obus up to the temporal unit.
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, IgnoresImplausibleCodecConfigObus) {
  DescriptorObuParser parser;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  std::vector<uint8_t> bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&input_codec_configs.at(kFirstCodecConfigId)});
  // Insert an invalid tiny Codec Config OBU. This is too small to be
  // syntactically valid.
  const std::vector<uint8_t> tiny_invalid_codec_config = {
      // First byte of the OBU header.
      0x00,
      // `obu_size`
      0x02,
      // `codec_config_id`.
      0x09,
      // Implausibly small `codec_id`.
      0x00};
  bitstream.insert(bitstream.end(), tiny_invalid_codec_config.begin(),
                   tiny_invalid_codec_config.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/true, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());

  // We only find the valid Codec Config OBU, with no sign of the tiny one.
  EXPECT_THAT(parsed_obus->codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  // The buffer advanced past the tiny Codec Config OBU.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
}

TEST(ProcessDescriptorObus, CollectsCodecConfigsAtEndOfBitstream) {
  DescriptorObuParser parser;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  const auto two_codec_configs_at_end_of_bitstream =
      AddSequenceHeaderAndSerializeObusExpectOk(
          {&input_codec_configs.at(kFirstCodecConfigId),
           &input_codec_configs.at(kSecondCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(two_codec_configs_at_end_of_bitstream));
  bool insufficient_data;
  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/true, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());
  // `is_exhaustive_and_exact` is true so it could not be a more-data situation.
  EXPECT_FALSE(insufficient_data);

  EXPECT_THAT(parsed_obus->codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId),
                                           Key(kSecondCodecConfigId))));
}

TEST(ProcessDescriptorObus,
     DoesNotCollectCodecConfigsAtEndOfBitstreamWithoutIsExhaustiveAndExact) {
  DescriptorObuParser parser;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  const auto two_codec_configs_at_end_of_bitstream =
      AddSequenceHeaderAndSerializeObusExpectOk(
          {&input_codec_configs.at(kFirstCodecConfigId),
           &input_codec_configs.at(kSecondCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(two_codec_configs_at_end_of_bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                       insufficient_data)
                   .ok());
  // `is_exhaustive_and_exact` is false so we won't know it's the end of the
  // bitstream until we see a temporal unit.  Need more data to know we're done.
  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(ProcessDescriptorObus, CollectsIaSequenceHeaderWithoutOtherObus) {
  DescriptorObuParser parser;
  const auto only_ia_sequence_header =
      AddSequenceHeaderAndSerializeObusExpectOk({});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(only_ia_sequence_header));
  bool insufficient_data;
  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/true, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());

  EXPECT_EQ(parsed_obus->ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(parsed_obus->ia_sequence_header.GetAdditionalProfile(),
            ProfileVersion::kIamfBaseProfile);
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, DescriptorObusMustStartWithIaSequenceHeader) {
  DescriptorObuParser parser;
  const IASequenceHeaderObu input_ia_sequence_header(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);

  // Descriptor OBUs must start with IA Sequence Header.
  const auto ia_sequence_header_then_codec_config =
      SerializeObusExpectOk({&input_ia_sequence_header,
                             &input_codec_configs.at(kFirstCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(ia_sequence_header_then_codec_config));
  bool insufficient_data;
  EXPECT_THAT(parser.ProcessDescriptorObus(
                  /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                  insufficient_data),
              IsOk());
  EXPECT_FALSE(insufficient_data);
  // The reverse order is not valid according to
  // https://aomediacodec.github.io/iamf/#standalone-descriptor-obus
  const auto codec_config_then_ia_sequence_header =
      SerializeObusExpectOk({&input_codec_configs.at(kFirstCodecConfigId),
                             &input_ia_sequence_header});

  read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(codec_config_then_ia_sequence_header));
  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                       insufficient_data)
                   .ok());
  // `insufficient_data` is false as the error was due to an invalid ordering of
  // OBUs, rather than not having enough data.
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, SucceedsWithSuccessiveRedundantSequenceHeaders) {
  DescriptorObuParser parser;
  const IASequenceHeaderObu input_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = true}, ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&input_redundant_ia_sequence_header});

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  EXPECT_THAT(parser.ProcessDescriptorObus(
                  /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                  insufficient_data),
              IsOk());
  EXPECT_FALSE(insufficient_data);
}

TEST(ProcessDescriptorObus, ConsumesUpToNextNonRedundantSequenceHeader) {
  DescriptorObuParser parser;
  const IASequenceHeaderObu input_non_redundant_ia_sequence_header(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);
  auto buffer =
      SerializeObusExpectOk({&input_non_redundant_ia_sequence_header});
  const int64_t first_ia_sequence_size = buffer.size();

  // Add a second non-redundant sequence header.
  const auto second_non_redundant_ia_sequence =
      SerializeObusExpectOk({&input_non_redundant_ia_sequence_header});
  buffer.insert(buffer.end(), second_non_redundant_ia_sequence.begin(),
                second_non_redundant_ia_sequence.end());

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
  bool insufficient_data;
  EXPECT_THAT(parser.ProcessDescriptorObus(
                  /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                  insufficient_data),
              IsOk());
  EXPECT_FALSE(insufficient_data);

  // Expect the reader position to be right next to the end of the first IA
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), first_ia_sequence_size * 8);
}

TEST(ProcessDescriptorObus, CollectsIaSequenceHeaderWithCodecConfigs) {
  DescriptorObuParser parser;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  const DecodedUleb128 kFirstCodecConfigId = 123;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  const DecodedUleb128 kSecondCodecConfigId = 124;
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  const auto ia_sequence_header_with_codec_configs =
      AddSequenceHeaderAndSerializeObusExpectOk(
          {&input_codec_configs.at(kFirstCodecConfigId),
           &input_codec_configs.at(kSecondCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(ia_sequence_header_with_codec_configs));
  bool insufficient_data;
  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/true, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());

  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(parsed_obus->ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_THAT(parsed_obus->codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId),
                                           Key(kSecondCodecConfigId))));
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
  DescriptorObuParser parser;
  auto zeroth_order_ambisonics_descriptor_obus =
      InitAllDescriptorsForZerothOrderAmbisonics();

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(zeroth_order_ambisonics_descriptor_obus));
  bool insufficient_data;

  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/true, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());

  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(parsed_obus->ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_THAT(parsed_obus->codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  EXPECT_THAT(parsed_obus->audio_elements,
              Pointee(UnorderedElementsAre(Key(kFirstAudioElementId))));
  EXPECT_EQ(parsed_obus->mix_presentation_obus.size(), 1);
  EXPECT_EQ(parsed_obus->mix_presentation_obus.front().GetMixPresentationId(),
            kFirstMixPresentationId);
}

// Descriptor obus only, is_exhaustive_and_exact = false.
TEST(ProcessDescriptorObus,
     RejectsWithoutTemporalUnitFollowingAndNotExhaustive) {
  DescriptorObuParser parser;
  auto zeroth_order_ambisonics_descriptor_obus =
      InitAllDescriptorsForZerothOrderAmbisonics();

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(zeroth_order_ambisonics_descriptor_obus));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;

  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                       insufficient_data)
                   .ok());

  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + temporal unit header following, is_exhaustive_and_exact =
// true
TEST(ProcessDescriptorObusTest,
     RejectDescriptorObusWithTemporalUnitFollowingAndIsExhaustiveAndExact) {
  DescriptorObuParser parser;
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;

  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/true, *read_bit_buffer,
                       insufficient_data)
                   .ok());

  // We failed with sufficient data.
  EXPECT_FALSE(insufficient_data);

  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + temporal unit header following, is_exhaustive_and_exact =
// false.
TEST(ProcessDescriptorObusTest, SucceedsWithTemporalUnitFollowing) {
  DescriptorObuParser parser;
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  const int64_t descriptors_size = bitstream.size();

  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                /*audio_frame=*/{2, 3, 4, 5, 6, 7, 8});
  const auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  auto parsed_obus = parser.ProcessDescriptorObus(
      /*is_exhaustive_and_exact=*/false, *read_bit_buffer, insufficient_data);
  ASSERT_THAT(parsed_obus, IsOk());

  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(parsed_obus->ia_sequence_header.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_THAT(parsed_obus->codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  EXPECT_THAT(parsed_obus->audio_elements,
              Pointee(UnorderedElementsAre(Key(kFirstAudioElementId))));
  EXPECT_EQ(parsed_obus->mix_presentation_obus.size(), 1);
  EXPECT_EQ(parsed_obus->mix_presentation_obus.front().GetMixPresentationId(),
            kFirstMixPresentationId);

  // Expect the reader position to be right next to the end of the descriptors.
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), descriptors_size * 8);
}

// Descriptor obus + non_temporal_unit_header following but not enough data to
// read last obu.
TEST(ProcessDescriptorObusTest,
     RejectDescriptorObusWithNonTemporalUnitHeaderFollowingAndNotEnoughData) {
  DescriptorObuParser parser;
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  std::vector<uint8_t> extra_descriptor_obu_header_bytes = {
      kObuIaCodecConfig << kObuTypeBitShift,
      // `obu_size`. -> Non-zero size, but we have no bytes following.
      0x7f};

  bitstream.insert(bitstream.end(), extra_descriptor_obu_header_bytes.begin(),
                   extra_descriptor_obu_header_bytes.end());

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;

  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                       insufficient_data)
                   .ok());

  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + partial header following.
TEST(ProcessDescriptorObus, RejectsDescriptorObusWithPartialHeaderFollowing) {
  DescriptorObuParser parser;
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  std::vector<uint8_t> partial_header_obu = {0x80};
  bitstream.insert(bitstream.end(), partial_header_obu.begin(),
                   partial_header_obu.end());

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;

  EXPECT_FALSE(parser
                   .ProcessDescriptorObus(
                       /*is_exhaustive_and_exact=*/false, *read_bit_buffer,
                       insufficient_data)
                   .ok());

  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

}  // namespace
}  // namespace iamf_tools
