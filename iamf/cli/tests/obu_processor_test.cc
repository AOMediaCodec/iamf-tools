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
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/descriptor_obu_parser.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/numeric_utils.h"
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
using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Key;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Pointwise;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

using absl::MakeConstSpan;

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
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr uint32_t kFrameSize = 1024;
constexpr uint32_t kBitDepth = 16;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;
constexpr size_t kTwoChannels = 2;

constexpr DecodedUleb128 kImplicitSubstreamId = 0;

constexpr int kObuTypeBitShift = 3;
constexpr int64_t kBufferCapacity = 1024;
constexpr std::nullopt_t kNoDesiredMixPresentationId = std::nullopt;

constexpr std::array<uint8_t, 16> kArbitraryAudioFrame = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

const absl::flat_hash_set<ProfileVersion> kIamfV1_0_0ErrataProfiles = {
    ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile};

// Matcher that checks if the container has the given number of rows
// and columns. Rendered samples in this context are 2D vectors, with the first
// dimension corresponding to the number of channels and the second dimension
// corresponding to the number of time ticks.
auto HasShape(size_t num_channels, size_t num_time_ticks) {
  return AllOf(SizeIs(num_channels), Each(SizeIs(num_time_ticks)));
}

std::vector<uint8_t> AddSequenceHeaderAndSerializeObusExpectOk(
    const std::list<const ObuBase*>& input_ia_sequence_without_header) {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(input_ia_sequence_without_header);
  input_ia_sequence.push_front(&ia_sequence_header);
  return SerializeObusExpectOk(input_ia_sequence);
}

TEST(Create, FailsWithEmptyBitstream) {
  const std::vector<uint8_t> bitstream_without_ia_sequence_header =
      SerializeObusExpectOk({});
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(bitstream_without_ia_sequence_header));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // There's no data (and `is_exhaustive_and_exact` is false), so we need more
  // data to proceed.
  EXPECT_TRUE(insufficient_data);
}

TEST(Create, CollectsCodecConfigsBeforeATemporalUnit) {
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
      MakeConstSpan(two_codec_configs_and_audio_frame));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_THAT(obu_processor->codec_config_obus_,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId),
                                           Key(kSecondCodecConfigId))));
  // `insufficient_data` is false because we have successfully read all
  // provided descriptor obus. The presence of a temporal unit OBU indicates
  // the end of the descriptor OBUs.
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, IgnoresImplausibleCodecConfigObus) {
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
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());

  // We only find the valid Codec Config OBU, with no sign of the tiny one.
  EXPECT_THAT(obu_processor->codec_config_obus_,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  // The buffer advanced past the tiny Codec Config OBU.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
}

TEST(Create, CollectsCodecConfigsAtEndOfBitstream) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  const auto two_codec_configs_at_end_of_bitstream =
      AddSequenceHeaderAndSerializeObusExpectOk(
          {&input_codec_configs.at(kFirstCodecConfigId),
           &input_codec_configs.at(kSecondCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(two_codec_configs_at_end_of_bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  // `is_exhaustive_and_exact` is true so it could not be a more-data situation.
  EXPECT_FALSE(insufficient_data);

  EXPECT_THAT(obu_processor->codec_config_obus_,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId),
                                           Key(kSecondCodecConfigId))));
}

