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

#include <array>
#include <cstdint>
#include <list>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
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
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

constexpr DecodedUleb128 kCodecConfigId = 99;
constexpr uint32_t kSampleRate = 48000;

constexpr uint32_t kAacNumSamplesPerFrame = 1024;
constexpr uint32_t kAacNumSamplesToTrimAtStart = 2048;

constexpr bool kSamplesToTrimAtStartIncludesCodecDelay = true;
constexpr bool kSamplesToTrimAtStartExcludesCodecDelay = false;

constexpr auto kFrame0L2EightSamples = std::to_array<InternalSampleType>(
    {1 << 16, 2 << 16, 3 << 16, 4 << 16, 5 << 16, 6 << 16, 7 << 16, 8 << 16});
constexpr auto kFrame0R2EightSamples = std::to_array<InternalSampleType>(
    {65535 << 16, 65534 << 16, 65533 << 16, 65532 << 16, 65531 << 16,
     65530 << 16, 65529 << 16, 65528 << 16});
constexpr std::array<InternalSampleType, 0> kEmptyFrame = {};

// TODO(b/301490667): Add more tests. Include tests with multiple substreams.
//                    Include tests to ensure the `*EncoderMetadata` are
//                    configured in the encoder. Test encoders work as expected
//                    with multiple Codec Config OBUs.

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
     SucceedsWhenInputPreSkipIsIsConfiguredIncorrectly) {
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

  const auto result = AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
      codec_config_metadata, codec_config_obus.at(kCodecConfigId));
  EXPECT_THAT(result, IsOk());
  EXPECT_NE(*result, kInvalidPreSkip);
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

void InitializeAudioFrameGenerator(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const absl::flat_hash_map<uint32_t, const ParamDefinition*>&
        param_definitions,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    DemixingModule& demixing_module, GlobalTimingModule& global_timing_module,
    std::optional<ParametersManager>& parameters_manager,
    std::optional<AudioFrameGenerator>& audio_frame_generator,
    bool expected_initialize_is_ok = true) {
  // Initialize pre-requisite OBUs and the global timing module. This is all
  // derived from the `user_metadata`.
  CodecConfigGenerator codec_config_generator(
      user_metadata.codec_config_metadata());
  ASSERT_THAT(codec_config_generator.Generate(codec_config_obus), IsOk());

  AudioElementGenerator audio_element_generator(
      user_metadata.audio_element_metadata());
  ASSERT_THAT(
      audio_element_generator.Generate(codec_config_obus, audio_elements),
      IsOk());

  ASSERT_THAT(demixing_module.InitializeForDownMixingAndReconstruction(
                  user_metadata, audio_elements),
              IsOk());
  ASSERT_THAT(
      global_timing_module.Initialize(audio_elements, param_definitions),
      IsOk());
  parameters_manager.emplace(audio_elements);
  ASSERT_TRUE(parameters_manager.has_value());
  ASSERT_THAT(parameters_manager->Initialize(), IsOk());

  // Generate the audio frames.
  audio_frame_generator.emplace(user_metadata.audio_frame_metadata(),
                                user_metadata.codec_config_metadata(),
                                audio_elements, demixing_module,
                                *parameters_manager, global_timing_module);
  ASSERT_TRUE(audio_frame_generator.has_value());

  // Initialize.
  if (expected_initialize_is_ok) {
    EXPECT_THAT(audio_frame_generator->Initialize(), IsOk());
  } else {
    EXPECT_FALSE(audio_frame_generator->Initialize().ok());
  }
}

void ExpectAudioFrameGeneratorInitializeIsNotOk(
    const iamf_tools_cli_proto::UserMetadata& user_metadata) {
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;

  InitializeAudioFrameGenerator(
      user_metadata, param_definitions, codec_config_obus, audio_elements,
      demixing_module, global_timing_module, parameters_manager,
      audio_frame_generator, /*expected_initialize_is_ok=*/false);
}

