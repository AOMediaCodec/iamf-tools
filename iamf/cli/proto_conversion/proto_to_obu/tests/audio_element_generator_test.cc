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
#include "iamf/cli/proto_conversion/proto_to_obu/audio_element_generator.h"

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

// TODO(b/296171268): Add more tests for `AudioElementGenerator`.

namespace iamf_tools {
namespace {
using ::absl_testing::IsOk;
using enum ChannelLabel::Label;

typedef ::google::protobuf::RepeatedPtrField<
    iamf_tools_cli_proto::AudioElementObuMetadata>
    AudioElementObuMetadatas;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr uint32_t kSampleRate = 48000;

constexpr DecodedUleb128 kMonoSubstreamId = 99;
constexpr DecodedUleb128 kL2SubstreamId = 100;

const ScalableChannelLayoutConfig kOneLayerStereoConfig{
    .num_layers = 1,
    .channel_audio_layer_configs = {
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
         .output_gain_is_present_flag = false,
         .substream_count = 1,
         .coupled_substream_count = 1}}};

const ScalableChannelLayoutConfig& GetScalableLayoutForAudioElementIdExpectOk(
    DecodedUleb128 audio_element_id,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        output_obus) {
  EXPECT_TRUE(output_obus.contains(audio_element_id));
  const auto& output_scalable_channel_layout_config =
      output_obus.at(audio_element_id).obu.config_;
  EXPECT_TRUE(std::holds_alternative<ScalableChannelLayoutConfig>(
      output_scalable_channel_layout_config));
  return std::get<ScalableChannelLayoutConfig>(
      output_scalable_channel_layout_config);
}

void AddFirstOrderAmbisonicsMetadata(
    AudioElementObuMetadatas& audio_element_metadatas) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 0, 1, 2, 3 ]
        num_parameters: 0
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 4
            channel_mapping: [ 0, 1, 2, 3 ]
          }
        }
      )pb",
      audio_element_metadatas.Add()));
}

void AddTwoLayerStereoMetadata(
    AudioElementObuMetadatas& audio_element_metadatas) {
  auto* new_audio_element_metadata = audio_element_metadatas.Add();
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 2
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_MONO
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 0
          }
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            output_gain_is_present_flag: 1
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 0
            output_gain_flag: 32
            output_gain: 32767
          }
        }
      )pb",
      new_audio_element_metadata));
  new_audio_element_metadata->mutable_audio_substream_ids()->Add(
      kMonoSubstreamId);
  new_audio_element_metadata->mutable_audio_substream_ids()->Add(
      kL2SubstreamId);
}

TEST(Generate, PopulatesExpandedLoudspeakerLayout) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
          num_layers: 1
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_EXPANDED
            substream_count: 1
            coupled_substream_count: 0
            expanded_loudspeaker_layout: EXPANDED_LOUDSPEAKER_LAYOUT_LFE
          }
        }
      )pb",
      audio_element_metadatas.Add()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  const auto& output_first_layer =
      GetScalableLayoutForAudioElementIdExpectOk(kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_EQ(output_first_layer.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  ASSERT_TRUE(output_first_layer.expanded_loudspeaker_layout.has_value());
  EXPECT_EQ(*output_first_layer.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutLFE);
}

TEST(Generate, InvalidWhenExpandedLoudspeakerLayoutIsSignalledButNotPresent) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
          num_layers: 1
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_EXPANDED
            substream_count: 1
            coupled_substream_count: 0
            # expanded_loudspeaker_layout: EXPANDED_LOUDSPEAKER_LAYOUT_LFE
          }
        }
      )pb",
      audio_element_metadatas.Add()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_FALSE(generator.Generate(codec_config_obus, output_obus).ok());
}

TEST(Generate, IgnoresExpandedLayoutWhenNotSignalled) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
          num_layers: 1
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            substream_count: 1
            coupled_substream_count: 1
            expanded_loudspeaker_layout: EXPANDED_LOUDSPEAKER_LAYOUT_LFE
          }
        }
      )pb",
      audio_element_metadatas.Add()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  const auto& output_first_layer =
      GetScalableLayoutForAudioElementIdExpectOk(kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_FALSE(output_first_layer.expanded_loudspeaker_layout.has_value());
}

