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
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/api/types.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
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

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::Not;

using api::OutputLayout;
using ::testing::Not;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint32_t kBitDepth = 16;
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

TEST(IsDescriptorProcessingComplete,
     ReturnsFalseBeforeDescriptorObusAreProcessed) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);

  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(IamfDecoder,
     MethodsDependingOnDescriptorsFailBeforeDescriptorObusAreProcessed) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);

  EXPECT_THAT(decoder->GetOutputLayout(), Not(IsOk()));
  EXPECT_THAT(decoder->GetNumberOfOutputChannels(), Not(IsOk()));
  EXPECT_THAT(decoder->GetSampleRate(), Not(IsOk()));
  EXPECT_THAT(decoder->GetFrameSize(), Not(IsOk()));
  std::vector<api::MixPresentationMetadata> output_mix_presentation_metadatas;
  EXPECT_THAT(decoder->GetMixPresentations(output_mix_presentation_metadatas),
              Not(IsOk()));
}

TEST(GetOutputLayout, ReturnsOutputLayoutAfterDescriptorObusAreProcessed) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, GenerateBasicDescriptorObus());

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  auto output_layout = decoder->GetOutputLayout();
  EXPECT_THAT(output_layout.status(), IsOk());
  EXPECT_EQ(*output_layout, OutputLayout::kItu2051_SoundSystemA_0_2_0);
  auto number_of_output_channels = decoder->GetNumberOfOutputChannels();
  EXPECT_THAT(number_of_output_channels.status(), IsOk());
  EXPECT_EQ(*number_of_output_channels, 2);
}

TEST(GetOutputLayout, ReturnVirtualDesiredLayoutIfNoMatchingLayoutExists) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemE_4_5_1, GenerateBasicDescriptorObus());

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  auto output_layout = decoder->GetOutputLayout();
  EXPECT_THAT(output_layout.status(), IsOk());
  EXPECT_EQ(*output_layout, OutputLayout::kItu2051_SoundSystemE_4_5_1);
  auto number_of_output_channels = decoder->GetNumberOfOutputChannels();
  EXPECT_THAT(number_of_output_channels.status(), IsOk());
  EXPECT_EQ(*number_of_output_channels, 11);
}

TEST(GetOutputLayout,
     ReturnsVirtualDesiredLayoutIfNoMatchingLayoutExistsUsingDecode) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemE_4_5_1);
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  TemporalDelimiterObu temporal_delimiter_obu =
      TemporalDelimiterObu(ObuHeader());
  auto temporal_delimiter_bytes =
      SerializeObusExpectOk({&temporal_delimiter_obu});
  source_data.insert(source_data.end(), temporal_delimiter_bytes.begin(),
                     temporal_delimiter_bytes.end());

  EXPECT_THAT(decoder->Decode(source_data), IsOk());

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  auto output_layout = decoder->GetOutputLayout();
  EXPECT_THAT(output_layout.status(), IsOk());
  EXPECT_EQ(*output_layout, OutputLayout::kItu2051_SoundSystemE_4_5_1);
  auto number_of_output_channels = decoder->GetNumberOfOutputChannels();
  EXPECT_THAT(number_of_output_channels.status(), IsOk());
  EXPECT_EQ(*number_of_output_channels, 11);
}

TEST(GetOutputLayout, ReturnsNonStereoLayoutWhenPresentInDescriptorObus) {
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
  std::vector<uint8_t> descriptor_obus = SerializeObusExpectOk(
      {&ia_sequence_header, &codec_configs.at(kFirstCodecConfigId),
       &audio_elements.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});

  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemB_0_5_0, descriptor_obus);

  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
  auto output_layout = decoder->GetOutputLayout();
  EXPECT_THAT(output_layout.status(), IsOk());
  EXPECT_EQ(*output_layout, OutputLayout::kItu2051_SoundSystemB_0_5_0);
  auto number_of_output_channels = decoder->GetNumberOfOutputChannels();
  EXPECT_THAT(number_of_output_channels.status(), IsOk());
  EXPECT_EQ(*number_of_output_channels, 6);
}

TEST(Create, SucceedsAndDecodeSucceedsWithPartialData) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
  EXPECT_THAT(decoder, IsOk());

  std::vector<uint8_t> source_data = {0x01, 0x23, 0x45};
  EXPECT_TRUE(decoder->Decode(source_data).ok());
  EXPECT_FALSE(decoder->IsDescriptorProcessingComplete());
}

TEST(Create, SucceedsWithNonStereoLayout) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemB_0_5_0);
  EXPECT_THAT(decoder, IsOk());
}

