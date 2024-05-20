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
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

// A 7-bit mask representing `channel_configuration`, and all three fields in
// the GA specific config.
constexpr uint8_t kChannelConfigurationAndGaSpecificConfigMask =
    AudioSpecificConfig::kChannelConfiguration << 3 |               // 4 bits.
    AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag << 2 |  // 1 bit.
    AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder << 1 |  // 1 bit.
    AudioSpecificConfig::GaSpecificConfig::kExtensionFlag << 0;        // 1 bit.

class AacTest : public testing::Test {
 public:
  AacTest()
      : aac_decoder_config_({
            .reserved_ = 0,
            .buffer_size_db_ = 0,
            .max_bitrate_ = 0,
            .average_bit_rate_ = 0,
            .decoder_specific_info_ =
                {.audio_specific_config =
                     {.sample_frequency_index_ = AudioSpecificConfig::
                          AudioSpecificConfig::kSampleFrequencyIndex64000}},
        }) {}

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
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // `object_type_indication`.
      AacDecoderConfig::kObjectTypeIndication,
      // `stream_type`, `upstream`, `reserved`.
      AacDecoderConfig::kStreamType << 2 | AacDecoderConfig::kUpstream << 1 |
          0 << 0,
      // `buffer_size_db`.
      0, 0, 0,
      // `max_bitrate`.
      0, 0, 0, 0,
      // `average_bit_rate`.
      0, 0, 0, 0,
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
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
      // `object_type_indication`.
      AacDecoderConfig::kObjectTypeIndication,
      // `stream_type`, `upstream`, `reserved`.
      AacDecoderConfig::kStreamType << 2 | AacDecoderConfig::kUpstream << 1 |
          0 << 0,
      // `buffer_size_db`.
      0, 0, 0,
      // `max_bitrate`.
      0, 0, 0, 0,
      // `average_bit_rate`.
      0, 0, 0, 0,
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
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

TEST_F(AacTest, IllegalDecoderConfigDescriptorTag) {
  aac_decoder_config_.decoder_config_descriptor_tag_ = 0;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalObjectTypeIndication) {
  aac_decoder_config_.object_type_indication_ = 0;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalStreamType) {
  aac_decoder_config_.stream_type_ = 0;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalUpstream) {
  aac_decoder_config_.upstream_ = true;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, MaxBufferSizeDb) {
  aac_decoder_config_.buffer_size_db_ = (1 << 24) - 1;

  expected_decoder_config_payload_ = {
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // `object_type_indication`.
      AacDecoderConfig::kObjectTypeIndication,
      // `stream_type`, `upstream`, `reserved`.
      AacDecoderConfig::kStreamType << 2 | AacDecoderConfig::kUpstream << 1 |
          0 << 0,
      // `buffer_size_db`.
      0xff, 0xff, 0xff,
      // `max_bitrate`.
      0, 0, 0, 0,
      // `average_bit_rate`.
      0, 0, 0, 0,
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
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

TEST_F(AacTest, IllegalDecoderSpecificInfoTag) {
  aac_decoder_config_.decoder_specific_info_.decoder_specific_info_tag = 0;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalAudioObjectType) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .audio_object_type_ = 0;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalChannelConfiguration) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .channel_configuration_ = 0;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalFrameLengthFlag) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.frame_length_flag = true;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalDependsOnCoreCoder) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.depends_on_core_coder = true;
  expected_write_is_ok_ = false;
  TestWriteDecoderConfig();
}

TEST_F(AacTest, IllegalExtensionFlag) {
  aac_decoder_config_.decoder_specific_info_.audio_specific_config
      .ga_specific_config_.extension_flag = true;
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
