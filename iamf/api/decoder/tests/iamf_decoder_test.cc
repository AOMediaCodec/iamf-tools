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
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint32_t kBitDepth = 16;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kSecondMixPresentationId = 4;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;
constexpr std::array<uint8_t, 16> kEightSampleAudioFrame = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
constexpr std::array<uint8_t, 16> kEightSampleAudioFrame2 = {
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};

std::vector<uint8_t> GenerateBasicDescriptorObus() {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddLpcmCodecConfig(kFirstCodecConfigId, kNumSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_configs);
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

std::vector<uint8_t> GenerateBaseEnhancedDescriptorObus() {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfBaseEnhancedProfile,
      ProfileVersion::kIamfBaseEnhancedProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddLpcmCodecConfig(kFirstCodecConfigId, kNumSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  // Fourth-order ambisonics uses too many channels for simple or base
  // profile, but it permitted in base-enhanced profile.
  constexpr std::array<DecodedUleb128, 25> kFourthOrderAmbisonicsSubstreamIds =
      {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
       13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24};
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId,
      kFourthOrderAmbisonicsSubstreamIds, codec_configs, audio_elements);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  return SerializeObusExpectOk({&ia_sequence_header,
                                &codec_configs.at(kFirstCodecConfigId),
                                &audio_elements.at(kFirstAudioElementId).obu,
                                &mix_presentation_obus.front()});
}

api::IamfDecoder::Settings GetStereoDecoderSettings() {
  return {.requested_mix = {
              .output_layout = api::OutputLayout::kItu2051_SoundSystemA_0_2_0}};
}

api::IamfDecoder::Settings Get5_1DecoderSettings() {
  return {
      .requested_mix = {.output_layout =
                            api::OutputLayout::kItu2051_SoundSystemB_0_5_0},
  };
}

TEST(IsDescriptorProcessingComplete,
     ReturnsFalseBeforeDescriptorObusAreProcessed) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());

  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(IamfDecoder,
     MethodsDependingOnDescriptorsFailBeforeDescriptorObusAreProcessed) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  api::SelectedMix selected_mix;
  EXPECT_FALSE(decoder->GetOutputMix(selected_mix).ok());
  int num_channels;
  EXPECT_FALSE(decoder->GetNumberOfOutputChannels(num_channels).ok());
  uint32_t sample_rate;
  EXPECT_FALSE(decoder->GetSampleRate(sample_rate).ok());
  uint32_t frame_size;
  EXPECT_FALSE(decoder->GetFrameSize(frame_size).ok());
}

TEST(GetOutputMix, ReturnVirtualDesiredLayoutIfNoMatchingLayoutExists) {
  std::unique_ptr<api::IamfDecoder> decoder;
  auto descriptors = GenerateBasicDescriptorObus();
  constexpr api::OutputLayout kDesiredLayout =
      api::OutputLayout::kItu2051_SoundSystemE_4_5_1;
  const api::IamfDecoder::Settings kSettings = {
      .requested_mix = {.output_layout = kDesiredLayout}};
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  kSettings, descriptors.data(), descriptors.size(), decoder)
                  .ok());

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  api::SelectedMix selected_mix;
  EXPECT_TRUE(decoder->GetOutputMix(selected_mix).ok());
  EXPECT_EQ(selected_mix.output_layout, kDesiredLayout);
  int num_output_channels;
  EXPECT_TRUE(decoder->GetNumberOfOutputChannels(num_output_channels).ok());
  EXPECT_EQ(num_output_channels, 11);
}

