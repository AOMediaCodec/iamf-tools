/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/decoder_config/aac_decoder_config.h"

#include <array>
#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

using SampleFrequencyIndex = AudioSpecificConfig::SampleFrequencyIndex;

constexpr int16_t kAudioRollDistance = -1;
constexpr int16_t kInvalidAudioRollDistance = 0;

constexpr int64_t kInitialBufferSize = 64;

// Despite being represented in 4-bits the AAC Sampling Frequency Index 64000 is
// serialized across a byte boundary.
constexpr uint8_t kUpperByteSerializedSamplingFrequencyIndex64000 =
    (static_cast<uint8_t>(SampleFrequencyIndex::k64000) & 0x0e) >> 1;
constexpr uint8_t kLowerByteSerializedSamplingFrequencyIndex64000 =
    (static_cast<uint8_t>(SampleFrequencyIndex::k64000) & 0x01) << 7;

// Despite being represented in 4-bits the AAC Sampling Frequency Index 24000 is
// serialized across a byte boundary.
constexpr uint8_t kUpperByteSerializedSamplingFrequencyIndex24000 =
    (static_cast<uint8_t>(SampleFrequencyIndex::k24000) & 0x0e) >> 1;
constexpr uint8_t kLowerByteSerializedSamplingFrequencyIndex24000 =
    (static_cast<uint8_t>(SampleFrequencyIndex::k24000) & 0x01) << 7;

// The ISOBMFF spec has an escape value for arbitrary sample rates. IAMF
// forbids the use of this escape value.
constexpr uint8_t kUpperByteSerializedSamplingFrequencyIndexEscape =
    (static_cast<uint8_t>(15) & 0x0e) >> 1;
constexpr uint8_t kLowerByteSerializedSamplingFrequencyIndexEscape =
    (static_cast<uint8_t>(15) & 0x01) << 7;

// A 7-bit mask representing `channel_configuration`, and all three fields in
// the GA specific config.
constexpr uint8_t kChannelConfigurationAndGaSpecificConfigMask =
    AudioSpecificConfig::kChannelConfiguration << 3 |               // 4 bits.
    AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag << 2 |  // 1 bit.
    AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder << 1 |  // 1 bit.
    AudioSpecificConfig::GaSpecificConfig::kExtensionFlag << 0;        // 1 bit.

constexpr uint8_t kStreamTypeUpstreamReserved =
    AacDecoderConfig::kStreamType << 2 | AacDecoderConfig::kUpstream << 1 |
    AacDecoderConfig::kReserved << 0;

constexpr auto kDefaultAudioSpecificConfigPayload = std::to_array<uint8_t>(
    {// `audio_object_type`, upper 3 bits of `sample_frequency_index`.
     AudioSpecificConfig::kAudioObjectType << 3 |
         kUpperByteSerializedSamplingFrequencyIndex64000,
     // lower bit of `sample_frequency_index`,
     // `channel_configuration`, `frame_length_flag`,
     // `depends_on_core_coder`, `extension_flag`.
     kLowerByteSerializedSamplingFrequencyIndex64000 |
         kChannelConfigurationAndGaSpecificConfigMask});

constexpr auto kExplicitSampleRate48000AudioSpecificConfigPayload =
    std::to_array<uint8_t>(
        {// `audio_object_type`, upper 3 bits of `sample_frequency_index`.
         AudioSpecificConfig::kAudioObjectType << 3 |
             kUpperByteSerializedSamplingFrequencyIndexEscape,
         // lower bit of `sample_frequency_index`, upper 7 bits of
         // `sampling_rate`.
         kLowerByteSerializedSamplingFrequencyIndexEscape |
             ((48000 & 0xe00000) >> 17),
         // Next 16 bits of `sampling_rate`.
         ((48000 & 0x1fe00) >> 9), ((48000 & 0x1fe) >> 1),
         // Upper bit of `sampling_rate`, `channel_configuration`,
         // `frame_length_flag`, `depends_on_core_coder`, `extension_flag`.
         (48000 & 1) | kChannelConfigurationAndGaSpecificConfigMask});