TEST(Create,
     DoesNotCollectCodecConfigsAtEndOfBitstreamWithoutIsExhaustiveAndExact) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);
  AddOpusCodecConfigWithId(kSecondCodecConfigId, input_codec_configs);
  const auto two_codec_configs_at_end_of_bitstream =
      AddSequenceHeaderAndSerializeObusExpectOk(
          {&input_codec_configs.at(kFirstCodecConfigId),
           &input_codec_configs.at(kSecondCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(two_codec_configs_at_end_of_bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // `is_exhaustive_and_exact` is false so we won't know it's the end of the
  // bitstream until we see a temporal unit.  Need more data to know we're done.
  EXPECT_TRUE(insufficient_data);
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

TEST(Create, CollectsIaSequenceHeaderWithoutOtherObus) {
  const auto only_ia_sequence_header =
      AddSequenceHeaderAndSerializeObusExpectOk({});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(only_ia_sequence_header));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_EQ(obu_processor->ia_sequence_header_.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu_processor->ia_sequence_header_.GetAdditionalProfile(),
            ProfileVersion::kIamfBaseProfile);
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, DescriptorObusMustStartWithIaSequenceHeader) {
  const IASequenceHeaderObu input_ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> input_codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, input_codec_configs);

  // Descriptor OBUs must start with IA Sequence Header.
  const auto ia_sequence_header_then_codec_config =
      SerializeObusExpectOk({&input_ia_sequence_header,
                             &input_codec_configs.at(kFirstCodecConfigId)});

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(ia_sequence_header_then_codec_config));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);

  // The reverse order is not valid according to
  // https://aomediacodec.github.io/iamf/#standalone-descriptor-obus
  const auto codec_config_then_ia_sequence_header =
      SerializeObusExpectOk({&input_codec_configs.at(kFirstCodecConfigId),
                             &input_ia_sequence_header});

  read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(codec_config_then_ia_sequence_header));
  obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  EXPECT_THAT(obu_processor, IsNull());
  // `insufficient_data` is false as the error was due to an invalid ordering of
  // OBUs, rather than not having enough data.
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, SucceedsWithSuccessiveRedundantSequenceHeaders) {
  const IASequenceHeaderObu input_redundant_ia_sequence_header(
      ObuHeader{.obu_redundant_copy = true}, IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&input_redundant_ia_sequence_header});

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, ConsumesUpToNextNonRedundantSequenceHeader) {
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

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);

  // Expect the reader position to be right next to the end of the first IA
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), first_ia_sequence_size * 8);
}

