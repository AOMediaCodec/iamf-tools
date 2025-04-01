/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/proto_conversion/proto_to_obu/codec_config_generator.h"

#include <array>
#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

const DecodedUleb128 kCodecConfigId = 200;

void InitMetadataForLpcm(
    ::google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata) {
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_LPCM
          num_samples_per_frame: 64
          audio_roll_distance: 0
          decoder_config_lpcm {
            sample_format_flags: LPCM_LITTLE_ENDIAN
            sample_size: 16
            sample_rate: 16000
          }
        }
      )pb",
      codec_config_metadata.Add());
}

void InitExpectedObuForLpcm(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& expected_obus) {
  expected_obus.emplace(
      kCodecConfigId,
      CodecConfigObu(ObuHeader(), kCodecConfigId,
                     {.codec_id = CodecConfig::kCodecIdLpcm,
                      .num_samples_per_frame = 64,
                      .audio_roll_distance = 0,
                      .decoder_config = LpcmDecoderConfig{
                          .sample_format_flags_bitmask_ =
                              LpcmDecoderConfig::kLpcmLittleEndian,
                          .sample_size_ = 16,
                          .sample_rate_ = 16000}}));
  ASSERT_THAT(expected_obus.at(kCodecConfigId).Initialize(), IsOk());
}

void InitMetadataForOpus(
    ::google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_OPUS
          num_samples_per_frame: 120
          automatically_override_audio_roll_distance: true
          automatically_override_codec_delay: false
          decoder_config_opus {
            version: 1
            output_channel_count: 2
            pre_skip: 0
            input_sample_rate: 48000
            output_gain: 0
            mapping_family: 0
            opus_encoder_metadata {
              target_bitrate_per_channel: 48000
              application: APPLICATION_AUDIO
            }
          }
        }
      )pb",
      codec_config_metadata.Add()));
}

void InitExpectedObuForOpus(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& expected_obus) {
  expected_obus.emplace(
      kCodecConfigId,
      CodecConfigObu(
          ObuHeader(), kCodecConfigId,
          {.codec_id = CodecConfig::CodecConfig::kCodecIdOpus,
           .num_samples_per_frame = 120,
           .audio_roll_distance = -32,
           .decoder_config = OpusDecoderConfig{
               .version_ = 1, .pre_skip_ = 0, .input_sample_rate_ = 48000}}));
  ASSERT_THAT(expected_obus.at(kCodecConfigId).Initialize(), IsOk());
}

void InitMetadataForAac(
    ::google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_AAC_LC
          num_samples_per_frame: 1024
          audio_roll_distance: -1
          automatically_override_codec_delay: false
          decoder_config_aac: {
            decoder_config_descriptor_tag: 0x04
            object_type_indication: 0x40
            stream_type: 0x05
            upstream: 0
            reserved: 1
            buffer_size_db: 0
            max_bitrate: 0
            average_bit_rate: 0
            decoder_specific_info {
              decoder_specific_info_descriptor_tag: 0x05
              audio_object_type: 2
              sample_frequency_index: AAC_SAMPLE_FREQUENCY_INDEX_48000
              channel_configuration: 2
            }
            ga_specific_config {
              frame_length_flag: false
              depends_on_core_coder: false
              extension_flag: false
            }
            aac_encoder_metadata {
              bitrate_mode: 0  #  Constant bit rate mode.
              enable_afterburner: true
              signaling_mode: 2  # Explicit hierarchical signaling.
            }
          }
        }
      )pb",
      codec_config_metadata.Add()));
}

void InitExpectedObuForAac(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& expected_obus) {
  expected_obus.emplace(
      kCodecConfigId,
      CodecConfigObu(
          ObuHeader(), kCodecConfigId,
          {.codec_id = CodecConfig::kCodecIdAacLc,
           .num_samples_per_frame = 1024,
           .audio_roll_distance = -1,
           .decoder_config = AacDecoderConfig{
               .buffer_size_db_ = 0,
               .max_bitrate_ = 0,
               .average_bit_rate_ = 0,
               .decoder_specific_info_{
                   .audio_specific_config =
                       {.sample_frequency_index_ =
                            AudioSpecificConfig::SampleFrequencyIndex::k48000}},
           }}));
  ASSERT_THAT(expected_obus.at(kCodecConfigId).Initialize(), IsOk());
}