TEST(GetOutputMix,
     ReturnsVirtualDesiredLayoutIfNoMatchingLayoutExistsUsingDecode) {
  std::unique_ptr<api::IamfDecoder> decoder;
  constexpr api::OutputLayout kDesiredLayout =
      api::OutputLayout::kItu2051_SoundSystemE_4_5_1;
  const api::IamfDecoder::Settings kSettings = {
      .requested_mix = {.output_layout = kDesiredLayout}};
  ASSERT_TRUE(api::IamfDecoder::Create(kSettings, decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());

  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  api::SelectedMix selected_mix;
  EXPECT_TRUE(decoder->GetOutputMix(selected_mix).ok());
  EXPECT_EQ(selected_mix.output_layout,
            api::OutputLayout::kItu2051_SoundSystemE_4_5_1);
  int num_output_channels;
  EXPECT_TRUE(decoder->GetNumberOfOutputChannels(num_output_channels).ok());
  EXPECT_EQ(num_output_channels, 11);
}

TEST(GetOutputMix, CanAcceptMixPresentationIdToSpecifyMix) {
  // Add a mix presentation with a non-stereo layout.
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
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      mix_presentation_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0};
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithConfigurableLayouts(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_layouts,
      mix_presentation_obus);

  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      mix_2_sound_system_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0,
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kSecondMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      mix_2_sound_system_layouts, mix_presentation_obus);

  std::vector<uint8_t> descriptor_obus = SerializeObusExpectOk(
      {&ia_sequence_header, &codec_configs.at(kFirstCodecConfigId),
       &audio_elements.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front(), &mix_presentation_obus.back()});

  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kSettings = {
      .requested_mix = {.mix_presentation_id = kSecondMixPresentationId}};
  ASSERT_TRUE(
      api::IamfDecoder::CreateFromDescriptors(kSettings, descriptor_obus.data(),
                                              descriptor_obus.size(), decoder)
          .ok());

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  api::SelectedMix selected_mix;
  EXPECT_TRUE(decoder->GetOutputMix(selected_mix).ok());
  EXPECT_EQ(selected_mix.mix_presentation_id, kSecondMixPresentationId);
  EXPECT_EQ(selected_mix.output_layout,
            api::OutputLayout::kItu2051_SoundSystemB_0_5_0);
  int num_output_channels;
  EXPECT_TRUE(decoder->GetNumberOfOutputChannels(num_output_channels).ok());
  EXPECT_EQ(num_output_channels, 6);
}

TEST(Create, SucceedsAndDecodeSucceedsWithPartialData) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());

  std::vector<uint8_t> source_data = {0x01, 0x23, 0x45};
  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(Create, SucceedsWithNonStereoLayout) {
  std::unique_ptr<api::IamfDecoder> decoder;
  EXPECT_TRUE(api::IamfDecoder::Create(Get5_1DecoderSettings(), decoder).ok());
}

TEST(CreateFromDescriptors, Succeeds) {
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  EXPECT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), descriptors.data(),
                  descriptors.size(), decoder)
                  .ok());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, SucceedsWithNonStereoLayout) {
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  EXPECT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  Get5_1DecoderSettings(), descriptors.data(),
                  descriptors.size(), decoder)
                  .ok());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, FailsWithIncompleteDescriptorObus) {
  auto descriptors = GenerateBasicDescriptorObus();
  // remove the last byte to make the descriptor OBUs incomplete.
  descriptors.pop_back();

  std::unique_ptr<api::IamfDecoder> decoder;
  EXPECT_FALSE(api::IamfDecoder::CreateFromDescriptors(
                   GetStereoDecoderSettings(), descriptors.data(),
                   descriptors.size(), decoder)
                   .ok());
}

TEST(CreateFromDescriptors, FailsWithDescriptorObuInSubsequentDecode) {
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), descriptors.data(),
                  descriptors.size(), decoder)
                  .ok());
  ASSERT_TRUE(decoder->IsDescriptorProcessingComplete());

  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId + 1, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  auto second_chunk = SerializeObusExpectOk({&mix_presentation_obus.front()});

  EXPECT_FALSE(decoder->Decode(second_chunk.data(), second_chunk.size()).ok());
}

