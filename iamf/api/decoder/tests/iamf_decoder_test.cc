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

#include "iamf/api/decoder/iamf_decoder.h"

#include <array>
#include <cstdint>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/api/types.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;
constexpr std::array<uint8_t, 16> kEightSampleAudioFrame = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

std::vector<uint8_t> GenerateBasicDescriptorObus() {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_configs, audio_elements);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  return SerializeObusExpectOk({&ia_sequence_header,
                                &codec_configs.at(kFirstCodecConfigId),
                                &audio_elements.at(kFirstAudioElementId).obu,
                                &mix_presentation_obus.front()});
}

TEST(IsDescriptorProcessingComplete,
     ReturnsFalseBeforeDescriptorObusAreProcessed) {
  auto decoder = api::IamfDecoder::Create();
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(Create, SucceedsAndDecodeSucceedsWithPartialData) {
  auto decoder = api::IamfDecoder::Create();
  EXPECT_THAT(decoder, IsOk());

  std::vector<uint8_t> source_data = {0x01, 0x23, 0x45};
  EXPECT_TRUE(decoder->Decode(source_data).ok());
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, Succeeds) {
  auto decoder =
      api::IamfDecoder::CreateFromDescriptors(GenerateBasicDescriptorObus());
  EXPECT_THAT(decoder, IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, FailsWithIncompleteDescriptorObus) {
  auto descriptors = GenerateBasicDescriptorObus();
  // remove the last byte to make the descriptor OBUs incomplete.
  descriptors.pop_back();
  auto decoder = api::IamfDecoder::CreateFromDescriptors(descriptors);
  EXPECT_FALSE(decoder.ok());
}

TEST(CreateFromDescriptors, FailsWithDescriptorObuInSubsequentDecode) {
  auto decoder =
      api::IamfDecoder::CreateFromDescriptors(GenerateBasicDescriptorObus());
  EXPECT_THAT(decoder, IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());

  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId + 1, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  auto second_chunk = SerializeObusExpectOk({&mix_presentation_obus.front()});

  EXPECT_FALSE(decoder->Decode(second_chunk).ok());
}

TEST(Decode, SucceedsAndProcessesDescriptorsWithTemporalDelimiterAtEnd) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());

  EXPECT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(Decode, SucceedsWithMultiplePushesOfDescriptorObus) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());
  auto first_chunk = absl::MakeConstSpan(source_data).first(2);
  auto second_chunk =
      absl::MakeConstSpan(source_data).last(source_data.size() - 2);

  EXPECT_THAT(decoder->Decode(first_chunk), IsOk());
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
  EXPECT_THAT(decoder->Decode(second_chunk), IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(Decode, SucceedsWithSeparatePushesOfDescriptorAndTemporalUnits) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  auto decoder = api::IamfDecoder::CreateFromDescriptors(source_data);
  ASSERT_THAT(decoder, IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});

  EXPECT_THAT(decoder->Decode(temporal_unit), IsOk());
}

TEST(Decode, SucceedsWithOneTemporalUnit) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  source_data.insert(source_data.end(), temporal_unit.begin(),
                     temporal_unit.end());

  EXPECT_THAT(decoder->Decode(source_data), IsOk());
}

TEST(Decode, SucceedsWithMultipleTemporalUnits) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  EXPECT_THAT(decoder->Decode(source_data), IsOk());
}

TEST(Decode, FailsWhenCalledAfterFlush) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  EXPECT_THAT(decoder->Decode(source_data), IsOk());
  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  bool output_is_done;
  EXPECT_THAT(decoder->Flush(output_decoded_temporal_unit, output_is_done),
              IsOk());
  EXPECT_FALSE(decoder->Decode(source_data).ok());
}

TEST(IsTemporalUnitAvailable, ReturnsFalseAfterCreateFromDescriptorObus) {
  auto decoder =
      api::IamfDecoder::CreateFromDescriptors(GenerateBasicDescriptorObus());
  ASSERT_THAT(decoder, IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable,
     TemporalUnitIsNotAvailableAfterDecodeWithNoTemporalDelimiterAtEnd) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  source_data.insert(source_data.end(), temporal_unit.begin(),
                     temporal_unit.end());

  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable,
     ReturnsTrueAfterDecodingOneTemporalUnitWithTemporalDelimiterAtEnd) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  auto temporal_unit =
      SerializeObusExpectOk({&audio_frame, &temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_unit.begin(),
                     temporal_unit.end());

  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  // Even though a temporal unit was provided, it has not been decoded yet. This
  // is because Decode() returns after processing the descriptor OBUs, even if
  // there is more data available. This is done to give the user a chance to do
  // any configurations based on the descriptors before beginning to decode the
  // temporal units.
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  // The user can call Decode() again to process the temporal unit still
  // available in the buffer.
  EXPECT_THAT(decoder->Decode({}), IsOk());
  // Now, the temporal unit has been decoded and is available for output.
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable, ReturnsTrueAfterDecodingMultipleTemporalUnits) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->Decode({}), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable, ReturnsFalseAfterPoppingLastTemporalUnit) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  source_data.insert(source_data.end(), temporal_unit.begin(),
                     temporal_unit.end());

  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  ASSERT_THAT(decoder->GetOutputTemporalUnit(output_decoded_temporal_unit),
              IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(GetOutputTemporalUnit, FillsOutputVectorWithAllButLastTemporalUnit) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->Decode({}), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  const std::vector<std::vector<int32_t>> expected_decoded_temporal_unit = {
      {23772706, 23773107},   {47591754, 47592556},   {71410802, 71412005},
      {95229849, 95231454},   {119048897, 119050903}, {142867944, 142870353},
      {166686992, 166689802}, {190506039, 190509251}};

  // Get the first temporal unit.
  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(output_decoded_temporal_unit),
              IsOk());
  EXPECT_EQ(output_decoded_temporal_unit, expected_decoded_temporal_unit);
}

TEST(GetOutputTemporalUnit,
     DoesNotFillOutputVectorWhenNoTemporalUnitIsAvailable) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  auto decoder = api::IamfDecoder::CreateFromDescriptors(source_data);
  ASSERT_THAT(decoder, IsOk());

  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(output_decoded_temporal_unit),
              IsOk());
  EXPECT_TRUE(output_decoded_temporal_unit.empty());
}

TEST(Flush, SucceedsWithMultipleTemporalUnits) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  auto temporal_units =
      SerializeObusExpectOk({&audio_frame, &temporal_delimiter_obu,
                             &audio_frame, &temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->Decode({}), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  bool output_is_done;
  EXPECT_THAT(decoder->Flush(output_decoded_temporal_unit, output_is_done),
              IsOk());
  EXPECT_FALSE(output_is_done);
  EXPECT_THAT(decoder->Flush(output_decoded_temporal_unit, output_is_done),
              IsOk());
  EXPECT_TRUE(output_is_done);
}

TEST(Flush, SucceedsWithNoTemporalUnits) {
  auto decoder = api::IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  bool output_is_done;
  EXPECT_THAT(decoder->Flush(output_decoded_temporal_unit, output_is_done),
              IsOk());
  EXPECT_TRUE(output_is_done);
}

}  // namespace
}  // namespace iamf_tools