void InitMetadataForFlac(
    ::google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_FLAC
          num_samples_per_frame: 64
          audio_roll_distance: 0
          decoder_config_flac: {
            metadata_blocks: {
              header: {
                last_metadata_block_flag: true
                block_type: FLAC_BLOCK_TYPE_STREAMINFO
                metadata_data_block_length: 34
              }
              stream_info {
                minimum_block_size: 64
                maximum_block_size: 64
                minimum_frame_size: 0
                maximum_frame_size: 0
                sample_rate: 48000
                number_of_channels: 1  # Flac interprets this as 2 channels.
                bits_per_sample: 15    # Flac interprets this as 16 bits.
                total_samples_in_stream: 24000
                md5_signature: "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
                               "\x00\x00\x00\x00\x00"
              }
            }
            flac_encoder_metadata { compression_level: 0 }

          }
        }
      )pb",
      codec_config_metadata.Add()));
}

void InitExpectedObuForFlac(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& expected_obus) {
  expected_obus.emplace(
      kCodecConfigId,
      CodecConfigObu(
          ObuHeader(), kCodecConfigId,
          {.codec_id = CodecConfig::kCodecIdFlac,
           .num_samples_per_frame = 64,
           .audio_roll_distance = 0,
           .decoder_config = FlacDecoderConfig{
               {{.header = {.last_metadata_block_flag = true,
                            .block_type = FlacMetaBlockHeader::kFlacStreamInfo,
                            .metadata_data_block_length = 34},
                 .payload = FlacMetaBlockStreamInfo{
                     .minimum_block_size = 64,
                     .maximum_block_size = 64,
                     .sample_rate = 48000,
                     .bits_per_sample = 15,
                     .total_samples_in_stream = 24000}}}}}));
  ASSERT_THAT(expected_obus.at(kCodecConfigId).Initialize(), IsOk());
}

class CodecConfigGeneratorTest : public testing::Test {
 public:
  CodecConfigGeneratorTest() = default;

  absl::StatusOr<absl::flat_hash_map<uint32_t, CodecConfigObu>>
  InitAndGenerate() {
    // Generate the OBUs.
    absl::flat_hash_map<uint32_t, CodecConfigObu> output_obus;
    CodecConfigGenerator generator(codec_config_metadata_);

    if (const auto status = generator.Generate(output_obus); status.ok()) {
      return output_obus;
    } else {
      return status;
    }
  }

 protected:
  ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::CodecConfigObuMetadata>
      codec_config_metadata_;
  absl::flat_hash_map<uint32_t, CodecConfigObu> expected_obus_;
};

TEST_F(CodecConfigGeneratorTest, SucceedsGeneratingNoCodecConfigObus) {
  codec_config_metadata_.Clear();

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_TRUE(output_obus->empty());
}

TEST_F(CodecConfigGeneratorTest, GeneratesObuForLpcm) {
  InitMetadataForLpcm(codec_config_metadata_);
  InitExpectedObuForLpcm(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest, InvalidLpcmDecoderConfigIsMissing) {
  InitMetadataForLpcm(codec_config_metadata_);
  ASSERT_EQ(codec_config_metadata_.at(0).codec_config().codec_id(),
            iamf_tools_cli_proto::CODEC_ID_LPCM);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->clear_decoder_config_lpcm();

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, ConfiguresRedundantCopy) {
  InitMetadataForLpcm(codec_config_metadata_);
  // Reconfigure the OBU to be a redundant copy.
  codec_config_metadata_.at(0).mutable_obu_header()->set_obu_redundant_copy(
      true);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(output_obus->at(kCodecConfigId).header_.obu_redundant_copy, true);
}

TEST_F(CodecConfigGeneratorTest, ConfiguresExtensionHeader) {
  InitMetadataForLpcm(codec_config_metadata_);
  // Reconfigure the OBU to have an extension header.
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        obu_extension_flag: true
        extension_header_size: 5
        extension_header_bytes: "extra"
      )pb",
      codec_config_metadata_.at(0).mutable_obu_header()));

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(output_obus->at(kCodecConfigId).header_.obu_extension_flag, true);
  EXPECT_EQ(output_obus->at(kCodecConfigId).header_.extension_header_size, 5);
  EXPECT_EQ(output_obus->at(kCodecConfigId).header_.extension_header_bytes,
            std::vector<uint8_t>({'e', 'x', 't', 'r', 'a'}));
}