TEST(CreateThenDecode, FailsWhenNoMatchingProfileVersionIsFound) {
  // Configure a "legacy" decoder with only the base profile. E.g. mimic a
  // client that may not want to spend additional CPU cycles on handling
  // base-enhanced profile.
  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kSettingsWithoutBaseEnhancedProfile = {
      .requested_profile_versions = {api::ProfileVersion::kIamfBaseProfile}};
  const auto status =
      api::IamfDecoder::Create(kSettingsWithoutBaseEnhancedProfile, decoder);
  EXPECT_TRUE(status.ok());

  // The descriptors are base-enhanced with no backwards compatibility features.
  auto descriptors = GenerateBaseEnhancedDescriptorObus();
  EXPECT_TRUE(decoder->Decode(descriptors.data(), descriptors.size()).ok());
  // Once we see the start of a temporal unit, we know that no remaining mixes
  // match the requested profile.
  const TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  const auto serialized_temporal_delimiter =
      SerializeObusExpectOk({&temporal_delimiter_obu});

  // No mix matches the requested profile. Nothing can be decoded.
  EXPECT_FALSE(decoder
                   ->Decode(serialized_temporal_delimiter.data(),
                            serialized_temporal_delimiter.size())
                   .ok());
}

TEST(CreateThenDecode, SucceedsWithBaseEnhancedProfileWhenConfigured) {
  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kSettingsWithBaseEnhancedProfile = {
      .requested_profile_versions = {
          api::ProfileVersion::kIamfBaseEnhancedProfile}};
  const auto status =
      api::IamfDecoder::Create(kSettingsWithBaseEnhancedProfile, decoder);
  EXPECT_TRUE(status.ok());

  auto descriptors = GenerateBaseEnhancedDescriptorObus();
  EXPECT_TRUE(decoder->Decode(descriptors.data(), descriptors.size()).ok());

  // Once we see the start of a temporal unit, we know all descriptors are
  // processed.
  const TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  const auto serialized_temporal_delimiter =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  EXPECT_TRUE(decoder
                  ->Decode(serialized_temporal_delimiter.data(),
                           serialized_temporal_delimiter.size())
                  .ok());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, FailsWhenNoMatchingProfileVersionIsFound) {
  // Configure a "legacy" decoder with only the base profile. E.g. mimic a
  // client that may not want to spend additional CPU cycles on handling
  // base-enhanced profile.
  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kSettingsWithoutBaseEnhancedProfile = {
      .requested_profile_versions = {api::ProfileVersion::kIamfSimpleProfile}};
  auto descriptors = GenerateBaseEnhancedDescriptorObus();

  auto status = api::IamfDecoder::CreateFromDescriptors(
      kSettingsWithoutBaseEnhancedProfile, descriptors.data(),
      descriptors.size(), decoder);

  // No relevant mix was found. Nothing can be decoded.
  EXPECT_FALSE(status.ok());
}

TEST(CreateFromDescriptors, SucceedsWithBaseEnhancedProfileWhenConfigured) {
  // Configure a decoder which may use base-enhanced profile.
  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kSettingsWithBaseEnhancedProfile = {
      .requested_profile_versions = {
          api::ProfileVersion::kIamfBaseEnhancedProfile}};
  auto descriptors = GenerateBaseEnhancedDescriptorObus();

  // Ok. The descriptors are suitable for the requested profiles.
  auto status = api::IamfDecoder::CreateFromDescriptors(
      kSettingsWithBaseEnhancedProfile, descriptors.data(), descriptors.size(),
      decoder);
  EXPECT_TRUE(status.ok());
}

TEST(Decode, SucceedsAndProcessesDescriptorsWithTemporalDelimiterAtEnd) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());

  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(Decode, SucceedsWithMultiplePushesOfDescriptorObus) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());
  EXPECT_TRUE(
      decoder->Decode(source_data.data(), /* input_buffer_size=*/2).ok());
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
  EXPECT_TRUE(
      decoder->Decode(source_data.data() + 2, source_data.size() - 2).ok());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(Decode, SucceedsWithSeparatePushesOfDescriptorAndTemporalUnits) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), source_data.data(),
                  source_data.size(), decoder)
                  .ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});

  EXPECT_TRUE(decoder->Decode(temporal_unit.data(), temporal_unit.size()).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
}