// Safe to run simultaneously with `FlushAudioFrameGenerator`.
void AddAllSamplesAndFinalizesExpectOk(
    DecodedUleb128 audio_element_id,
    const absl::flat_hash_map<
        ChannelLabel::Label, std::vector<absl::Span<const InternalSampleType>>>&
        label_to_frames,
    AudioFrameGenerator& audio_frame_generator) {
  // Avoid overflow below.
  const int common_num_frames = label_to_frames.begin()->second.size();
  for (const auto& [label, frames] : label_to_frames) {
    ASSERT_EQ(common_num_frames, frames.size());
  }

  // Push in the user data.
  for (int frame_count = 0; frame_count < common_num_frames; ++frame_count) {
    EXPECT_TRUE((audio_frame_generator.TakingSamples()));
    for (const auto& [label, frames] : label_to_frames) {
      EXPECT_THAT(audio_frame_generator.AddSamples(audio_element_id, label,
                                                   frames[frame_count]),
                  IsOk());
    }
  }

  // Flush out the remaining frames. Several flushes could be required if the
  // codec delay is longer than a frame duration.
  while (audio_frame_generator.TakingSamples()) {
    for (const auto& [label, frames] : label_to_frames) {
      EXPECT_THAT(audio_frame_generator.AddSamples(audio_element_id, label,
                                                   kEmptyFrame),
                  IsOk());
    }

    EXPECT_THAT(audio_frame_generator.Finalize(), IsOk());
  }
}

// Safe to run simultaneously with `AddAllSamplesAndFinalizesExpectOk`.
void FlushAudioFrameGeneratorExpectOk(
    AudioFrameGenerator& audio_frame_generator,
    std::list<AudioFrameWithData>& output_audio_frames) {
  while (audio_frame_generator.GeneratingFrames()) {
    std::list<AudioFrameWithData> temp_audio_frames;
    EXPECT_THAT(audio_frame_generator.OutputFrames(temp_audio_frames), IsOk());
    output_audio_frames.splice(output_audio_frames.end(), temp_audio_frames);
  }
}

void GenerateAudioFrameWithEightSamplesExpectOk(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    std::list<AudioFrameWithData>& output_audio_frames) {
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  // For simplicity this function does not use parameters. Pass in empty
  // containers.
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  // For delayed initialization.
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  // Initialize, add samples, generate frames, and finalize.
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  // Add only one "real" frame and an empty frame to signal the end of the
  // stream.
  const absl::flat_hash_map<ChannelLabel::Label,
                            std::vector<absl::Span<const InternalSampleType>>>
      label_to_frames = {{ChannelLabel::kL2, {kFrame0L2EightSamples}},
                         {ChannelLabel::kR2, {kFrame0R2EightSamples}}};
  ASSERT_FALSE(audio_elements.empty());
  const DecodedUleb128 audio_element_id = audio_elements.begin()->first;

  AddAllSamplesAndFinalizesExpectOk(audio_element_id, label_to_frames,
                                    *audio_frame_generator);
  FlushAudioFrameGeneratorExpectOk(*audio_frame_generator, output_audio_frames);
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

void ConfigureAacCodecConfigMetadata(
    iamf_tools_cli_proto::CodecConfigObuMetadata& codec_config_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_AAC_LC
          automatically_override_audio_roll_distance: true
          decoder_config_aac: {
            decoder_specific_info {
              sample_frequency_index: AAC_SAMPLE_FREQUENCY_INDEX_48000
            }
            aac_encoder_metadata {
              bitrate_mode: 0  #  Constant bit rate mode.
              enable_afterburner: true
              signaling_mode: 2  # Explicit hierarchical signaling.
            }
          }
        }
      )pb",
      &codec_config_metadata));
  codec_config_metadata.mutable_codec_config()->set_num_samples_per_frame(
      kAacNumSamplesPerFrame);
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
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
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
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
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
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
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
  const bool kCommonSamplesToTrimAtEndIncludesPadding = true;
  const bool kCommonSamplesToTrimAtStartIncludesCodecDelay = true;
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      kCommonNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(1)->set_samples_to_trim_at_start(
      kCommonNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(
      kCommonNumSamplesToTrimAtEnd);
  user_metadata.mutable_audio_frame_metadata(1)->set_samples_to_trim_at_end(
      kCommonNumSamplesToTrimAtEnd);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(
          kCommonSamplesToTrimAtEndIncludesPadding);
  user_metadata.mutable_audio_frame_metadata(1)
      ->set_samples_to_trim_at_end_includes_padding(
          kCommonSamplesToTrimAtEndIncludesPadding);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_start_includes_codec_delay(
          kCommonSamplesToTrimAtStartIncludesCodecDelay);
  user_metadata.mutable_audio_frame_metadata(1)
      ->set_samples_to_trim_at_end_includes_padding(
          kCommonSamplesToTrimAtStartIncludesCodecDelay);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
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

  ExpectAudioFrameGeneratorInitializeIsNotOk(user_metadata);
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

  ExpectAudioFrameGeneratorInitializeIsNotOk(user_metadata);
}

