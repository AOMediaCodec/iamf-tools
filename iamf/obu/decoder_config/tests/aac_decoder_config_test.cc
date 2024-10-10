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

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr int16_t kAudioRollDistance = -1;
constexpr int16_t kInvalidAudioRollDistance = 0;

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
          {.audio_specific_config =
               {.sample_frequency_index_ = AudioSpecificConfig::
                    AudioSpecificConfig::kSampleFrequencyIndex64000}},
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
      .sample_frequency_index_ =
      AudioSpecificConfig::kSampleFrequencyIndexEscapeValue;
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .sampling_frequency_ = 48;

  EXPECT_THAT(GetAacDecoderConfig().Validate(), IsOk());
}

TEST(AacDecoderConfig, ValidatesDecoderConfigDescriptorTag) {
  constexpr uint8_t kInvalidDecoderConfigDescriptorTag = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_config_descriptor_tag_ =
      kInvalidDecoderConfigDescriptorTag;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesObjectTypeIndication) {
  constexpr uint8_t kInvalidObjectTypeIndication = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.object_type_indication_ = kInvalidObjectTypeIndication;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesStreamType) {
  constexpr uint8_t kInvalidStreamType = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.stream_type_ = kInvalidStreamType;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesUpstream) {
  constexpr bool kInvalidUpstream = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.upstream_ = kInvalidUpstream;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesReserved) {
  constexpr bool kInvalidReserved = false;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.reserved_ = kInvalidReserved;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesDecoderSpecificInfoTag) {
  constexpr uint8_t kInvalidDecoderSpecificInfoTag = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.decoder_specific_info_tag =
      kInvalidDecoderSpecificInfoTag;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesAudioObjectType) {
  constexpr uint8_t kInvalidAudioObjectType = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .audio_object_type_ = kInvalidAudioObjectType;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesChannelConfiguration) {
  constexpr uint8_t kInvalidChannelConfiguration = 0;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .channel_configuration_ = kInvalidChannelConfiguration;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesFrameLengthFlag) {
  constexpr bool kInvalidFrameLengthFlag = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.frame_length_flag = kInvalidFrameLengthFlag;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesDepenedsOnCoreCoder) {
  constexpr bool kInvalidDependsOnCoreCoder = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.depends_on_core_coder = kInvalidDependsOnCoreCoder;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AacDecoderConfig, ValidatesExtensionFlag) {
  constexpr bool kInvalidExtensionFlag = true;
  auto aac_decoder_config = GetAacDecoderConfig();
  aac_decoder_config.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.extension_flag = kInvalidExtensionFlag;

  EXPECT_FALSE(aac_decoder_config.Validate().ok());
}