TEST(Decode, SucceedsWithOneTemporalUnit) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  source_data.insert(source_data.end(), temporal_unit.begin(),
                     temporal_unit.end());

  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  const size_t expected_output_size =
      8 * 4 * 2;  // 8 samples, 32-bit ints, stereo.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(Decode, ReordersSamplesIfRequested) {
  api::IamfDecoder::Settings settings = {
      .requested_mix = {.output_layout =
                            api::OutputLayout::kItu2051_SoundSystemI_0_7_0},
      .channel_ordering = api::ChannelOrdering::kIamfOrdering,
  };
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> regular_decoder;
  ASSERT_TRUE(
      api::IamfDecoder::CreateFromDescriptors(
          settings, descriptors.data(), descriptors.size(), regular_decoder)
          .ok());
  settings.channel_ordering = api::ChannelOrdering::kOrderingForAndroid;
  std::unique_ptr<api::IamfDecoder> reordering_decoder;
  ASSERT_TRUE(
      api::IamfDecoder::CreateFromDescriptors(
          settings, descriptors.data(), descriptors.size(), reordering_decoder)
          .ok());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  ASSERT_TRUE(
      regular_decoder->Decode(temporal_unit.data(), temporal_unit.size()).ok());
  ASSERT_TRUE(
      reordering_decoder->Decode(temporal_unit.data(), temporal_unit.size())
          .ok());

  const size_t expected_output_size =
      8 * 4 * 8;  // 8 samples, 32-bit ints, 7.1.
  std::vector<uint8_t> regular_output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(regular_decoder
                  ->GetOutputTemporalUnit(regular_output_data.data(),
                                          regular_output_data.size(),
                                          bytes_written)
                  .ok());
  std::vector<uint8_t> reordered_output_data(expected_output_size);
  EXPECT_TRUE(reordering_decoder
                  ->GetOutputTemporalUnit(reordered_output_data.data(),
                                          reordered_output_data.size(),
                                          bytes_written)
                  .ok());

  auto regular = absl::MakeSpan(regular_output_data);
  auto reordered = absl::MakeSpan(reordered_output_data);
  // First 4 samples should be same.
  EXPECT_EQ(regular.first(16), reordered.first(16));
  // Expect last 4 to be swapped.
  EXPECT_EQ(regular.subspan(16, 8), reordered.subspan(24, 8));
  EXPECT_EQ(regular.subspan(24, 8), reordered.subspan(16, 8));
}

TEST(Decode, SucceedsWithMultipleTemporalUnits) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());

  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());

  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  const size_t expected_output_size =
      8 * 4 * 2;  // 8 samples, 32-bit ints, stereo.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  output_data.clear();
  output_data.resize(expected_output_size);
  bytes_written = 0;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(Decode, SucceedsWithMultipleTemporalUnitsForNonStereoLayout) {
  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kMonoSettings = {
      .requested_mix = {
          .output_layout =
              api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0}};
  ASSERT_TRUE(api::IamfDecoder::Create(kMonoSettings, decoder).ok());

  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      sound_system_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, sound_system_layouts,
      mix_presentation_obus);

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  input_ia_sequence.push_front(&ia_sequence_header);

  std::vector<uint8_t> source_data = SerializeObusExpectOk(input_ia_sequence);
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  // Calling with empty due to forced exit after descriptor processing, so that
  // we can get the output temporal unit.
  EXPECT_TRUE(decoder->Decode(source_data.data(), 0).ok());

  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());

  const size_t expected_output_size = 8 * 4;  // 8 samples, 32-bit ints, mono.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);

  // Should be able to get the second temporal unit as well.
  bytes_written = 0;
  output_data.clear();
  output_data.resize(expected_output_size);
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);
}

