/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/user_metadata_builder/audio_element_metadata_builder.h"

#include <cstdint>
#include <limits>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

constexpr uint32_t kLargeAudioElementId = std::numeric_limits<uint32_t>::max();
constexpr uint32_t kAudioElementId = 999;
constexpr uint32_t kCodecConfigId = 1000;

TEST(PopulateAudioElementMetadata, SetsAudioElementId) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  audio_element_metadata),
              IsOk());

  EXPECT_EQ(audio_element_metadata.audio_element_id(), kAudioElementId);
}

TEST(PopulateAudioElementMetadata, SetsLargeAudioElementId) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kLargeAudioElementId, kCodecConfigId,
                  IamfInputLayout::kStereo, audio_element_metadata),
              IsOk());

  EXPECT_EQ(audio_element_metadata.audio_element_id(), kLargeAudioElementId);
}

TEST(PopulateAudioElementMetadata, SetsCodecConfigId) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  audio_element_metadata),
              IsOk());

  EXPECT_EQ(audio_element_metadata.codec_config_id(), kCodecConfigId);
}

TEST(PopulateAudioElementMetadata, ConfiguresStereo) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  audio_element_metadata),
              IsOk());

  EXPECT_EQ(audio_element_metadata.audio_substream_ids().size(), 1);
  EXPECT_EQ(audio_element_metadata.audio_substream_ids(0), 0);
  ASSERT_TRUE(audio_element_metadata.has_scalable_channel_layout_config());

  EXPECT_EQ(audio_element_metadata.scalable_channel_layout_config()
                .channel_audio_layer_configs()
                .size(),
            1);
  const auto& channel_audio_layer_configs =
      audio_element_metadata.scalable_channel_layout_config()
          .channel_audio_layer_configs(0);
  EXPECT_EQ(channel_audio_layer_configs.loudspeaker_layout(),
            iamf_tools_cli_proto::LOUDSPEAKER_LAYOUT_STEREO);

  EXPECT_FALSE(channel_audio_layer_configs.output_gain_is_present_flag());
  EXPECT_FALSE(channel_audio_layer_configs.recon_gain_is_present_flag());
  EXPECT_EQ(channel_audio_layer_configs.substream_count(), 1);
  EXPECT_EQ(channel_audio_layer_configs.coupled_substream_count(), 1);
  EXPECT_FALSE(channel_audio_layer_configs.output_gain_flag());
}

TEST(PopulateAudioElementMetadata, ConfiguresLoudspeakerLayoutForBinaural) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kBinaural,
                  audio_element_metadata),
              IsOk());
  ASSERT_TRUE(audio_element_metadata.has_scalable_channel_layout_config());

  EXPECT_EQ(audio_element_metadata.scalable_channel_layout_config()
                .channel_audio_layer_configs(0)
                .loudspeaker_layout(),
            iamf_tools_cli_proto::LOUDSPEAKER_LAYOUT_BINAURAL);
}

TEST(PopulateAudioElementMetadata, ConfiguresLFE) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kLFE,
                  audio_element_metadata),
              IsOk());

  EXPECT_EQ(audio_element_metadata.audio_substream_ids().size(), 1);
  EXPECT_EQ(audio_element_metadata.audio_substream_ids().at(0), 0);
  ASSERT_TRUE(audio_element_metadata.has_scalable_channel_layout_config());
  EXPECT_EQ(audio_element_metadata.scalable_channel_layout_config()
                .channel_audio_layer_configs()
                .size(),
            1);
  const auto& first_channel_audio_layer_config =
      audio_element_metadata.scalable_channel_layout_config()
          .channel_audio_layer_configs(0);
  EXPECT_EQ(first_channel_audio_layer_config.loudspeaker_layout(),
            iamf_tools_cli_proto::LOUDSPEAKER_LAYOUT_EXPANDED);
  EXPECT_EQ(first_channel_audio_layer_config.expanded_loudspeaker_layout(),
            iamf_tools_cli_proto::EXPANDED_LOUDSPEAKER_LAYOUT_LFE);
  EXPECT_EQ(first_channel_audio_layer_config.substream_count(), 1);
  EXPECT_EQ(first_channel_audio_layer_config.coupled_substream_count(), 0);
}

TEST(PopulateAudioElementMetadata, ConfiguresFirstOrderAmbisonics) {
  iamf_tools_cli_proto::AudioElementObuMetadata audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;

  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId,
                  IamfInputLayout::kAmbisonicsOrder1, audio_element_metadata),
              IsOk());

  EXPECT_EQ(audio_element_metadata.audio_substream_ids().size(), 4);
  EXPECT_EQ(audio_element_metadata.audio_substream_ids().at(0), 0);
  EXPECT_EQ(audio_element_metadata.audio_substream_ids().at(1), 1);
  EXPECT_EQ(audio_element_metadata.audio_substream_ids().at(2), 2);
  EXPECT_EQ(audio_element_metadata.audio_substream_ids().at(3), 3);
  ASSERT_TRUE(audio_element_metadata.has_ambisonics_config());

  EXPECT_EQ(audio_element_metadata.ambisonics_config().ambisonics_mode(),
            iamf_tools_cli_proto::AMBISONICS_MODE_MONO);
  ASSERT_TRUE(
      audio_element_metadata.ambisonics_config().has_ambisonics_mono_config());
  const auto& ambisonics_mono_config =
      audio_element_metadata.ambisonics_config().ambisonics_mono_config();
  EXPECT_EQ(ambisonics_mono_config.output_channel_count(), 4);
  EXPECT_EQ(ambisonics_mono_config.substream_count(), 4);
  EXPECT_EQ(ambisonics_mono_config.channel_mapping(0), 0);
  EXPECT_EQ(ambisonics_mono_config.channel_mapping(1), 1);
  EXPECT_EQ(ambisonics_mono_config.channel_mapping(2), 2);
  EXPECT_EQ(ambisonics_mono_config.channel_mapping(3), 3);
}

TEST(PopulateAudioElementMetadata, GeneratesUniqueSubstreamIds) {
  iamf_tools_cli_proto::AudioElementObuMetadata first_audio_element_metadata;
  iamf_tools_cli_proto::AudioElementObuMetadata second_audio_element_metadata;
  AudioElementMetadataBuilder audio_element_metadata_builder;
  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId, kCodecConfigId, IamfInputLayout::kStereo,
                  first_audio_element_metadata),
              IsOk());
  EXPECT_THAT(audio_element_metadata_builder.PopulateAudioElementMetadata(
                  kAudioElementId + 1, kCodecConfigId, IamfInputLayout::kStereo,
                  second_audio_element_metadata),
              IsOk());

  EXPECT_EQ(first_audio_element_metadata.audio_substream_ids(0), 0);
  EXPECT_EQ(second_audio_element_metadata.audio_substream_ids(0), 1);
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
