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
#include "iamf/cli/proto_to_obu/audio_frame_generator.h"

#include <cstdint>
#include <list>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_to_obu/audio_element_generator.h"
#include "iamf/cli/proto_to_obu/codec_config_generator.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

constexpr DecodedUleb128 kCodecConfigId = 99;
constexpr uint32_t kSampleRate = 48000;
// TODO(b/301490667): Add more tests. Include tests with samples trimmed at
//                    the start and tests with multiple substreams. Include
//                    tests to ensure the `*EncoderMetadata` are configured in
//                    the encoder. Test encoders work as expected with multiple
//                    Codec Config OBUs.

TEST(GetNumberOfSamplesToDelayAtStart, ReturnsZeroForLpcm) {
  iamf_tools_cli_proto::CodecConfig kUnusedCodecConfigMetadata = {};
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);

  EXPECT_THAT(
      AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
          kUnusedCodecConfigMetadata, codec_config_obus.at(kCodecConfigId)),
      IsOkAndHolds(0));
}

constexpr uint16_t kApplicationAudioPreSkip = 312;
constexpr uint16_t kLowdelayPreskip = 120;
void AddOpusCodecConfigWithIdAndPreSkip(
    uint32_t codec_config_id, uint16_t pre_skip,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Initialize the Codec Config OBU.
  ASSERT_EQ(codec_config_obus.find(codec_config_id), codec_config_obus.end());

  CodecConfigObu obu(
      ObuHeader(), codec_config_id,
      {.codec_id = CodecConfig::kCodecIdOpus,
       .num_samples_per_frame = 960,
       .audio_roll_distance = -4,
       .decoder_config = OpusDecoderConfig{.version_ = 1,
                                           .pre_skip_ = pre_skip,
                                           .input_sample_rate_ = kSampleRate}});
  ASSERT_THAT(obu.Initialize(), IsOk());
  codec_config_obus.emplace(codec_config_id, std::move(obu));
}

TEST(GetNumberOfSamplesToDelayAtStart,
     ReturnsErrorWhenCodecConfigObuIsConfiguredIncorrectly) {
  const uint16_t kInvalidPreSkip = 1000;
  iamf_tools_cli_proto::CodecConfig codec_config_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        decoder_config_opus {
          opus_encoder_metadata {
            target_bitrate_per_channel: 48000
            application: APPLICATION_AUDIO
          }
        }
      )pb",
      &codec_config_metadata));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithIdAndPreSkip(kCodecConfigId, kInvalidPreSkip,
                                     codec_config_obus);

  EXPECT_FALSE(AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
                   codec_config_metadata, codec_config_obus.at(kCodecConfigId))
                   .ok());
}

TEST(GetNumberOfSamplesToDelayAtStart, ReturnsNonZeroForOpus) {
  iamf_tools_cli_proto::CodecConfig codec_config_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        decoder_config_opus {
          opus_encoder_metadata {
            target_bitrate_per_channel: 48000
            application: APPLICATION_AUDIO
          }
        }
      )pb",
      &codec_config_metadata));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithIdAndPreSkip(kCodecConfigId, kApplicationAudioPreSkip,
                                     codec_config_obus);

  const auto result = AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
      codec_config_metadata, codec_config_obus.at(kCodecConfigId));

  ASSERT_THAT(result, IsOk());
  EXPECT_NE(*result, 0);
}

TEST(GetNumberOfSamplesToDelayAtStart, ResultMayVaryWithEncoderMetadata) {
  const DecodedUleb128 kApplicationAudioCodecConfigId = 1;
  const DecodedUleb128 kApplicationRestrictedLowdelayCodecConfigId = 2;
  iamf_tools_cli_proto::CodecConfig application_audio_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        decoder_config_opus {
          opus_encoder_metadata {
            target_bitrate_per_channel: 48000
            application: APPLICATION_AUDIO
          }
        }
      )pb",
      &application_audio_metadata));
  iamf_tools_cli_proto::CodecConfig application_restricted_lowdelay_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        decoder_config_opus {
          opus_encoder_metadata {
            target_bitrate_per_channel: 48000
            application: APPLICATION_RESTRICTED_LOWDELAY
          }
        }
      )pb",
      &application_restricted_lowdelay_metadata));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithIdAndPreSkip(kApplicationAudioCodecConfigId,
                                     kApplicationAudioPreSkip,
                                     codec_config_obus);
  AddOpusCodecConfigWithIdAndPreSkip(
      kApplicationRestrictedLowdelayCodecConfigId, kLowdelayPreskip,
      codec_config_obus);

  const auto application_audio_result =
      AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
          application_audio_metadata,
          codec_config_obus.at(kApplicationAudioCodecConfigId));
  const auto low_delay_result =
      AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
          application_restricted_lowdelay_metadata,
          codec_config_obus.at(kApplicationRestrictedLowdelayCodecConfigId));

  ASSERT_THAT(application_audio_result, IsOk());
  ASSERT_THAT(low_delay_result, IsOk());
  EXPECT_NE(*application_audio_result, *low_delay_result);
}