TEST(CreateFromDescriptors, Succeeds) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, GenerateBasicDescriptorObus());

  EXPECT_THAT(decoder, IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, SucceedsWithNonStereoLayout) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemB_0_5_0, GenerateBasicDescriptorObus());

  EXPECT_THAT(decoder, IsOk());
  EXPECT_TRUE(decoder->IsDescriptorProcessingComplete());
}

TEST(CreateFromDescriptors, FailsWithIncompleteDescriptorObus) {
  auto descriptors = GenerateBasicDescriptorObus();
  // remove the last byte to make the descriptor OBUs incomplete.
  descriptors.pop_back();

  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, descriptors);

  EXPECT_FALSE(decoder.ok());
}

TEST(CreateFromDescriptors, FailsWithDescriptorObuInSubsequentDecode) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, GenerateBasicDescriptorObus());
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
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, source_data);
  ASSERT_THAT(decoder, IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});

  EXPECT_THAT(decoder->Decode(temporal_unit), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
}

TEST(Decode, SucceedsWithOneTemporalUnit) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());

  EXPECT_THAT(decoder->Decode(source_data), IsOk());
}

TEST(Decode, SucceedsWithMultipleTemporalUnitsForNonStereoLayout) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kIAMF_SoundSystemExtension_0_1_0);
  ASSERT_THAT(decoder, IsOk());

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

  EXPECT_THAT(decoder->Decode(source_data), IsOk());
  // Calling with empty due to forced exit after descriptor processing, so that
  // we can get the output temporal unit.
  EXPECT_THAT(decoder->Decode({}), IsOk());

  const size_t expected_output_size = 8 * 4;  // 8 samples, 32-bit ints, mono.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_EQ(bytes_written, expected_output_size);
}

TEST(Decode, CreatedFromDescriptorsSucceedsWithMultipleTemporalUnits) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, GenerateBasicDescriptorObus());
  ASSERT_THAT(decoder, IsOk());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});

  // We expect for decode to succeed and fully process both temporal units. This
  // means that we should be able to pull two temporal units from the decoder,
  // and then there should be nothing left.
  EXPECT_THAT(decoder->Decode(temporal_units), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  const size_t expected_output_size =
      8 * 4 * 2;  // 8 samples, 32-bit ints, stereo.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());

  output_data.clear();
  output_data.resize(expected_output_size);
  bytes_written = 0;

  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(Decode,
     CreatedFromDescriptorsSucceedsWithTemporalUnitsDecodedInSeparatePushes) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, GenerateBasicDescriptorObus());
  ASSERT_THAT(decoder, IsOk());
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_unit = SerializeObusExpectOk({&audio_frame});

  // We expect for decode to succeed and fully process the singular temporal
  // unit that was pushed.
  EXPECT_THAT(decoder->Decode(temporal_unit), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  const size_t expected_output_size =
      8 * 4 * 2;  // 8 samples, 32-bit ints, stereo.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());

  output_data.clear();
  output_data.resize(expected_output_size);
  bytes_written = 0;

  // Now, we expect for decode to succeed and fully process the second temporal
  // unit that was pushed.
  EXPECT_THAT(decoder->Decode(temporal_unit), IsOk());
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
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

  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  std::list<const ObuBase*> input_ia_sequence(
      {&codec_config_obus.at(kFirstCodecConfigId),
       &audio_elements_with_data.at(kFirstAudioElementId).obu,
       &mix_presentation_obus.front()});
  input_ia_sequence.push_front(&ia_sequence_header);
  std::vector<uint8_t> descriptors = SerializeObusExpectOk(input_ia_sequence);

  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kIAMF_SoundSystemExtension_0_1_0, descriptors);
  ASSERT_THAT(decoder, IsOk());

  const std::list<AudioFrameWithData> empty_audio_frames_with_data = {};
  const std::list<ParameterBlockWithData> empty_parameter_blocks_with_data = {};

  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});

  // Call decode with both temporal units.
  EXPECT_THAT(decoder->Decode(temporal_units), IsOk());

  // We expect to get the first temporal unit with the correct number of
  // samples.
  const size_t expected_output_size = 8 * 4;  // 8 samples, 32-bit ints, mono.
  std::vector<uint8_t> output_data(expected_output_size);
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_EQ(bytes_written, expected_output_size);

  output_data.clear();
  output_data.resize(expected_output_size);
  bytes_written = 0;

  // We expect to get the second temporal unit with the correct number of
  // samples.
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_EQ(bytes_written, expected_output_size);
}