constexpr auto kDefaultAudioDecoderConfigPayload = std::to_array<uint8_t>(
    {// `decoder_config_descriptor_tag`
     AacDecoderConfig::kDecoderConfigDescriptorTag,
     // ISO 14496:1 expandable size field.
     17,
     // `object_type_indication`.
     AacDecoderConfig::kObjectTypeIndication,
     // `stream_type`, `upstream`, `reserved`.
     kStreamTypeUpstreamReserved,
     // `buffer_size_db`.
     0, 0, 0,
     // `max_bitrate`.
     0, 0, 0, 0,
     // `average_bit_rate`.
     0, 0, 0, 0,
     // `decoder_specific_info_tag`
     AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
     // ISO 14496:1 expandable size field.
     2,
     // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
     AudioSpecificConfig::kAudioObjectType << 3 |
         kUpperByteSerializedSamplingFrequencyIndex64000,
     // lower bit of `sample_frequency_index`,
     // `channel_configuration`, `frame_length_flag`,
     // `depends_on_core_coder`, `extension_flag`.
     kLowerByteSerializedSamplingFrequencyIndex64000 |
         kChannelConfigurationAndGaSpecificConfigMask});

TEST(GetRequiredAudioRollDistance, ReturnsFixedValue) {
  EXPECT_EQ(AacDecoderConfig::GetRequiredAudioRollDistance(),
            kAudioRollDistance);
}

AacDecoderConfig GetAacDecoderConfig() {
  return AacDecoderConfig{
      .buffer_size_db_ = 0,
      .max_bitrate_ = 0,
      .average_bit_rate_ = 0,
      .decoder_specific_info_ =
          {.audio_specific_config = {.sample_frequency_index_ =
                                         SampleFrequencyIndex::k64000}},
  };
}

TEST(AacDecoderConfig, ValidateWithCommonValues) {
  EXPECT_THAT(GetAacDecoderConfig().Validate(), IsOk());
}

TEST(AacDecoderConfig, ValidateWithManyVaryingValues) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.buffer_size_db_ = 1;
  aac_decoder_config.max_bitrate_ = 1;
  aac_decoder_config.average_bit_rate_ = 1;
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ = SampleFrequencyIndex::k11025;

  EXPECT_THAT(aac_decoder_config.Validate(), IsOk());
}

TEST(AacDecoderConfig, ValidatesDecoderConfigDescriptorTag) {
  constexpr uint8_t kInvalidDecoderConfigDescriptorTag = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_config_descriptor_tag_ =
      kInvalidDecoderConfigDescriptorTag;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesObjectTypeIndication) {
  constexpr uint8_t kInvalidObjectTypeIndication = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.object_type_indication_ = kInvalidObjectTypeIndication;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesStreamType) {
  constexpr uint8_t kInvalidStreamType = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.stream_type_ = kInvalidStreamType;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesUpstream) {
  constexpr bool kInvalidUpstream = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.upstream_ = kInvalidUpstream;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesReserved) {
  constexpr bool kInvalidReserved = false;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.reserved_ = kInvalidReserved;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesDecoderSpecificInfoTag) {
  constexpr uint8_t kInvalidDecoderSpecificInfoTag = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.decoder_specific_info_tag =
      kInvalidDecoderSpecificInfoTag;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesAudioObjectType) {
  constexpr uint8_t kInvalidAudioObjectType = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .audio_object_type_ = kInvalidAudioObjectType;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesChannelConfiguration) {
  constexpr uint8_t kInvalidChannelConfiguration = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .channel_configuration_ = kInvalidChannelConfiguration;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesFrameLengthFlag) {
  constexpr bool kInvalidFrameLengthFlag = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.frame_length_flag = kInvalidFrameLengthFlag;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesDepenedsOnCoreCoder) {
  constexpr bool kInvalidDependsOnCoreCoder = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.depends_on_core_coder = kInvalidDependsOnCoreCoder;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AacDecoderConfig, ValidatesExtensionFlag) {
  constexpr bool kInvalidExtensionFlag = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.extension_flag = kInvalidExtensionFlag;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(Validate, ValidatesSampleRateIsNotReserved) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ = SampleFrequencyIndex::kReservedA;

  EXPECT_THAT(aac_decoder_config.Validate(), Not(IsOk()));
}