TEST(Decode, CreatedFromDescriptorsSucceedsWithMultipleTemporalUnits) {
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), descriptors.data(),
                  descriptors.size(), decoder)
                  .ok());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});

  // We expect for decode to succeed and fully process both temporal units. This
  // means that we should be able to pull two temporal units from the decoder,
  // and then there should be nothing left.
  EXPECT_TRUE(
      decoder->Decode(temporal_units.data(), temporal_units.size()).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  const size_t expected_output_size =
      8 * 4 * 2;  // 8 samples, 32-bit ints, stereo.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());

  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  output_data.clear();
  output_data.resize(expected_output_size);
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(Decode,
     CreatedFromDescriptorsSucceedsWithTemporalUnitsDecodedInSeparatePushes) {
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), descriptors.data(),
                  descriptors.size(), decoder)
                  .ok());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});

  // We expect for decode to succeed and fully process the singular temporal
  // unit that was pushed.
  EXPECT_TRUE(decoder->Decode(temporal_unit.data(), temporal_unit.size()).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  const size_t expected_output_size =
      8 * 4 * 2;  // 8 samples, 32-bit ints, stereo.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());

  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());

  output_data.clear();
  output_data.resize(expected_output_size);

  // Now, we expect for decode to succeed and fully process the second temporal
  // unit that was pushed.
  EXPECT_TRUE(decoder->Decode(temporal_unit.data(), temporal_unit.size()).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(
    Decode,
    CreatedFromDescriptorsSucceedsWithMultipleTemporalUnitsForNonStereoLayout) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kFirstCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_elements_with_data;
  AddScalableAudioElementWithSubstreamIds(
      IamfInputLayout::kMono, kFirstAudioElementId, kFirstCodecConfigId,
      {kFirstSubstreamId}, codec_config_obus, audio_elements_with_data);
  std::list<MixPresentationObu> mix_presentation_obus;
  std::vector<LoudspeakersSsConventionLayout::SoundSystem>
      sound_system_layouts = {
          LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
          LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0};
  AddMixPresentationObuWithConfigurableLayouts(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, sound_system_layouts,
      mix_presentation_obus);

  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  input_ia_sequence.push_front(&ia_sequence_header);
  std::vector<uint8_t> descriptors = SerializeObusExpectOk(input_ia_sequence);

  std::unique_ptr<api::IamfDecoder> decoder;
  const api::IamfDecoder::Settings kMonoSettings = {
      .requested_mix =
          {.output_layout =
               api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0},
      .requested_output_sample_type = api::OutputSampleType::kInt16LittleEndian,
  };
  ASSERT_TRUE(
      api::IamfDecoder::CreateFromDescriptors(kMonoSettings, descriptors.data(),
                                              descriptors.size(), decoder)
          .ok());

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  AudioFrameObu audio_frame2(ObuHeader(), kFirstSubstreamId,
                             kEightSampleAudioFrame2);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame2});

  // Call decode with both temporal units.
  EXPECT_TRUE(
      decoder->Decode(temporal_units.data(), temporal_units.size()).ok());

  // We expect to get the first temporal unit with the correct number of
  // samples.
  const size_t expected_output_size = 8 * 2;  // 8 samples, 16-bit ints, mono.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);

  std::vector<uint8_t> output_data2(expected_output_size);
  bytes_written = 0;

  // We expect to get the second temporal unit with the correct number of
  // samples.
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data2.data(),
                                          output_data2.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_output_size);
  // Test case is intentionally transparent, using mono with 16 bits. So the
  // output should be the same as the input.
  EXPECT_THAT(output_data, testing::ElementsAreArray(kEightSampleAudioFrame));
  EXPECT_THAT(output_data2, testing::ElementsAreArray(kEightSampleAudioFrame2));

  std::vector<uint8_t> output_data3(expected_output_size);
  bytes_written = 0;

  // If we call GetOutputTemporalUnit again, we not have anything left to
  // output.
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data3.data(),
                                          output_data3.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, 0);
}

TEST(Decode, FailsWhenCalledAfterSignalEndOfDecoding) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());
  EXPECT_FALSE(decoder->Decode(source_data.data(), source_data.size()).ok());
}

TEST(IsTemporalUnitAvailable, ReturnsFalseAfterCreateFromDescriptorObus) {
  auto descriptors = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), descriptors.data(),
                  descriptors.size(), decoder)
                  .ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable,
     TemporalUnitIsNotAvailableAfterDecodeWithNoTemporalDelimiterAtEnd) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});
  source_data.insert(source_data.end(), temporal_unit.begin(),
                     temporal_unit.end());

  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable, ReturnsTrueAfterDecodingMultipleTemporalUnits) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder->Decode({}, 0).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
}