TEST(Generate, LeavesExpandedLayoutEmptyWhenNotSignalled) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
          num_layers: 1
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            substream_count: 1
            coupled_substream_count: 1
          }
        }
      )pb",
      audio_element_metadatas.Add()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  const auto& output_first_layer =
      GetScalableLayoutForAudioElementIdExpectOk(kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_FALSE(output_first_layer.expanded_loudspeaker_layout.has_value());
}

class AudioElementGeneratorTest : public ::testing::Test {
 public:
  AudioElementGeneratorTest() {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus_);
  }

  void InitAndTestGenerate() {
    AudioElementGenerator generator(audio_element_metadata_);

    EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

    EXPECT_EQ(output_obus_, expected_obus_);
  }

 protected:
  AudioElementObuMetadatas audio_element_metadata_;

  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus_;

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> expected_obus_;
};

TEST_F(AudioElementGeneratorTest, NoAudioElementObus) { InitAndTestGenerate(); }

TEST_F(AudioElementGeneratorTest, FirstOrderMonoAmbisonicsNumericalOrder) {
  AddFirstOrderAmbisonicsMetadata(audio_element_metadata_);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {0, 1, 2, 3}, codec_config_obus_,
      expected_obus_);

  InitAndTestGenerate();
}

TEST_F(AudioElementGeneratorTest, FirstOrderMonoAmbisonicsLargeSubstreamIds) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 1000, 2000, 3000, 4000 ]
        num_parameters: 0
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 4
            channel_mapping: [ 0, 1, 2, 3 ]
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {1000, 2000, 3000, 4000},
      codec_config_obus_, expected_obus_);

  InitAndTestGenerate();
}

TEST_F(AudioElementGeneratorTest, FirstOrderMonoAmbisonicsArbitraryOrder) {
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {100, 101, 102, 103}, codec_config_obus_,
      expected_obus_);
  auto expected_obu_iter = expected_obus_.find(kAudioElementId);
  ASSERT_NE(expected_obu_iter, expected_obus_.end());

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 100, 101, 102, 103 ]
        num_parameters: 0
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 4
            channel_mapping: [ 3, 1, 0, 2 ]
          }
        }
      )pb",
      audio_element_metadata_.Add()));
  auto& expected_obu = expected_obu_iter->second;
  std::get<AmbisonicsMonoConfig>(
      std::get<AmbisonicsConfig>(expected_obu.obu.config_).ambisonics_config)
      .channel_mapping = {/*A0:*/ 3, /*A1:*/ 1, /*A2:*/ 0, /*A3:*/ 2};

  // Configures the remapped `substream_id_to_labels` correctly.
  expected_obu.substream_id_to_labels = {
      {103, {kA0}},
      {101, {kA1}},
      {100, {kA2}},
      {102, {kA3}},
  };

  InitAndTestGenerate();
}

TEST_F(AudioElementGeneratorTest,
       SubstreamWithMultipleAmbisonicsChannelNumbers) {
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {100, 101, 102}, codec_config_obus_,
      expected_obus_);
  auto expected_obu_iter = expected_obus_.find(kAudioElementId);
  ASSERT_NE(expected_obu_iter, expected_obus_.end());

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 100, 101, 102 ]
        num_parameters: 0
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 3
            channel_mapping: [ 0, 2, 1, 0 ]
          }
        }
      )pb",
      audio_element_metadata_.Add()));
  auto& expected_obu = expected_obu_iter->second;
  std::get<AmbisonicsMonoConfig>(
      std::get<AmbisonicsConfig>(expected_obu.obu.config_).ambisonics_config)
      .channel_mapping = {/*A0:*/ 0, /*A1:*/ 2, /*A2:*/ 1, /*A3:*/ 0};

  // Configures the remapped `substream_id_to_labels` correctly.
  expected_obu.substream_id_to_labels = {
      {100, {kA0, kA3}},
      {101, {kA2}},
      {102, {kA1}},
  };

  InitAndTestGenerate();
}

TEST_F(AudioElementGeneratorTest, MixedFirstOrderMonoAmbisonics) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 1000, 2000, 3000 ]
        num_parameters: 0
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 3
            channel_mapping: [ 0, 1, 2, 255 ]
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {1000, 2000, 3000}, codec_config_obus_,
      expected_obus_);

  InitAndTestGenerate();
}