TEST(AudioFrameGenerator,
     ErrorAudioElementsMustHaveSameSamplesToTrimAtEndIncludesPadding) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kSecondAudioElementId, kSecondSubstreamId);
  // IAMF requires that all audio elements have the same number of samples
  // trimmed at the start.
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(false);
  user_metadata.mutable_audio_frame_metadata(1)
      ->set_samples_to_trim_at_end_includes_padding(true);

  ExpectAudioFrameGeneratorInitializeIsNotOk(user_metadata);
}

TEST(AudioFrameGenerator,
     ErrorAudioElementsMustHaveSameSamplesToTrimAtStartIncludesCodecDelay) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kSecondAudioElementId, kSecondSubstreamId);
  // IAMF requires that all audio elements have the same number of samples
  // trimmed at the start.
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_start_includes_codec_delay(false);
  user_metadata.mutable_audio_frame_metadata(1)
      ->set_samples_to_trim_at_start_includes_codec_delay(true);

  ExpectAudioFrameGeneratorInitializeIsNotOk(user_metadata);
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
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
  // Validate the generated audio frames.
  ValidateAudioFrames(audio_frames, expected_audio_frames);
}

TEST(AudioFrameGenerator,
     CopiesNumSamplesPerFrameWhenSamplesToTrimAtEndIncludesPaddingIsTrue) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  // Reconfigure `user_metadata` to result in two padded samples.
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(10);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(3);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(true);
  // Oney the user's request for three samples trimmed from the input data. Two
  // of these samples represent padding.
  constexpr uint32_t kExpectedNumSamplesToTrimAtEnd = 3;

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);

  ASSERT_FALSE(audio_frames.empty());
  const auto& audio_frame = audio_frames.front();
  EXPECT_EQ(audio_frame.obu.header_.num_samples_to_trim_at_end,
            kExpectedNumSamplesToTrimAtEnd);
}

TEST(AudioFrameGenerator,
     IncrementsNumSamplesPerFrameWhenSamplesToTrimAtEndIncludesPaddingIsFalse) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  // Reconfigure `user_metadata` to result in two padded samples.
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(10);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(3);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(false);
  // The user requested three samples trimmed from the input data. Plus an
  // additional two samples are required to ensure the frame has ten samples.
  constexpr uint32_t kExpectedNumSamplesToTrimAtEnd = 5;

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);

  ASSERT_FALSE(audio_frames.empty());
  const auto& audio_frame = audio_frames.front();
  EXPECT_EQ(audio_frame.obu.header_.num_samples_to_trim_at_end,
            kExpectedNumSamplesToTrimAtEnd);
}

TEST(AudioFrameGenerator, InvalidIfTooFewSamplesToTrimAtEnd) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  user_metadata.mutable_codec_config_metadata(0)
      ->mutable_codec_config()
      ->set_num_samples_per_frame(10);
  // Normally two samples would be required.
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(1);
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  EXPECT_THAT(
      audio_frame_generator->AddSamples(kFirstAudioElementId, ChannelLabel::kL2,
                                        kFrame0L2EightSamples),
      IsOk());

  // Once all channels are added, frame creation will trigger. The user's
  // request for one sample trimmed at the end will be rejected because two
  // samples were required.
  EXPECT_FALSE(audio_frame_generator
                   ->AddSamples(kFirstAudioElementId, ChannelLabel::kR2,
                                kFrame0L2EightSamples)
                   .ok());
}

