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
#include "iamf/cli/proto_to_obu/parameter_block_generator.h"

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kMixPresentationId = 1337;
constexpr DecodedUleb128 kParameterId = 100;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 8;
constexpr bool kOverrideComputedReconGains = false;

TEST(ParameterBlockGeneratorTest, NoParameterBlocks) {
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    parameter_id_to_metadata);

  // Add metadata.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());
  }

  // Generate.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  GlobalTimingModule global_timing_module;
  EXPECT_THAT(
      generator.GenerateDemixing(global_timing_module, output_parameter_blocks),
      IsOk());
  EXPECT_TRUE(output_parameter_blocks.empty());
  EXPECT_THAT(
      generator.GenerateMixGain(global_timing_module, output_parameter_blocks),
      IsOk());
  EXPECT_TRUE(output_parameter_blocks.empty());

  IdLabeledFrameMap id_to_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(generator.GenerateReconGain(
                  id_to_labeled_frame, id_to_labeled_decoded_frame,
                  global_timing_module, output_parameter_blocks),
              IsOk());
  EXPECT_TRUE(output_parameter_blocks.empty());
}

void ConfigureDemixingParameterBlocks(
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  // Two blocks, each spanning 8 ticks.
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_3 } }
        start_timestamp: 0
      )pb",
      user_metadata.add_parameter_block_metadata()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks { demixing_info_parameter_data { dmixp_mode: DMIXP_MODE_2 } }
        start_timestamp: 8
      )pb",
      user_metadata.add_parameter_block_metadata()));
}

void InitializePrerequisiteObus(
    const std::vector<DecodedUleb128>& substream_ids,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  constexpr uint32_t kSampleRate = 48000;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddScalableAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                          substream_ids, codec_config_obus,
                                          audio_elements);
}

void ValidateParameterBlocksCommon(
    const std::list<ParameterBlockWithData>& output_parameter_blocks,
    const DecodedUleb128 expected_parameter_id,
    const std::vector<int32_t>& expected_start_timestamps,
    const std::vector<int32_t>& expected_end_timestamps) {
  EXPECT_EQ(expected_start_timestamps.size(), output_parameter_blocks.size());
  EXPECT_EQ(expected_end_timestamps.size(), output_parameter_blocks.size());
  int block_index = 0;
  for (const auto& parameter_block : output_parameter_blocks) {
    EXPECT_EQ(parameter_block.start_timestamp,
              expected_start_timestamps[block_index]);
    EXPECT_EQ(parameter_block.end_timestamp,
              expected_end_timestamps[block_index]);

    const auto& obu = parameter_block.obu;
    EXPECT_EQ(obu->parameter_id_, expected_parameter_id);
    EXPECT_EQ(obu->GetDuration(), 8);
    EXPECT_EQ(obu->GetNumSubblocks(), 1);
    EXPECT_EQ(obu->GetSubblockDuration(0).value(), 8);
    EXPECT_EQ(obu->GetConstantSubblockDuration(), 8);
    block_index++;
  }
}