TEST_F(AudioElementGeneratorTest, ThirdOrderMonoAmbisonics) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [
          0,
          1,
          2,
          3,
          4,
          5,
          6,
          7,
          8,
          9,
          10,
          11,
          12,
          13,
          14,
          15
        ]
        num_parameters: 0
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 16
            substream_count: 16
            channel_mapping: [
              0,
              1,
              2,
              3,
              4,
              5,
              6,
              7,
              8,
              9,
              10,
              11,
              12,
              13,
              14,
              15
            ]
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId,
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
      codec_config_obus_, expected_obus_);

  InitAndTestGenerate();
}

TEST_F(AudioElementGeneratorTest, FillsAudioElementWithDataFields) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 1, .lfe = 0, .height = 0},
      {.surround = 2, .lfe = 0, .height = 0}};
  AddTwoLayerStereoMetadata(audio_element_metadata_);
  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  const auto& audio_element_with_data = output_obus_.at(kAudioElementId);
  EXPECT_EQ(audio_element_with_data.substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
  EXPECT_EQ(audio_element_with_data.channel_numbers_for_layers,
            kExpectedChannelNumbersForLayer);
  ASSERT_TRUE(audio_element_with_data.label_to_output_gain.contains(kL2));
  EXPECT_FLOAT_EQ(audio_element_with_data.label_to_output_gain.at(kL2),
                  128.0 - 1 / 256.0);
}

TEST_F(AudioElementGeneratorTest, DeprecatedLoudspeakerLayoutIsNotSupported) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs {
            deprecated_loudspeaker_layout: 1  # Stereo
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 1
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_FALSE(generator.Generate(codec_config_obus_, output_obus_).ok());
}

TEST_F(AudioElementGeneratorTest, DefaultLoudspeakerLayoutIsNotSupported) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs {
            # loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 1
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_FALSE(generator.Generate(codec_config_obus_, output_obus_).ok());
}

void AddTwoLayer7_1_0_And7_1_4(::google::protobuf::RepeatedPtrField<
                               iamf_tools_cli_proto::AudioElementObuMetadata>&
                                   audio_element_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 700, 701, 702, 703, 704, 740, 741 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 2
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_7_1_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 5
            coupled_substream_count: 3
          }
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_7_1_4_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 2
            coupled_substream_count: 2
          }
        }
      )pb",
      audio_element_metadata.Add()));
}

TEST_F(AudioElementGeneratorTest, GeneratesDemixingParameterDefinition) {
  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        param_definition_type: PARAM_DEFINITION_TYPE_DEMIXING
        demixing_param: {
          param_definition {
            parameter_id: 998
            parameter_rate: 48000
            param_definition_mode: 0
            reserved: 10
            duration: 8
            constant_subblock_duration: 8
          }
          default_demixing_info_parameter_data: {
            dmixp_mode: DMIXP_MODE_2
            reserved: 11
          }
          default_w: 2
          reserved: 12
        }
      )pb",
      audio_element_metadata_.at(0).add_audio_element_params()));

  // Configure matching expected values.
  DemixingParamDefinition expected_demixing_param_definition;
  expected_demixing_param_definition.parameter_id_ = 998;
  expected_demixing_param_definition.parameter_rate_ = 48000;
  expected_demixing_param_definition.param_definition_mode_ = 0;
  expected_demixing_param_definition.duration_ = 8;
  expected_demixing_param_definition.constant_subblock_duration_ = 8;
  expected_demixing_param_definition.reserved_ = 10;

  auto& expected_default_demixing_info_parameter_data =
      expected_demixing_param_definition.default_demixing_info_parameter_data_;
  // `DemixingInfoParameterData` in the IAMF spec.
  expected_default_demixing_info_parameter_data.dmixp_mode =
      DemixingInfoParameterData::kDMixPMode2;
  expected_default_demixing_info_parameter_data.reserved = 11;
  // Extension portion of `DefaultDemixingInfoParameterData` in the IAMF spec.
  expected_default_demixing_info_parameter_data.default_w = 2;
  expected_default_demixing_info_parameter_data.reserved_for_future_use = 12;

  const AudioElementParam kExpectedAudioElementParam = {
      expected_demixing_param_definition};

  // Generate and validate the parameter-related information matches expected
  // results.
  AudioElementGenerator generator(audio_element_metadata_);
  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  const auto& obu = output_obus_.at(kAudioElementId).obu;
  EXPECT_EQ(obu.audio_element_params_.size(), 1);
  ASSERT_FALSE(obu.audio_element_params_.empty());
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_element_params_.front(),
            kExpectedAudioElementParam);
}

