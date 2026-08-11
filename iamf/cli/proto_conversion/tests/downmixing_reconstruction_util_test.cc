/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/check.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer_manager.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::SizeIs;

using ProtoAudioFrameObuMetadata =
    ::iamf_tools_cli_proto::AudioFrameObuMetadata;
using ProtoChannelMetadata = ::iamf_tools_cli_proto::ChannelMetadata;
using ProtoChannelLabel = ::iamf_tools_cli_proto::ChannelLabel;
using AudioElementsById = DescriptorObus::AudioElementsById;

// Helper function to make an AudioElementWithData.
AudioElementWithData MakeAudioElement(
    DecodedUleb128 audio_element_id,
    const SubstreamIdLabelsMap& substream_id_to_labels = {},
    const LabelGainMap& label_to_output_gain = {},
    const CodecConfigObu* codec_config = nullptr,
    AudioElementObu::AudioElementType audio_element_type =
        AudioElementObu::kAudioElementChannelBased) {
  std::vector<DecodedUleb128> audio_substream_ids;
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    audio_substream_ids.push_back(substream_id);
  }

  auto obu = AudioElementObu::CreateForExtension(
      ObuHeader(), audio_element_id, audio_element_type,
      /*reserved=*/0,
      /*codec_config_id=*/0, audio_substream_ids, {});
  ABSL_CHECK_OK(obu);

  return {
      .obu = *std::move(obu),
      .codec_config = codec_config,
      .substream_id_to_labels = substream_id_to_labels,
      .label_to_output_gain = label_to_output_gain,
  };
}

// Helper function to make a UserMetadata proto.
ProtoAudioFrameObuMetadata MakeAudioFrameObuMetadata(
    uint32_t audio_element_id,
    const std::vector<std::pair<uint32_t, ProtoChannelLabel>>&
        ids_and_channels) {
  ProtoAudioFrameObuMetadata audio_frame_metadata;
  audio_frame_metadata.set_audio_element_id(audio_element_id);
  for (const auto& data : ids_and_channels) {
    ProtoChannelMetadata metadata;
    metadata.set_channel_id(data.first);
    metadata.set_channel_label(data.second);
    *audio_frame_metadata.add_channel_metadatas() = metadata;
  }
  return audio_frame_metadata;
}

TEST(CreateAudioElementIdToDownmixingConfig, EmptyInputsEmptyOutputs_IsOk) {
  absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
      id_to_config_map = CreateAudioElementIdToDownmixingConfig({}, {});

  EXPECT_THAT(id_to_config_map, IsOk());
  EXPECT_TRUE(id_to_config_map->empty());
}

TEST(CreateAudioElementIdToDownmixingConfig,
     AudioElementIdNotFound_ReturnsError) {
  // Create user metadata with an ID that does not exist in the audio elements.
  const DecodedUleb128 user_id = 2;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() =
      MakeAudioFrameObuMetadata(user_id, {});
  const DecodedUleb128 audio_element_id = 1;
  AudioElementsById audio_elements;
  audio_elements.emplace(audio_element_id, MakeAudioElement(audio_element_id));

  absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
      id_to_config_map =
          CreateAudioElementIdToDownmixingConfig(user_metadata, audio_elements);

  EXPECT_THAT(id_to_config_map, Not(IsOk()));
  EXPECT_THAT(id_to_config_map.status().message(), HasSubstr("not found"));
}

TEST(CreateAudioElementIdToDownmixingConfig, MustHaveConvertibleLabels) {
  const DecodedUleb128 element_id = 1;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{1, ProtoChannelLabel::CHANNEL_LABEL_L_2},
                   {2, ProtoChannelLabel::CHANNEL_LABEL_L_2}});
  AudioElementsById audio_elements;
  audio_elements.emplace(element_id, MakeAudioElement(element_id));

  absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
      id_to_config_map =
          CreateAudioElementIdToDownmixingConfig(user_metadata, audio_elements);

  EXPECT_THAT(id_to_config_map, Not(IsOk()));
  EXPECT_THAT(id_to_config_map.status().message(), HasSubstr("Duplicate"));
}