TEST(ParameterBlockGeneratorTest, GenerateTwoDemixingParameterBlocks) {
  absl::flat_hash_map<uint32_t, PerIdParameterMetadata>
      parameter_id_to_metadata;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ConfigureDemixingParameterBlocks(user_metadata);

  // Initialize pre-requisite OBUs.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(/*substream_ids=*/{0}, codec_config_obus,
                             audio_elements);

  // Add a demixing parameter definition inside the Audio Element OBU.
  absl::flat_hash_map<uint32_t, const ParamDefinition*> param_definitions;
  AddDemixingParamDefinition(kParameterId, kParameterRate, kDuration,
                             audio_elements.begin()->second.obu,
                             &param_definitions);

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    parameter_id_to_metadata);
  EXPECT_THAT(generator.Initialize(audio_elements, param_definitions), IsOk());

  // Global timing Module; needed when calling `GenerateDemixing()`.
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(
      global_timing_module.Initialize(audio_elements, param_definitions),
      IsOk());

  // Loop to add and generate.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    // Add metadata.
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());

    // Generate parameter blocks.
    std::list<ParameterBlockWithData> parameter_blocks_for_frame;
    EXPECT_THAT(generator.GenerateDemixing(global_timing_module,
                                           parameter_blocks_for_frame),
                IsOk());
    EXPECT_EQ(parameter_blocks_for_frame.size(), 1);
    output_parameter_blocks.splice(output_parameter_blocks.end(),
                                   parameter_blocks_for_frame);
  }

  // Validate common parts.
  ValidateParameterBlocksCommon(output_parameter_blocks, kParameterId,
                                /*expected_start_timestamps=*/{0, 8},
                                /*expected_end_timestamps=*/{8, 16});

  // Validate `DemixingInfoParameterData` parts.
  const std::vector<DemixingInfoParameterData::DMixPMode> expected_dmixp_mode =
      {DemixingInfoParameterData::kDMixPMode3,
       DemixingInfoParameterData::kDMixPMode2};
  int block_index = 0;
  for (const auto& parameter_block : output_parameter_blocks) {
    const auto& obu = parameter_block.obu;
    EXPECT_TRUE(std::holds_alternative<DemixingInfoParameterData>(
        obu->subblocks_[0].param_data));
    const auto& demixing_param_data =
        std::get<DemixingInfoParameterData>(obu->subblocks_[0].param_data);
    EXPECT_EQ(demixing_param_data.dmixp_mode, expected_dmixp_mode[block_index]);
    EXPECT_EQ(demixing_param_data.reserved, 0);
    block_index++;
  }
}

void ConfigureMixGainParameterBlocks(
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  // Two blocks, each spanning 8 ticks.
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks:
        [ {
          mix_gain_parameter_data {
            animation_type: ANIMATE_STEP
            param_data { step { start_point_value: 0 } }
          }
        }],
        start_timestamp: 0
      )pb",
      user_metadata.add_parameter_block_metadata()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks:
        [ {
          mix_gain_parameter_data {
            animation_type: ANIMATE_STEP
            param_data { step { start_point_value: 0 } }
          }
        }],
        start_timestamp: 8
      )pb",
      user_metadata.add_parameter_block_metadata()));
}

void AddMixGainParamDefinition(
    const int16_t default_mix_gain, MixGainParamDefinition& param_definition,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  param_definitions.insert({kParameterId, &param_definition});

  param_definition.default_mix_gain_ = default_mix_gain;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 48000;
  param_definition.param_definition_mode_ = 1;
  param_definition.reserved_ = 0;
}

TEST(ParameterBlockGeneratorTest, GenerateMixGainParameterBlocks) {
  absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>
      parameter_id_to_metadata;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ConfigureMixGainParameterBlocks(user_metadata);

  // Initialize pre-requisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(/*substream_ids=*/{0}, codec_config_obus,
                             audio_elements);

  // Add param definition. It would normally be owned by a Mix Presentation OBU.
  MixGainParamDefinition param_definition;
  const int16_t kDefaultMixGain = -123;
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> param_definitions;
  AddMixGainParamDefinition(kDefaultMixGain, param_definition,
                            param_definitions);

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    parameter_id_to_metadata);
  EXPECT_THAT(generator.Initialize(audio_elements, param_definitions), IsOk());

  // Global timing Module; needed when calling `GenerateDemixing()`.
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(
      global_timing_module.Initialize(audio_elements, param_definitions),
      IsOk());

  // Loop to add and generate.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    // Add metadata.
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());

    // Generate parameter blocks.
    std::list<ParameterBlockWithData> parameter_blocks_for_frame;
    EXPECT_THAT(generator.GenerateMixGain(global_timing_module,
                                          parameter_blocks_for_frame),
                IsOk());
    EXPECT_EQ(parameter_blocks_for_frame.size(), 1);
    output_parameter_blocks.splice(output_parameter_blocks.end(),
                                   parameter_blocks_for_frame);
  }

  // Validate common parts.
  ValidateParameterBlocksCommon(output_parameter_blocks, kParameterId,
                                /*expected_start_timestamps=*/{0, 8},
                                /*expected_end_timestamps=*/{8, 16});

  // Validate `MixGainParameterData` parts.
  int block_index = 0;
  for (const auto& parameter_block : output_parameter_blocks) {
    const auto& obu = parameter_block.obu;
    EXPECT_TRUE(std::holds_alternative<MixGainParameterData>(
        obu->subblocks_[0].param_data));
    const auto& mix_gain_param_data =
        std::get<MixGainParameterData>(obu->subblocks_[0].param_data);
    EXPECT_EQ(mix_gain_param_data.animation_type,
              MixGainParameterData::kAnimateStep);
    EXPECT_EQ(std::get<AnimationStepInt16>(mix_gain_param_data.param_data)
                  .start_point_value,
              0);
    block_index++;
  }
}

