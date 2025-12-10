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

#include <array>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
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
using ::google::protobuf::TextFormat;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::FloatEq;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using enum ChannelLabel::Label;

using AudioElementObuMetadata = ::iamf_tools_cli_proto::AudioElementObuMetadata;
using AudioElementObuMetadatas =
    ::google::protobuf::RepeatedPtrField<AudioElementObuMetadata>;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr uint32_t kSampleRate = 48000;

constexpr DecodedUleb128 kMonoSubstreamId = 99;
constexpr DecodedUleb128 kL2SubstreamId = 100;

template <typename T>
const T& GetConfigForAudioElementIdExpectOk(
    DecodedUleb128 audio_element_id,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        output_obus) {
  EXPECT_TRUE(output_obus.contains(audio_element_id));
  const T* config =
      std::get_if<T>(&output_obus.at(audio_element_id).obu.config_);
  EXPECT_NE(config, nullptr);
  return *config;
}

void FillFirstOrderAmbisonicsMetadata(
    AudioElementObuMetadata& audio_element_metadata) {
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        audio_substream_ids: [ 0, 1, 2, 3 ]
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 4
            channel_mapping: [ 0, 1, 2, 3 ]
          }
        }
      )pb",
      &audio_element_metadata));
  audio_element_metadata.set_audio_element_id(kAudioElementId);
  audio_element_metadata.set_codec_config_id(kCodecConfigId);
}

void FillObjectsMetadata(AudioElementObuMetadata& audio_element_metadata) {
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_type: AUDIO_ELEMENT_OBJECT_BASED
        reserved: 0
        audio_substream_ids: [ 0 ]
        objects_config { num_objects: 1 objects_config_extension_bytes: "1234" }
      )pb",
      &audio_element_metadata));
  audio_element_metadata.set_audio_element_id(kAudioElementId);
  audio_element_metadata.set_codec_config_id(kCodecConfigId);
}

void FillTwoLayerStereoMetadata(
    AudioElementObuMetadata& audio_element_metadata) {
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        scalable_channel_layout_config {
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
      &audio_element_metadata));
  audio_element_metadata.set_audio_element_id(kAudioElementId);
  audio_element_metadata.set_codec_config_id(kCodecConfigId);
  audio_element_metadata.mutable_audio_substream_ids()->Add(kMonoSubstreamId);
  audio_element_metadata.mutable_audio_substream_ids()->Add(kL2SubstreamId);
}

