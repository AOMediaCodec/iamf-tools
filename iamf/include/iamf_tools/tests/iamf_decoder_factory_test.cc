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

#include "iamf/include/iamf_tools/iamf_decoder_factory.h"

#include <cstdint>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace {

using ::iamf_tools::DecodedUleb128;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint32_t kBitDepth = 16;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

std::vector<uint8_t> GenerateBasicDescriptorObus() {
  const iamf_tools::IASequenceHeaderObu ia_sequence_header(
      iamf_tools::ObuHeader(), iamf_tools::ProfileVersion::kIamfSimpleProfile,
      iamf_tools::ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, iamf_tools::CodecConfigObu> codec_configs;
  AddLpcmCodecConfig(kFirstCodecConfigId, kNumSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_configs);
  absl::flat_hash_map<DecodedUleb128, iamf_tools::AudioElementWithData>
      audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_configs, audio_elements);
  std::list<iamf_tools::MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  return iamf_tools::SerializeObusExpectOk(
      {&ia_sequence_header, &codec_configs.at(kFirstCodecConfigId),
       &audio_elements.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
}

using ::iamf_tools::api::IamfDecoderFactory;

TEST(Create, SucceedsWithSimpleSettings) {
  IamfDecoderFactory::Settings settings = {
      .requested_mix =
          {.output_layout =
               iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0},
      .channel_ordering = iamf_tools::api::ChannelOrdering::kIamfOrdering,
      .requested_profile_versions =
          {iamf_tools::api::ProfileVersion::kIamfSimpleProfile,
           iamf_tools::api::ProfileVersion::kIamfBaseProfile,
           iamf_tools::api::ProfileVersion::kIamfBaseEnhancedProfile},
      .requested_output_sample_type =
          iamf_tools::api::OutputSampleType::kInt32LittleEndian,
  };
  auto decoder = IamfDecoderFactory::Create(settings);
  EXPECT_NE(decoder, nullptr);
}

TEST(Create, SucceedsWithEmptySettings) {
  auto decoder = IamfDecoderFactory::Create({});
  EXPECT_NE(decoder, nullptr);
}

TEST(CreateFromDescriptors, SucceedsWithSimpleSettings) {
  IamfDecoderFactory::Settings settings = {
      .requested_mix =
          {.output_layout =
               iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0},
      .channel_ordering = iamf_tools::api::ChannelOrdering::kIamfOrdering,
      .requested_profile_versions =
          {iamf_tools::api::ProfileVersion::kIamfSimpleProfile,
           iamf_tools::api::ProfileVersion::kIamfBaseProfile,
           iamf_tools::api::ProfileVersion::kIamfBaseEnhancedProfile},
      .requested_output_sample_type =
          iamf_tools::api::OutputSampleType::kInt32LittleEndian,
  };
  auto descriptors = GenerateBasicDescriptorObus();
  auto decoder = IamfDecoderFactory::CreateFromDescriptors(
      settings, descriptors.data(), descriptors.size());
  EXPECT_NE(decoder, nullptr);
}

TEST(CreateFromDescriptors, FailsWithIncompleteDescriptorObus) {
  IamfDecoderFactory::Settings settings = {
      .requested_mix =
          {.output_layout =
               iamf_tools::api::OutputLayout::kItu2051_SoundSystemA_0_2_0},
      .channel_ordering = iamf_tools::api::ChannelOrdering::kIamfOrdering,
      .requested_profile_versions =
          {iamf_tools::api::ProfileVersion::kIamfSimpleProfile,
           iamf_tools::api::ProfileVersion::kIamfBaseProfile,
           iamf_tools::api::ProfileVersion::kIamfBaseEnhancedProfile},
      .requested_output_sample_type =
          iamf_tools::api::OutputSampleType::kInt32LittleEndian,
  };
  auto descriptors = GenerateBasicDescriptorObus();
  // remove the last byte to make the descriptor OBUs incomplete.
  descriptors.pop_back();
  auto decoder = IamfDecoderFactory::CreateFromDescriptors(
      settings, descriptors.data(), descriptors.size());
  EXPECT_EQ(decoder, nullptr);
}

}  // namespace