TEST_F(CodecConfigGeneratorTest, ConfiguresLpcmBigEndian) {
  InitMetadataForLpcm(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_lpcm()
      ->set_sample_format_flags(iamf_tools_cli_proto::LPCM_BIG_ENDIAN);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(std::get<LpcmDecoderConfig>(
                output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
                .sample_format_flags_bitmask_,
            LpcmDecoderConfig::kLpcmBigEndian);
}

TEST_F(CodecConfigGeneratorTest, FailsForUnknownSampleFormatFlags) {
  InitMetadataForLpcm(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_lpcm()
      ->set_sample_format_flags(iamf_tools_cli_proto::LPCM_INVALID);

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, DeprecatedCodecIdIsNotSupported) {
  InitMetadataForLpcm(codec_config_metadata_);
  codec_config_metadata_.at(0).mutable_codec_config()->clear_codec_id();
  codec_config_metadata_.at(0).mutable_codec_config()->set_deprecated_codec_id(
      CodecConfig::kCodecIdLpcm);

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, FailsForUnknownCodecId) {
  InitMetadataForLpcm(codec_config_metadata_);
  codec_config_metadata_.at(0).mutable_codec_config()->set_codec_id(
      iamf_tools_cli_proto::CODEC_ID_INVALID);

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, FailsWhenCodecIdIsMissing) {
  InitMetadataForLpcm(codec_config_metadata_);
  codec_config_metadata_.at(0).mutable_codec_config()->clear_codec_id();

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, FailsWhenRollDistanceIsTooLarge) {
  InitMetadataForLpcm(codec_config_metadata_);
  auto* codec_config = codec_config_metadata_.at(0).mutable_codec_config();
  // Roll distance must be castable to `int16_t`.
  codec_config->set_automatically_override_audio_roll_distance(false);
  codec_config->set_audio_roll_distance(std::numeric_limits<int16_t>::max() +
                                        1);

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, GeneratesObuForOpus) {
  InitMetadataForOpus(codec_config_metadata_);
  InitExpectedObuForOpus(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest, IamfOpusFixedFieldsMayBeOmitted) {
  InitMetadataForOpus(codec_config_metadata_);
  // Some fields are fixed in IAMF. It is OK to omit these from the input data.
  auto* metadata_decodec_config_opus = codec_config_metadata_.at(0)
                                           .mutable_codec_config()
                                           ->mutable_decoder_config_opus();
  metadata_decodec_config_opus->clear_output_channel_count();
  metadata_decodec_config_opus->clear_output_gain();
  metadata_decodec_config_opus->clear_mapping_family();
  InitExpectedObuForOpus(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest,
       RollDistanceIsAutomaticallyDeterminedByDefault) {
  InitMetadataForOpus(codec_config_metadata_);
  // Roll distance is mechanically determined by default.
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->clear_audio_roll_distance();
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->clear_automatically_override_codec_delay();

  InitExpectedObuForOpus(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_NE(
      output_obus->at(kCodecConfigId).GetCodecConfig().audio_roll_distance, 0);
}

TEST_F(CodecConfigGeneratorTest,
       AutomaticOverrideRollDistanceFailsWhenNumSamplesPerFrameIsInvalid) {
  constexpr uint32_t kInvalidNumSamplesPerFrame = 0;
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->set_num_samples_per_frame(kInvalidNumSamplesPerFrame);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->set_automatically_override_codec_delay(true);

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, OverridesIncorrectAudioRollDistance) {
  constexpr int16_t kInvalidAudioRollDistance = 100;
  InitMetadataForOpus(codec_config_metadata_);
  // Roll distance is mechanically determined by default.
  codec_config_metadata_.at(0).mutable_codec_config()->set_audio_roll_distance(
      kInvalidAudioRollDistance);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->set_automatically_override_audio_roll_distance(true);

  InitExpectedObuForOpus(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_NE(
      output_obus->at(kCodecConfigId).GetCodecConfig().audio_roll_distance,
      kInvalidAudioRollDistance);
}

TEST_F(CodecConfigGeneratorTest,
       AutomaticallyOverrideCodecDelayOverridesPreSkip) {
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_opus()
      ->clear_pre_skip();
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->set_automatically_override_codec_delay(true);

  const auto output_obus = InitAndGenerate();
  EXPECT_THAT(output_obus, IsOk());
  const auto& decoder_config =
      output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config;
  ASSERT_TRUE(std::holds_alternative<OpusDecoderConfig>(decoder_config));

  EXPECT_NE(std::get<OpusDecoderConfig>(decoder_config).pre_skip_, 0);
}

TEST_F(CodecConfigGeneratorTest,
       AutomaticallyOverrideCodecDelayOverridesIgnoresInputPreSkip) {
  const uint16_t kInvalidInputPreSkip = 9999;
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_opus()
      ->set_pre_skip(kInvalidInputPreSkip);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->set_automatically_override_codec_delay(true);

  const auto output_obus = InitAndGenerate();
  EXPECT_THAT(output_obus, IsOk());
  const auto& decoder_config =
      output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config;
  ASSERT_TRUE(std::holds_alternative<OpusDecoderConfig>(decoder_config));

  EXPECT_NE(std::get<OpusDecoderConfig>(decoder_config).pre_skip_,
            kInvalidInputPreSkip);
}

TEST_F(CodecConfigGeneratorTest,
       ObeysInvalidAudioRollDistanceWhenOverrideDistanceIsFalse) {
  // IAMF requires specific audio roll distance values. The generator does not
  // validate OBU requirements when not directed to override it with the correct
  // value.
  constexpr uint8_t kInvalidAudioRollDistance = 99;
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0).mutable_codec_config()->set_audio_roll_distance(
      kInvalidAudioRollDistance);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->set_automatically_override_audio_roll_distance(false);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(
      output_obus->at(kCodecConfigId).GetCodecConfig().audio_roll_distance,
      kInvalidAudioRollDistance);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidOpusOutputChannelCount) {
  // IAMF requires `output_channel_count` is fixed. The generator does not
  // validate OBU requirements.
  const uint8_t kInvalidOutputChannelCount = 99;
  ASSERT_NE(kInvalidOutputChannelCount, OpusDecoderConfig::kOutputChannelCount);
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_opus()
      ->set_output_channel_count(kInvalidOutputChannelCount);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(std::get<OpusDecoderConfig>(
                output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
                .output_channel_count_,
            kInvalidOutputChannelCount);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidOpusOutputGain) {
  // IAMF requires `output_gain` is fixed. The generator does not validate OBU
  // requirements.
  const uint8_t kInvalidOutputGain = 99;
  ASSERT_NE(kInvalidOutputGain, OpusDecoderConfig::kOutputGain);
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_opus()
      ->set_output_gain(kInvalidOutputGain);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(std::get<OpusDecoderConfig>(
                output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
                .output_gain_,
            kInvalidOutputGain);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidOpusChannelMapping) {
  // IAMF requires `mapping_family` is fixed. The generator does not validate
  // OBU requirements.
  const uint8_t kInvalidMappingFamily = 99;
  ASSERT_NE(kInvalidMappingFamily, OpusDecoderConfig::kMappingFamily);
  InitMetadataForOpus(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_opus()
      ->set_mapping_family(kInvalidMappingFamily);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(std::get<OpusDecoderConfig>(
                output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
                .mapping_family_,
            kInvalidMappingFamily);
}

TEST_F(CodecConfigGeneratorTest, InvalidOpusDecoderConfigIsMissing) {
  InitMetadataForOpus(codec_config_metadata_);
  ASSERT_EQ(codec_config_metadata_.at(0).codec_config().codec_id(),
            iamf_tools_cli_proto::CODEC_ID_OPUS);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->clear_decoder_config_opus();

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, GeneratesObuForAac) {
  InitMetadataForAac(codec_config_metadata_);
  InitExpectedObuForAac(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest, InvalidWhenDecoderSpecificInfoIsMissing) {
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->clear_decoder_specific_info();

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, IamfAacFixedFieldsMayBeOmitted) {
  InitMetadataForAac(codec_config_metadata_);
  // Several fields are fixed in IAMF. It is OK to omit these from the input
  // data.
  auto* metadata_decodec_config_aac = codec_config_metadata_.at(0)
                                          .mutable_codec_config()
                                          ->mutable_decoder_config_aac();
  metadata_decodec_config_aac->clear_decoder_config_descriptor_tag();
  metadata_decodec_config_aac->clear_object_type_indication();
  metadata_decodec_config_aac->clear_stream_type();
  metadata_decodec_config_aac->clear_upstream();
  auto* decoder_specific_info =
      metadata_decodec_config_aac->mutable_decoder_specific_info();
  auto* ga_specific_config =
      metadata_decodec_config_aac->mutable_ga_specific_config();
  decoder_specific_info->clear_decoder_specific_info_descriptor_tag();
  decoder_specific_info->clear_audio_object_type();
  decoder_specific_info->clear_channel_configuration();
  ga_specific_config->clear_frame_length_flag();
  ga_specific_config->clear_depends_on_core_coder();
  ga_specific_config->clear_extension_flag();
  InitExpectedObuForAac(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidAacDecoderConfig) {
  // IAMF requires several fields in the AAC Decoder Config are fixed. The
  // generator does not validate OBU requirements.
  const uint8_t kInvalidDecoderConfigDescriptorTag = 99;
  ASSERT_NE(kInvalidDecoderConfigDescriptorTag,
            AacDecoderConfig::kDecoderConfigDescriptorTag);
  const uint8_t kInvalidObjectTypeIndication = 98;
  ASSERT_NE(kInvalidObjectTypeIndication,
            AacDecoderConfig::kObjectTypeIndication);
  const uint8_t kInvalidStreamType = 97;
  ASSERT_NE(kInvalidStreamType, AacDecoderConfig::kStreamType);
  const bool kInvalidUpstream = true;
  ASSERT_NE(kInvalidUpstream, AacDecoderConfig::kUpstream);

  InitMetadataForAac(codec_config_metadata_);

  auto* decoder_config_aac = codec_config_metadata_.at(0)
                                 .mutable_codec_config()
                                 ->mutable_decoder_config_aac();
  decoder_config_aac->set_decoder_config_descriptor_tag(
      kInvalidDecoderConfigDescriptorTag);
  decoder_config_aac->set_object_type_indication(kInvalidObjectTypeIndication);
  decoder_config_aac->set_stream_type(kInvalidStreamType);
  decoder_config_aac->set_upstream(kInvalidUpstream);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());
  const auto& decoder_config = std::get<AacDecoderConfig>(
      output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config);

  EXPECT_EQ(decoder_config.decoder_config_descriptor_tag_,
            kInvalidDecoderConfigDescriptorTag);
  EXPECT_EQ(decoder_config.object_type_indication_,
            kInvalidObjectTypeIndication);
  EXPECT_EQ(decoder_config.stream_type_, kInvalidStreamType);
  EXPECT_EQ(decoder_config.upstream_, kInvalidUpstream);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidAacAudioSpecificConfig) {
  // IAMF requires `audio_object_type` is fixed. The generator does
  // not validate OBU requirements.
  const uint8_t kInvalidAudioObjectType = 99;
  ASSERT_NE(kInvalidAudioObjectType, AudioSpecificConfig::kAudioObjectType);
  const uint8_t kInvalidChannelConfiguration = 98;
  ASSERT_NE(kInvalidChannelConfiguration,
            AudioSpecificConfig::kChannelConfiguration);
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_audio_object_type(kInvalidAudioObjectType);
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_channel_configuration(kInvalidChannelConfiguration);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  const auto& audio_specific_config =
      std::get<AacDecoderConfig>(
          output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
          .decoder_specific_info_.audio_specific_config;
  EXPECT_EQ(audio_specific_config.audio_object_type_, kInvalidAudioObjectType);
  EXPECT_EQ(audio_specific_config.channel_configuration_,
            kInvalidChannelConfiguration);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidDecoderSpecificInfo) {
  // IAMF requires one field in the Decoder Specific Config is fixed. The
  // generator does not validate OBU requirements.
  const uint8_t kInvalidDecoderSpecificInfoTag = 99;
  ASSERT_NE(kInvalidDecoderSpecificInfoTag,
            AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag);
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_decoder_specific_info_descriptor_tag(
          kInvalidDecoderSpecificInfoTag);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(std::get<AacDecoderConfig>(
                output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
                .decoder_specific_info_.decoder_specific_info_tag,
            kInvalidDecoderSpecificInfoTag);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidAacGaSpecificConfig) {
  // IAMF requires several fields in the GA specific config are fixed. The
  // generator does not validate OBU requirements.
  const bool kInvalidFrameLengthFlag = true;
  ASSERT_NE(kInvalidFrameLengthFlag,
            AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag);
  const bool kDependsOnCoreCoder = true;
  ASSERT_NE(kDependsOnCoreCoder,
            AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder);
  const bool kExtensionFlag = true;
  ASSERT_NE(kDependsOnCoreCoder,
            AudioSpecificConfig::GaSpecificConfig::kExtensionFlag);
  InitMetadataForAac(codec_config_metadata_);
  auto* ga_specific_config = codec_config_metadata_.at(0)
                                 .mutable_codec_config()
                                 ->mutable_decoder_config_aac()
                                 ->mutable_ga_specific_config();
  ga_specific_config->set_frame_length_flag(kInvalidFrameLengthFlag);
  ga_specific_config->set_depends_on_core_coder(kDependsOnCoreCoder);
  ga_specific_config->set_extension_flag(kExtensionFlag);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());
  const auto& generated_ga_specific_config =
      std::get<AacDecoderConfig>(
          output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
          .decoder_specific_info_.audio_specific_config.ga_specific_config_;

  EXPECT_EQ(generated_ga_specific_config.frame_length_flag,
            kInvalidFrameLengthFlag);
  EXPECT_EQ(generated_ga_specific_config.depends_on_core_coder,
            kDependsOnCoreCoder);
  EXPECT_EQ(generated_ga_specific_config.extension_flag, kExtensionFlag);
}

TEST_F(CodecConfigGeneratorTest, InvalidUnknownSamplingFrequencyIndex) {
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_sample_frequency_index(
          iamf_tools_cli_proto::AAC_SAMPLE_FREQUENCY_INDEX_INVALID);

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, ConfiguresAacWithImplicitSamplingFrequency) {
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_sample_frequency_index(
          iamf_tools_cli_proto::AAC_SAMPLE_FREQUENCY_INDEX_24000);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  const auto& audio_specific_config =
      std::get<AacDecoderConfig>(
          output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
          .decoder_specific_info_.audio_specific_config;
  EXPECT_EQ(audio_specific_config.sample_frequency_index_,
            AudioSpecificConfig::SampleFrequencyIndex::k24000);
}

TEST_F(CodecConfigGeneratorTest, ConfiguresAacWithExplicitSamplingFrequency) {
  InitMetadataForAac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_sample_frequency_index(
          iamf_tools_cli_proto::AAC_SAMPLE_FREQUENCY_INDEX_ESCAPE_VALUE);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_aac()
      ->mutable_decoder_specific_info()
      ->set_sampling_frequency(9876);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  const auto& audio_specific_config =
      std::get<AacDecoderConfig>(
          output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
          .decoder_specific_info_.audio_specific_config;
  EXPECT_EQ(audio_specific_config.sample_frequency_index_,
            AudioSpecificConfig::SampleFrequencyIndex::kEscapeValue);
  EXPECT_EQ(audio_specific_config.sampling_frequency_, 9876);
}

TEST_F(CodecConfigGeneratorTest, InvalidAacDecoderConfigIsMissing) {
  InitMetadataForAac(codec_config_metadata_);
  ASSERT_EQ(codec_config_metadata_.at(0).codec_config().codec_id(),
            iamf_tools_cli_proto::CODEC_ID_AAC_LC);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->clear_decoder_config_aac();

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, GeneratesObuForFlac) {
  InitMetadataForFlac(codec_config_metadata_);
  InitExpectedObuForFlac(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest, IamfFlacFixedFieldsMayBeOmitted) {
  InitMetadataForFlac(codec_config_metadata_);
  // Some fields are fixed in IAMF. It is OK to omit these from the input data.
  auto* stream_info = codec_config_metadata_.at(0)
                          .mutable_codec_config()
                          ->mutable_decoder_config_flac()
                          ->mutable_metadata_blocks(0);
  stream_info->mutable_stream_info()->clear_minimum_frame_size();
  stream_info->mutable_stream_info()->clear_maximum_frame_size();
  stream_info->mutable_stream_info()->clear_number_of_channels();
  stream_info->mutable_stream_info()->clear_md5_signature();
  InitExpectedObuForFlac(expected_obus_);

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  EXPECT_EQ(*output_obus, expected_obus_);
}

TEST_F(CodecConfigGeneratorTest, ObeysInvalidFlacStreamInfo) {
  // IAMF requires several fields in the Stream Info block are fixed. The
  // generator does not validate OBU requirements.
  const uint32_t kInvalidMinimumFrameSize = 99;
  ASSERT_NE(kInvalidMinimumFrameSize,
            FlacStreamInfoLooseConstraints::kMinFrameSize);
  const uint32_t kInvalidMaximumFrameSize = 98;
  ASSERT_NE(kInvalidMaximumFrameSize,
            FlacStreamInfoLooseConstraints::kMaxFrameSize);
  const uint8_t kInvalidNumberOfChannels = 97;
  ASSERT_NE(kInvalidNumberOfChannels,
            FlacStreamInfoStrictConstraints::kNumberOfChannels);
  const std::array<uint8_t, 16> kInvalidMd5Signature = {1};
  ASSERT_NE(kInvalidMd5Signature,
            FlacStreamInfoLooseConstraints::kMd5Signature);
  InitMetadataForFlac(codec_config_metadata_);
  auto* stream_info_metadata = codec_config_metadata_.at(0)
                                   .mutable_codec_config()
                                   ->mutable_decoder_config_flac()
                                   ->mutable_metadata_blocks(0)
                                   ->mutable_stream_info();
  stream_info_metadata->set_minimum_frame_size(kInvalidMinimumFrameSize);
  stream_info_metadata->set_maximum_frame_size(kInvalidMaximumFrameSize);
  stream_info_metadata->set_number_of_channels(kInvalidNumberOfChannels);
  stream_info_metadata->set_md5_signature(kInvalidMd5Signature.data(),
                                          kInvalidMd5Signature.size());

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  const auto& stream_info = std::get<FlacMetaBlockStreamInfo>(
      std::get<FlacDecoderConfig>(
          output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config)
          .metadata_blocks_[0]
          .payload);
  EXPECT_EQ(stream_info.minimum_frame_size, kInvalidMinimumFrameSize);
  EXPECT_EQ(stream_info.maximum_frame_size, kInvalidMaximumFrameSize);
  EXPECT_EQ(stream_info.number_of_channels, kInvalidNumberOfChannels);
  EXPECT_EQ(stream_info.md5_signature, kInvalidMd5Signature);
}

TEST_F(CodecConfigGeneratorTest, ConfiguresFlacWithExtraBlocks) {
  InitMetadataForFlac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_flac()
      ->mutable_metadata_blocks(0)
      ->mutable_header()
      ->set_last_metadata_block_flag(false);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        header: {
          last_metadata_block_flag: true
          block_type: FLAC_BLOCK_TYPE_PICTURE
          metadata_data_block_length: 3
        }
        generic_block: "abc"
      )pb",
      codec_config_metadata_.at(0)
          .mutable_codec_config()
          ->mutable_decoder_config_flac()
          ->add_metadata_blocks()));
  const auto kExpectedPictureBlock = FlacMetadataBlock{
      .header = {.last_metadata_block_flag = true,
                 .block_type = FlacMetaBlockHeader::kFlacPicture,
                 .metadata_data_block_length = 3},
      .payload = std::vector<uint8_t>({'a', 'b', 'c'})};

  const auto output_obus = InitAndGenerate();
  ASSERT_THAT(output_obus, IsOk());

  const auto& decoder_config = std::get<FlacDecoderConfig>(
      output_obus->at(kCodecConfigId).GetCodecConfig().decoder_config);
  ASSERT_EQ(decoder_config.metadata_blocks_.size(), 2);

  EXPECT_EQ(decoder_config.metadata_blocks_[0].header.block_type,
            FlacMetaBlockHeader::kFlacStreamInfo);
  EXPECT_EQ(decoder_config.metadata_blocks_[0].header.last_metadata_block_flag,
            false);
  EXPECT_EQ(decoder_config.metadata_blocks_[1], kExpectedPictureBlock);
}

TEST_F(CodecConfigGeneratorTest, FailsWhenMd5SignatureIsNotSixteenBytes) {
  InitMetadataForFlac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_flac()
      ->mutable_metadata_blocks(0)
      ->mutable_stream_info()
      ->mutable_md5_signature()
      ->assign("0");

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, InvalidUnknownBlockType) {
  InitMetadataForFlac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_flac()
      ->mutable_metadata_blocks(0)
      ->mutable_header()
      ->set_last_metadata_block_flag(false);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        header: {
          last_metadata_block_flag: true
          block_type: FLAC_BLOCK_TYPE_INVALID
          metadata_data_block_length: 0
        }
      )pb",
      codec_config_metadata_.at(0)
          .mutable_codec_config()
          ->mutable_decoder_config_flac()
          ->add_metadata_blocks()));

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, InvalidMissingGenericBlock) {
  InitMetadataForFlac(codec_config_metadata_);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_flac()
      ->mutable_metadata_blocks(0)
      ->mutable_header()
      ->set_last_metadata_block_flag(false);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        header: {
          last_metadata_block_flag: true
          block_type: FLAC_BLOCK_TYPE_PICTURE
          metadata_data_block_length: 3
        }
      )pb",
      codec_config_metadata_.at(0)
          .mutable_codec_config()
          ->mutable_decoder_config_flac()
          ->add_metadata_blocks()));
  ASSERT_FALSE(codec_config_metadata_.at(0)
                   .codec_config()
                   .decoder_config_flac()
                   .metadata_blocks(1)
                   .has_generic_block());

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, InvalidFlacDecoderConfigIsMissing) {
  InitMetadataForFlac(codec_config_metadata_);
  ASSERT_EQ(codec_config_metadata_.at(0).codec_config().codec_id(),
            iamf_tools_cli_proto::CODEC_ID_FLAC);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->clear_decoder_config_flac();

  EXPECT_FALSE(InitAndGenerate().ok());
}

TEST_F(CodecConfigGeneratorTest, InvalidMissingStreamInfoBlock) {
  InitMetadataForFlac(codec_config_metadata_);
  ASSERT_EQ(codec_config_metadata_.at(0)
                .codec_config()
                .decoder_config_flac()
                .metadata_blocks(0)
                .header()
                .block_type(),
            iamf_tools_cli_proto::FLAC_BLOCK_TYPE_STREAMINFO);
  codec_config_metadata_.at(0)
      .mutable_codec_config()
      ->mutable_decoder_config_flac()
      ->mutable_metadata_blocks(0)
      ->clear_stream_info();

  EXPECT_FALSE(InitAndGenerate().ok());
}

}  // namespace
}  // namespace iamf_tools