TEST(AudioSpecificConfig, ReadsWithImplicitSampleFrequency) {
  std::vector<uint8_t> data = {
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  AudioSpecificConfig audio_specific_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_THAT(audio_specific_config.Read(rb), IsOk());

  EXPECT_EQ(audio_specific_config.audio_object_type_,
            AudioSpecificConfig::kAudioObjectType);
  EXPECT_EQ(audio_specific_config.sample_frequency_index_,
            AudioSpecificConfig::kSampleFrequencyIndex64000);
  EXPECT_EQ(audio_specific_config.channel_configuration_,
            AudioSpecificConfig::kChannelConfiguration);
  EXPECT_EQ(audio_specific_config.ga_specific_config_.frame_length_flag,
            AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag);
  EXPECT_EQ(audio_specific_config.ga_specific_config_.depends_on_core_coder,
            AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder);
  EXPECT_EQ(audio_specific_config.ga_specific_config_.extension_flag,
            AudioSpecificConfig::GaSpecificConfig::kExtensionFlag);
}

TEST(AudioSpecificConfig, ReadsWithExplicitSampleFrequency) {
  constexpr uint32_t kSampleFrequency = 48000;
  std::vector<uint8_t> data = {
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          ((AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`, upper 7 bits of
      // `sampling_rate`.
      (AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x01) << 7 |
          ((kSampleFrequency & 0xe00000) >> 17),
      // Next 16 bits of `sampling_rate`.
      ((kSampleFrequency & 0x1fe00) >> 9), ((kSampleFrequency & 0x1fe) >> 1),
      // Upper bit of `sampling_rate`, `channel_configuration`,
      // `frame_length_flag`, `depends_on_core_coder`, `extension_flag`.
      ((kSampleFrequency & 1)) | kChannelConfigurationAndGaSpecificConfigMask};
  AudioSpecificConfig audio_specific_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_THAT(audio_specific_config.Read(rb), IsOk());

  EXPECT_EQ(audio_specific_config.sample_frequency_index_,
            AudioSpecificConfig::kSampleFrequencyIndexEscapeValue);
  EXPECT_EQ(audio_specific_config.sampling_frequency_, kSampleFrequency);
}

TEST(AacDecoderConfig, ReadAndValidateReadsAllFields) {
  std::vector<uint8_t> data = {
      // `decoder_config_descriptor_tag`
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
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  AacDecoderConfig decoder_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_THAT(decoder_config.ReadAndValidate(kAudioRollDistance, rb), IsOk());

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

TEST(AacDecoderConfig, ReadAndValidateWithExplicitSampleFrequency) {
  std::vector<uint8_t> data = {
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // ISO 14496:1 expandable size field.
      20,
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
          ((AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`, upper 7 bits of
      // `sampling_rate`.
      (AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x01) << 7 |
          ((48000 & 0xe00000) >> 17),
      // Next 16 bits of `sampling_rate`.
      ((48000 & 0x1fe00) >> 9), ((48000 & 0x1fe) >> 1),
      // Upper bit of `sampling_rate`, `channel_configuration`,
      // `frame_length_flag`, `depends_on_core_coder`, `extension_flag`.
      ((48000 & 1)) | kChannelConfigurationAndGaSpecificConfigMask};
  AacDecoderConfig decoder_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_THAT(decoder_config.ReadAndValidate(kAudioRollDistance, rb), IsOk());

  uint32_t sample_frequency;
  EXPECT_THAT(decoder_config.GetOutputSampleRate(sample_frequency), IsOk());
  EXPECT_EQ(sample_frequency, 48000);
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
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  AacDecoderConfig decoder_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_FALSE(decoder_config.ReadAndValidate(kAudioRollDistance, rb).ok());
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
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask,
      'd', 'e', 'f', 'a', 'b', 'c'};
  AacDecoderConfig decoder_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_THAT(decoder_config.ReadAndValidate(kAudioRollDistance, rb), IsOk());

  EXPECT_EQ(
      decoder_config.decoder_specific_info_.decoder_specific_info_extension,
      std::vector<uint8_t>({'d', 'e', 'f'}));
  EXPECT_EQ(decoder_config.decoder_config_extension_,
            std::vector<uint8_t>({'a', 'b', 'c'}));
}

TEST(AacDecoderConfig, ValidatesAudioRollDistance) {
  std::vector<uint8_t> data = {
      // `decoder_config_descriptor_tag`
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
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  AacDecoderConfig decoder_config;
  ReadBitBuffer rb(1024, &data);

  EXPECT_FALSE(
      decoder_config.ReadAndValidate(kInvalidAudioRollDistance, rb).ok());
}

class AacTest : public testing::Test {
 public:
  AacTest() : aac_decoder_config_(GetAacDecoderConfig()) {}

  ~AacTest() = default;

 protected:
  void TestWriteAudioSpecificConfig() {
    WriteBitBuffer wb(expected_audio_specific_config_.size());

    EXPECT_EQ(aac_decoder_config_.decoder_specific_info_.audio_specific_config
                  .ValidateAndWrite(wb)
                  .ok(),
              expected_write_is_ok_);

    if (expected_write_is_ok_) {
      ValidateWriteResults(wb, expected_audio_specific_config_);
    }
  }

  void TestWriteDecoderConfig() {
    WriteBitBuffer wb(expected_decoder_config_payload_.size());

    EXPECT_EQ(
        aac_decoder_config_.ValidateAndWrite(audio_roll_distance_, wb).ok(),
        expected_write_is_ok_);

    if (expected_write_is_ok_) {
      ValidateWriteResults(wb, expected_decoder_config_payload_);
    }
  }

  // `audio_roll_distance_` would typically come from the associated Codec
  // Config OBU. The IAMF specification REQUIRES this be -1.
  int16_t audio_roll_distance_ = -1;

  AacDecoderConfig aac_decoder_config_;

  bool expected_write_is_ok_ = true;
  std::vector<uint8_t> expected_decoder_config_payload_;
  std::vector<uint8_t> expected_audio_specific_config_;
};

TEST_F(AacTest, DefaultWriteDecoderConfig) {
  expected_decoder_config_payload_ = {
      // Start `DecoderConfigDescriptor`.
      // `decoder_config_descriptor_tag`
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
      // Start `DecoderSpecificInfo`.
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
      // ISO 14496:1 expandable size field.
      2,
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  TestWriteDecoderConfig();
}

TEST_F(AacTest, WritesWithExtension) {
  aac_decoder_config_.decoder_config_extension_ = {'a', 'b', 'c'};
  aac_decoder_config_.decoder_specific_info_.decoder_specific_info_extension = {
      'c', 'd', 'e'};
  expected_decoder_config_payload_ = {
      // Start `DecoderConfigDescriptor`.
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
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask,
      'c', 'd', 'e', 'a', 'b', 'c'};
  TestWriteDecoderConfig();
}

TEST_F(AacTest, DefaultWriteAudioSpecificConfig) {
  expected_audio_specific_config_ = {
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      2 << 3 | ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  TestWriteAudioSpecificConfig();
}

TEST_F(AacTest, ExplicitSampleRate) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ =
      AudioSpecificConfig::kSampleFrequencyIndexEscapeValue;
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sampling_frequency_ = 48000;

  expected_decoder_config_payload_ = {
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // ISO 14496:1 expandable size field.
      20,
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
          ((AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`, upper 7 bits of
      // `sampling_rate`.
      (AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x01) << 7 |
          ((48000 & 0xe00000) >> 17),
      // Next 16 bits of `sampling_rate`.
      ((48000 & 0x1fe00) >> 9), ((48000 & 0x1fe) >> 1),
      // Upper bit of `sampling_rate`, `channel_configuration`,
      // `frame_length_flag`, `depends_on_core_coder`, `extension_flag`.
      ((48000 & 1)) | kChannelConfigurationAndGaSpecificConfigMask};
  TestWriteDecoderConfig();
  uint32_t sample_frequency;
  EXPECT_THAT(aac_decoder_config_.GetOutputSampleRate(sample_frequency),
              IsOk());
  EXPECT_EQ(sample_frequency, 48000);
}

TEST_F(AacTest, ExplicitSampleRateAudioSpecificConfig) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ =
      AudioSpecificConfig::kSampleFrequencyIndexEscapeValue;
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sampling_frequency_ = 48000;

  expected_audio_specific_config_ = {
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      2 << 3 |
          ((AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`, upper 7 bits of
      // `sampling_rate`.
      (AudioSpecificConfig::kSampleFrequencyIndexEscapeValue & 0x01) << 7 |
          ((48000 & 0xe00000) >> 17),
      // Next 16 bits of `sampling_rate`.
      ((48000 & 0x1fe00) >> 9), ((48000 & 0x1fe) >> 1),
      // Upper bit of `sampling_rate`, `channel_configuration`,
      // `frame_length_flag`, `depends_on_core_coder`, `extension_flag`.
      ((48000 & 1)) | kChannelConfigurationAndGaSpecificConfigMask};
  TestWriteAudioSpecificConfig();
}

TEST_F(AacTest, IllegalAudioRollDistanceMustBeNegativeOne) {
  audio_roll_distance_ = 1;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, MaxBufferSizeDb) {
  aac_decoder_config_.buffer_size_db_ = (1 << 24) - 1;

  expected_decoder_config_payload_ = {
      // `decoder_config_descriptor_tag`
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
          ((AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x0e) >> 1),
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      (AudioSpecificConfig::kSampleFrequencyIndex64000 & 0x01) << 7 |
          kChannelConfigurationAndGaSpecificConfigMask};
  TestWriteDecoderConfig();
}

TEST_F(AacTest, OverflowBufferSizeDbOver24Bits) {
  // The spec defines this field as 24 bits. However it is represented in a
  // field that is 32 bits. Any value that cannot be represented in 24 bits
  // should fail.
  aac_decoder_config_.buffer_size_db_ = (1 << 24);
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, GetImplicitSampleRate) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ =
      AudioSpecificConfig::kSampleFrequencyIndex64000;

  uint32_t output_sample_rate;
  EXPECT_THAT(aac_decoder_config_.GetOutputSampleRate(output_sample_rate),
              IsOk());

  EXPECT_EQ(output_sample_rate, 64000);
}

TEST_F(AacTest, GetExplicitSampleRate) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ =
      AudioSpecificConfig::kSampleFrequencyIndexEscapeValue;
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sampling_frequency_ = 1234;

  uint32_t output_sample_rate;
  EXPECT_THAT(aac_decoder_config_.GetOutputSampleRate(output_sample_rate),
              IsOk());
  EXPECT_EQ(output_sample_rate, 1234);
}

TEST_F(AacTest, InvalidReservedSampleRate) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ =
      AudioSpecificConfig::kSampleFrequencyIndexReservedA;

  uint32_t undetermined_output_sample_rate;
  EXPECT_FALSE(
      aac_decoder_config_.GetOutputSampleRate(undetermined_output_sample_rate)
          .ok());
}

TEST_F(AacTest, InvalidSampleFrequencyIndexIsFourBits) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .sample_frequency_index_ =
      static_cast<AudioSpecificConfig::SampleFrequencyIndex>(16);

  uint32_t undetermined_output_sample_rate;
  EXPECT_FALSE(
      aac_decoder_config_.GetOutputSampleRate(undetermined_output_sample_rate)
          .ok());
}

}  // namespace
}  // namespace iamf_tools