TEST(Create, CollectsIaSequenceHeaderWithCodecConfigs) {
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
      MakeConstSpan(ia_sequence_header_with_codec_configs));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(obu_processor->ia_sequence_header_.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_THAT(obu_processor->codec_config_obus_,
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
TEST(Create, SucceedsWithoutTemporalUnitFollowing) {
  auto zeroth_order_ambisonics_descriptor_obus =
      InitAllDescriptorsForZerothOrderAmbisonics();

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(zeroth_order_ambisonics_descriptor_obus));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(obu_processor->ia_sequence_header_.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_THAT(obu_processor->codec_config_obus_,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  EXPECT_THAT(obu_processor->audio_elements_,
              Pointee(UnorderedElementsAre(Key(kFirstAudioElementId))));
  EXPECT_EQ(obu_processor->mix_presentations_.size(), 1);
  EXPECT_EQ(obu_processor->mix_presentations_.front().GetMixPresentationId(),
            kFirstMixPresentationId);
}

// Descriptor obus only, is_exhaustive_and_exact = false.
TEST(Create, RejectsWithoutTemporalUnitFollowingAndNotExhaustive) {
  auto zeroth_order_ambisonics_descriptor_obus =
      InitAllDescriptorsForZerothOrderAmbisonics();

  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(zeroth_order_ambisonics_descriptor_obus));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + temporal unit header following, is_exhaustive_and_exact =
// true
TEST(Create,
     RejectDescriptorObusWithTemporalUnitFollowingAndIsExhaustiveAndExact) {
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
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // We failed with sufficient data.
  EXPECT_FALSE(insufficient_data);

  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + temporal unit header following, is_exhaustive_and_exact =
// false.
TEST(Create, SucceedsWithTemporalUnitFollowing) {
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
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  EXPECT_EQ(obu_processor->ia_sequence_header_.GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_THAT(obu_processor->codec_config_obus_,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  EXPECT_THAT(obu_processor->audio_elements_,
              Pointee(UnorderedElementsAre(Key(kFirstAudioElementId))));
  EXPECT_EQ(obu_processor->mix_presentations_.size(), 1);
  EXPECT_EQ(obu_processor->mix_presentations_.front().GetMixPresentationId(),
            kFirstMixPresentationId);

  // Expect the reader position to be right next to the end of the descriptors.
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), descriptors_size * 8);
}

// Descriptor obus + non_temporal_unit_header following but not enough data to
// read last obu.
TEST(Create,
     RejectDescriptorObusWithNonTemporalUnitHeaderFollowingAndNotEnoughData) {
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
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

// Descriptor obus + partial header following.
TEST(Create, RejectsDescriptorObusWithPartialHeaderFollowing) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();

  std::vector<uint8_t> partial_header_obu = {0x80};
  bitstream.insert(bitstream.end(), partial_header_obu.begin(),
                   partial_header_obu.end());

  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  auto start_position = read_bit_buffer->Tell();
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, IsNull());
  // We've received a valid bitstream so far but not complete.
  EXPECT_TRUE(insufficient_data);
  // Expect the reader position to be unchanged since we returned an error.
  EXPECT_EQ(read_bit_buffer->Tell(), start_position);
}

using OutputTemporalUnit = ObuProcessor::OutputTemporalUnit;

TEST(ProcessTemporalUnit, DoesNotCreateTemporalUnitWithNoData) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  // We expect the call to `ProcessTemporalUnit()` to succeed, but since there
  // is no data left to consume in the input_buffer, we should not create an
  // output temporal unit.
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  // We expect that no output_temporal_unit is created.
  EXPECT_FALSE(continue_processing);
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, DoesNotCreateTemporalUnitWithOnlyATemporalDelimiter) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  auto temporal_unit_obus = SerializeObusExpectOk({&temporal_delimiter_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/false,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  ASSERT_FALSE(insufficient_data);

  std::optional<OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = true;
  // We expect the call to `ProcessTemporalUnit()` to succeed, but not consume
  // any data.
  EXPECT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              IsOk());

  // We should NOT signal that we ought to continue processing, because we have
  // no data to consume.
  EXPECT_FALSE(continue_processing);
  // We expect that no output_temporal_unit is created since we only have a
  // temporal delimiter.
  EXPECT_FALSE(output_temporal_unit.has_value());
}

TEST(ProcessTemporalUnit, ConsumesOneAudioFrameAsTemporalUnit) {
  // Set up inputs with a single audio frame.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);
  auto temporal_unit_obus = SerializeObusExpectOk({&audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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

TEST(ProcessTemporalUnit, SkipsStrayParameterBlocks) {
  // Set up inputs with a single audio frame.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  // Insert an extra parameter block, the descriptors don't have a parameter
  // definition for it.
  constexpr DecodedUleb128 kStrayParameterBlockId =
      std::numeric_limits<DecodedUleb128>::max();
  MixGainParamDefinition param_definition;
  param_definition.parameter_id_ = kStrayParameterBlockId;
  constexpr DecodedUleb128 kParameterBlockDuration = 10;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = kParameterBlockDuration;
  param_definition.constant_subblock_duration_ = kParameterBlockDuration;
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant> param_definitions;
  param_definitions.emplace(kStrayParameterBlockId, param_definition);
  auto parameter_block =
      ParameterBlockObu::CreateMode0(ObuHeader(), param_definition);
  ASSERT_THAT(parameter_block, NotNull());
  parameter_block->subblocks_[0].param_data =
      std::make_unique<MixGainParameterData>(
          MixGainParameterData::kAnimateStep,
          AnimationStepInt16{.start_point_value = 99});
  AudioFrameObu audio_frame_obu(ObuHeader(), kFirstSubstreamId,
                                kArbitraryAudioFrame);
  auto temporal_unit_obus =
      SerializeObusExpectOk({&*parameter_block, &audio_frame_obu});
  bitstream.insert(bitstream.end(), temporal_unit_obus.begin(),
                   temporal_unit_obus.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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

  // The temporal unit is consumed, but the parameter block is gracefully
  // ignored.
  EXPECT_FALSE(continue_processing);
  EXPECT_TRUE(output_temporal_unit->output_parameter_blocks.empty());
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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
      SerializeObusExpectOk({&temporal_delimiter_obu, &audio_frame_obus[0],
                             &temporal_delimiter_obu, &audio_frame_obus[1]});
  bitstream.insert(bitstream.end(),
                   two_temporal_units_with_delimiter_obu.begin(),
                   two_temporal_units_with_delimiter_obu.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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
  // No more data to consume.
  EXPECT_FALSE(continue_processing);
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
}

TEST(ProcessTemporalUnit, ConsumesOneTemporalUnitsWithNextTemporalDelimiter) {
  // Set up inputs with two audio frames and temporal delimiters.
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  std::vector<AudioFrameObu> audio_frame_obus;
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId, kArbitraryAudioFrame));
  audio_frame_obus.push_back(
      AudioFrameObu(ObuHeader(), kFirstSubstreamId, kArbitraryAudioFrame));
  const auto two_temporal_units_with_delimiter_obu = SerializeObusExpectOk(
      {&temporal_delimiter_obu, &audio_frame_obus[0], &temporal_delimiter_obu});
  bitstream.insert(bitstream.end(),
                   two_temporal_units_with_delimiter_obu.begin(),
                   two_temporal_units_with_delimiter_obu.end());
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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

  // continue_processing is true because we exited after processing the second
  // temporal delimiter.
  EXPECT_TRUE(continue_processing);
  // Expect the first temporal unit to be consumed.
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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

  DescriptorObuParser::ParsedDescriptorObus descriptor_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, descriptor_obus,
                                        audio_frames, parameter_blocks),
              IsOk());
  EXPECT_EQ(read_bit_buffer->Tell(), ia_sequence_size * 8);

  // Reaching the end of the stream.
  EXPECT_FALSE(read_bit_buffer->IsDataAvailable());
  EXPECT_THAT(descriptor_obus.codec_config_obus,
              Pointee(UnorderedElementsAre(Key(kFirstCodecConfigId))));
  EXPECT_THAT(descriptor_obus.audio_elements,
              Pointee(UnorderedElementsAre(Key(kFirstAudioElementId))));
  EXPECT_FALSE(descriptor_obus.mix_presentation_obus.empty());
  EXPECT_EQ(
      descriptor_obus.mix_presentation_obus.front().GetMixPresentationId(),
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

  DescriptorObuParser::ParsedDescriptorObus descriptor_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(two_ia_sequences));
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, descriptor_obus,
                                        audio_frames, parameter_blocks),
              IsOk());
  EXPECT_EQ(read_bit_buffer->Tell(), trivial_ia_sequence_size * 8);

  // The first IA sequence is trivial and should be consumed.
  EXPECT_THAT(descriptor_obus.codec_config_obus, Pointee(IsEmpty()));
  EXPECT_THAT(descriptor_obus.audio_elements, Pointee(IsEmpty()));
  EXPECT_THAT(descriptor_obus.mix_presentation_obus, IsEmpty());
  EXPECT_TRUE(audio_frames.empty());
  EXPECT_TRUE(parameter_blocks.empty());

  // A second call retrieves the next IA sequence, which has an audio frame.
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, descriptor_obus,
                                        audio_frames, parameter_blocks),
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

  DescriptorObuParser::ParsedDescriptorObus descriptor_obus;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, descriptor_obus,
                                        audio_frames, parameter_blocks),
              IsOk());

  // Expect the reader position to be right next to the end of the first IA
  // sequence.
  EXPECT_EQ(read_bit_buffer->Tell(), first_ia_sequence_size * 8);
}