TEST(Decode, FailsWhenCalledAfterSignalEndOfStream) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
  ASSERT_THAT(decoder, IsOk());
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  AudioFrameObu audio_frame(ObuHeader(), kFirstSubstreamId,
                            kEightSampleAudioFrame);
  auto temporal_units = SerializeObusExpectOk({&audio_frame, &audio_frame});
  source_data.insert(source_data.end(), temporal_units.begin(),
                     temporal_units.end());
  ASSERT_THAT(decoder->Decode(source_data), IsOk());
  decoder->SignalEndOfStream();
  EXPECT_FALSE(decoder->Decode(source_data).ok());
}

TEST(IsTemporalUnitAvailable, ReturnsFalseAfterCreateFromDescriptorObus) {
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, GenerateBasicDescriptorObus());
  ASSERT_THAT(decoder, IsOk());
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(IsTemporalUnitAvailable,
     TemporalUnitIsNotAvailableAfterDecodeWithNoTemporalDelimiterAtEnd) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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

TEST(GetOutputTemporalUnit, FillsOutputVectorWithLastTemporalUnit) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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

  size_t expected_size = 2 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());

  EXPECT_EQ(bytes_written, expected_size);
  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(GetOutputTemporalUnit, FillsOutputVectorWithInt16) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
  decoder->ConfigureOutputSampleType(api::OutputSampleType::kInt16LittleEndian);
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

  size_t expected_size = 2 * 8 * 2;  // Stereo * 8 samples * 2 bytes (int16).
  std::vector<uint8_t> output_data(expected_size);
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());

  EXPECT_EQ(bytes_written, expected_size);
}

TEST(GetOutputTemporalUnit, FailsWhenBufferTooSmall) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
  decoder->ConfigureOutputSampleType(api::OutputSampleType::kInt16LittleEndian);
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

  size_t needed_size = 2 * 8 * 2;  // Stereo * 8 samples * 2 bytes (int16).
  std::vector<uint8_t> output_data(needed_size - 1);  // Buffer too small.
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              Not(IsOk()));
  EXPECT_EQ(bytes_written, 0);
}

TEST(GetOutputTemporalUnit,
     DoesNotFillOutputVectorWhenNoTemporalUnitIsAvailable) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      OutputLayout::kItu2051_SoundSystemA_0_2_0, source_data);
  ASSERT_THAT(decoder, IsOk());

  std::vector<uint8_t> output_data;
  size_t bytes_written;
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());

  EXPECT_EQ(bytes_written, 0);
}

TEST(SignalEndOfStream, GetMultipleTemporalUnitsOutAfterCall) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
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

  decoder->SignalEndOfStream();

  // Stereo * 8 samples * 4 bytes per sample
  const size_t expected_size_per_temp_unit = 2 * 8 * 4;
  std::vector<uint8_t> output_data(expected_size_per_temp_unit);
  size_t bytes_written;
  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_EQ(bytes_written, expected_size_per_temp_unit);

  EXPECT_TRUE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_EQ(bytes_written, expected_size_per_temp_unit);

  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
}

TEST(SignalEndOfStream, SucceedsWithNoTemporalUnits) {
  auto decoder =
      api::IamfDecoder::Create(OutputLayout::kItu2051_SoundSystemA_0_2_0);
  ASSERT_THAT(decoder, IsOk());

  std::vector<std::vector<int32_t>> output_decoded_temporal_unit;
  std::vector<uint8_t> output_data;
  size_t bytes_written;

  decoder->SignalEndOfStream();

  EXPECT_FALSE(decoder->IsTemporalUnitAvailable());
  EXPECT_THAT(decoder->GetOutputTemporalUnit(absl::MakeSpan(output_data),
                                             bytes_written),
              IsOk());
  EXPECT_EQ(bytes_written, 0);
}

TEST(GetSampleRate, ReturnsSampleRateBasedOnCodecConfigObu) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      api::OutputLayout::kItu2051_SoundSystemA_0_2_0, source_data);
  ASSERT_THAT(decoder, IsOk());

  EXPECT_THAT(decoder->GetSampleRate(), IsOkAndHolds(kSampleRate));
}

TEST(GetFrameSize, ReturnsFrameSizeBasedOnCodecConfigObu) {
  std::vector<uint8_t> source_data = GenerateBasicDescriptorObus();
  auto decoder = api::IamfDecoder::CreateFromDescriptors(
      api::OutputLayout::kItu2051_SoundSystemA_0_2_0, source_data);
  ASSERT_THAT(decoder, IsOk());

  EXPECT_THAT(decoder->GetFrameSize(), IsOkAndHolds(kNumSamplesPerFrame));
}

}  // namespace
}  // namespace iamf_tools
