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

#include "iamf/api/iamf_decoder.h"

#include <cstdint>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
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

// TODO(b/396453922): Move this to a common test utils file.
std::vector<uint8_t> SerializeObus(
    const std::list<const ObuBase*>& input_ia_sequence) {
  WriteBitBuffer expected_wb(0);
  for (const auto* expected_obu : input_ia_sequence) {
    EXPECT_NE(expected_obu, nullptr);
    EXPECT_THAT(expected_obu->ValidateAndWriteObu(expected_wb), IsOk());
  }

  return expected_wb.bit_buffer();
}

std::vector<uint8_t> GenerateBasicDescriptorObus() {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_configs, audio_elements);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  return SerializeObus({&ia_sequence_header,
                        &codec_configs.at(kFirstCodecConfigId),
                        &audio_elements.at(kFirstAudioElementId).obu,
                        &mix_presentation_obus.front()});
}

TEST(IsDescriptorProcessingComplete,
     ReturnsFalseBeforeDescriptorObusAreProcessed) {
  auto decoder = IamfDecoder::Create();
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(Create, SucceedsAndDecodeSucceedsWithPartialData) {
  auto decoder = IamfDecoder::Create();
  EXPECT_THAT(decoder, IsOk());

  std::vector<uint8_t> source_data = {0x01, 0x23, 0x45};
  EXPECT_TRUE(decoder->Decode(source_data).ok());
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, Succeeds) {
  auto decoder =
      IamfDecoder::CreateFromDescriptors(GenerateBasicDescriptorObus());
  EXPECT_THAT(decoder, IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(Decode, SucceedsAndProcessesDescriptorsWithTemporalDelimiterAtEnd) {
  auto decoder = IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes = SerializeObus({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());

  EXPECT_THAT(decoder->Decode(source_data), IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(Decode, SucceedsWithMultiplePushesOfDescriptorObus) {
  auto decoder = IamfDecoder::Create();
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes = SerializeObus({&temporal_delimiter_obu});
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

// TODO(b/396466476): Switch to failing this test once we process temporal units
// and detect extra descriptor OBUs in a contract-breaking way.
TEST(CreateFromDescriptors, SucceedsWithDescriptorObuInSubsequentDecode) {
  auto decoder =
      IamfDecoder::CreateFromDescriptors(GenerateBasicDescriptorObus());
  EXPECT_THAT(decoder, IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());

  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId + 1, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  auto second_chunk = SerializeObus({&mix_presentation_obus.front()});
  EXPECT_THAT(decoder->Decode(second_chunk), IsOk());
}

TEST(CreateFromDescriptors, FailsWithIncompleteDescriptorObus) {
  auto descriptors = GenerateBasicDescriptorObus();
  // remove the last byte to make the descriptor OBUs incomplete.
  descriptors.pop_back();
  auto decoder = IamfDecoder::CreateFromDescriptors(descriptors);
  EXPECT_FALSE(decoder.ok());
}

}  // namespace
}  // namespace iamf_tools