// TODO(b/296815263): Add tests with the same parameter ID but different
//                    parameter definitions, which should fail.

void ConfigureReconGainParameterBlocks(
    iamf_tools_cli_proto::UserMetadata& user_metadata) {
  // Two blocks, each spanning 8 ticks.
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks:
        [ {
          recon_gain_info_parameter_data {
            recon_gains_for_layer {}  # layer 1
            recon_gains_for_layer {
              recon_gain { key: 0 value: 255 }
              recon_gain { key: 2 value: 255 }
              recon_gain { key: 3 value: 255 }
              recon_gain { key: 4 value: 255 }
            }  # layer 2
          }
        }],
        start_timestamp: 0
      )pb",
      user_metadata.add_parameter_block_metadata()));
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        parameter_id: 100
        duration: 8
        num_subblocks: 1
        constant_subblock_duration: 8
        subblocks:
        [ {
          recon_gain_info_parameter_data {
            recon_gains_for_layer {}  # layer 1
            recon_gains_for_layer {
              recon_gain { key: 0 value: 255 }
              recon_gain { key: 2 value: 255 }
              recon_gain { key: 3 value: 255 }
              recon_gain { key: 4 value: 255 }
            }  # layer 2
          }
        }],
        start_timestamp: 8
      )pb",
      user_metadata.add_parameter_block_metadata()));
}

void AddReconGainParamDefinition(
    AudioElementObu& audio_element_obu,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  auto param_definition = std::make_unique<ReconGainParamDefinition>(
      audio_element_obu.GetAudioElementId());
  param_definitions.insert({kParameterId, param_definition.get()});

  param_definition->parameter_id_ = kParameterId;
  param_definition->parameter_rate_ = 48000;
  param_definition->param_definition_mode_ = 0;
  param_definition->reserved_ = 0;
  param_definition->duration_ = 8;
  param_definition->constant_subblock_duration_ = 8;

  // Add to the Audio Element OBU.
  audio_element_obu.InitializeParams(1);
  audio_element_obu.audio_element_params_[0] = AudioElementParam{
      .param_definition_type = ParamDefinition::kParameterDefinitionReconGain,
      .param_definition = std::move(param_definition)};
}

void PrepareAudioElementWithDataForReconGain(
    AudioElementWithData& audio_element_with_data) {
  audio_element_with_data.channel_numbers_for_layers = {
      {2, 0, 0},  // Stereo.
      {5, 1, 0},  // 5.1.
  };

  // To compute recon gains, we need at least two layers in the
  // `ScalableChannelLayoutConfig`.
  auto& audio_element_obu = audio_element_with_data.obu;
  EXPECT_THAT(audio_element_obu.InitializeScalableChannelLayout(2, 0), IsOk());
  auto& layer_configs =
      std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_)
          .channel_audio_layer_configs;

  // First layer.
  layer_configs[0] = ChannelAudioLayerConfig{
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
      .output_gain_is_present_flag = 0,
      .recon_gain_is_present_flag = 0,
      .reserved_a = 0,
      .substream_count = 1,
      .coupled_substream_count = 1,
  };
  layer_configs[1] = ChannelAudioLayerConfig{
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_ch,
      .output_gain_is_present_flag = 0,
      .recon_gain_is_present_flag = 1,
      .reserved_a = 0,
      .substream_count = 3,
      .coupled_substream_count = 1,
  };
}