TEST(Generate, PopulatesExpandedLoudspeakerLayout) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
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
      GetConfigForAudioElementIdExpectOk<ScalableChannelLayoutConfig>(
          kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_EQ(output_first_layer.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  ASSERT_TRUE(output_first_layer.expanded_loudspeaker_layout.has_value());
  EXPECT_EQ(*output_first_layer.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutLFE);
}

TEST(Generate, PopulatesExpandedLayoutBottom3Ch) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99, 100 ]
        scalable_channel_layout_config {
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_EXPANDED
            substream_count: 2
            coupled_substream_count: 1
            expanded_loudspeaker_layout: EXPANDED_LOUDSPEAKER_LAYOUT_BOTTOM_3_CH
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
      GetConfigForAudioElementIdExpectOk<ScalableChannelLayoutConfig>(
          kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_EQ(output_first_layer.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  ASSERT_TRUE(output_first_layer.expanded_loudspeaker_layout.has_value());
  EXPECT_EQ(*output_first_layer.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch);
}

TEST(Generate, PopulatesExpandedLayoutTop1Ch) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
          channel_audio_layer_configs {
            loudspeaker_layout: LOUDSPEAKER_LAYOUT_EXPANDED
            substream_count: 1
            coupled_substream_count: 0
            expanded_loudspeaker_layout: EXPANDED_LOUDSPEAKER_LAYOUT_TOP_1_CH
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
      GetConfigForAudioElementIdExpectOk<ScalableChannelLayoutConfig>(
          kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_EQ(output_first_layer.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  ASSERT_TRUE(output_first_layer.expanded_loudspeaker_layout.has_value());
  EXPECT_EQ(*output_first_layer.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutTop1Ch);
}

TEST(Generate, InvalidWhenExpandedLoudspeakerLayoutIsSignalledButNotPresent) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
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
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
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
      GetConfigForAudioElementIdExpectOk<ScalableChannelLayoutConfig>(
          kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_FALSE(output_first_layer.expanded_loudspeaker_layout.has_value());
}

TEST(Generate, LeavesExpandedLayoutEmptyWhenNotSignalled) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
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
      GetConfigForAudioElementIdExpectOk<ScalableChannelLayoutConfig>(
          kAudioElementId, output_obus)
          .channel_audio_layer_configs[0];
  EXPECT_FALSE(output_first_layer.expanded_loudspeaker_layout.has_value());
}

TEST(Generate, NoAudioElementObus) {
  AudioElementObuMetadatas audio_element_metadatas;
  AudioElementGenerator generator(audio_element_metadatas);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;

  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_TRUE(output_obus.empty());
}

TEST(Generate, GeneratesObjectsConfig) {
  AudioElementObuMetadatas audio_element_metadatas;
  FillObjectsMetadata(*audio_element_metadatas.Add());
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  const auto& audio_element_with_data = output_obus.at(kAudioElementId);
  EXPECT_THAT(audio_element_with_data.obu.GetAudioElementType(),
              AudioElementObu::kAudioElementObjectBased);
  EXPECT_THAT(audio_element_with_data.obu.audio_substream_ids_, ElementsAre(0));
  const auto& objects_config =
      GetConfigForAudioElementIdExpectOk<ObjectsConfig>(kAudioElementId,
                                                        output_obus);
  EXPECT_EQ(objects_config.num_objects, 1);
  EXPECT_THAT(objects_config.objects_config_extension_bytes,
              ElementsAre('1', '2', '3', '4'));
}

TEST(Generate, InvalidObjectsConfigWithMultipleSubstreams) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto audio_element_metadata = *audio_element_metadatas.Add();
  FillObjectsMetadata(audio_element_metadata);
  audio_element_metadata.mutable_audio_substream_ids()->Add(kMonoSubstreamId);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), Not(IsOk()));
}

TEST(Generate, GeneratesFirstOrderAmbisonics) {
  AudioElementObuMetadatas audio_element_metadatas;
  FillFirstOrderAmbisonicsMetadata(*audio_element_metadatas.Add());
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  const auto& audio_element_with_data = output_obus.at(kAudioElementId);
  EXPECT_THAT(audio_element_with_data.obu.GetAudioElementType(),
              AudioElementObu::kAudioElementSceneBased);
  EXPECT_THAT(audio_element_with_data.obu.audio_substream_ids_,
              ElementsAre(0, 1, 2, 3));
  EXPECT_EQ(
      audio_element_with_data.substream_id_to_labels,
      SubstreamIdLabelsMap({{0, {kA0}}, {1, {kA1}}, {2, {kA2}}, {3, {kA3}}}));
  const auto& ambisonics_config =
      GetConfigForAudioElementIdExpectOk<AmbisonicsConfig>(kAudioElementId,
                                                           output_obus);
  EXPECT_EQ(ambisonics_config.ambisonics_mode,
            AmbisonicsConfig::kAmbisonicsModeMono);
  const auto* ambisonics_mono_config =
      std::get_if<AmbisonicsMonoConfig>(&ambisonics_config.ambisonics_config);
  ASSERT_NE(ambisonics_mono_config, nullptr);
  EXPECT_EQ(ambisonics_mono_config->output_channel_count, 4);
  EXPECT_EQ(ambisonics_mono_config->substream_count, 4);
  EXPECT_THAT(ambisonics_mono_config->channel_mapping, ElementsAre(0, 1, 2, 3));
}