TEST(AudioSpecificConfig, ReadsWithImplicitSampleFrequency64000) {
  AudioSpecificConfig audio_specific_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      kDefaultAudioSpecificConfigPayload);

  EXPECT_THAT(audio_specific_config.Read(*rb), IsOk());

  EXPECT_EQ(audio_specific_config.audio_object_type_,
            AudioSpecificConfig::kAudioObjectType);
  EXPECT_EQ(audio_specific_config.sample_frequency_index_,
            SampleFrequencyIndex::k64000);
  EXPECT_EQ(audio_specific_config.channel_configuration_,
            AudioSpecificConfig::kChannelConfiguration);
  EXPECT_EQ(audio_specific_config.ga_specific_config_.frame_length_flag,
            AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag);
  EXPECT_EQ(audio_specific_config.ga_specific_config_.depends_on_core_coder,
            AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder);
  EXPECT_EQ(audio_specific_config.ga_specific_config_.extension_flag,
            AudioSpecificConfig::GaSpecificConfig::kExtensionFlag);
}

TEST(AudioSpecificConfig, ReadsWithImplicitSampleFrequency24000) {
  const std::vector<uint8_t> data = {
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          kUpperByteSerializedSamplingFrequencyIndex24000,
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      kLowerByteSerializedSamplingFrequencyIndex24000 |
          kChannelConfigurationAndGaSpecificConfigMask};
  AudioSpecificConfig audio_specific_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);

  EXPECT_THAT(audio_specific_config.Read(*rb), IsOk());

  EXPECT_EQ(audio_specific_config.sample_frequency_index_,
            SampleFrequencyIndex::k24000);
}

TEST(AudioSpecificConfig, ReadFailsWithExplicitSampleFrequency) {
  AudioSpecificConfig audio_specific_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      kExplicitSampleRate48000AudioSpecificConfigPayload);

  EXPECT_THAT(audio_specific_config.Read(*rb), Not(IsOk()));
}

TEST(AacDecoderConfig, ReadAndValidateReadsAllFields) {
  AacDecoderConfig decoder_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      kDefaultAudioDecoderConfigPayload);

  EXPECT_THAT(decoder_config.ReadAndValidate(kAudioRollDistance, *rb), IsOk());

  EXPECT_EQ(decoder_config.decoder_config_descriptor_tag_,
            AacDecoderConfig::kDecoderConfigDescriptorTag);
  EXPECT_EQ(decoder_config.object_type_indication_,
            AacDecoderConfig::kObjectTypeIndication);
  EXPECT_EQ(decoder_config.stream_type_, AacDecoderConfig::kStreamType);
  EXPECT_EQ(decoder_config.upstream_, AacDecoderConfig::kUpstream);
  EXPECT_EQ(decoder_config.buffer_size_db_, 0);
  EXPECT_EQ(decoder_config.max_bitrate_, 0);
  EXPECT_EQ(decoder_config.average_bit_rate_, 0);
  EXPECT_EQ(decoder_config.decoder_specific_info_.decoder_specific_info_tag,
            AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag);
  EXPECT_TRUE(decoder_config.decoder_specific_info_
                  .decoder_specific_info_extension.empty());
  EXPECT_TRUE(decoder_config.decoder_config_extension_.empty());
  uint32_t sample_frequency;
  EXPECT_THAT(decoder_config.GetOutputSampleRate(sample_frequency), IsOk());
  EXPECT_EQ(sample_frequency, 64000);
}

TEST(AacDecoderConfig, FailsIfDecoderConfigDescriptorExpandableSizeIsTooSmall) {
  std::vector<uint8_t> data = {
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // ISO 14496:1 expandable size field.
      16,
      // `object_type_indication`.
      AacDecoderConfig::kObjectTypeIndication,
      // `stream_type`, `upstream`, `reserved`.
      kStreamTypeUpstreamReserved,
      // `buffer_size_db`.
      0, 0, 0,
      // `max_bitrate`.
      0, 0, 0, 0,
      // `average_bit_rate`.
      0, 0, 0, 0,
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
      // ISO 14496:1 expandable size field.
      3,
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          kUpperByteSerializedSamplingFrequencyIndex64000,
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      kLowerByteSerializedSamplingFrequencyIndex64000 |
          kChannelConfigurationAndGaSpecificConfigMask};
  AacDecoderConfig decoder_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);

  EXPECT_THAT(decoder_config.ReadAndValidate(kAudioRollDistance, *rb),
              Not(IsOk()));
}

