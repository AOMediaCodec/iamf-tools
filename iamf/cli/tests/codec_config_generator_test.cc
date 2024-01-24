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
#include "iamf/cli/codec_config_generator.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/aac_decoder_config.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/codec_config.h"
#include "iamf/flac_decoder_config.h"
#include "iamf/lpcm_decoder_config.h"
#include "iamf/obu_header.h"
#include "iamf/opus_decoder_config.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

class CodecConfigGeneratorTest : public testing::Test {
 public:
  CodecConfigGeneratorTest() {
    google::protobuf::TextFormat::ParseFromString(
        R"pb(
          codec_config_id: 0
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
        codec_config_metadata_.Add());
  }

  void InitAndTestGenerate() {
    // The generator initializes all OBUs. Make sure expected OBUs are
    // initialized.
    for (auto& [codec_config_id, obu] : expected_obus_) {
      ASSERT_TRUE(obu.Initialize().ok());
    }

    // Generate the OBUs.
    absl::flat_hash_map<uint32_t, CodecConfigObu> output_obus;
    CodecConfigGenerator generator(codec_config_metadata_);
    EXPECT_EQ(generator.Generate(output_obus).code(),
              expected_generate_status_code_);

    EXPECT_EQ(expected_obus_, output_obus);
  }

 protected:
  ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::CodecConfigObuMetadata>
      codec_config_metadata_;
  absl::StatusCode expected_generate_status_code_ = absl::StatusCode::kOk;
  absl::flat_hash_map<uint32_t, CodecConfigObu> expected_obus_;
};

TEST_F(CodecConfigGeneratorTest, DefaultLpcm) {
  expected_obus_.emplace(
      0, CodecConfigObu(
             ObuHeader(), 0,
             {.codec_id = CodecConfig::kCodecIdLpcm,
              .num_samples_per_frame = 64,
              .audio_roll_distance = 0,
              .decoder_config = LpcmDecoderConfig{
                  .sample_format_flags_ = LpcmDecoderConfig::kLpcmLittleEndian,
                  .sample_size_ = 16,
                  .sample_rate_ = 16000}}));
  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, RedundantCopy) {
  codec_config_metadata_.at(0).mutable_obu_header()->set_obu_redundant_copy(
      true);

  expected_obus_.emplace(
      0, CodecConfigObu(
             ObuHeader{.obu_redundant_copy = true}, 0,
             {.codec_id = CodecConfig::kCodecIdLpcm,
              .num_samples_per_frame = 64,
              .audio_roll_distance = 0,
              .decoder_config = LpcmDecoderConfig{
                  .sample_format_flags_ = LpcmDecoderConfig::kLpcmLittleEndian,
                  .sample_size_ = 16,
                  .sample_rate_ = 16000}}));
  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, ExtensionHeader) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        obu_extension_flag: true
        extension_header_size: 5
        extension_header_bytes: "extra"
      )pb",
      codec_config_metadata_.at(0).mutable_obu_header()));

  expected_obus_.emplace(
      0, CodecConfigObu(
             ObuHeader{.obu_extension_flag = true,
                       .extension_header_size = 5,
                       .extension_header_bytes = {'e', 'x', 't', 'r', 'a'}},
             0,
             {.codec_id = CodecConfig::CodecConfig::kCodecIdLpcm,
              .num_samples_per_frame = 64,
              .audio_roll_distance = 0,
              .decoder_config = LpcmDecoderConfig{
                  .sample_format_flags_ = LpcmDecoderConfig::kLpcmLittleEndian,
                  .sample_size_ = 16,
                  .sample_rate_ = 16000}}));
  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, NoCodecConfigObus) {
  codec_config_metadata_.Clear();
  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, FallsBackToDeprecatedCodecIdField) {
  // `deprecated_codec_id` is used as a fallback when `codec_id` is missing.
  codec_config_metadata_.at(0).mutable_codec_config()->clear_codec_id();
  codec_config_metadata_.at(0).mutable_codec_config()->set_deprecated_codec_id(
      CodecConfig::kCodecIdLpcm);

  expected_obus_.emplace(
      0, CodecConfigObu(
             ObuHeader(), 0,
             {.codec_id = CodecConfig::kCodecIdLpcm,
              .num_samples_per_frame = 64,
              .audio_roll_distance = 0,
              .decoder_config = LpcmDecoderConfig{
                  .sample_format_flags_ = LpcmDecoderConfig::kLpcmLittleEndian,
                  .sample_size_ = 16,
                  .sample_rate_ = 16000}}));

  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, UnknownCodecId) {
  codec_config_metadata_.at(0).mutable_codec_config()->set_codec_id(
      iamf_tools_cli_proto::CODEC_ID_INVALID);
  expected_generate_status_code_ = absl::StatusCode::kInvalidArgument;

  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, BadRollDistanceCast) {
  codec_config_metadata_.at(0).mutable_codec_config()->set_audio_roll_distance(
      std::numeric_limits<int16_t>::max() + 1);
  expected_generate_status_code_ = absl::StatusCode::kInvalidArgument;

  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, Opus) {
  codec_config_metadata_.Clear();

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_OPUS
          num_samples_per_frame: 120
          audio_roll_distance: -32
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
      codec_config_metadata_.Add()));

  expected_obus_.emplace(
      200, CodecConfigObu(
               ObuHeader(), 200,
               {.codec_id = CodecConfig::CodecConfig::kCodecIdOpus,
                .num_samples_per_frame = 120,
                .audio_roll_distance = -32,
                .decoder_config = OpusDecoderConfig{.version_ = 1,
                                                    .output_channel_count_ = 2,
                                                    .pre_skip_ = 0,
                                                    .input_sample_rate_ = 48000,
                                                    .output_gain_ = 0,
                                                    .mapping_family_ = 0}}));
  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, Aac) {
  codec_config_metadata_.Clear();

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_AAC_LC
          num_samples_per_frame: 1024
          audio_roll_distance: -1
          decoder_config_aac: {
            decoder_config_descriptor_tag: 0x04
            object_type_indication: 0x40
            stream_type: 0x05
            upstream: 0
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
      codec_config_metadata_.Add()));

  expected_obus_.emplace(
      200,
      CodecConfigObu(
          ObuHeader(), 200,
          {.codec_id = CodecConfig::kCodecIdAacLc,
           .num_samples_per_frame = 1024,
           .audio_roll_distance = -1,
           .decoder_config = AacDecoderConfig{
               .decoder_config_descriptor_tag_ = 0x04,
               .object_type_indication_ = 0x40,
               .stream_type_ = 0x05,
               .upstream_ = 0,
               .reserved_ = 0,
               .buffer_size_db_ = 0,
               .max_bitrate_ = 0,
               .average_bit_rate_ = 0,
               .decoder_specific_info_{
                   .decoder_specific_info_tag = 0x05,
                   .audio_specific_config =
                       {.audio_object_type_ = 2,
                        .sample_frequency_index_ =
                            AudioSpecificConfig::kSampleFrequencyIndex48000,
                        .sampling_frequency_ = 0,
                        .channel_configuration_ = 2,
                        .ga_specific_config_ =
                            {
                                .frame_length_flag = 0,
                                .depends_on_core_coder = 0,
                                .extension_flag = 0,
                            }}},
           }}));
  InitAndTestGenerate();
}