TEST(Generate, FirstOrderMonoAmbisonicsLargeSubstreamIds) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 1000, 2000, 3000, 4000 ]
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
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {1000, {kA0}}, {2000, {kA1}}, {3000, {kA2}}, {4000, {kA3}}};
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_EQ(output_obus.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST(Generate, FirstOrderMonoAmbisonicsArbitraryOrder) {
  AudioElementObuMetadatas audio_element_metadatas;

  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 100, 101, 102, 103 ]
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 4
            channel_mapping: [ 3, 1, 0, 2 ]
          }
        }
      )pb",
      audio_element_metadatas.Add()));
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {103, {kA0}}, {101, {kA1}}, {100, {kA2}}, {102, {kA3}}};
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_EQ(output_obus.at(kAudioElementId).substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
}

TEST(Generate, SubstreamWithMultipleAmbisonicsChannelNumbers) {
  AudioElementObuMetadatas audio_element_metadatas;

  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 100, 101, 102 ]
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 3
            channel_mapping: [ 0, 2, 1, 0 ]
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

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_EQ(
      output_obus.at(kAudioElementId).substream_id_to_labels,
      SubstreamIdLabelsMap({{100, {kA0, kA3}}, {102, {kA1}}, {101, {kA2}}}));
}

TEST(Generate, MixedFirstOrderMonoAmbisonics) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 1000, 2000, 3000 ]
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 4
            substream_count: 3
            channel_mapping: [ 0, 1, 2, 255 ]
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

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_EQ(
      output_obus.at(kAudioElementId).substream_id_to_labels,
      SubstreamIdLabelsMap({{1000, {kA0}}, {2000, {kA1}}, {3000, {kA2}}}));
}

TEST(Generate, ThirdOrderMonoAmbisonics) {
  AudioElementObuMetadatas audio_element_metadatas;
  constexpr std::array<uint32_t, 16> kSubstreamIds = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  constexpr std::array<uint8_t, 16> kChannelMapping = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_SCENE_BASED
        reserved: 0
        codec_config_id: 200
        ambisonics_config {
          ambisonics_mode: AMBISONICS_MODE_MONO
          ambisonics_mono_config {
            output_channel_count: 16
            substream_count: 16
          }
        }
      )pb",
      &audio_element_metadata));
  audio_element_metadata.mutable_audio_substream_ids()->Add(
      kSubstreamIds.begin(), kSubstreamIds.end());
  audio_element_metadata.mutable_ambisonics_config()
      ->mutable_ambisonics_mono_config()
      ->mutable_channel_mapping()
      ->Add(kChannelMapping.begin(), kChannelMapping.end());
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_EQ(output_obus.at(kAudioElementId).substream_id_to_labels,
            SubstreamIdLabelsMap({{0, {kA0}},
                                  {1, {kA1}},
                                  {2, {kA2}},
                                  {3, {kA3}},
                                  {4, {kA4}},
                                  {5, {kA5}},
                                  {6, {kA6}},
                                  {7, {kA7}},
                                  {8, {kA8}},
                                  {9, {kA9}},
                                  {10, {kA10}},
                                  {11, {kA11}},
                                  {12, {kA12}},
                                  {13, {kA13}},
                                  {14, {kA14}},
                                  {15, {kA15}}}));
}

TEST(Generate, FillsAudioElementWithDataFields) {
  AudioElementObuMetadatas audio_element_metadatas;
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 1, .lfe = 0, .height = 0, .bottom = 0},
      {.surround = 2, .lfe = 0, .height = 0, .bottom = 0}};
  FillTwoLayerStereoMetadata(*audio_element_metadatas.Add());
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  AudioElementGenerator generator(audio_element_metadatas);
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());
  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  const auto& audio_element_with_data = output_obus.at(kAudioElementId);
  EXPECT_EQ(audio_element_with_data.substream_id_to_labels,
            kExpectedSubstreamIdToLabels);
  EXPECT_EQ(audio_element_with_data.channel_numbers_for_layers,
            kExpectedChannelNumbersForLayer);
  EXPECT_THAT(audio_element_with_data.label_to_output_gain,
              Contains(Pair(kL2, FloatEq(128.0 - 1 / 256.0))));
}