TEST(AacDecoderConfig, ReadExtensions) {
  std::vector<uint8_t> data = {
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // ISO 14496:1 expandable size field.
      23,
      // `object_type_indication`.
      AacDecoderConfig::kObjectTypeIndication,
      // `stream_type`, `upstream`, `reserved`.
      kStreamTypeUpstreamReserved,
      // `buffer_size_db`.
      0, 0, 0,
      // `max_bitrate`.
      0, 0, 0, 0,
      // `average_bit_rate`.
      0, 0, 0, 0,
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
      // ISO 14496:1 expandable size field.
      5,
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          kUpperByteSerializedSamplingFrequencyIndex64000,
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      kLowerByteSerializedSamplingFrequencyIndex64000 |
          kChannelConfigurationAndGaSpecificConfigMask,
      'd', 'e', 'f', 'a', 'b', 'c'};
  AacDecoderConfig decoder_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(data);

  EXPECT_THAT(decoder_config.ReadAndValidate(kAudioRollDistance, *rb), IsOk());

  EXPECT_EQ(
      decoder_config.decoder_specific_info_.decoder_specific_info_extension,
      std::vector<uint8_t>({'d', 'e', 'f'}));
  EXPECT_EQ(decoder_config.decoder_config_extension_,
            std::vector<uint8_t>({'a', 'b', 'c'}));
}

TEST(AacDecoderConfig, ValidatesAudioRollDistance) {
  AacDecoderConfig decoder_config;
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      kDefaultAudioDecoderConfigPayload);

  EXPECT_THAT(decoder_config.ReadAndValidate(kInvalidAudioRollDistance, *rb),
              Not(IsOk()));
}

TEST(ValidateAndWrite, WritesDefaultDecoderConfig) {
  const auto aac_decoder_config = GetAacDecoderConfig();

  WriteBitBuffer wb(kInitialBufferSize);
  EXPECT_THAT(aac_decoder_config.ValidateAndWrite(kAudioRollDistance, wb),
              IsOk());

  ValidateWriteResults(wb, kDefaultAudioDecoderConfigPayload);
}

