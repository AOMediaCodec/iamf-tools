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
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_generator.h"

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::NotNull;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kParameterId = 100;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 8;
constexpr bool kOverrideComputedReconGains = false;
constexpr std::array<DecodedUleb128, 1> kOneSubstreamId{0};
constexpr std::array<DecodedUleb128, 4> kFourSubtreamIds{0, 1, 2, 3};

TEST(ParameterBlockGeneratorTest, NoParameterBlocks) {
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants;
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    param_definition_variants);

  // Add metadata.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());
  }

  // Generate.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  auto global_timing_module = GlobalTimingModule::Create(
      /*audio_elements=*/{}, /*param_definitions=*/{});
  ASSERT_THAT(global_timing_module, NotNull());
  EXPECT_THAT(generator.GenerateDemixing(*global_timing_module,
                                         output_parameter_blocks),
              IsOk());
  EXPECT_TRUE(output_parameter_blocks.empty());
  EXPECT_THAT(
      generator.GenerateMixGain(*global_timing_module, output_parameter_blocks),
      IsOk());
  EXPECT_TRUE(output_parameter_blocks.empty());

  IdLabeledFrameMap id_to_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(generator.GenerateReconGain(
                  id_to_labeled_frame, id_to_labeled_decoded_frame,
                  *global_timing_module, output_parameter_blocks),
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
    IamfInputLayout input_layout,
    absl::Span<const DecodedUleb128> substream_ids,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  constexpr uint32_t kSampleRate = 48000;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddScalableAudioElementWithSubstreamIds(input_layout, kAudioElementId,
                                          kCodecConfigId, substream_ids,
                                          codec_config_obus, audio_elements);
}