TEST(GetOutputTemporalUnit, FillsOutputVectorWithLastTemporalUnit) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder->Decode({}, 0).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  size_t expected_size = 2 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());

  EXPECT_EQ(bytes_written, expected_size);
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(GetOutputTemporalUnit, FillsOutputVectorWithInt16) {
  std::unique_ptr<api::IamfDecoder> decoder;
  auto decoder_settings = GetStereoDecoderSettings();
  decoder_settings.requested_output_sample_type =
      api::OutputSampleType::kInt16LittleEndian;
  ASSERT_TRUE(api::IamfDecoder::Create(decoder_settings, decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder->Decode({}, 0).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  size_t expected_size = 2 * 8 * 2;  // Stereo * 8 samples * 2 bytes (int16).
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size);
}

TEST(GetOutputTemporalUnit, FillsOutputVectorWithInt16BasedOnInitialSettings) {
  std::unique_ptr<api::IamfDecoder> decoder;
  auto settings = GetStereoDecoderSettings();
  settings.requested_output_sample_type =
      api::OutputSampleType::kInt16LittleEndian;
  ASSERT_TRUE(api::IamfDecoder::Create(settings, decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder->Decode({}, 0).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  size_t expected_size = 2 * 8 * 2;  // Stereo * 8 samples * 2 bytes (int16).
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size);
}

TEST(GetOutputTemporalUnit, FailsWhenBufferTooSmall) {
  std::unique_ptr<api::IamfDecoder> decoder;
  auto decoder_settings = GetStereoDecoderSettings();
  decoder_settings.requested_output_sample_type =
      api::OutputSampleType::kInt16LittleEndian;
  ASSERT_TRUE(api::IamfDecoder::Create(decoder_settings, decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder->Decode({}, 0).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  size_t needed_size = 2 * 8 * 2;  // Stereo * 8 samples * 2 bytes (int16).
  std::vector<uint8_t> output_data(needed_size - 1);  // Buffer too small.
  size_t bytes_written;
  EXPECT_FALSE(decoder
                   ->GetOutputTemporalUnit(output_data.data(),
                                           output_data.size(), bytes_written)
                   .ok());
  EXPECT_EQ(bytes_written, 0);
}

TEST(GetOutputTemporalUnit,
     DoesNotFillOutputVectorWhenNoTemporalUnitIsAvailable) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), source_data.data(),
                  source_data.size(), decoder)
                  .ok());

  std::vector<uint8_t> output_data;
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, 0);
}

TEST(SignalEndOfDecoding, GetMultipleTemporalUnitsOutAfterCall) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());

  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());

  // Stereo * 8 samples * 4 bytes per sample
  const size_t expected_size_per_temp_unit = 2 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size_per_temp_unit);
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size_per_temp_unit);

  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size_per_temp_unit);
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(SignalEndOfDecoding,
     GetMultipleTemporalUnitsOutAfterCallWithTemporalDelimiters) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_delimiter_obu = TemporalDelimiterObu(ObuHeader());
  auto temporal_units =
      SerializeObusExpectOk({&temporal_delimiter_obu, &audio_frame,
                             &temporal_delimiter_obu, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());

  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());

  // Stereo * 8 samples * 4 bytes per sample
  const size_t expected_size_per_temp_unit = 2 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size_per_temp_unit);
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size_per_temp_unit);

  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size_per_temp_unit);
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(SignalEndOfDecoding, SucceedsWithNoTemporalUnits) {
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());

  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  std::vector<uint8_t> output_data;
  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());

  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, 0);
}

TEST(GetSampleRate, ReturnsSampleRateBasedOnCodecConfigObu) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), source_data.data(),
                  source_data.size(), decoder)
                  .ok());

  uint32_t sample_rate;
  ASSERT_TRUE(decoder->GetSampleRate(sample_rate).ok());
  EXPECT_EQ(sample_rate, kSampleRate);
}