TEST(ValidateAndWrite, WritesWithExtension) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_config_extension_ = {'a', 'b', 'c'};
  aac_decoder_config.decoder_specific_info_.decoder_specific_info_extension = {
      'c', 'd', 'e'};
  const auto kExpectedPayload = std::to_array<uint8_t>(
      {// Start `DecoderConfigDescriptor`.
       // `decoder_config_descriptor_tag`
       AacDecoderConfig::kDecoderConfigDescriptorTag,
       // ISO 14496:1 expandable size field.
       23,
       // `object_type_indication`.
       AacDecoderConfig::kObjectTypeIndication,
       // `stream_type`, `upstream`, `reserved`.
       kStreamTypeUpstreamReserved,
       // `buffer_size_db`.
       0, 0, 0,
       // `max_bitrate`.
       0, 0, 0, 0,
       // `average_bit_rate`.
       0, 0, 0, 0,
       // Start `DecoderSpecificInfo`.
       // `decoder_specific_info_tag`
       AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
       // ISO 14496:1 expandable size field.
       5,
       // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
       AudioSpecificConfig::kAudioObjectType << 3 |
           kUpperByteSerializedSamplingFrequencyIndex64000,
       // lower bit of `sample_frequency_index`,
       // `channel_configuration`, `frame_length_flag`,
       // `depends_on_core_coder`, `extension_flag`.
       kLowerByteSerializedSamplingFrequencyIndex64000 |
           kChannelConfigurationAndGaSpecificConfigMask,
       'c', 'd', 'e', 'a', 'b', 'c'});

  WriteBitBuffer wb(kInitialBufferSize);
  EXPECT_THAT(aac_decoder_config.ValidateAndWrite(kAudioRollDistance, wb),
              IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(AudioSpecificConfigValidateAndWrite, DefaultValuesAreExpected) {
  const auto audio_specific_config =
      GetAacDecoderConfig().decoder_specific_info_.audio_specific_config;

  WriteBitBuffer wb(kInitialBufferSize);
  EXPECT_THAT(audio_specific_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kDefaultAudioSpecificConfigPayload);
}

TEST(ValidateAndWrite, IllegalAudioRollDistanceMustBeNegativeOne) {
  auto aac_decoder_config = GetAacDecoderConfig();

  constexpr int16_t kIllegalAudioRollDistance = 1;
  WriteBitBuffer wb(kInitialBufferSize);
  EXPECT_THAT(
      aac_decoder_config.ValidateAndWrite(kIllegalAudioRollDistance, wb),
      Not(IsOk()));
}

TEST(ValidateAndWrite, WritesMaxBufferSizeDb) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.buffer_size_db_ = (1 << 24) - 1;

  constexpr auto kExpectedPayload = std::to_array<uint8_t>(
      {// `decoder_config_descriptor_tag`
       AacDecoderConfig::kDecoderConfigDescriptorTag,
       // ISO 14496:1 expandable size field.
       17,
       // `object_type_indication`.
       AacDecoderConfig::kObjectTypeIndication,
       // `stream_type`, `upstream`, `reserved`.
       kStreamTypeUpstreamReserved,
       // `buffer_size_db`.
       0xff, 0xff, 0xff,
       // `max_bitrate`.
       0, 0, 0, 0,
       // `average_bit_rate`.
       0, 0, 0, 0,
       // `decoder_specific_info_tag`
       AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
       // ISO 14496:1 expandable size field.
       2,
       // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
       AudioSpecificConfig::kAudioObjectType << 3 |
           kUpperByteSerializedSamplingFrequencyIndex64000,
       // lower bit of `sample_frequency_index`,
       // `channel_configuration`, `frame_length_flag`,
       // `depends_on_core_coder`, `extension_flag`.
       kLowerByteSerializedSamplingFrequencyIndex64000 |
           kChannelConfigurationAndGaSpecificConfigMask});

  WriteBitBuffer wb(kInitialBufferSize);
  EXPECT_THAT(aac_decoder_config.ValidateAndWrite(kAudioRollDistance, wb),
              IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, InvalidOverflowBufferSizeDbOver24Bits) {
  auto aac_decoder_config = GetAacDecoderConfig();
  // The spec defines this field as 24 bits. However it is represented in a
  // field that is 32 bits. Any value that cannot be represented in 24 bits
  // should fail.
  aac_decoder_config.buffer_size_db_ = (1 << 24);

  WriteBitBuffer wb(kInitialBufferSize);
  EXPECT_THAT(aac_decoder_config.ValidateAndWrite(kAudioRollDistance, wb),
              Not(IsOk()));
}

TEST(GetOutputSampleRate, GetImplicitSampleRate64000) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ = SampleFrequencyIndex::k64000;

  uint32_t output_sample_rate;
  EXPECT_THAT(aac_decoder_config.GetOutputSampleRate(output_sample_rate),
              IsOk());

  EXPECT_EQ(output_sample_rate, 64000);
}

TEST(GetOutputSampleRate, GetImplicitSampleRate24000) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ = SampleFrequencyIndex::k24000;

  uint32_t output_sample_rate;
  EXPECT_THAT(aac_decoder_config.GetOutputSampleRate(output_sample_rate),
              IsOk());

  EXPECT_EQ(output_sample_rate, 24000);
}

TEST(GetOutputSampleRate, InvalidReservedSampleRateA) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ = SampleFrequencyIndex::kReservedA;

  uint32_t output_sample_rate;
  EXPECT_THAT(aac_decoder_config.GetOutputSampleRate(output_sample_rate),
              Not(IsOk()));
  EXPECT_EQ(output_sample_rate, 0);
}

TEST(GetOutputSampleRate, InvalidReservedSampleRateB) {
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ = SampleFrequencyIndex::kReservedB;

  uint32_t output_sample_rate;
  EXPECT_THAT(aac_decoder_config.GetOutputSampleRate(output_sample_rate),
              Not(IsOk()));
  EXPECT_EQ(output_sample_rate, 0);
}

}  // namespace
}  // namespace iamf_tools