TEST(Create, Succeeds) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  EXPECT_THAT(obu_processor->codec_config_obus_, Pointee(SizeIs(1)));
  EXPECT_THAT(obu_processor->audio_elements_, Pointee(SizeIs(1)));
  EXPECT_EQ(obu_processor->mix_presentations_.size(), 1);
}

TEST(Create, SucceedsForTrivialIaSequence) {
  const IASequenceHeaderObu kIaSequenceHeader(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  auto buffer = SerializeObusExpectOk({&kIaSequenceHeader});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  EXPECT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
}

TEST(Create, FailsOnInsufficientData) {
  auto bitstream = InitAllDescriptorsForZerothOrderAmbisonics();
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(buffer));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());

  EXPECT_THAT(obu_processor->GetOutputFrameSize(), Not(IsOk()));
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
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};
  std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const auto descriptors = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(descriptors));
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, kStereoLayout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);

  absl::Span<const absl::Span<const InternalSampleType>>
      output_rendered_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, empty_parameter_blocks_with_data,
                  empty_audio_frames_with_data, output_rendered_samples),
              IsOk());
  EXPECT_TRUE(output_rendered_samples.empty());
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness, RendersFoaToStereoWav) {
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
  const std::list<ParameterBlockWithData> kNoParameterBlocks = {};
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, kStereoLayout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);

  absl::Span<const absl::Span<const InternalSampleType>>
      output_rendered_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, kNoParameterBlocks, audio_frames_with_data,
                  output_rendered_samples),
              IsOk());
  constexpr int kExpectedNumSamplesPerFrame = 1;
  EXPECT_THAT(output_rendered_samples,
              HasShape(kTwoChannels, kExpectedNumSamplesPerFrame));
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     SupportsMixGainParameterBlocks) {
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
  constexpr DecodedUleb128 kDuration = 1;
  constexpr DecodedUleb128 kConstantSubblockDuration = 1;
  constexpr uint32_t kNumSubblocks = 1;
  auto parameter_block = ParameterBlockObu::CreateMode1(
      ObuHeader(), mix_presentation_obus.front().sub_mixes_[0].output_mix_gain,
      kDuration, kConstantSubblockDuration, kNumSubblocks);
  EXPECT_THAT(parameter_block, NotNull());
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, kStereoLayout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);

  absl::Span<const absl::Span<const InternalSampleType>>
      output_rendered_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, parameter_blocks_with_data,
                  audio_frames_with_data, output_rendered_samples),
              IsOk());
  constexpr int kExpectedNumSamplesPerFrame = 1;
  EXPECT_THAT(output_rendered_samples,
              HasShape(kTwoChannels, kExpectedNumSamplesPerFrame));
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