TEST_F(CodecConfigGeneratorTest, Flac) {
  codec_config_metadata_.Clear();

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
                last_metadata_block_flag: false
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
                md5_signature: "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
              }
            }
            metadata_blocks: {
              header: {
                last_metadata_block_flag: true
                block_type: FLAC_BLOCK_TYPE_PICTURE
                metadata_data_block_length: 3
              }
              generic_block: "abc"
            }
            flac_encoder_metadata { compression_level: 0 }

          }
        }
      )pb",
      codec_config_metadata_.Add()));

  expected_obus_.emplace(
      200,
      CodecConfigObu(
          ObuHeader(), 200,
          {.codec_id = CodecConfig::kCodecIdFlac,
           .num_samples_per_frame = 64,
           .audio_roll_distance = 0,
           .decoder_config = FlacDecoderConfig{
               {{.header = {.last_metadata_block_flag = false,
                            .block_type = FlacMetaBlockHeader::kFlacStreamInfo,
                            .metadata_data_block_length = 34},
                 .payload =
                     FlacMetaBlockStreamInfo{
                         .minimum_block_size = 64,
                         .maximum_block_size = 64,
                         .minimum_frame_size = 0,
                         .maximum_frame_size = 0,
                         .sample_rate = 48000,
                         .number_of_channels = 1,
                         .bits_per_sample = 15,
                         .total_samples_in_stream = 24000,
                         .md5_signature = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00}}},
                {.header = {.last_metadata_block_flag = true,
                            .block_type = FlacMetaBlockHeader::kFlacPicture,
                            .metadata_data_block_length = 3},
                 .payload = std::vector<uint8_t>({'a', 'b', 'c'})}}}}));
  InitAndTestGenerate();
}

}  // namespace
}  // namespace iamf_tools