TEST_F(AudioElementGeneratorTest, MissingParamDefinitionTypeIsNotSupported) {
  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        # `param_definition_type` is omitted.
        # param_definition_type: PARAM_DEFINITION_TYPE_DEMIXING
      )pb",
      audio_element_metadata_.at(0).add_audio_element_params()));

  AudioElementGenerator generator(audio_element_metadata_);
  EXPECT_FALSE(generator.Generate(codec_config_obus_, output_obus_).ok());
}

TEST_F(AudioElementGeneratorTest, DeprecatedParamDefinitionTypeIsNotSupported) {
  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        deprecated_param_definition_type: 1  # PARAMETER_DEFINITION_DEMIXING
      )pb",
      audio_element_metadata_.at(0).add_audio_element_params()));

  AudioElementGenerator generator(audio_element_metadata_);
  EXPECT_FALSE(generator.Generate(codec_config_obus_, output_obus_).ok());
}

TEST_F(AudioElementGeneratorTest, GeneratesReconGainParameterDefinition) {
  // Recon gain requires an associated lossy codec (e.g. Opus or AAC).
  codec_config_obus_.clear();
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus_);

  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);

  // Reconfigure the audio element to add a recon gain parameter.
  auto& audio_element_metadata = audio_element_metadata_.at(0);
  audio_element_metadata.mutable_scalable_channel_layout_config()
      ->mutable_channel_audio_layer_configs(1)
      ->set_recon_gain_is_present_flag(true);
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        param_definition_type: PARAM_DEFINITION_TYPE_RECON_GAIN
        recon_gain_param: {
          param_definition {
            parameter_id: 998
            parameter_rate: 48000
            param_definition_mode: 0
            reserved: 10
            duration: 8
            constant_subblock_duration: 8
          }
        }
      )pb",
      audio_element_metadata.add_audio_element_params()));
  // Configure matching expected values.
  ReconGainParamDefinition expected_recon_gain_param_definition(
      kAudioElementId);
  expected_recon_gain_param_definition.parameter_id_ = 998;
  expected_recon_gain_param_definition.parameter_rate_ = 48000;
  expected_recon_gain_param_definition.param_definition_mode_ = 0;
  expected_recon_gain_param_definition.duration_ = 8;
  expected_recon_gain_param_definition.constant_subblock_duration_ = 8;
  expected_recon_gain_param_definition.reserved_ = 10;

  const AudioElementParam kExpectedAudioElementParam = {
      expected_recon_gain_param_definition};

  // Generate and validate the parameter-related information matches expected
  // results.
  AudioElementGenerator generator(audio_element_metadata_);
  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  const auto& obu = output_obus_.at(kAudioElementId).obu;
  EXPECT_EQ(obu.audio_element_params_.size(), 1);
  ASSERT_FALSE(obu.audio_element_params_.empty());
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_element_params_.front(),
            kExpectedAudioElementParam);
}

TEST_F(AudioElementGeneratorTest, IgnoresDeprecatedNumSubstreamsField) {
  AddFirstOrderAmbisonicsMetadata(audio_element_metadata_);
  auto& first_order_ambisonics_metadata = audio_element_metadata_.at(0);
  // Normally first-order ambisonics has four substreams.
  constexpr DecodedUleb128 kExpectedNumSubstreams = 4;
  // Corrupt the `num_substreams` field.
  const auto kIgnoredNumSubstreams = 9999;
  first_order_ambisonics_metadata.set_num_substreams(kIgnoredNumSubstreams);
  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  // The field is deprecated and ignored, the actual number of substreams are
  // set based on the `audio_substream_ids` field.
  ASSERT_TRUE(output_obus_.contains(kAudioElementId));
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_substream_ids_.size(),
            kExpectedNumSubstreams);
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.GetNumSubstreams(),
            kExpectedNumSubstreams);
}