TEST(AudioFrameGenerator, UserMayRequestAdditionalSamplesToTrimAtEnd) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  const uint32_t kRequestedNumSamplesToTrimAtEnd = 1;
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_end(
      kRequestedNumSamplesToTrimAtEnd);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
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
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  const absl::flat_hash_map<ChannelLabel::Label,
                            std::vector<absl::Span<const InternalSampleType>>>
      label_to_frames = {{ChannelLabel::kL2, {kFrame0L2EightSamples}},
                         {ChannelLabel::kR2, {kFrame0R2EightSamples}}};
  AddAllSamplesAndFinalizesExpectOk(kFirstAudioElementId, label_to_frames,
                                    *audio_frame_generator);
  std::list<AudioFrameWithData> unused_audio_frames;
  EXPECT_THAT(audio_frame_generator->OutputFrames(unused_audio_frames), IsOk());

  // Preparing the final frame reveals the user requested a fully trimmed frame.
  EXPECT_FALSE(audio_frame_generator->OutputFrames(unused_audio_frames).ok());
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
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
  ASSERT_FALSE(audio_frames.empty());

  EXPECT_EQ(audio_frames.front().obu.header_.num_samples_to_trim_at_start, 4);
  EXPECT_TRUE(audio_frames.front().obu.header_.obu_trimming_status_flag);
}

TEST(AudioFrameGenerator, EncodingSucceedsWithFullFramesTrimmedAtStart) {
  // The test is written under the assumption AAC has at least one full frame
  // trimmed from the start.
  ASSERT_GE(kAacNumSamplesToTrimAtStart, kAacNumSamplesPerFrame);
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureAacCodecConfigMetadata(
      *user_metadata.mutable_codec_config_metadata()->Add());
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kFirstAudioElementId, kFirstSubstreamId);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_start_includes_codec_delay(
          kSamplesToTrimAtStartIncludesCodecDelay);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      kAacNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(false);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);
  ASSERT_FALSE(audio_frames.empty());

  // Check the "cumulative" samples to trim from the start matches the requested
  // value.
  uint32_t observed_cumulative_samples_to_trim_at_start = 0;
  uint32_t unused_common_samples_to_trim_at_end = 0;
  ASSERT_THAT(
      ValidateAndGetCommonTrim(kAacNumSamplesPerFrame, audio_frames,
                               unused_common_samples_to_trim_at_end,
                               observed_cumulative_samples_to_trim_at_start),
      IsOk());
  EXPECT_EQ(observed_cumulative_samples_to_trim_at_start,
            kAacNumSamplesToTrimAtStart);
}

TEST(AudioFrameGenerator, TrimsAdditionalSamplesAtStart) {
  // Request more samples to be trimmed from the start than required by the
  // codec delay. The output audio will have one fewer sample than the input
  // audio.
  constexpr uint32_t kNumSamplesToTrimAtStart = kAacNumSamplesToTrimAtStart + 1;
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureAacCodecConfigMetadata(
      *user_metadata.mutable_codec_config_metadata()->Add());
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kFirstAudioElementId, kFirstSubstreamId);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_start_includes_codec_delay(
          kSamplesToTrimAtStartIncludesCodecDelay);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      kNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(false);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);

  // Check the "cumulative" samples to trim from the start matches the requested
  // value.
  uint32_t observed_cumulative_samples_to_trim_at_start = 0;
  uint32_t unused_common_samples_to_trim_at_end = 0;
  ASSERT_THAT(
      ValidateAndGetCommonTrim(kAacNumSamplesPerFrame, audio_frames,
                               unused_common_samples_to_trim_at_end,
                               observed_cumulative_samples_to_trim_at_start),
      IsOk());
  EXPECT_EQ(observed_cumulative_samples_to_trim_at_start,
            kNumSamplesToTrimAtStart);
}