TEST(CreateAudioElementIdToDownmixingConfig, SucceedsWithValidInputs) {
  const DecodedUleb128 element_id = 1;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{1, ProtoChannelLabel::CHANNEL_LABEL_L_2},
                   {2, ProtoChannelLabel::CHANNEL_LABEL_R_2}});
  AudioElementsById audio_elements;
  audio_elements.emplace(element_id, MakeAudioElement(element_id));

  absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
      id_to_config_map =
          CreateAudioElementIdToDownmixingConfig(user_metadata, audio_elements);

  EXPECT_THAT(id_to_config_map, IsOk());
}

TEST(CreateAudioElementIdToDownmixingConfig,
     CopiesSubStreamIdToLabelsAndOutputGains) {
  const DecodedUleb128 element_id = 1;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{1, ProtoChannelLabel::CHANNEL_LABEL_L_2},
                   {2, ProtoChannelLabel::CHANNEL_LABEL_R_2}});
  AudioElementsById audio_elements;
  // Arbitrary values in the `substream_id_to_labels` and `label_to_output_gain`
  SubstreamIdLabelsMap substream_id_to_labels = {
      {34, {ChannelLabel::Label::kA11}},
      {35, {ChannelLabel::Label::kLrs7, ChannelLabel::Label::kA24}}};
  LabelGainMap label_to_output_gain = {{ChannelLabel::Label::kLrs7, 420.0},
                                       {ChannelLabel::Label::kA24, 555.0}};
  audio_elements.emplace(element_id,
                         MakeAudioElement(element_id, substream_id_to_labels,
                                          label_to_output_gain));

  absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
      id_to_config_map =
          CreateAudioElementIdToDownmixingConfig(user_metadata, audio_elements);
  EXPECT_THAT(id_to_config_map, IsOk());
  EXPECT_THAT(id_to_config_map->at(element_id).substream_id_to_labels,
              ContainerEq(substream_id_to_labels));
  EXPECT_THAT(id_to_config_map->at(element_id).label_to_output_gain,
              ContainerEq(label_to_output_gain));
}

iamf_tools_cli_proto::UserMetadata MakeAmbisonicsPresetUserMetadata(
    DecodedUleb128 element_id, iamf_tools_cli_proto::AmbisonicsPreset preset) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  auto* element_metadata = user_metadata.add_audio_element_metadata();
  element_metadata->set_audio_element_id(element_id);
  element_metadata->mutable_ambisonics_config()
      ->mutable_ambisonics_preset_config()
      ->set_ambisonics_preset(preset);
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{0, ProtoChannelLabel::CHANNEL_LABEL_A_0}});
  return user_metadata;
}

TEST(CreateAudioElementIdToDownmixingConfig, AmbisonicsPresetReturnsDownmixer) {
  constexpr DecodedUleb128 kAudioElementId = 10;
  constexpr uint32_t kCodecConfigId = 1;
  const auto user_metadata = MakeAmbisonicsPresetUserMetadata(
      kAudioElementId,
      iamf_tools_cli_proto::AMBISONICS_PRESET_BEST_PRACTICE_FOR_ORDER3);
  DescriptorObus::CodecConfigsById codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  AudioElementsById audio_elements;
  audio_elements.emplace(
      kAudioElementId,
      MakeAudioElement(kAudioElementId, {}, {}, &codec_config_obus.at(1),
                       AudioElementObu::kAudioElementSceneBased));

  auto id_to_config_map =
      CreateAudioElementIdToDownmixingConfig(user_metadata, audio_elements);

  ASSERT_THAT(id_to_config_map, IsOk());
  EXPECT_THAT(id_to_config_map->at(kAudioElementId).down_mixers, SizeIs(1));
}

TEST(CreateAudioElementIdToDownmixingConfig,
     AmbisonicsPresetWithoutCodecConfigReturnsError) {
  constexpr DecodedUleb128 kAudioElementId = 10;
  const auto user_metadata = MakeAmbisonicsPresetUserMetadata(
      kAudioElementId,
      iamf_tools_cli_proto::AMBISONICS_PRESET_BEST_PRACTICE_FOR_ORDER1);
  AudioElementsById audio_elements;
  const CodecConfigObu* kMissingCodecConfig = nullptr;
  audio_elements.emplace(
      kAudioElementId,
      MakeAudioElement(kAudioElementId, {}, {}, kMissingCodecConfig,
                       AudioElementObu::kAudioElementSceneBased));

  EXPECT_THAT(
      CreateAudioElementIdToDownmixingConfig(user_metadata, audio_elements),
      Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
