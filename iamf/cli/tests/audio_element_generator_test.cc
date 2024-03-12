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
#include "iamf/cli/audio_element_generator.h"

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gtest/gtest.h"
#include "iamf/audio_element.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/codec_config.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/param_definitions.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

// TODO(b/296171268): Add more tests for `AudioElementGenerator`.

namespace iamf_tools {
namespace {

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;

class AudioElementGeneratorTest : public ::testing::Test {
 public:
  AudioElementGeneratorTest() {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000,
                                          codec_config_obus_);
  }

  void InitAndTestGenerate() {
    AudioElementGenerator generator(audio_element_metadata_);

    EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

    EXPECT_EQ(output_obus_, expected_obus_);
  }

 protected:
  ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::AudioElementObuMetadata>
      audio_element_metadata_;

  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus_;

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> expected_obus_;
};

TEST_F(AudioElementGeneratorTest, NoAudioElementObus) { InitAndTestGenerate(); }

TEST_F(AudioElementGeneratorTest, FirstOrderMonoAmbisonicsNumericalOrder) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 4
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
      audio_element_metadata_.Add()));

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
        num_substreams: 4
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
        num_substreams: 4
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
      {103, {"A0"}},
      {101, {"A1"}},
      {100, {"A2"}},
      {102, {"A3"}},
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
        num_substreams: 3
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
      {100, {"A0", "A3"}},
      {101, {"A2"}},
      {102, {"A1"}},
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
        num_substreams: 3
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
        num_substreams: 16
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

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForOneLayerStereo) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {99, {"L2", "R2"}}};

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 1
        audio_substream_ids: [ 99 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
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

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest, FallsBackToDeprecatedLoudspeakerLayoutField) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 1
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

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForOneLayer5_1_4) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {55, {"L5", "R5"}},     {77, {"Ls5", "Rs5"}}, {66, {"Ltf4", "Rtf4"}},
      {11, {"Ltb4", "Rtb4"}}, {22, {"C"}},          {88, {"LFE"}}};

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 6
        audio_substream_ids: [ 55, 77, 66, 11, 22, 88 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_5_1_4_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 6
            coupled_substream_count: 4
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForOneLayer7_1_4) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {6, {"L7", "R7"}},     {5, {"Lss7", "Rss7"}}, {4, {"Lrs7", "Rrs7"}},
      {3, {"Ltf4", "Rtf4"}}, {2, {"Ltb4", "Rtb4"}}, {1, {"C"}},
      {0, {"LFE"}}};

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 7
        audio_substream_ids: [ 6, 5, 4, 3, 2, 1, 0 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 1
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_7_1_4_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 7
            coupled_substream_count: 5
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForTwoLayerMonoStereo) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{0, {"M"}},
                                                             {1, {"L2"}}};

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 2
        audio_substream_ids: [ 0, 1 ]
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
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 0
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForTwoLayerStereo3_1_2) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {"L2", "R2"}},
      {1, {"Ltf3", "Rtf3"}},
      {2, {"C"}},
      {3, {"LFE"}},
  };

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 4
        audio_substream_ids: [ 0, 1, 2, 3 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 2
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_STEREO
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 1
            coupled_substream_count: 1
          }
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_3_1_2_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 3
            coupled_substream_count: 1
          }
        }
      )pb",
      audio_element_metadata_.Add()));

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForTwoLayer3_1_2And5_1_2) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {300, {"L3", "R3"}}, {301, {"Ltf3", "Rtf3"}}, {302, {"C"}},
      {303, {"LFE"}},      {514, {"L5", "R5"}},
  };

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 5
        audio_substream_ids: [ 300, 301, 302, 303, 514 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 2
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_3_1_2_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 4
            coupled_substream_count: 2
          }
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_5_1_2_CH
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

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForTwoLayer5_1_2And5_1_4) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {520, {"L5", "R5"}}, {521, {"Ls5", "Rs5"}}, {522, {"Ltf2", "Rtf2"}},
      {523, {"C"}},        {524, {"LFE"}},        {540, {"Ltf4", "Rtf4"}},
  };

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 6
        audio_substream_ids: [ 520, 521, 522, 523, 524, 540 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 2
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_5_1_2_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 5
            coupled_substream_count: 3
          }
          channel_audio_layer_configs: {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_5_1_4_CH
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

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForTwoLayer5_1_0And7_1_0) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {500, {"L5", "R5"}}, {501, {"Ls5", "Rs5"}},   {502, {"C"}},
      {503, {"LFE"}},      {704, {"Lss7", "Rss7"}},
  };

  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 5
        audio_substream_ids: [ 500, 501, 502, 503, 704 ]
        num_parameters: 0
        scalable_channel_layout_config {
          num_layers: 2
          reserved: 0
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_5_1_CH
            output_gain_is_present_flag: 0
            recon_gain_is_present_flag: 0
            reserved_a: 0
            substream_count: 4
            coupled_substream_count: 2
          }
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_7_1_CH
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

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
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
        num_substreams: 7
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

TEST_F(AudioElementGeneratorTest,
       GeneratesCorrectSubstreamIdToLabelsForTwoLayer7_1_0And7_1_4) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {700, {"L7", "R7"}},     {701, {"Lss7", "Rss7"}},
      {702, {"Lrs7", "Rrs7"}}, {703, {"C"}},
      {704, {"LFE"}},          {740, {"Ltf4", "Rtf4"}},
      {741, {"Ltb4", "Rtb4"}},
  };

  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);

  AudioElementGenerator generator(audio_element_metadata_);

  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  EXPECT_EQ(output_obus_.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST_F(AudioElementGeneratorTest, GeneratesDemixingParameterDefinition) {
  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);
  audio_element_metadata_.at(0).set_num_parameters(1);
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
            num_subblocks: 1
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
  expected_default_demixing_info_parameter_data.reserved_default = 12;

  const AudioElementParam kExpectedAudioElementParam = {
      ParamDefinition::kParameterDefinitionDemixing,
      std::make_unique<DemixingParamDefinition>(
          expected_demixing_param_definition)};

  // Generate and validate the parameter-related information matches expected
  // results.
  AudioElementGenerator generator(audio_element_metadata_);
  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  const auto& obu = output_obus_.at(kAudioElementId).obu;
  EXPECT_EQ(obu.audio_element_params_.size(), 1);
  ASSERT_FALSE(obu.audio_element_params_.empty());
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_element_params_.front(),
            kExpectedAudioElementParam);
}

TEST_F(AudioElementGeneratorTest, MissingParamDefinitionTypeIsNotSupported) {
  AddTwoLayer7_1_0_And7_1_4(audio_element_metadata_);
  audio_element_metadata_.at(0).set_num_parameters(1);
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
  audio_element_metadata_.at(0).set_num_parameters(1);
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
  audio_element_metadata.set_num_parameters(1);
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
            num_subblocks: 1
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
      ParamDefinition::kParameterDefinitionReconGain,
      std::make_unique<ReconGainParamDefinition>(
          expected_recon_gain_param_definition)};

  // Generate and validate the parameter-related information matches expected
  // results.
  AudioElementGenerator generator(audio_element_metadata_);
  EXPECT_TRUE(generator.Generate(codec_config_obus_, output_obus_).ok());

  const auto& obu = output_obus_.at(kAudioElementId).obu;
  EXPECT_EQ(obu.audio_element_params_.size(), 1);
  ASSERT_FALSE(obu.audio_element_params_.empty());
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_element_params_.front(),
            kExpectedAudioElementParam);
}

}  // namespace
}  // namespace iamf_tools