TEST(AudioFrameGenerator, AddsCodecDelayToSamplesToTrimAtStartWhenRequested) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureAacCodecConfigMetadata(
      *user_metadata.mutable_codec_config_metadata()->Add());
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kFirstAudioElementId, kFirstSubstreamId);
  // Request one sample to be trimmed. In addition to the codec delay.
  constexpr uint32_t kNumSamplesToTrimAtStart = 1;
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_start_includes_codec_delay(
          kSamplesToTrimAtStartExcludesCodecDelay);
  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      kNumSamplesToTrimAtStart);
  user_metadata.mutable_audio_frame_metadata(0)
      ->set_samples_to_trim_at_end_includes_padding(false);

  std::list<AudioFrameWithData> audio_frames;
  GenerateAudioFrameWithEightSamplesExpectOk(user_metadata, audio_frames);

  uint32_t observed_cumulative_samples_to_trim_at_start = 0;
  uint32_t unused_common_samples_to_trim_at_end = 0;
  ASSERT_THAT(
      ValidateAndGetCommonTrim(kAacNumSamplesPerFrame, audio_frames,
                               unused_common_samples_to_trim_at_end,
                               observed_cumulative_samples_to_trim_at_start),
      IsOk());
  // The actual cumulative trim values in the OBU include both the codec delay
  // and the user requested trim.
  constexpr uint32_t kExpectedNumSamplesToTrimAtStart =
      kAacNumSamplesToTrimAtStart + kNumSamplesToTrimAtStart;
  EXPECT_EQ(observed_cumulative_samples_to_trim_at_start,
            kExpectedNumSamplesToTrimAtStart);
}

TEST(AudioFrameGenerator, InitFailsWithTooFewSamplesToTrimAtStart) {
  const uint32_t kInvalidNumSamplesToTrimAtStart =
      kAacNumSamplesToTrimAtStart - 1;
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureAacCodecConfigMetadata(
      *user_metadata.mutable_codec_config_metadata()->Add());
  AddStereoAudioElementAndAudioFrameMetadata(
      user_metadata, kFirstAudioElementId, kFirstSubstreamId);

  user_metadata.mutable_audio_frame_metadata(0)->set_samples_to_trim_at_start(
      kInvalidNumSamplesToTrimAtStart);

  ExpectAudioFrameGeneratorInitializeIsNotOk(user_metadata);
}

TEST(AudioFrameGenerator, NoAudioFrames) {
  const iamf_tools_cli_proto::UserMetadata& user_metadata = {};
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  EXPECT_THAT(audio_frame_generator->Finalize(), IsOk());
  // Omit adding any samples to the generator.
  //  AddSamplesToAudioFrameGeneratorExpectOk(kFirstAudioElementId,
  //  label_to_frames, *audio_frame_generator);

  std::list<AudioFrameWithData> audio_frames;
  FlushAudioFrameGeneratorExpectOk(*audio_frame_generator, audio_frames);
  EXPECT_TRUE(audio_frames.empty());
}

TEST(AudioFrameGenerator, FirstCallToAddSamplesMayBeEmpty) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  EXPECT_THAT(audio_frame_generator->AddSamples(kFirstAudioElementId,
                                                ChannelLabel::kL2, kEmptyFrame),
              IsOk());
  EXPECT_THAT(audio_frame_generator->AddSamples(kFirstAudioElementId,
                                                ChannelLabel::kR2, kEmptyFrame),
              IsOk());
  EXPECT_THAT(audio_frame_generator->Finalize(), IsOk());

  std::list<AudioFrameWithData> audio_frames;
  FlushAudioFrameGeneratorExpectOk(*audio_frame_generator, audio_frames);
  EXPECT_TRUE(audio_frames.empty());
}

