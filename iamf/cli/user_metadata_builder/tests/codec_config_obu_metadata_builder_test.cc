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

#include "iamf/cli/user_metadata_builder/codec_config_obu_metadata_builder.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto_to_obu/codec_config_generator.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr uint32_t kOpusNumSamplesPerFrame = 960;
constexpr uint32_t kLpcmSampleRate = 16000;
constexpr uint8_t kLpcmSampleSize = 16;
constexpr int64_t kLpcmNumSamplesPerFrame = 64;

constexpr bool kAutomaticallyOverrideAudioRollDistance = true;
constexpr bool kAutomaticallyOverrideCodecDelay = true;

void ExpectGeneratingCodecConfigObuSucceeds(
    const iamf_tools_cli_proto::CodecConfigObuMetadata&
        codec_config_obu_metadata) {
  ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::CodecConfigObuMetadata>
      codec_config_obu_metadatas;
  *codec_config_obu_metadatas.Add() = codec_config_obu_metadata;

  absl::flat_hash_map<uint32_t, CodecConfigObu> output_obus;
  EXPECT_THAT(
      CodecConfigGenerator(codec_config_obu_metadatas).Generate(output_obus),
      IsOk());
}

TEST(GetLpcmCodecConfigObuMetadata, OutputHasRequestedValues) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
          kCodecConfigId, kLpcmNumSamplesPerFrame, kLpcmSampleSize,
          kLpcmSampleRate);

  EXPECT_EQ(codec_config_obu_metadata.codec_config_id(), kCodecConfigId);
  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.num_samples_per_frame(), kLpcmNumSamplesPerFrame);
  const auto& decoder_config_lpcm = codec_config.decoder_config_lpcm();
  EXPECT_EQ(decoder_config_lpcm.sample_size(), kLpcmSampleSize);
  EXPECT_EQ(decoder_config_lpcm.sample_rate(), kLpcmSampleRate);
}

TEST(GetLpcmCodecConfigObuMetadata, OutputHasReasonableDefaults) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
          kCodecConfigId, kLpcmNumSamplesPerFrame, kLpcmSampleSize,
          kLpcmSampleRate);

  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.codec_id(), iamf_tools_cli_proto::CODEC_ID_LPCM);
  EXPECT_EQ(codec_config.decoder_config_lpcm().sample_format_flags(),
            iamf_tools_cli_proto::LPCM_LITTLE_ENDIAN);
}

TEST(GetLpcmCodecConfigObuMetadata, UsesAutomaticOverrideFields) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
          kCodecConfigId, kLpcmNumSamplesPerFrame, kLpcmSampleSize,
          kLpcmSampleRate);

  // Ensure the automatic configuration fields are set, instead of having to
  // consider specific required values based on the codec.
  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.automatically_override_audio_roll_distance(),
            kAutomaticallyOverrideAudioRollDistance);
  EXPECT_EQ(codec_config.automatically_override_codec_delay(),
            kAutomaticallyOverrideCodecDelay);
}

TEST(FillLpcmCodecConfigObuMetadata, IsCompatibleWithCodecConfigGenerator) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetLpcmCodecConfigObuMetadata(
          kCodecConfigId, kLpcmNumSamplesPerFrame, kLpcmSampleSize,
          kLpcmSampleRate);

  ExpectGeneratingCodecConfigObuSucceeds(codec_config_obu_metadata);
}

TEST(GetOpusCodecConfigObuMetadata, OutputHasRequestedValues) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
          kCodecConfigId, kOpusNumSamplesPerFrame);

  EXPECT_EQ(codec_config_obu_metadata.codec_config_id(), kCodecConfigId);
  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.num_samples_per_frame(), kOpusNumSamplesPerFrame);
}

TEST(GetOpusCodecConfigObuMetadata, OutputHasReasonableDefaults) {
  constexpr uint8_t kOpusVersion = 1;
  constexpr uint32_t kOpusInputSampleRate = 48000;
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
          kCodecConfigId, kOpusNumSamplesPerFrame);

  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.codec_id(), iamf_tools_cli_proto::CODEC_ID_OPUS);
  EXPECT_EQ(codec_config.decoder_config_opus().version(), kOpusVersion);
  EXPECT_EQ(codec_config.decoder_config_opus().input_sample_rate(),
            kOpusInputSampleRate);
}

TEST(GetOpusCodecConfigObuMetadata, UsesAutomaticOverrideFields) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
          kCodecConfigId, kOpusNumSamplesPerFrame);

  // Ensure the automatic configuration fields are set, instead of having to
  // consider specific required values based on the codec.
  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.automatically_override_audio_roll_distance(),
            kAutomaticallyOverrideAudioRollDistance);
  EXPECT_EQ(codec_config.automatically_override_codec_delay(),
            kAutomaticallyOverrideCodecDelay);
}

TEST(GetOpusCodecConfigObuMetadata, IsCompatibleWithCodecConfigGenerator) {
  const auto codec_config_obu_metadata =
      CodecConfigObuMetadataBuilder::GetOpusCodecConfigObuMetadata(
          kCodecConfigId, kOpusNumSamplesPerFrame);

  ExpectGeneratingCodecConfigObuSucceeds(codec_config_obu_metadata);
}

}  // namespace
}  // namespace iamf_tools
