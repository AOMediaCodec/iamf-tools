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
#include "iamf/cli/audio_frame_generator.h"

#include <cstdint>
#include <list>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "iamf/audio_frame.h"
#include "iamf/cli/audio_element_generator.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec_config_generator.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/codec_config.h"
#include "iamf/obu_header.h"
#include "iamf/param_definitions.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

// TODO(b/301490667): Add more tests. Include tests with samples trimmed at
//                    the start and tests with multiple substreams. Include
//                    tests to ensure the `*EncoderMetadata` are configured in
//                    the encoder. Test encoders work as expected with multiple
//                    Codec Config OBUs.

void ValidateAudioFrames(
    const std::list<AudioFrameWithData>& output_audio_frames,
    const std::list<AudioFrameWithData>& expected_audio_frames) {
  // Validate several fields in the audio frames match the expected results.
  auto output_iter = output_audio_frames.begin();
  auto expected_iter = expected_audio_frames.begin();
  EXPECT_EQ(output_audio_frames.size(), expected_audio_frames.size());
  while (expected_iter != expected_audio_frames.end()) {
    // Validate the OBU.
    EXPECT_EQ(output_iter->obu, expected_iter->obu);

    // Validate some fields directly in `AudioFrameWithData`.
    EXPECT_EQ(output_iter->start_timestamp, expected_iter->start_timestamp);
    EXPECT_EQ(output_iter->end_timestamp, expected_iter->end_timestamp);
    EXPECT_EQ(output_iter->down_mixing_params.in_bitstream,
              expected_iter->down_mixing_params.in_bitstream);
    if (expected_iter->down_mixing_params.in_bitstream) {
      EXPECT_EQ(output_iter->down_mixing_params.alpha,
                expected_iter->down_mixing_params.alpha);
      EXPECT_EQ(output_iter->down_mixing_params.beta,
                expected_iter->down_mixing_params.beta);
      EXPECT_EQ(output_iter->down_mixing_params.gamma,
                expected_iter->down_mixing_params.gamma);
      EXPECT_EQ(output_iter->down_mixing_params.delta,
                expected_iter->down_mixing_params.delta);
      EXPECT_EQ(output_iter->down_mixing_params.w_idx_offset,
                expected_iter->down_mixing_params.w_idx_offset);
      EXPECT_EQ(output_iter->down_mixing_params.w,
                expected_iter->down_mixing_params.w);
    }

    output_iter++;
    expected_iter++;
  }
}

void TestGenerateAudioFramesWithoutParameters(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const std::list<AudioFrameWithData>& expected_audio_frames) {
  // Initialize pre-requisite OBUs and the global timing module. This is all
  // derived from the `user_metadata`.
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  ASSERT_TRUE(codec_config_generator.Generate(codec_config_obus).ok());

  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  ASSERT_TRUE(
      audio_element_generator.Generate(codec_config_obus, audio_elements).ok());

  // For simplicity this function does not use parameters. Pass in empty
  // containers.
  const std::list<ParameterBlockWithData>& parameter_blocks = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  const std::string output_wav_directory = "/dev/null";

  DemixingModule demixing_module(user_metadata, audio_elements);
  GlobalTimingModule global_timing_module(user_metadata);
  ASSERT_TRUE(
      global_timing_module
          .Initialize(audio_elements, codec_config_obus, param_definitions)
          .ok());
  ParametersManager parameters_manager(audio_elements, parameter_blocks);
  ASSERT_TRUE(parameters_manager.Initialize().ok());

  // Generate the audio frames.
  AudioFrameGenerator audio_frame_generator(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), audio_elements,
      output_wav_directory,
      /*file_name_prefix=*/"test", demixing_module, parameters_manager,
      global_timing_module);
  std::list<AudioFrameWithData> audio_frames = {};

  // Initialize, iteratively add samples, generate frames, and finalize.
  EXPECT_TRUE(audio_frame_generator.Initialize().ok());

  // Add only one frame.
  int frame_count = 0;
  const std::vector<int32_t> frame_0_l2 = {1 << 16, 2 << 16, 3 << 16, 4 << 16,
                                           5 << 16, 6 << 16, 7 << 16, 8 << 16};
  const std::vector<int32_t> frame_0_r2 = {
      65535 << 16, 65534 << 16, 65533 << 16, 65532 << 16,
      65531 << 16, 65530 << 16, 65529 << 16, 65528 << 16};
  while (!audio_frame_generator.Finished()) {
    for (const auto& audio_frame_metadata :
         user_metadata.audio_frame_metadata()) {
      EXPECT_TRUE(audio_frame_generator
                      .AddSamples(audio_frame_metadata.audio_element_id(), "L2",
                                  frame_count == 0 ? frame_0_l2
                                                   : std::vector<int32_t>())
                      .ok());
      EXPECT_TRUE(audio_frame_generator
                      .AddSamples(audio_frame_metadata.audio_element_id(), "R2",
                                  frame_count == 0 ? frame_0_r2
                                                   : std::vector<int32_t>())
                      .ok());
    }
    EXPECT_TRUE(audio_frame_generator.GenerateFrames().ok());
    frame_count++;
  }
  EXPECT_TRUE(audio_frame_generator.Finalize(audio_frames).ok());

  // Validate the generated audio frames.
  ValidateAudioFrames(audio_frames, expected_audio_frames);
}