TEST(GetNumberOfSamplesToDelayAtStart, ReturnsNonZeroForAac) {
  iamf_tools_cli_proto::CodecConfig codec_config_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        decoder_config_aac: {
          aac_encoder_metadata {
            bitrate_mode: 0  #  Constant bit rate mode.
            enable_afterburner: true
            signaling_mode: 2  # Explicit hierarchical signaling.
          }
        }
      )pb",
      &codec_config_metadata));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddAacCodecConfigWithId(kCodecConfigId, codec_config_obus);

  const auto result = AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
      codec_config_metadata, codec_config_obus.at(kCodecConfigId));

  ASSERT_THAT(result, IsOk());
  EXPECT_NE(*result, 0);
}

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

void GenerateAudioFrameWithEightSamples(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    std::list<AudioFrameWithData>& output_audio_frames,
    bool expected_initialize_is_ok = true,
    bool expected_add_samples_is_ok = true,
    bool expected_output_frames_all_ok = true) {
  // Initialize pre-requisite OBUs and the global timing module. This is all
  // derived from the `user_metadata`.
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  ASSERT_THAT(codec_config_generator.Generate(codec_config_obus), IsOk());

  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  ASSERT_THAT(
      audio_element_generator.Generate(codec_config_obus, audio_elements),
      IsOk());

  // For simplicity this function does not use parameters. Pass in empty
  // containers.
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  const std::string output_wav_directory = "/dev/null";

  DemixingModule demixing_module;
  ASSERT_THAT(demixing_module.InitializeForDownMixingAndReconstruction(
                  user_metadata, audio_elements),
              IsOk());
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(
      global_timing_module.Initialize(audio_elements, param_definitions),
      IsOk());
  ParametersManager parameters_manager(audio_elements);
  ASSERT_THAT(parameters_manager.Initialize(), IsOk());

  // Generate the audio frames.
  AudioFrameGenerator audio_frame_generator(
      user_metadata.audio_frame_metadata(),
      user_metadata.codec_config_metadata(), audio_elements, demixing_module,
      parameters_manager, global_timing_module);

  // Initialize, iteratively add samples, generate frames, and finalize.
  EXPECT_EQ(expected_initialize_is_ok, audio_frame_generator.Initialize().ok());
  if (!expected_initialize_is_ok) {
    return;
  }

  // Add only one frame.
  int frame_count = 0;
  const std::vector<int32_t> frame_0_l2 = {1 << 16, 2 << 16, 3 << 16, 4 << 16,
                                           5 << 16, 6 << 16, 7 << 16, 8 << 16};
  const std::vector<int32_t> frame_0_r2 = {
      65535 << 16, 65534 << 16, 65533 << 16, 65532 << 16,
      65531 << 16, 65530 << 16, 65529 << 16, 65528 << 16};
  const std::vector<int32_t> empty_frame;

  // TODO(b/329375123): Test adding samples and outputing frames in different
  //                    threads.
  while (audio_frame_generator.TakingSamples()) {
    for (const auto& audio_frame_metadata :
         user_metadata.audio_frame_metadata()) {
      EXPECT_THAT(audio_frame_generator.AddSamples(
                      audio_frame_metadata.audio_element_id(), "L2",
                      frame_count == 0 ? frame_0_l2 : empty_frame),
                  IsOk());

      // `AddSamples()` will trigger encoding once all samples for an
      // audio element have been added and thus may return a non-OK status.
      const auto add_samples_status = audio_frame_generator.AddSamples(
          audio_frame_metadata.audio_element_id(), "R2",
          frame_count == 0 ? frame_0_r2 : empty_frame);
      EXPECT_EQ(expected_add_samples_is_ok, add_samples_status.ok());
      if (!expected_add_samples_is_ok) {
        return;
      }
    }
    frame_count++;
  }
  EXPECT_THAT(audio_frame_generator.Finalize(), IsOk());

  bool output_frames_all_ok = true;
  while (audio_frame_generator.GeneratingFrames()) {
    std::list<AudioFrameWithData> temp_audio_frames;
    const auto output_frames_status =
        audio_frame_generator.OutputFrames(temp_audio_frames);
    if (!output_frames_status.ok()) {
      output_frames_all_ok = false;
      break;
    }
    output_audio_frames.splice(output_audio_frames.end(), temp_audio_frames);
  }
  EXPECT_EQ(expected_output_frames_all_ok, output_frames_all_ok);
}

void AddStereoAudioElementAndAudioFrameMetadata(
    iamf_tools_cli_proto::UserMetadata& user_metadata,
    uint32_t audio_element_id, uint32_t audio_substream_id) {
  auto* audio_frame_metadata = user_metadata.add_audio_frame_metadata();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        wav_filename: ""
        samples_to_trim_at_end: 0
        samples_to_trim_at_start: 0
        channel_ids: [ 0, 1 ]
        channel_labels: [ "L2", "R2" ]
      )pb",
      audio_frame_metadata));
  audio_frame_metadata->set_audio_element_id(audio_element_id);

  auto* audio_element_metadata = user_metadata.add_audio_element_metadata();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 1
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
      audio_element_metadata));
  audio_element_metadata->set_audio_element_id(audio_element_id);
  audio_element_metadata->mutable_audio_substream_ids()->Add(
      audio_substream_id);
}