TEST_F(AudioElementGeneratorTest, IgnoresDeprecatedNumParametersField) {
  AddFirstOrderAmbisonicsMetadata(audio_element_metadata_);
  auto& first_order_ambisonics_metadata = audio_element_metadata_.at(0);
  constexpr DecodedUleb128 kExpectedNumParameters = 0;
  // Corrupt the `num_parameters` field.
  const auto kIgnoredNumParameters = 9999;
  first_order_ambisonics_metadata.set_num_parameters(kIgnoredNumParameters);
  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  // The field is deprecated and ignored, the actual number of parameters are
  // set based on the `audio_element_params` field.
  ASSERT_TRUE(output_obus_.contains(kAudioElementId));
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.GetNumParameters(),
            kExpectedNumParameters);
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_element_params_.size(),
            kExpectedNumParameters);
}

TEST_F(AudioElementGeneratorTest, IgnoresDeprecatedParamDefinitionSizeField) {
  AddFirstOrderAmbisonicsMetadata(audio_element_metadata_);
  auto& first_order_ambisonics_metadata = audio_element_metadata_.at(0);
  auto* audio_element_param =
      first_order_ambisonics_metadata.mutable_audio_element_params()->Add();
  audio_element_param->set_param_definition_type(
      iamf_tools_cli_proto::PARAM_DEFINITION_TYPE_RESERVED_3);
  // Corrupt the `num_parameters` field.
  constexpr absl::string_view kParamDefinitionBytes = "abc";
  constexpr DecodedUleb128 kExpectedParamDefinitionSize =
      kParamDefinitionBytes.size();
  const auto kInconsistentParamDefinitionSize = 9999;
  audio_element_param->mutable_param_definition_extension()
      ->set_param_definition_size(kInconsistentParamDefinitionSize);
  audio_element_param->mutable_param_definition_extension()
      ->set_param_definition_bytes(kParamDefinitionBytes);
  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  // The field is deprecatred and ignored, the actual number of parameters are
  // set based on the `param_definition_bytes` field.
  ASSERT_TRUE(output_obus_.contains(kAudioElementId));
  ASSERT_FALSE(
      output_obus_.at(kAudioElementId).obu.audio_element_params_.empty());
  const auto* extended_param_definition = std::get_if<ExtendedParamDefinition>(
      &output_obus_.at(kAudioElementId)
           .obu.audio_element_params_.front()
           .param_definition);
  ASSERT_NE(extended_param_definition, nullptr);
  EXPECT_EQ(extended_param_definition->param_definition_size_,
            kExpectedParamDefinitionSize);
  EXPECT_EQ(extended_param_definition->param_definition_bytes_.size(),
            kExpectedParamDefinitionSize);
}

TEST_F(AudioElementGeneratorTest, IgnoresDeprecatedNumLayers) {
  AddTwoLayerStereoMetadata(audio_element_metadata_);
  auto& first_order_ambisonics_metadata = audio_element_metadata_.at(0);
  // Two layers are set in the metadata.
  constexpr uint8_t kExpectedNumLayers = 2;
  // Corrupt the `num_layers` field.
  const auto kIgnoredNumLayers = 7;
  first_order_ambisonics_metadata.mutable_scalable_channel_layout_config()
      ->set_num_layers(kIgnoredNumLayers);
  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  // The corrupted value is ignored, and the actual number of parameters are set
  // correctly.
  ASSERT_TRUE(output_obus_.contains(kAudioElementId));
  const auto* scalable_channel_layout_config =
      std::get_if<ScalableChannelLayoutConfig>(
          &output_obus_.at(kAudioElementId).obu.config_);
  ASSERT_NE(scalable_channel_layout_config, nullptr);
  EXPECT_EQ(scalable_channel_layout_config->num_layers, kExpectedNumLayers);
  EXPECT_EQ(scalable_channel_layout_config->channel_audio_layer_configs.size(),
            kExpectedNumLayers);
}

}  // namespace
}  // namespace iamf_tools