void ConfigureOneStereoSubstreamLittleEndian(
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        wav_filename: ""
        samples_to_trim_at_end: 0
        samples_to_trim_at_start: 0
        audio_element_id: 300
        channel_ids: [ 0, 1 ]
        channel_labels: [ "L2", "R2" ]
      )pb",
      user_metadata.add_audio_frame_metadata()));

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_LPCM
          num_samples_per_frame: 8
          audio_roll_distance: 0
          decoder_config_lpcm {
            sample_format_flags: LPCM_LITTLE_ENDIAN
            sample_size: 16
            sample_rate: 48000
          }
        }
      )pb",
      user_metadata.add_codec_config_metadata()));

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 1
        audio_substream_ids: [ 0 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs:
          [ {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 1
          }]
        }
      )pb",
      user_metadata.add_audio_element_metadata()));
}

TEST(AudioFrameGenerator, OneStereoSubstreamOneFrame) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);

  // Test with a single frame.
  std::list<AudioFrameWithData> expected_audio_frames = {};
  expected_audio_frames.push_back(
      {.obu = AudioFrameObu(
           ObuHeader(), 0,
           {1, 0, 255, 255, 2, 0, 254, 255, 3, 0, 253, 255, 4, 0, 252, 255,
            5, 0, 251, 255, 6, 0, 250, 255, 7, 0, 249, 255, 8, 0, 248, 255}),
       .start_timestamp = 0,
       .end_timestamp = 8,
       .down_mixing_params = {.in_bitstream = false}});

  TestGenerateAudioFramesWithoutParameters(user_metadata,
                                           expected_audio_frames);
}

TEST(AudioFrameGenerator, AllowsOutputToHaveHigherBitDepthThanInput) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->mutable_decoder_config_lpcm()
      ->set_sample_size(32);

  // It is OK to encode to a higher-bit depth than the input wav file. The extra
  // bits of precision are set to '0's.
  std::list<AudioFrameWithData> expected_audio_frames = {};
  expected_audio_frames.push_back(
      {.obu = AudioFrameObu(
           ObuHeader(), 0,
           {0, 0, 1, 0, 0, 0, 255, 255, 0, 0, 2, 0, 0, 0, 254, 255,
            0, 0, 3, 0, 0, 0, 253, 255, 0, 0, 4, 0, 0, 0, 252, 255,
            0, 0, 5, 0, 0, 0, 251, 255, 0, 0, 6, 0, 0, 0, 250, 255,
            0, 0, 7, 0, 0, 0, 249, 255, 0, 0, 8, 0, 0, 0, 248, 255}),
       .start_timestamp = 0,
       .end_timestamp = 8,
       .down_mixing_params = {.in_bitstream = false}});

  TestGenerateAudioFramesWithoutParameters(user_metadata,
                                           expected_audio_frames);
}

TEST(AudioFrameGenerator, OneStereoSubstreamTwoFrames) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);

  // Reconfigure `num_samples_per_frame` to result in two frames.
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(4);

  std::list<AudioFrameWithData> expected_audio_frames = {};
  expected_audio_frames.push_back(
      {.obu = AudioFrameObu(
           ObuHeader(), 0,
           {1, 0, 255, 255, 2, 0, 254, 255, 3, 0, 253, 255, 4, 0, 252, 255}),
       .start_timestamp = 0,
       .end_timestamp = 4,
       .down_mixing_params = {.in_bitstream = false}});
  expected_audio_frames.push_back(
      {.obu = AudioFrameObu(
           ObuHeader(), 0,
           {5, 0, 251, 255, 6, 0, 250, 255, 7, 0, 249, 255, 8, 0, 248, 255}),
       .start_timestamp = 4,
       .end_timestamp = 8,
       .down_mixing_params = {.in_bitstream = false}});

  TestGenerateAudioFramesWithoutParameters(user_metadata,
                                           expected_audio_frames);
}

TEST(AudioFrameGenerator, OneStereoSubstreamOnePaddedFrame) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);

  // Reconfigure `user_metadata` to result in two padded samples.
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(10);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(2);

  std::list<AudioFrameWithData> expected_audio_frames = {};
  expected_audio_frames.push_back(
      {.obu = AudioFrameObu(
           {.obu_trimming_status_flag = true, .num_samples_to_trim_at_end = 2},
           0,
           {1, 0, 255, 255, 2, 0, 254, 255, 3, 0, 253, 255, 4, 0, 252, 255, 5,
            0, 251, 255, 6, 0, 250, 255, 7, 0, 249, 255, 8, 0, 248, 255,
            // First tick (per channel) of padded samples.
            0, 0, 0, 0,
            // Second tick (per channel) of padded samples.
            0, 0, 0, 0}),
       .start_timestamp = 0,
       .end_timestamp = 10,
       .down_mixing_params = {.in_bitstream = false}});
  TestGenerateAudioFramesWithoutParameters(user_metadata,
                                           expected_audio_frames);
}

TEST(AudioFrameGenerator, NoAudioFrames) {
  const iamf_tools_cli_proto::UserMetadata& user_metadata = {};
  TestGenerateAudioFramesWithoutParameters(user_metadata, {});
}

}  // namespace
}  // namespace iamf_tools