TEST(GetFrameSize, ReturnsFrameSizeBasedOnCodecConfigObu) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), source_data.data(),
                  source_data.size(), decoder)
                  .ok());

  uint32_t frame_size;
  ASSERT_TRUE(decoder->GetFrameSize(frame_size).ok());
  EXPECT_EQ(frame_size, kNumSamplesPerFrame);
}

TEST(Reset, DecodingAfterResetSucceedsAfterCreateFromDescriptors) {
  // Create a decoder from descriptors.
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), source_data.data(),
                  source_data.size(), decoder)
                  .ok());
  // Decode a temporal unit.
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame});
  EXPECT_TRUE(
      decoder->Decode(temporal_units.data(), temporal_units.size()).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  // Signal end of decoding and reset.
  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());
  EXPECT_TRUE(decoder->Reset().ok());

  // Confirm that there is no temporal unit available after reset.
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());

  // Decode another temporal unit.
  EXPECT_TRUE(
      decoder->Decode(temporal_units.data(), temporal_units.size()).ok());
  // Confirm that the temporal unit is available and can be retrieved.
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  size_t expected_size = 2 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size);
}

TEST(Reset, DecodingAfterResetFailsInStandaloneCase) {
  // Create a decoder from descriptors.
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  // Add descriptors.
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  // Add temporal unit.
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());

  // Signal end of decoding and reset.
  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());
  // The decoder should fail to reset because we are in a standalone case.
  EXPECT_FALSE(decoder->Reset().ok());
}

TEST(Reset, ResetFailsWhenDescriptorProcessingIncomplete) {
  // Create a decoder without descriptors.
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  EXPECT_FALSE(decoder->Reset().ok());
}

TEST(ResetWithNewLayout,
     DecodingAfterResetWithNewLayoutSucceedsInContainerizedCase) {
  // Create a decoder from descriptors.
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(api::IamfDecoder::CreateFromDescriptors(
                  GetStereoDecoderSettings(), source_data.data(),
                  source_data.size(), decoder)
                  .ok());
  // Decode a temporal unit.
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame});
  EXPECT_TRUE(
      decoder->Decode(temporal_units.data(), temporal_units.size()).ok());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  // Signal end of decoding and reset with 5.1 layout, which is different from
  // the original stereo layout.
  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());
  api::SelectedMix selected_mix;
  EXPECT_TRUE(
      decoder
          ->ResetWithNewMix(
              {.output_layout = api::OutputLayout::kItu2051_SoundSystemB_0_5_0},
              selected_mix)
          .ok());
  EXPECT_EQ(selected_mix.output_layout,
            api::OutputLayout::kItu2051_SoundSystemB_0_5_0);

  // Confirm that there is no temporal unit available after reset.
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());

  // Decode another temporal unit.
  EXPECT_TRUE(
      decoder->Decode(temporal_units.data(), temporal_units.size()).ok());
  // Confirm that the temporal unit is available and can be retrieved.
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  // We now have 6 channels instead of 2, so we updated the expected size
  // accordingly.
  size_t expected_size = 6 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_TRUE(decoder
                  ->GetOutputTemporalUnit(output_data.data(),
                                          output_data.size(), bytes_written)
                  .ok());
  EXPECT_EQ(bytes_written, expected_size);
}

TEST(ResetWithNewLayout, ResetWithNewLayoutFailsInStandaloneCase) {
  // Create a decoder from descriptors.
  std::unique_ptr<api::IamfDecoder> decoder;
  ASSERT_TRUE(
      api::IamfDecoder::Create(GetStereoDecoderSettings(), decoder).ok());
  // Add descriptors.
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  // Add temporal unit.
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  EXPECT_TRUE(decoder->Decode(source_data.data(), source_data.size()).ok());

  EXPECT_TRUE(decoder->SignalEndOfDecoding().ok());
  // The decoder should fail to reset with a new layout because we are in a
  // standalone case.
  api::SelectedMix selected_mix;
  EXPECT_FALSE(
      decoder
          ->ResetWithNewMix(
              {.output_layout = api::OutputLayout::kItu2051_SoundSystemB_0_5_0},
              selected_mix)
          .ok());
}

}  // namespace
}  // namespace iamf_tools