TEST(RenderTemporalUnitAndMeasureLoudness, RendersPassthroughStereo) {
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));

  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, kStereoLayout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);
  absl::Span<const absl::Span<const InternalSampleType>>
      output_rendered_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, kNoParameterBlocks, audio_frames_with_data,
                  output_rendered_samples),
              IsOk());

  // Outer vector is for each channel, inner vector is for each tick.
  const auto kLeftChannel =
      Int32ToInternalSampleType({0x33110000, 0x77550000, 0x0a990000});
  const auto kRightChannel =
      Int32ToInternalSampleType({0x44220000, 0x08660000, 0x0dbb0000});
  EXPECT_THAT(output_rendered_samples,
              ElementsAre(Pointwise(Eq(), kLeftChannel),
                          Pointwise(Eq(), kRightChannel)));
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     RendersPassthroughStereoToWav_2) {
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
    auto read_bit_buffer =
        MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));

    bool insufficient_data;
    auto obu_processor = ObuProcessor::CreateForRendering(
        kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, kStereoLayout,
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
      absl::Span<const absl::Span<const InternalSampleType>>
          output_rendered_samples;
      EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                      /*timestamp=*/i, kNoParameterBlocks,
                      audio_frames_with_data, output_rendered_samples),
                  IsOk());
      const auto kExpectedSample =
          Int32ToNormalizedFloatingPoint<InternalSampleType>(i << 16 |
                                                             (i << 24));
      const std::vector<InternalSampleType> expected_channel(kTwoChannels,
                                                             kExpectedSample);
      EXPECT_THAT(output_rendered_samples,
                  ElementsAre(Pointwise(Eq(), expected_channel),
                              Pointwise(Eq(), expected_channel)));
    }
  }
}

TEST(RenderAudioFramesWithDataAndMeasureLoudness,
     SelectsFirstMixPresentationWhenSupported) {
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
  constexpr auto kExpectedFirstSampleForFirstMixPresentation =
      Int32ToNormalizedFloatingPoint<InternalSampleType>(1 << 16);
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
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, kStereoLayout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  ASSERT_THAT(obu_processor, NotNull());
  EXPECT_FALSE(insufficient_data);

  absl::Span<const absl::Span<const InternalSampleType>>
      output_rendered_samples;
  EXPECT_THAT(obu_processor->RenderTemporalUnitAndMeasureLoudness(
                  /*timestamp=*/0, kNoParameterBlocks, audio_frames_with_data,
                  output_rendered_samples),
              IsOk());
  EXPECT_THAT(output_rendered_samples, HasShape(2, 1));
  EXPECT_EQ(output_rendered_samples[0][0],
            kExpectedFirstSampleForFirstMixPresentation);
}

TEST(GetOutputMixPresentationId, FailsWhenNotCreatedForRendering) {
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk({});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  absl::StatusOr<DecodedUleb128> mix_presentation_id =
      obu_processor->GetOutputMixPresentationId();
  EXPECT_THAT(mix_presentation_id, Not(IsOk()));
}

TEST(GetOutputLayout, FailsWhenNotCreastedForRendering) {
  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk({});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor =
      ObuProcessor::Create(/*is_exhaustive_and_exact=*/true,
                           read_bit_buffer.get(), insufficient_data);

  ASSERT_THAT(obu_processor, NotNull());
  absl::StatusOr<Layout> layout = obu_processor->GetOutputLayout();
  EXPECT_THAT(layout, Not(IsOk()));
}