TEST(Generate, DeprecatedLoudspeakerLayoutIsNotSupported) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
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
      audio_element_metadatas.Add()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  AudioElementGenerator generator(audio_element_metadatas);
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), Not(IsOk()));
  EXPECT_TRUE(output_obus.empty());
}

TEST(Generate, DefaultLoudspeakerLayoutIsNotSupported) {
  AudioElementObuMetadatas audio_element_metadatas;
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        audio_substream_ids: [ 99 ]
        scalable_channel_layout_config {
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
      audio_element_metadatas.Add()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  AudioElementGenerator generator(audio_element_metadatas);

  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), Not(IsOk()));
  EXPECT_TRUE(output_obus.empty());
}

void FillTwoLayer7_1_0_And7_1_4(
    AudioElementObuMetadata& audio_element_metadata) {
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        audio_substream_ids: [ 700, 701, 702, 703, 704, 740, 741 ]
        scalable_channel_layout_config {
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
      &audio_element_metadata));
  audio_element_metadata.set_audio_element_id(kAudioElementId);
  audio_element_metadata.set_codec_config_id(kCodecConfigId);
}

TEST(Generate, GeneratesDemixingParameterDefinition) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillTwoLayer7_1_0_And7_1_4(audio_element_metadata);
  ASSERT_TRUE(TextFormat::ParseFromString(
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
      audio_element_metadata.add_audio_element_params()));
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
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_THAT(
      output_obus.at(kAudioElementId).obu.audio_element_params_,
      ElementsAre(AudioElementParam{expected_demixing_param_definition}));
}

TEST(Generate, MissingParamDefinitionTypeIsNotSupported) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillTwoLayer7_1_0_And7_1_4(audio_element_metadata);
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        # `param_definition_type` is omitted.
        # param_definition_type: PARAM_DEFINITION_TYPE_DEMIXING
      )pb",
      audio_element_metadata.add_audio_element_params()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), Not(IsOk()));
  EXPECT_TRUE(output_obus.empty());
}

TEST(Generate, DeprecatedParamDefinitionTypeIsNotSupported) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillTwoLayer7_1_0_And7_1_4(audio_element_metadata);
  ASSERT_TRUE(TextFormat::ParseFromString(
      R"pb(
        deprecated_param_definition_type: 1  # PARAMETER_DEFINITION_DEMIXING
      )pb",
      audio_element_metadata.add_audio_element_params()));
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;

  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), Not(IsOk()));
  EXPECT_TRUE(output_obus.empty());
}

TEST(Generate, GeneratesReconGainParameterDefinition) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillTwoLayer7_1_0_And7_1_4(audio_element_metadata);
  // Reconfigure the audio element to add a recon gain parameter.
  audio_element_metadata.mutable_scalable_channel_layout_config()
      ->mutable_channel_audio_layer_configs(1)
      ->set_recon_gain_is_present_flag(true);
  ASSERT_TRUE(TextFormat::ParseFromString(
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

  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  // Recon gain requires an associated lossy codec (e.g. Opus or AAC).
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  AudioElementGenerator generator(audio_element_metadatas);

  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  EXPECT_THAT(
      output_obus.at(kAudioElementId).obu.audio_element_params_,
      ElementsAre(AudioElementParam{expected_recon_gain_param_definition}));
}

TEST(Generate, IgnoresDeprecatedNumSubstreamsField) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillFirstOrderAmbisonicsMetadata(audio_element_metadata);
  // Normally first-order ambisonics has four substreams.
  constexpr DecodedUleb128 kExpectedNumSubstreams = 4;
  // Corrupt the `num_substreams` field.
  const auto kIgnoredNumSubstreams = 9999;
  audio_element_metadata.set_num_substreams(kIgnoredNumSubstreams);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  AudioElementGenerator generator(audio_element_metadatas);

  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  const auto& audio_element_obu = output_obus.at(kAudioElementId).obu;
  // The field is deprecated and ignored, the actual number of substreams are
  // set based on the `audio_substream_ids` field.
  EXPECT_EQ(audio_element_obu.GetNumSubstreams(), kExpectedNumSubstreams);
  EXPECT_EQ(audio_element_obu.audio_substream_ids_.size(),
            kExpectedNumSubstreams);
}