const uint32_t kFirstAudioElementId = 300;
const uint32_t kSecondAudioElementId = 301;
const uint32_t kFirstSubstreamId = 0;
const uint32_t kSecondSubstreamId = 1;
void ConfigureOneStereoSubstreamLittleEndian(
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
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

  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kFirstAudioElementId, kFirstSubstreamId);
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

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  ValidateAudioFrames(audio_frames, expected_audio_frames);
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

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  ValidateAudioFrames(audio_frames, expected_audio_frames);
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

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  ValidateAudioFrames(audio_frames, expected_audio_frames);
}

TEST(AudioFrameGenerator, AllAudioElementsHaveMatchingTrimmingInformation) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kSecondAudioElementId, kSecondSubstreamId);
  // Configure them with the same trimming information.
  const uint32_t kCommonNumSamplesToTrimAtStart = 2;
  const uint32_t kCommonNumSamplesToTrimAtEnd = 1;
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      kCommonNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(1)->set_samples_to_trim_at_start(
      kCommonNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(
      kCommonNumSamplesToTrimAtEnd);
  user_metadata.mutable_audio_frame_metadata(1)->set_samples_to_trim_at_end(
      kCommonNumSamplesToTrimAtEnd);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  EXPECT_FALSE(audio_frames.empty());
  for (const auto& audio_frame : audio_frames) {
    EXPECT_EQ(audio_frame.obu.header_.num_samples_to_trim_at_start,
              kCommonNumSamplesToTrimAtStart);
    EXPECT_EQ(audio_frame.obu.header_.num_samples_to_trim_at_end,
              kCommonNumSamplesToTrimAtEnd);
  }
}

TEST(AudioFrameGenerator,
     ErrorAudioElementsMustHaveSameTrimmingInformationAtEnd) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kSecondAudioElementId, kSecondSubstreamId);
  // IAMF requires that all audio elements have the same number of samples
  // trimmed at the end.
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(1);
  user_metadata.mutable_audio_frame_metadata(1)->set_samples_to_trim_at_end(2);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames,
                                     /*expected_initialize_is_ok=*/false);
}

TEST(AudioFrameGenerator,
     ErrorAudioElementsMustHaveSameTrimmingInformationAtStart) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kSecondAudioElementId, kSecondSubstreamId);
  // IAMF requires that all audio elements have the same number of samples
  // trimmed at the start.
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      1);
  user_metadata.mutable_audio_frame_metadata(1)->set_samples_to_trim_at_start(
      2);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames,
                                     /*expected_initialize_is_ok=*/false);
}

TEST(AudioFrameGenerator, NumSamplesToTrimAtEndWithPaddedFrames) {
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

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  // Validate the generated audio frames.
  ValidateAudioFrames(audio_frames, expected_audio_frames);
}

TEST(AudioFrameGenerator, InvalidIfTooFewSamplesToTrimAtEnd) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(10);
  // Normally two samples would be required.
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(1);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames,
                                     /*expected_initialize_is_ok=*/true,
                                     /*expected_add_samples_is_ok=*/false);
}

TEST(AudioFrameGenerator, UserMayRequestAdditionalSamplesToTrimAtEnd) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  const uint32_t kRequestedNumSamplesToTrimAtEnd = 1;
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(
      kRequestedNumSamplesToTrimAtEnd);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  ASSERT_FALSE(audio_frames.empty());

  EXPECT_EQ(audio_frames.front().obu.header_.num_samples_to_trim_at_end,
            kRequestedNumSamplesToTrimAtEnd);
}

TEST(AudioFrameGenerator, InvalidWhenAFullFrameAtEndIsRequestedToBeTrimmed) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);

  // Reconfigure `num_samples_per_frame` to result in two frames.
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(4);

  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(4);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames,
                                     /*expected_initialize_is_ok=*/true,
                                     /*expected_add_samples_is_ok=*/true,
                                     /*expected_output_frames_all_ok=*/false);
}

TEST(AudioFrameGenerator, ValidWhenAFullFrameAtStartIsRequestedToBeTrimmed) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);

  // Reconfigure `num_samples_per_frame` to result in two frames.
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(4);

  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      4);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  ASSERT_FALSE(audio_frames.empty());

  EXPECT_EQ(audio_frames.front().obu.header_.num_samples_to_trim_at_start, 4);
  EXPECT_TRUE(audio_frames.front().obu.header_.obu_trimming_status_flag);
}

TEST(AudioFrameGenerator, NoAudioFrames) {
  const iamf_tools_cli_proto::UserMetadata& user_metadata = {};
  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamples(user_metadata, audio_frames);
  EXPECT_TRUE(audio_frames.empty());
}

}  // namespace
}  // namespace iamf_tools