TEST(CreateForRendering,
     ReturnsNullptrWhenDesiredProfileVersionIsNotSupported) {
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

  // The only mix presentation requires Base-Enhanced profile, but the user
  // restricted the profiles to Simple and Base.
  const absl::flat_hash_set<ProfileVersion> kProfilesTooLow = {
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile};
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;
  auto obu_processor = ObuProcessor::CreateForRendering(
      kProfilesTooLow, kNoDesiredMixPresentationId, kStereoLayout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);

  EXPECT_FALSE(insufficient_data);
  EXPECT_THAT(obu_processor, IsNull());
}

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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  // We expect to use the second layout, since this is the only one that matches
  // the desired layout.
  const auto& forwarded_layout =
      mix_presentation_obus.front().sub_mixes_[0].layouts[1].loudness_layout;

  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, k5_1_Layout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  EXPECT_THAT(obu_processor, NotNull());
  absl::StatusOr<Layout> output_layout = obu_processor->GetOutputLayout();
  EXPECT_THAT(output_layout, IsOkAndHolds(forwarded_layout));
  absl::StatusOr<DecodedUleb128> output_mix_presentation_id =
      obu_processor->GetOutputMixPresentationId();
  EXPECT_THAT(output_mix_presentation_id,
              IsOkAndHolds(kFirstMixPresentationId));
}

TEST(CreateForRendering, ForwardsVirtualChosenLayoutToSampleProcessorFactory) {
  // Set up inputs; key aspect is that the mix presentation does not contain the
  // desired layout.
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

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  // We expect to default to the first mix presentation and first submix.
  // However, because the requested layout is not found, it is 'virtualized' and
  // appended to the list of layouts.
  constexpr int kSubmixIndex = 0;
  constexpr int kLayoutIndex = 2;
  const auto& forwarded_layout = k5_1_Layout;

  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kNoDesiredMixPresentationId, k5_1_Layout,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  EXPECT_THAT(obu_processor, NotNull());
  absl::StatusOr<Layout> output_layout = obu_processor->GetOutputLayout();
  EXPECT_THAT(output_layout, IsOkAndHolds(forwarded_layout));
  absl::StatusOr<DecodedUleb128> output_mix_presentation_id =
      obu_processor->GetOutputMixPresentationId();
  EXPECT_THAT(output_mix_presentation_id,
              IsOkAndHolds(kFirstMixPresentationId));
  EXPECT_EQ(obu_processor->mix_presentations_.front()
                .sub_mixes_[kSubmixIndex]
                .layouts[kLayoutIndex]
                .loudness_layout,
            k5_1_Layout);
}

TEST(CreateForRendering, CanChooseLayoutByMixPresentationIdOnly) {
  // Set up inputs; key aspect is that the mix presentation does not contain the
  // desired layout.
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
  // We will use the second Mix Presentation and its ID, using different layouts
  // so we can see it is different.
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      mix_2_sound_system_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0,
          LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kSecondMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      mix_2_sound_system_layouts, mix_presentation_obus);

  const auto bitstream = AddSequenceHeaderAndSerializeObusExpectOk(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front(), &mix_presentation_obus.back()});
  auto read_bit_buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  bool insufficient_data;

  // Without a Layout, we expect it to pick the first sub-mix, first Layout.
  const auto& forwarded_layout = kStereoLayout;

  auto obu_processor = ObuProcessor::CreateForRendering(
      kIamfV1_0_0ErrataProfiles, kSecondMixPresentationId,
      /*desired_layout=*/std::nullopt,
      /*is_exhaustive_and_exact=*/true, read_bit_buffer.get(),
      insufficient_data);
  EXPECT_THAT(obu_processor, NotNull());
  absl::StatusOr<Layout> output_layout = obu_processor->GetOutputLayout();
  EXPECT_THAT(output_layout, IsOkAndHolds(forwarded_layout));
  absl::StatusOr<DecodedUleb128> output_mix_presentation_id =
      obu_processor->GetOutputMixPresentationId();
  EXPECT_THAT(output_mix_presentation_id,
              IsOkAndHolds(kSecondMixPresentationId));
}

}  // namespace
}  // namespace iamf_tools