TEST(Generate, IgnoresDeprecatedNumParametersField) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillFirstOrderAmbisonicsMetadata(audio_element_metadata);
  constexpr DecodedUleb128 kExpectedNumParameters = 0;
  // Corrupt the `num_parameters` field.
  const auto kIgnoredNumParameters = 9999;
  audio_element_metadata.set_num_parameters(kIgnoredNumParameters);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  const auto& audio_element_obu = output_obus.at(kAudioElementId).obu;
  // The field is deprecated and ignored, the actual number of parameters are
  // set based on the `audio_element_params` field.
  EXPECT_EQ(audio_element_obu.GetNumParameters(), kExpectedNumParameters);
  EXPECT_EQ(audio_element_obu.audio_element_params_.size(),
            kExpectedNumParameters);
}

TEST(Generate, IgnoresDeprecatedParamDefinitionSizeField) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& audio_element_metadata = *audio_element_metadatas.Add();
  FillFirstOrderAmbisonicsMetadata(audio_element_metadata);
  auto* audio_element_param =
      audio_element_metadata.mutable_audio_element_params()->Add();
  audio_element_param->set_param_definition_type(
      iamf_tools_cli_proto::PARAM_DEFINITION_TYPE_RESERVED_255);
  // Corrupt the `num_parameters` field.
  constexpr absl::string_view kParamDefinitionBytes = "abc";
  const auto kInconsistentParamDefinitionSize = 9999;
  audio_element_param->mutable_param_definition_extension()
      ->set_param_definition_size(kInconsistentParamDefinitionSize);
  audio_element_param->mutable_param_definition_extension()
      ->set_param_definition_bytes(kParamDefinitionBytes);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  EXPECT_THAT(output_obus, UnorderedElementsAre(Key(kAudioElementId)));
  // The field is deprecated and ignored, the actual number of parameters are
  // set based on the `param_definition_bytes` field.
  ExtendedParamDefinition expected_extended_param_definition(
      ParamDefinition::kParameterDefinitionReservedEnd);
  expected_extended_param_definition.param_definition_bytes_ = {
      kParamDefinitionBytes.begin(), kParamDefinitionBytes.end()};
  EXPECT_THAT(
      output_obus.at(kAudioElementId).obu.audio_element_params_,
      ElementsAre(AudioElementParam{expected_extended_param_definition}));
}

TEST(Generate, IgnoresDeprecatedNumLayers) {
  AudioElementObuMetadatas audio_element_metadatas;
  auto& two_layer_stereo_metadata = *audio_element_metadatas.Add();
  FillTwoLayerStereoMetadata(two_layer_stereo_metadata);
  // Two layers are set in the metadata.
  constexpr uint8_t kExpectedNumLayers = 2;
  // Corrupt the `num_layers` field.
  const auto kIgnoredNumLayers = 7;
  two_layer_stereo_metadata.mutable_scalable_channel_layout_config()
      ->set_num_layers(kIgnoredNumLayers);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AudioElementGenerator generator(audio_element_metadatas);

  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> output_obus;
  EXPECT_THAT(generator.Generate(codec_config_obus, output_obus), IsOk());

  // The corrupted value is ignored, and the actual number of parameters are set
  // correctly.
  const auto& scalable_channel_layout_config =
      GetConfigForAudioElementIdExpectOk<ScalableChannelLayoutConfig>(
          kAudioElementId, output_obus);
  EXPECT_EQ(scalable_channel_layout_config.GetNumLayers(), kExpectedNumLayers);
  EXPECT_EQ(scalable_channel_layout_config.channel_audio_layer_configs.size(),
            kExpectedNumLayers);
}

}  // namespace
}  // namespace iamf_tools