IdLabeledFrameMap PrepareIdLabeledFrameMap() {
  using enum ChannelLabel::Label;
  const std::vector<int32_t> samples(8, 10000);
  LabelSamplesMap label_to_samples;
  for (const auto& label :
       {kL2, kR2, kDemixedL3, kDemixedR3, kDemixedLs5, kDemixedRs5}) {
    label_to_samples[label] = samples;
  }
  IdLabeledFrameMap id_to_labeled_frame;
  id_to_labeled_frame[kAudioElementId] = {.label_to_samples = label_to_samples};
  return id_to_labeled_frame;
}

TEST(ParameterBlockGeneratorTest, GenerateReconGainParameterBlocks) {
  absl::flat_hash_map<uint32_t, PerIdParameterMetadata>
      parameter_id_to_metadata;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ConfigureReconGainParameterBlocks(user_metadata);

  // Initialize pre-requisite OBUs.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(/*substream_ids=*/{0, 1, 2, 3}, codec_config_obus,
                             audio_elements);

  // Extra data needed to compute recon gain.
  PrepareAudioElementWithDataForReconGain(audio_elements.begin()->second);

  // Add a recon gain parameter definition inside the Audio Element OBU.
  absl::flat_hash_map<uint32_t, const ParamDefinition*> param_definitions;
  AddReconGainParamDefinition(audio_elements.begin()->second.obu,
                              param_definitions);

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    parameter_id_to_metadata);
  EXPECT_THAT(generator.Initialize(audio_elements, param_definitions), IsOk());

  // Global timing Module; needed when calling `GenerateDemixing()`.
  GlobalTimingModule global_timing_module;
  ASSERT_THAT(
      global_timing_module.Initialize(audio_elements, param_definitions),
      IsOk());

  // Loop to add all metadata and generate recon gain parameter blocks.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    // Add metadata.
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());

    // Generate.
    // Set the decoded frames identical to the original frames, so that recon
    // gains will be identity.
    IdLabeledFrameMap id_to_labeled_frame = PrepareIdLabeledFrameMap();
    IdLabeledFrameMap id_to_labeled_decoded_frame = id_to_labeled_frame;
    std::list<ParameterBlockWithData> parameter_blocks_for_frame;
    EXPECT_THAT(generator.GenerateReconGain(
                    id_to_labeled_frame, id_to_labeled_decoded_frame,
                    global_timing_module, parameter_blocks_for_frame),
                IsOk());
    EXPECT_EQ(parameter_blocks_for_frame.size(), 1);
    output_parameter_blocks.splice(output_parameter_blocks.end(),
                                   parameter_blocks_for_frame);
  }

  // Validate common parts.
  ValidateParameterBlocksCommon(output_parameter_blocks, kParameterId,
                                /*expected_start_timestamps=*/{0, 8},
                                /*expected_end_timestamps=*/{8, 16});
}

TEST(Initialize, FailsWhenThereAreStrayParameterBlocks) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  absl::flat_hash_map<uint32_t, PerIdParameterMetadata>
      parameter_id_to_metadata;
  // Initialize pre-requisite OBUs.
  ConfigureDemixingParameterBlocks(user_metadata);
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(/*substream_ids=*/{0, 1, 2, 3}, codec_config_obus,
                             audio_elements);

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    parameter_id_to_metadata);
  const absl::flat_hash_map<uint32_t, const ParamDefinition*>
      empty_param_definitions;
  EXPECT_THAT(generator.Initialize(audio_elements, empty_param_definitions),
              IsOk());

  // Try to add metadata, but since the param definitions are empty, these
  // will fail because the generator cannot find the corresponding param
  // definitions for the parameter (i.e. they are "stray").
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    EXPECT_FALSE(generator.AddMetadata(metadata).ok());
  }
}

}  // namespace
}  // namespace iamf_tools