void ValidateParameterBlocksCommon(
    const std::list<ParameterBlockWithData>& output_parameter_blocks,
    const DecodedUleb128 expected_parameter_id,
    const std::vector<InternalTimestamp>& expected_start_timestamps,
    const std::vector<InternalTimestamp>& expected_end_timestamps) {
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
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ConfigureDemixingParameterBlocks(user_metadata);

  // Initialize pre-requisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(IamfInputLayout::kStereo, kOneSubstreamId,
                             codec_config_obus, audio_elements);

  // Add a demixing parameter definition inside the Audio Element OBU.
  AddDemixingParamDefinition(kParameterId, kParameterRate, kDuration,
                             audio_elements.begin()->second.obu);
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants;
  ASSERT_THAT(CollectAndValidateParamDefinitions(audio_elements,
                                                 /*mix_presentation_obus=*/{},
                                                 param_definition_variants),
              IsOk());

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    param_definition_variants);
  EXPECT_THAT(generator.Initialize(audio_elements), IsOk());

  // Global timing Module; needed when calling `GenerateDemixing()`.
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements, param_definition_variants);
  ASSERT_THAT(global_timing_module, NotNull());

  // Loop to add and generate.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    // Add metadata.
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());

    // Generate parameter blocks.
    std::list<ParameterBlockWithData> parameter_blocks_for_frame;
    EXPECT_THAT(generator.GenerateDemixing(*global_timing_module,
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
    auto demixing_info_parameter_data = static_cast<DemixingInfoParameterData*>(
        parameter_block.obu->subblocks_[0].param_data.get());
    EXPECT_EQ(demixing_info_parameter_data->dmixp_mode,
              expected_dmixp_mode[block_index]);
    EXPECT_EQ(demixing_info_parameter_data->reserved, 0);
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
    absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants) {
  param_definition.default_mix_gain_ = default_mix_gain;
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 48000;
  param_definition.param_definition_mode_ = 1;
  param_definition.reserved_ = 0;
  param_definition_variants.emplace(kParameterId, param_definition);
}

TEST(ParameterBlockGeneratorTest, GenerateMixGainParameterBlocks) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ConfigureMixGainParameterBlocks(user_metadata);

  // Initialize pre-requisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(IamfInputLayout::kStereo, kOneSubstreamId,
                             codec_config_obus, audio_elements);

  // Add param definition. It would normally be owned by a Mix Presentation OBU.
  MixGainParamDefinition param_definition;
  const int16_t kDefaultMixGain = -123;
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants;
  AddMixGainParamDefinition(kDefaultMixGain, param_definition,
                            param_definition_variants);

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    param_definition_variants);
  EXPECT_THAT(generator.Initialize(audio_elements), IsOk());

  // Global timing Module; needed when calling `GenerateDemixing()`.
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements, param_definition_variants);
  ASSERT_THAT(global_timing_module, NotNull());

  // Loop to add and generate.
  std::list<ParameterBlockWithData> output_parameter_blocks;
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    // Add metadata.
    EXPECT_THAT(generator.AddMetadata(metadata), IsOk());

    // Generate parameter blocks.
    std::list<ParameterBlockWithData> parameter_blocks_for_frame;
    EXPECT_THAT(generator.GenerateMixGain(*global_timing_module,
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
  for (const auto& parameter_block : output_parameter_blocks) {
    auto mix_gain_parameter_data = static_cast<MixGainParameterData*>(
        parameter_block.obu->subblocks_[0].param_data.get());
    EXPECT_EQ(mix_gain_parameter_data->animation_type,
              MixGainParameterData::kAnimateStep);
    EXPECT_EQ(std::get<AnimationStepInt16>(mix_gain_parameter_data->param_data)
                  .start_point_value,
              0);
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
  const std::vector<InternalSampleType> samples(8, 10000);
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
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ConfigureReconGainParameterBlocks(user_metadata);

  // Initialize pre-requisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(IamfInputLayout::k5_1, kFourSubtreamIds,
                             codec_config_obus, audio_elements);

  // Extra data needed to compute recon gain.
  PrepareAudioElementWithDataForReconGain(audio_elements.begin()->second);

  // Add a recon gain parameter definition inside the Audio Element OBU.
  AddReconGainParamDefinition(kParameterId, kParameterRate, kDuration,
                              audio_elements.begin()->second.obu);
  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants;
  ASSERT_THAT(CollectAndValidateParamDefinitions(audio_elements,
                                                 /*mix_presentation_obus=*/{},
                                                 param_definition_variants),
              IsOk());

  // Construct and initialize.
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    param_definition_variants);
  EXPECT_THAT(generator.Initialize(audio_elements), IsOk());

  // Global timing Module; needed when calling `GenerateDemixing()`.
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements, param_definition_variants);
  ASSERT_THAT(global_timing_module, NotNull());

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
                    *global_timing_module, parameter_blocks_for_frame),
                IsOk());
    EXPECT_EQ(parameter_blocks_for_frame.size(), 1);
    output_parameter_blocks.splice(output_parameter_blocks.end(),
                                   parameter_blocks_for_frame);
  }

  // Validate common parts.
  ValidateParameterBlocksCommon(output_parameter_blocks, kParameterId,
                                /*expected_start_timestamps=*/{0, 8},
                                /*expected_end_timestamps=*/{8, 16});

  // Validate `ReconGainInfoParameterData` parts.
  int block_index = 0;
  for (const auto& parameter_block : output_parameter_blocks) {
    auto recon_gain_info_parameter_data =
        static_cast<ReconGainInfoParameterData*>(
            parameter_block.obu->subblocks_[0].param_data.get());

    // Expect the first recon gain element to hold no value.
    EXPECT_FALSE(
        recon_gain_info_parameter_data->recon_gain_elements[0].has_value());

    // Expect the second recon gain element to hold values as specified in
    // the user metadata via `ConfigureReconGainParameterBlocks()`:
    // - `recon_gain_flag` =  (1 << 0 | 1 << 2 | 1 << 3 | 1 << 4) = 29.
    // - `recon_gain` value = 255 at positions 0, 2, 3, 4.
    const auto& recon_gain_element_1 =
        recon_gain_info_parameter_data->recon_gain_elements[1];
    EXPECT_TRUE(recon_gain_element_1.has_value());
    EXPECT_EQ(recon_gain_element_1->recon_gain_flag, 29);
    EXPECT_THAT(recon_gain_element_1->recon_gain,
                testing::ElementsAreArray(
                    {255, 0, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0}));
    block_index++;
  }
}

TEST(Initialize, FailsWhenThereAreStrayParameterBlocks) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  // Initialize pre-requisite OBUs.
  ConfigureDemixingParameterBlocks(user_metadata);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitializePrerequisiteObus(IamfInputLayout::k5_1, kFourSubtreamIds,
                             codec_config_obus, audio_elements);

  // Construct and initialize.
  const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      empty_param_definition_variants;
  ParameterBlockGenerator generator(kOverrideComputedReconGains,
                                    empty_param_definition_variants);
  EXPECT_THAT(generator.Initialize(audio_elements), IsOk());

  // Try to add metadata, but since the param definitions are empty, these
  // will fail because the generator cannot find the corresponding param
  // definitions for the parameter (i.e. they are "stray").
  for (const auto& metadata : user_metadata.parameter_block_metadata()) {
    EXPECT_FALSE(generator.AddMetadata(metadata).ok());
  }
}

}  // namespace
}  // namespace iamf_tools
