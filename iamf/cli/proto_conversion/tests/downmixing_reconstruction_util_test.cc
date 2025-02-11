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
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ContainerEq;
using ::testing::HasSubstr;
using ::testing::Not;

using ProtoAudioFrameObuMetadata =
    ::iamf_tools_cli_proto::AudioFrameObuMetadata;
using ProtoChannelMetadata = ::iamf_tools_cli_proto::ChannelMetadata;
using ProtoChannelLabel = ::iamf_tools_cli_proto::ChannelLabel;

// Helper function to make an AudioElementWithData.
AudioElementWithData MakeAudioElement(
    DecodedUleb128 audio_element_id,
    const SubstreamIdLabelsMap& substream_id_to_labels = {},
    const LabelGainMap& label_to_output_gain = {}) {
  return {
      .obu = AudioElementObu(ObuHeader(), audio_element_id,
                             AudioElementObu::kAudioElementChannelBased,
                             /*reserved=*/0,
                             /*codec_config_id=*/0),
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

TEST(CreateAudioElementIdToDemixingMetadata, EmptyInputsEmptyOutputs_IsOk) {
  absl::StatusOr<absl::flat_hash_map<
      DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
      id_to_config_map = CreateAudioElementIdToDemixingMetadata({}, {});

  EXPECT_THAT(id_to_config_map, IsOk());
  EXPECT_TRUE(id_to_config_map->empty());
}

TEST(CreateAudioElementIdToDemixingMetadata,
     AudioElementIdNotFound_ReturnsError) {
  // Create user metadata with an ID that does not exist in the audio elements.
  const DecodedUleb128 user_id = 2;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() =
      MakeAudioFrameObuMetadata(user_id, {});
  const DecodedUleb128 audio_element_id = 1;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  audio_elements.emplace(audio_element_id, MakeAudioElement(audio_element_id));

  absl::StatusOr<absl::flat_hash_map<
      DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
      id_to_config_map =
          CreateAudioElementIdToDemixingMetadata(user_metadata, audio_elements);

  EXPECT_THAT(id_to_config_map, Not(IsOk()));
  EXPECT_THAT(id_to_config_map.status().message(), HasSubstr("not found"));
}

TEST(CreateAudioElementIdToDemixingMetadata, MustHaveConvertibleLabels) {
  const DecodedUleb128 element_id = 1;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{1, ProtoChannelLabel::CHANNEL_LABEL_L_2},
                   {2, ProtoChannelLabel::CHANNEL_LABEL_L_2}});
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  audio_elements.emplace(element_id, MakeAudioElement(element_id));

  absl::StatusOr<absl::flat_hash_map<
      DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
      id_to_config_map =
          CreateAudioElementIdToDemixingMetadata(user_metadata, audio_elements);

  EXPECT_THAT(id_to_config_map, Not(IsOk()));
  EXPECT_THAT(id_to_config_map.status().message(), HasSubstr("Duplicate"));
}

TEST(CreateAudioElementIdToDemixingMetadata, SucceedsWithValidInputs) {
  const DecodedUleb128 element_id = 1;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{1, ProtoChannelLabel::CHANNEL_LABEL_L_2},
                   {2, ProtoChannelLabel::CHANNEL_LABEL_R_2}});
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  audio_elements.emplace(element_id, MakeAudioElement(element_id));

  absl::StatusOr<absl::flat_hash_map<
      DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
      id_to_config_map =
          CreateAudioElementIdToDemixingMetadata(user_metadata, audio_elements);

  EXPECT_THAT(id_to_config_map, IsOk());
}

TEST(CreateAudioElementIdToDemixingMetadata,
     CopiesSubStreamIdToLabelsAndOutputGains) {
  const DecodedUleb128 element_id = 1;
  iamf_tools_cli_proto::UserMetadata user_metadata;
  *user_metadata.add_audio_frame_metadata() = MakeAudioFrameObuMetadata(
      element_id, {{1, ProtoChannelLabel::CHANNEL_LABEL_L_2},
                   {2, ProtoChannelLabel::CHANNEL_LABEL_R_2}});
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  // Arbitrary values in the `substream_id_to_labels` and `label_to_output_gain`
  SubstreamIdLabelsMap substream_id_to_labels = {
      {34, {ChannelLabel::Label::kA11}},
      {35, {ChannelLabel::Label::kLrs7, ChannelLabel::Label::kA24}}};
  LabelGainMap label_to_output_gain = {{ChannelLabel::Label::kLrs7, 420.0},
                                       {ChannelLabel::Label::kA24, 555.0}};
  audio_elements.emplace(element_id,
                         MakeAudioElement(element_id, substream_id_to_labels,
                                          label_to_output_gain));

  absl::StatusOr<absl::flat_hash_map<
      DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
      id_to_config_map =
          CreateAudioElementIdToDemixingMetadata(user_metadata, audio_elements);
  EXPECT_THAT(id_to_config_map, IsOk());
  EXPECT_THAT(id_to_config_map->at(element_id).substream_id_to_labels,
              ContainerEq(substream_id_to_labels));
  EXPECT_THAT(id_to_config_map->at(element_id).label_to_output_gain,
              ContainerEq(label_to_output_gain));
}

}  // namespace
}  // namespace iamf_tools