TEST(AudioFrameGenerator, MultipleCallsToAddSamplesSucceed) {
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  constexpr int kNumFrames = 3;
  const std::vector<absl::Span<const InternalSampleType>> kThreeFrames(
      kNumFrames, kFrame0L2EightSamples);
  const absl::flat_hash_map<ChannelLabel::Label,
                            std::vector<absl::Span<const InternalSampleType>>>
      label_to_frames = {{ChannelLabel::kL2, kThreeFrames},
                         {ChannelLabel::kR2, kThreeFrames}};
  AddAllSamplesAndFinalizesExpectOk(kFirstAudioElementId, label_to_frames,
                                    *audio_frame_generator);

  std::list<AudioFrameWithData> audio_frames;
  FlushAudioFrameGeneratorExpectOk(*audio_frame_generator, audio_frames);
  EXPECT_EQ(audio_frames.size(), kNumFrames);
}

TEST(AudioFrameGenerator, ManyFramesThreaded) {
  // Create a large number of frames, to increase the likelihood of exposing
  // possible concurrency issues.
  constexpr int kNumFrames = 1000;
  iamf_tools_cli_proto::UserMetadata user_metadata = {};
  ConfigureOneStereoSubstreamLittleEndian(user_metadata);
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      param_definitions = {};
  DemixingModule demixing_module;
  GlobalTimingModule global_timing_module;
  std::optional<ParametersManager> parameters_manager;
  std::optional<AudioFrameGenerator> audio_frame_generator;
  InitializeAudioFrameGenerator(user_metadata, param_definitions,
                                codec_config_obus, audio_elements,
                                demixing_module, global_timing_module,
                                parameters_manager, audio_frame_generator);
  // Vector backing the samples passed to `audio_frame_generator`.
  const int kFrameSize = 8;
  std::vector<std::vector<InternalSampleType>> all_samples(kNumFrames);
  for (int i = 0; i < kNumFrames; ++i) {
    all_samples[i].resize(kFrameSize, static_cast<InternalSampleType>(i));
  }

  const auto label_to_frames = [&]() -> auto {
    absl::flat_hash_map<ChannelLabel::Label,
                        std::vector<absl::Span<const InternalSampleType>>>
        result;
    result.reserve(kNumFrames);
    for (int i = 0; i < kNumFrames; ++i) {
      result[ChannelLabel::kL2].push_back(absl::MakeConstSpan(all_samples[i]));
      result[ChannelLabel::kR2].push_back(absl::MakeConstSpan(all_samples[i]));
    }
    return result;
  }();

  std::thread sample_adder([&] {
    AddAllSamplesAndFinalizesExpectOk(kFirstAudioElementId, label_to_frames,
                                      *audio_frame_generator);
  });
  std::list<AudioFrameWithData> output_audio_frames;
  std::thread sample_collector([&] {
    FlushAudioFrameGeneratorExpectOk(*audio_frame_generator,
                                     output_audio_frames);
  });

  sample_adder.join();
  sample_collector.join();
  // We expect `kNumFrames` frames. The samples should count up incrementally.
  EXPECT_EQ(output_audio_frames.size(), kNumFrames);
  int index = 0;
  for (const auto& audio_frame : output_audio_frames) {
    // Examine the first sample in each channel. We expect them to be in the
    // same order as the input frames.
    constexpr int kFirstSample = 0;
    constexpr int kLeftChannel = 0;
    constexpr int kRightChannel = 1;
    const InternalSampleType expected_sample = all_samples[index][kFirstSample];
    // The timestamp should count up by the number of samples in each frame.
    EXPECT_EQ(audio_frame.start_timestamp, kFrameSize * index);
    ASSERT_TRUE(audio_frame.pcm_samples.has_value());
    EXPECT_DOUBLE_EQ((*audio_frame.pcm_samples)[kFirstSample][kLeftChannel],
                     expected_sample);
    EXPECT_DOUBLE_EQ((*audio_frame.pcm_samples)[kFirstSample][kRightChannel],
                     expected_sample);
    index++;
  }
}

}  // namespace
}  // namespace iamf_tools
