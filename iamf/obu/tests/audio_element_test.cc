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
#include "iamf/obu/audio_element.h"

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/tests/obu_test_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using ::testing::Not;

using absl::MakeConstSpan;

constexpr int64_t kInitialBufferCapacity = 1024;

// TODO(b/272003291): Add more "expected failure" tests. Add more "successful"
//                    test cases to existing tests.

constexpr uint8_t kParameterDefinitionDemixingAsUint8 =
    static_cast<uint8_t>(ParamDefinition::kParameterDefinitionDemixing);

DemixingParamDefinition CreateDemixingInfoParamDefinition(
    DemixingInfoParameterData::DMixPMode dmixp_mode) {
  DemixingParamDefinition param_definition;
  param_definition.parameter_id_ = 4;
  param_definition.parameter_rate_ = 5;
  param_definition.param_definition_mode_ = false;
  param_definition.reserved_ = 0;
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 64;
  param_definition.default_demixing_info_parameter_data_.dmixp_mode =
      dmixp_mode;
  param_definition.default_demixing_info_parameter_data_.default_w = 0;
  param_definition.default_demixing_info_parameter_data_.reserved = 0;
  param_definition.default_demixing_info_parameter_data_
      .reserved_for_future_use = 0;
  param_definition.InitializeSubblockDurations(1);
  return param_definition;
}

// A structure for arguments common to all `AudioElementObu` constructors.
struct CommonAudioElementArgs {
  ObuHeader header;
  DecodedUleb128 audio_element_id;
  AudioElementObu::AudioElementType audio_element_type;
  uint8_t reserved;

  DecodedUleb128 codec_config_id;

  // Length `num_substreams`.
  std::vector<DecodedUleb128> substream_ids;

  // Length `num_parameters`.
  std::vector<AudioElementParam> audio_element_params;
};

// Returns suitable common arguments for a channel-based `AudioElementObu`.
CommonAudioElementArgs CreateScalableAudioElementArgs() {
  return {
      .header = ObuHeader(),
      .audio_element_id = 1,
      .audio_element_type = AudioElementObu::kAudioElementChannelBased,
      .reserved = 0,
      .codec_config_id = 2,
      .substream_ids = {3},
      .audio_element_params = {AudioElementParam{
          CreateDemixingInfoParamDefinition(
              DemixingInfoParameterData::kDMixPMode1)}},
  };
}

// Returns a one-layer stereo `ScalableChannelLayoutConfig`.
ScalableChannelLayoutConfig GetOneLayerStereoScalableChannelLayout() {
  return ScalableChannelLayoutConfig{
      .reserved = 0,
      .channel_audio_layer_configs = std::vector<ChannelAudioLayerConfig>(
          1, ChannelAudioLayerConfig{.loudspeaker_layout =
                                         ChannelAudioLayerConfig::kLayoutStereo,
                                     .output_gain_is_present_flag = true,
                                     .recon_gain_is_present_flag = true,
                                     .reserved_a = 0,
                                     .substream_count = 1,
                                     .coupled_substream_count = 1,
                                     .output_gain_flag = 1,
                                     .reserved_b = 0,
                                     .output_gain = 1}),
  };
}

absl::StatusOr<AudioElementObu> CreateScalableAudioElementObu(
    const CommonAudioElementArgs& common_args,
    const ScalableChannelLayoutConfig& scalable_channel_layout_config) {
  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      common_args.header, common_args.audio_element_id, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids,
      scalable_channel_layout_config);
  if (!obu.ok()) {
    return obu.status();
  }
  obu->InitializeParams(common_args.audio_element_params.size());
  for (const auto& param : common_args.audio_element_params) {
    obu->audio_element_params_.push_back(param);
  }
  return obu;
}

// Payload agreeing with `CreateScalableAudioElementArgs`,
// `GetOneLayerStereoScalableChannelLayout`.
constexpr auto kExpectedOneLayerStereoPayload = std::to_array<uint8_t>(
    {// `audio_element_id`.
     1,
     // `audio_element_type (3), reserved (5).
     AudioElementObu::kAudioElementChannelBased << 5,
     // `codec_config_id`.
     2,
     // `num_substreams`.
     1,
     // `audio_substream_ids`
     3,
     // `num_parameters`.
     1,
     // `audio_element_params[0]`.
     kParameterDefinitionDemixingAsUint8, 4, 5, 0x00, 64, 64, 0, 0,
     // `scalable_channel_layout_config`.
     // `num_layers` (3), reserved (5).
     1 << 5,
     // `channel_audio_layer_config[0]`.
     // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
     // `recon_gain_is_present_flag` (1), `reserved` (2).
     ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
     // `substream_count`.
     1,
     // `coupled_substream_count`.
     1,
     // `output_gain_flags` (6) << reserved.
     1 << 2,
     // `output_gain`.
     0, 1});

TEST(CreateScalableAudioElementArgs, SetsObuType) {
  auto args = CreateScalableAudioElementArgs();
  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      args.header, args.audio_element_id, args.reserved, args.codec_config_id,
      args.substream_ids, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  EXPECT_EQ(obu->header_.obu_type, kObuIaAudioElement);
  EXPECT_EQ(obu->GetAudioElementType(),
            AudioElementObu::kAudioElementChannelBased);
}

TEST(CreateScalableAudioElementObu, FailsWithInvalidNumSubstreams) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.substream_ids = {};

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());

  EXPECT_THAT(obu, Not(IsOk()));
}

TEST(ValidateAndWriteObu, SerializesOneLayerStereoScalableChannelLayout) {
  auto obu =
      CreateScalableAudioElementObu(CreateScalableAudioElementArgs(),
                                    GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 21});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedOneLayerStereoPayload);
}

TEST(ValidateAndWriteObu, WritesRedundantCopyFlag) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.header.obu_redundant_copy = true;
  constexpr auto kExpectedHeader = std::to_array<uint8_t>(
      {kObuIaAudioElement << 3 | ObuTestBase::kObuRedundantCopyBitMask, 21});

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedOneLayerStereoPayload);
}

TEST(ValidateAndWriteObu, FailsWithInvalidObuTrimmingStatusFlag) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.header.obu_trimming_status_flag = true;

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer undefined_wb(kInitialBufferCapacity);
  EXPECT_THAT(obu->ValidateAndWriteObu(undefined_wb), Not(IsOk()));
}

TEST(ValidateAndWriteObu, WritesParamDefinitionExtensionZero) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.audio_element_params.clear();
  common_args.audio_element_params.emplace_back(
      AudioElementParam{ExtendedParamDefinition{
          ParamDefinition::kParameterDefinitionReservedStart}});
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 15});

  const auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id`.
       1,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementChannelBased << 5,
       // `codec_config_id`.
       2,
       // `num_substreams`.
       1,
       // `audio_substream_ids`
       3,
       // `num_parameters`.
       1,
       // `audio_element_params[0]`.
       3, 0,
       // `scalable_channel_layout_config`.
       // `num_layers` (3), reserved (5).
       1 << 5,
       // `channel_audio_layer_config[0]`.
       // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
       // `recon_gain_is_present_flag` (1), `reserved` (2).
       ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
       // `substream_count`.
       1,
       // `coupled_substream_count`.
       1,
       // `output_gain_flags` (6) << reserved.
       1 << 2,
       // `output_gain`.
       0, 1});

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesMaxParamDefinitionType) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.audio_element_params.clear();
  common_args.audio_element_params.emplace_back(
      AudioElementParam{ExtendedParamDefinition{
          ParamDefinition::kParameterDefinitionReservedEnd}});
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 19});
  const auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id`.
       1,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementChannelBased << 5,
       // `codec_config_id`.
       2,
       // `num_substreams`.
       1,
       // `audio_substream_ids`
       3,
       // `num_parameters`.
       1,
       // `audio_element_params[0]`.
       0xff, 0xff, 0xff, 0xff, 0x0f, 0,
       // `scalable_channel_layout_config`.
       // `num_layers` (3), reserved (5).
       1 << 5,
       // `channel_audio_layer_config[0]`.
       // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
       // `recon_gain_is_present_flag` (1), `reserved` (2).
       ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
       // `substream_count`.
       1,
       // `coupled_substream_count`.
       1,
       // `output_gain_flags` (6) << reserved.
       1 << 2,
       // `output_gain`.
       0, 1});

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesParamDefinitionExtensionNonZero) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  ExtendedParamDefinition param_definition(
      ParamDefinition::kParameterDefinitionReservedStart);
  param_definition.param_definition_bytes_ = {'e', 'x', 't', 'r', 'a'};
  common_args.audio_element_params.clear();
  common_args.audio_element_params.emplace_back(
      AudioElementParam{param_definition});
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 20});
  const auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id`.
       1,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementChannelBased << 5,
       // `codec_config_id`.
       2,
       // `num_substreams`.
       1,
       // `audio_substream_ids`
       3,
       // `num_parameters`.
       1,
       // `audio_element_params[0]`.
       3, 5, 'e', 'x', 't', 'r', 'a',
       // `scalable_channel_layout_config`.
       // `num_layers` (3), reserved (5).
       1 << 5,
       // `channel_audio_layer_config[0]`.
       // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
       // `recon_gain_is_present_flag` (1), `reserved` (2).
       ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
       // `substream_count`.
       1,
       // `coupled_substream_count`.
       1,
       // `output_gain_flags` (6) << reserved.
       1 << 2,
       // `output_gain`.
       0, 1});

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

constexpr int kLoudspeakerLayoutBitShift = 4;
constexpr int kOutputGainIsPresentBitShift = 3;
constexpr int kReconGainIsPresentBitShift = 2;
constexpr int kOutputGainIsPresentFlagBitShift = 2;

constexpr uint8_t kBinauralSubstreamCount = 1;
constexpr uint8_t kBinauralCoupledSubstreamCount = 1;
const ChannelAudioLayerConfig kChannelAudioLayerConfigBinaural = {
    .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutBinaural,
    .output_gain_is_present_flag = false,
    .recon_gain_is_present_flag = false,
    .substream_count = kBinauralSubstreamCount,
    .coupled_substream_count = kBinauralCoupledSubstreamCount};

constexpr uint8_t kOneLayerStereoSubstreamCount = 1;
constexpr uint8_t kOneLayerStereoCoupledSubstreamCount = 1;
const ChannelAudioLayerConfig kChannelAudioLayerConfigStereo = {
    .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
    .output_gain_is_present_flag = false,
    .recon_gain_is_present_flag = false,
    .substream_count = kOneLayerStereoSubstreamCount,
    .coupled_substream_count = kOneLayerStereoCoupledSubstreamCount};

TEST(ChannelAudioLayerConfig, WritesBinauralLayer) {
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigBinaural.Write(wb), IsOk());

  ValidateWriteResults(
      wb, std::vector<uint8_t>{ChannelAudioLayerConfig::kLayoutBinaural
                                   << kLoudspeakerLayoutBitShift,
                               kBinauralSubstreamCount,
                               kBinauralCoupledSubstreamCount});
}

TEST(ChannelAudioLayerConfig, WritesStereoLayer) {
  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift,
      kOneLayerStereoSubstreamCount, kOneLayerStereoCoupledSubstreamCount};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigStereo.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesReserved10Layer) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved10 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved10,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutReserved10 << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved10.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesReserved11Layer) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved11 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved11,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutReserved11 << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved11.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesReserved12Layer) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved12 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved12,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutReserved12 << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved12.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesReserved13Layer) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved13 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved13,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutReserved13 << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved13.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesReserved14Layer) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved14 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved14,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutReserved14 << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved14.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesExpandedLayoutLFE) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved15 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount,
      .expanded_loudspeaker_layout =
          ChannelAudioLayerConfig::kExpandedLayoutLFE};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount,
      ChannelAudioLayerConfig::kExpandedLayoutLFE};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved15.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesExpandedLayout10_2_9_3) {
  constexpr uint8_t kExpectedSubstreamCount = 16;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 8;
  const ChannelAudioLayerConfig kChannelAudioLayerConfig10_2_9_3 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount,
      .expanded_loudspeaker_layout =
          ChannelAudioLayerConfig::kExpandedLayout10_2_9_3};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount,
      ChannelAudioLayerConfig::kExpandedLayout10_2_9_3};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfig10_2_9_3.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesExpandedLayoutLfePair) {
  constexpr uint8_t kExpectedSubstreamCount = 2;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 0;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigLfePair = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount,
      .expanded_loudspeaker_layout =
          ChannelAudioLayerConfig::kExpandedLayoutLfePair};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount,
      ChannelAudioLayerConfig::kExpandedLayoutLfePair};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigLfePair.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesExpandedLayoutBottom3Ch) {
  constexpr uint8_t kExpectedSubstreamCount = 2;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigBottom3Ch = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount,
      .expanded_loudspeaker_layout =
          ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount,
      ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigBottom3Ch.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesExpandedLayoutReserved16) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;
  const ChannelAudioLayerConfig kChannelAudioLayerConfigReserved16 = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .substream_count = kExpectedSubstreamCount,
      .coupled_substream_count = kExpectedCoupledSubstreamCount,
      .expanded_loudspeaker_layout =
          ChannelAudioLayerConfig::kExpandedLayoutReserved16};

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift,
      kExpectedSubstreamCount, kExpectedCoupledSubstreamCount,
      ChannelAudioLayerConfig::kExpandedLayoutReserved16};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kChannelAudioLayerConfigReserved16.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig,
     DoesNotWriteWhenExpandedLoudspeakerLayoutIsInconsistent) {
  constexpr uint8_t kExpectedSubstreamCount = 1;
  constexpr uint8_t kExpectedCoupledSubstreamCount = 1;

  const ChannelAudioLayerConfig
      kChannelAudioLayerConfigWithInconsistentExpandedLayout = {
          .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
          .output_gain_is_present_flag = false,
          .recon_gain_is_present_flag = false,
          .substream_count = kExpectedSubstreamCount,
          .coupled_substream_count = kExpectedCoupledSubstreamCount,
          .expanded_loudspeaker_layout = std::nullopt};

  WriteBitBuffer wb(1024);
  EXPECT_FALSE(
      kChannelAudioLayerConfigWithInconsistentExpandedLayout.Write(wb).ok());
}

TEST(ChannelAudioLayerConfig, WritesOutputGainIsPresentFields) {
  constexpr bool kOutputGainIsPresent = true;
  constexpr uint8_t kOutputGainFlag = 0b100000;
  constexpr uint8_t kReservedB = 0b01;
  constexpr int16_t kOutputGain = 5;
  const ChannelAudioLayerConfig kSecondLayerStereo = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
      .output_gain_is_present_flag = kOutputGainIsPresent,
      .recon_gain_is_present_flag = false,
      .substream_count = 1,
      .coupled_substream_count = 0,
      .output_gain_flag = kOutputGainFlag,
      .reserved_b = kReservedB,
      .output_gain = kOutputGain,
  };

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift |
          kOutputGainIsPresent << kOutputGainIsPresentBitShift,
      1,
      0,
      kOutputGainFlag << kOutputGainIsPresentFlagBitShift | kReservedB,
      0,
      5};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kSecondLayerStereo.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesReconGainIsPresentFlag) {
  constexpr bool kReconGainIsPresent = true;
  const ChannelAudioLayerConfig kSecondLayerStereo = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = kReconGainIsPresent,
      .substream_count = 1,
      .coupled_substream_count = 0,
  };

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift |
          kReconGainIsPresent << kReconGainIsPresentBitShift,
      1, 0};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kSecondLayerStereo.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, WritesFirstReservedField) {
  const uint8_t kFirstReservedField = 3;
  const ChannelAudioLayerConfig kSecondLayerStereo = {
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
      .output_gain_is_present_flag = false,
      .recon_gain_is_present_flag = false,
      .reserved_a = kFirstReservedField,
      .substream_count = 1,
      .coupled_substream_count = 0,
  };

  const std::vector<uint8_t> kExpectedData = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift |
          kFirstReservedField,
      1, 0};
  WriteBitBuffer wb(1024);
  EXPECT_THAT(kSecondLayerStereo.Write(wb), IsOk());

  ValidateWriteResults(wb, kExpectedData);
}

TEST(ChannelAudioLayerConfig, ReadsBinauralLayer) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutBinaural << kLoudspeakerLayoutBitShift, 1,
      1};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutBinaural);
  EXPECT_EQ(config.output_gain_is_present_flag, false);
  EXPECT_EQ(config.recon_gain_is_present_flag, false);
  EXPECT_EQ(config.reserved_a, 0);
  EXPECT_EQ(config.substream_count, kBinauralSubstreamCount);
  EXPECT_EQ(config.coupled_substream_count, kBinauralCoupledSubstreamCount);
}

TEST(ChannelAudioLayerConfig, ReadsReserved10Layer) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutReserved10 << kLoudspeakerLayoutBitShift,
      1, 1};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutReserved10);
}

TEST(ChannelAudioLayerConfig, ReadsReserved11Layer) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutReserved11 << kLoudspeakerLayoutBitShift,
      1, 1};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutReserved11);
}

TEST(ChannelAudioLayerConfig, ReadsReserved12Layer) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutReserved12 << kLoudspeakerLayoutBitShift,
      1, 1};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutReserved12);
}

TEST(ChannelAudioLayerConfig, ReadsReserved13Layer) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutReserved13 << kLoudspeakerLayoutBitShift,
      1, 1};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutReserved13);
}

TEST(ChannelAudioLayerConfig, ReadsReserved14Layer) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutReserved14 << kLoudspeakerLayoutBitShift,
      1, 1};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutReserved14);
}

TEST(ChannelAudioLayerConfig, ReadsExpandedLayoutLFE) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift, 1,
      1, ChannelAudioLayerConfig::kExpandedLayoutLFE};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  EXPECT_EQ(config.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutLFE);
}

TEST(ChannelAudioLayerConfig,
     DoesNotReadWhenExpandedLoudspeakerLayoutIsInconsistent) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift, 1,
      1
      /*`expanded_loudspeaker_layout` is omitted*/};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_FALSE(config.Read(*buffer).ok());
}

TEST(ChannelAudioLayerConfig, ReadsExpandedLayout10_2_9_3) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift,
      16, 8, ChannelAudioLayerConfig::kExpandedLayout10_2_9_3};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  EXPECT_EQ(config.substream_count, 16);
  EXPECT_EQ(config.coupled_substream_count, 8);
  EXPECT_EQ(config.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayout10_2_9_3);
}

TEST(ChannelAudioLayerConfig, ReadsExpandedLayoutLFEPair) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift, 2,
      0, ChannelAudioLayerConfig::kExpandedLayoutLfePair};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  EXPECT_EQ(config.substream_count, 2);
  EXPECT_EQ(config.coupled_substream_count, 0);
  EXPECT_EQ(config.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutLfePair);
}

TEST(ChannelAudioLayerConfig, ReadsExpandedLayoutBottom3Ch) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift, 2,
      1, ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  EXPECT_EQ(config.substream_count, 2);
  EXPECT_EQ(config.coupled_substream_count, 1);
  EXPECT_EQ(config.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutBottom3Ch);
}

TEST(ChannelAudioLayerConfig, ReadsExpandedLayoutReserved16) {
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutExpanded << kLoudspeakerLayoutBitShift, 1,
      1, ChannelAudioLayerConfig::kExpandedLayoutReserved16};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.loudspeaker_layout,
            ChannelAudioLayerConfig::kLayoutExpanded);
  EXPECT_EQ(config.expanded_loudspeaker_layout,
            ChannelAudioLayerConfig::kExpandedLayoutReserved16);
}

TEST(ChannelAudioLayerConfig, ReadsOutputGainIsPresentRelatedFields) {
  constexpr bool kOutputGainIsPresent = true;
  constexpr uint8_t kOutputGainFlag = 0b100000;
  constexpr uint8_t kReservedB = 0b01;
  constexpr int16_t kOutputGain = 5;
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift |
          kOutputGainIsPresent << kOutputGainIsPresentBitShift,
      1,
      0,
      kOutputGainFlag << kOutputGainIsPresentFlagBitShift | kReservedB,
      0,
      5};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.output_gain_is_present_flag, kOutputGainIsPresent);
  EXPECT_EQ(config.output_gain_flag, kOutputGainFlag);
  EXPECT_EQ(config.reserved_b, kReservedB);
  EXPECT_EQ(config.output_gain, kOutputGain);
}

TEST(ChannelAudioLayerConfig, ReadsReconGainIsPresent) {
  constexpr bool kReconGainIsPresent = true;
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift |
          kReconGainIsPresent << kReconGainIsPresentBitShift,
      1, 0};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.recon_gain_is_present_flag, kReconGainIsPresent);
}

TEST(ChannelAudioLayerConfig, ReadsFirstReservedField) {
  constexpr uint8_t kReservedField = 3;
  std::vector<uint8_t> data = {
      ChannelAudioLayerConfig::kLayoutStereo << kLoudspeakerLayoutBitShift |
          kReservedField,
      1, 0};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(data));
  ChannelAudioLayerConfig config;

  EXPECT_THAT(config.Read(*buffer), IsOk());

  EXPECT_EQ(config.reserved_a, kReservedField);
}

const ScalableChannelLayoutConfig kTwoLayerStereoConfig = {
    .channel_audio_layer_configs = {
        ChannelAudioLayerConfig{
            .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
            .output_gain_is_present_flag = false,
            .recon_gain_is_present_flag = false,
            .substream_count = 1,
            .coupled_substream_count = 0},
        ChannelAudioLayerConfig{
            .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
            .output_gain_is_present_flag = false,
            .recon_gain_is_present_flag = false,
            .substream_count = 1,
            .coupled_substream_count = 0}}};
const DecodedUleb128 kTwoLayerStereoSubstreamCount = 2;

TEST(ScalableChannelLayoutConfigValidate, IsOkWithMultipleLayers) {
  EXPECT_THAT(kTwoLayerStereoConfig.Validate(kTwoLayerStereoSubstreamCount),
              IsOk());
}

TEST(ScalableChannelLayoutConfigValidate,
     IsNotOkWhenSubstreamCountDoesNotMatchWithMultipleLayers) {
  EXPECT_FALSE(
      kTwoLayerStereoConfig.Validate(kTwoLayerStereoSubstreamCount + 1).ok());
}

TEST(ScalableChannelLayoutConfigValidate, TooFewLayers) {
  const ScalableChannelLayoutConfig kConfigWithZeroLayer = {};

  EXPECT_FALSE(kConfigWithZeroLayer.Validate(0).ok());
}

TEST(ScalableChannelLayoutConfigValidate, TooManyLayers) {
  const ScalableChannelLayoutConfig kConfigWithZeroLayer = {
      .channel_audio_layer_configs = std::vector<ChannelAudioLayerConfig>(7)};

  EXPECT_FALSE(kConfigWithZeroLayer.Validate(0).ok());
}

TEST(ScalableChannelLayoutConfigValidate, IsOkWithOneLayerBinaural) {
  const ScalableChannelLayoutConfig kBinauralConfig = {
      .channel_audio_layer_configs = {kChannelAudioLayerConfigBinaural}};

  EXPECT_THAT(kBinauralConfig.Validate(1), IsOk());
}

TEST(ScalableChannelLayoutConfigValidate,
     MustHaveExactlyOneLayerIfBinauralIsPresent) {
  const ScalableChannelLayoutConfig kInvalidBinauralConfigWithFirstLayerStereo =
      {.channel_audio_layer_configs = {kChannelAudioLayerConfigStereo,
                                       kChannelAudioLayerConfigBinaural}};
  const ScalableChannelLayoutConfig
      kInvalidBinauralConfigWithSecondLayerStereo = {
          .channel_audio_layer_configs = {kChannelAudioLayerConfigBinaural,
                                          kChannelAudioLayerConfigStereo}};

  EXPECT_FALSE(kInvalidBinauralConfigWithFirstLayerStereo.Validate(2).ok());
  EXPECT_FALSE(kInvalidBinauralConfigWithSecondLayerStereo.Validate(2).ok());
}

TEST(ObjectsConfigCreate, IsOkWithOneObject) {
  EXPECT_THAT(ObjectsConfig::Create(/*num_objects=*/1,
                                    /*objects_config_extension_bytes=*/{}),
              IsOk());
}
TEST(ObjectsConfigCreate, IsOkWithTwoObject) {
  EXPECT_THAT(ObjectsConfig::Create(/*num_objects=*/2,
                                    /*objects_config_extension_bytes=*/{}),
              IsOk());
}
TEST(ObjectsConfigCreate, IsNotOkWithNoObject) {
  EXPECT_THAT(ObjectsConfig::Create(/*num_objects=*/0,
                                    /*objects_config_extension_bytes=*/{}),
              Not(IsOk()));
}

// Returns suitable common arguments for an object-based `AudioElementObu`.
CommonAudioElementArgs CreateObjectsAudioElementArgs() {
  return {
      .header = ObuHeader(),
      .audio_element_id = 1,
      .audio_element_type = AudioElementObu::kAudioElementObjectBased,
      .reserved = 0,
      .codec_config_id = 2,
      .substream_ids = {3},
      .audio_element_params = {AudioElementParam{
          CreateDemixingInfoParamDefinition(
              DemixingInfoParameterData::kDMixPMode1)}},
  };
}

ObjectsConfig GetObjectsConfigExpectOk(
    uint32_t num_objects,
    absl::Span<const uint8_t> objects_config_extension_bytes) {
  auto objects_config =
      ObjectsConfig::Create(num_objects, objects_config_extension_bytes);
  EXPECT_THAT(objects_config, IsOk());
  return *objects_config;
}

TEST(CreateObjectsAudioElementObu, SetsObuType) {
  auto args = CreateObjectsAudioElementArgs();
  auto obu = AudioElementObu::CreateForObjects(
      args.header, args.audio_element_id, args.reserved, args.codec_config_id,
      args.substream_ids[0], GetObjectsConfigExpectOk(1, {}));
  ASSERT_THAT(obu, IsOk());

  EXPECT_EQ(obu->header_.obu_type, kObuIaAudioElement);
  EXPECT_EQ(obu->GetAudioElementType(),
            AudioElementObu::kAudioElementObjectBased);
}

absl::StatusOr<AudioElementObu> CreateObjectsAudioElementObu(
    const CommonAudioElementArgs& common_args,
    const ObjectsConfig& objects_config) {
  auto obu = AudioElementObu::CreateForObjects(
      common_args.header, common_args.audio_element_id, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids[0],
      objects_config);
  if (!obu.ok()) {
    return obu.status();
  }
  obu->InitializeParams(common_args.audio_element_params.size());
  for (const auto& param : common_args.audio_element_params) {
    obu->audio_element_params_.push_back(param);
  }
  return obu;
}

// Payload agreeing with `CreateObjectsAudioElementObu`,
// `kOneObjectConfigWithExtension`.
constexpr auto kExpectedOneObjectPayload = std::to_array<uint8_t>({
    // `audio_element_id`.
    1,
    // `audio_element_type (3), reserved (5).
    AudioElementObu::kAudioElementObjectBased << 5,
    // `codec_config_id`.
    2,
    // `num_substreams`.
    1,
    // `audio_substream_ids`
    3,
    // `num_parameters`.
    1,
    // `audio_element_params[0]`.
    kParameterDefinitionDemixingAsUint8,
    4,
    5,
    0x00,
    64,
    64,
    0,
    0,
    // `objects_config`
    // `objects_config_size`.
    4,
    // `num_objects`.
    1,
    // `objects_config_extension_bytes`.
    0x01,
    0x02,
    0x03,
});

TEST(ValidateAndWriteObu, SerializesOneObjectAudioElementObu) {
  auto obu = CreateObjectsAudioElementObu(
      CreateObjectsAudioElementArgs(),
      GetObjectsConfigExpectOk(1, {0x01, 0x02, 0x03}));
  ASSERT_THAT(obu, IsOk());
  // Shift is based on obu size.
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 19});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedOneObjectPayload);
}

TEST(ValidateAndWriteObu, WritesWithTwoSubstreams) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.substream_ids = {1, 2};
  auto scalable_channel_layout = GetOneLayerStereoScalableChannelLayout();
  scalable_channel_layout.channel_audio_layer_configs[0].substream_count = 2;

  auto obu =
      CreateScalableAudioElementObu(common_args, scalable_channel_layout);
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 22});
  const auto kExpectedPayload = std::to_array<uint8_t>(
      {1, AudioElementObu::kAudioElementChannelBased << 5, 2,
       // `num_substreams`.
       2,
       // `audio_substream_ids`.
       1, 2,
       // `num_parameters`.
       1, kParameterDefinitionDemixingAsUint8,
       // Start `DemixingParamDefinition`.
       4, 5, 0x00, 64, 64, 0, 0,
       // `scalable_channel_layout_config`.
       // `num_layers` (3), reserved (5).
       1 << 5,
       // `channel_audio_layer_config[0]`.
       // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
       // `recon_gain_is_present_flag` (1), `reserved` (2).
       ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
       // `substream_count`.
       2,
       // `coupled_substream_count`.
       1,
       // `output_gain_flags` (6) << reserved.
       1 << 2,
       // `output_gain`.
       0, 1});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu,
     FailsWithInvalidDuplicateParamDefinitionTypesExtension) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.audio_element_params.clear();
  const auto kDuplicateParameterDefinition =
      ParamDefinition::kParameterDefinitionReservedStart;

  common_args.audio_element_params.emplace_back(AudioElementParam{
      ExtendedParamDefinition(kDuplicateParameterDefinition)});
  common_args.audio_element_params.emplace_back(AudioElementParam{
      ExtendedParamDefinition(kDuplicateParameterDefinition)});

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(obu->ValidateAndWriteObu(unused_wb), Not(IsOk()));
}

TEST(ValidateAndWriteObu,
     FailsWithInvalidDuplicateParamDefinitionTypesDemixing) {
  CommonAudioElementArgs common_args = CreateScalableAudioElementArgs();
  common_args.audio_element_params.clear();
  const auto demixing_param_definition =
      CreateDemixingInfoParamDefinition(DemixingInfoParameterData::kDMixPMode1);
  for (int i = 0; i < 2; i++) {
    common_args.audio_element_params.emplace_back(
        AudioElementParam{demixing_param_definition});
  }

  auto obu = CreateScalableAudioElementObu(
      common_args, GetOneLayerStereoScalableChannelLayout());
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(obu->ValidateAndWriteObu(unused_wb), Not(IsOk()));
}

// Reasonable for mono or projection ambisonics.
CommonAudioElementArgs CreateAmbisonicsArgs() {
  return {
      .header = ObuHeader(),
      .audio_element_id = 1,
      .audio_element_type = AudioElementObu::kAudioElementSceneBased,
      .reserved = 0,
      .codec_config_id = 2,
      .substream_ids = {3},
      .audio_element_params = {},
  };
}

TEST(CreateMonoAmbisonicsAudioElement, SetsObuType) {
  const auto common_args = CreateAmbisonicsArgs();

  constexpr std::array<uint8_t, 1> kChannelMapping = {0};
  auto obu = AudioElementObu::CreateForMonoAmbisonics(
      common_args.header, common_args.audio_element_id, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids, kChannelMapping);
  ASSERT_THAT(obu, IsOk());

  EXPECT_EQ(obu->GetAudioElementType(),
            AudioElementObu::kAudioElementSceneBased);
}

TEST(CreateMonoAmbisonicsAudioElement, FailsWithInvalidChannelMapping) {
  auto common_args = CreateAmbisonicsArgs();
  common_args.substream_ids = {0, 1, 2};

  // The size of the channel mapping represents the output channel count; a
  // square number.
  constexpr std::array<uint8_t, 3> kInvalidChannelMapping = {0, 1, 2};
  auto obu = AudioElementObu::CreateForMonoAmbisonics(
      common_args.header, common_args.audio_element_id, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids,
      kInvalidChannelMapping);
}

absl::StatusOr<AudioElementObu> CreateMonoAmbisonicsAudioElement(
    const CommonAudioElementArgs& common_args,
    const absl::Span<const uint8_t> channel_mapping) {
  auto obu = AudioElementObu::CreateForMonoAmbisonics(
      common_args.header, common_args.audio_element_id, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids, channel_mapping);
  if (!obu.ok()) {
    return obu.status();
  }
  obu->InitializeParams(common_args.audio_element_params.size());
  for (const auto& param : common_args.audio_element_params) {
    obu->audio_element_params_.push_back(param);
  }
  return obu;
}

TEST(ValidateAndWriteObu, WritesAmbisonicsMono) {
  auto obu = CreateMonoAmbisonicsAudioElement(CreateAmbisonicsArgs(), {0});
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 10});
  const auto kExpectedPayload =
      std::to_array<uint8_t>({// `audio_element_id`.
                              1,
                              // `audio_element_type (3), reserved (5).
                              AudioElementObu::kAudioElementSceneBased << 5,
                              // `codec_config_id`.
                              2,
                              // `num_substreams`.
                              1,
                              // `audio_substream_ids`
                              3,
                              // `num_parameters`.
                              0,
                              // Start `ambisonics_config`.
                              // `ambisonics_mode`.
                              AmbisonicsConfig::kAmbisonicsModeMono,
                              // `output_channel_count`.
                              1,
                              // `substream_count`.
                              1,
                              // `channel_mapping`.
                              0});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, NonMinimalLebGeneratorAffectsAllLeb128s) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  ASSERT_NE(leb_generator, nullptr);
  auto obu = CreateMonoAmbisonicsAudioElement(CreateAmbisonicsArgs(), {0});
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 0x80 | 16, 0x00});

  const auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id` is affected by the `LebGenerator`.
       0x80 | 1, 0x00,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementSceneBased << 5,
       // `codec_config_id` is affected by the `LebGenerator`.
       0x80 | 2, 0x00,
       // `num_substreams` is affected by the `LebGenerator`.
       0x80 | 1, 0x00,
       // `audio_substream_ids` is affected by the `LebGenerator`.
       0x80 | 3, 0x00,
       // `num_parameters`. is affected by the `LebGenerator`.
       0x80 | 0, 0x00,
       // Start `ambisonics_config`.
       // `ambisonics_mode` is affected by the `LebGenerator`.
       0x80 | AmbisonicsConfig::kAmbisonicsModeMono, 0x00,
       // `output_channel_count`.
       1,
       // `substream_count`.
       1,
       // `channel_mapping`.
       0});

  WriteBitBuffer wb(kInitialBufferCapacity, *leb_generator);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesFoaAmbisonicsMono) {
  auto common_args = CreateAmbisonicsArgs();
  common_args.substream_ids = {10, 20, 30, 40};
  auto obu = CreateMonoAmbisonicsAudioElement(common_args, {0, 1, 2, 3});
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 16});
  const auto kExpectedPayload =
      std::to_array<uint8_t>({// `audio_element_id`.
                              1,
                              // `audio_element_type (3), reserved (5).
                              AudioElementObu::kAudioElementSceneBased << 5,
                              // `codec_config_id`.
                              2,
                              // `num_substreams`.
                              4,
                              // `audio_substream_ids`
                              10, 20, 30, 40,
                              // `num_parameters`.
                              0,
                              // Start `ambisonics_config`.
                              // `ambisonics_mode`.
                              AmbisonicsConfig::kAmbisonicsModeMono,
                              // `output_channel_count`.
                              4,
                              // `substream_count`.
                              4,
                              // `channel_mapping`.
                              0, 1, 2, 3});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesMaxAmbisonicsMono) {
  auto common_args = CreateAmbisonicsArgs();
  common_args.substream_ids = std::vector<DecodedUleb128>(225);
  std::iota(common_args.substream_ids.begin(), common_args.substream_ids.end(),
            0);
  std::vector<uint8_t> channel_mapping(225, 0);
  std::iota(channel_mapping.begin(), channel_mapping.end(), 0);
  auto obu = CreateMonoAmbisonicsAudioElement(common_args, channel_mapping);
  constexpr auto kExpectedSizeOfObu = 559;
  ASSERT_THAT(obu, IsOk());

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());
  EXPECT_EQ(wb.bit_buffer().size(), kExpectedSizeOfObu);
}

absl::StatusOr<AudioElementObu> CreateProjectionAmbisonicsAudioElement(
    const CommonAudioElementArgs& common_args, uint8_t output_channel_count,
    uint8_t coupled_substream_count,
    const absl::Span<const int16_t> demixing_matrix) {
  auto obu = AudioElementObu::CreateForProjectionAmbisonics(
      common_args.header, common_args.audio_element_id, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids,
      output_channel_count, coupled_substream_count, demixing_matrix);
  if (!obu.ok()) {
    return obu.status();
  }
  obu->InitializeParams(common_args.audio_element_params.size());
  for (const auto& param : common_args.audio_element_params) {
    obu->audio_element_params_.push_back(param);
  }
  return obu;
}

TEST(ValidateAndWriteObu, WritesAmbisonicsProjection) {
  constexpr uint8_t kOutputChannelCount = 1;
  constexpr uint8_t kCoupledSubstreamCount = 0;
  auto obu = CreateProjectionAmbisonicsAudioElement(
      CreateAmbisonicsArgs(), kOutputChannelCount, kCoupledSubstreamCount, {1});
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 12});
  const auto kExpectedPayload =
      std::to_array<uint8_t>({// `audio_element_id`.
                              1,
                              // `audio_element_type (3), reserved (5).
                              AudioElementObu::kAudioElementSceneBased << 5,
                              // `codec_config_id`.
                              2,
                              // `num_substreams`.
                              1,
                              // `audio_substream_ids`
                              3,
                              // `num_parameters`.
                              0,
                              // Start `ambisonics_config`.
                              // `ambisonics_mode`.
                              AmbisonicsConfig::kAmbisonicsModeProjection,
                              // `output_channel_count`.
                              1,
                              // `substream_count`.
                              1,
                              // `coupled_substream_count`.
                              0,
                              // `demixing_matrix`.
                              /*             ACN#:    0*/
                              /* Substream   0: */ 0, 1});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesFoaAmbisonicsProjection) {
  auto common_args = CreateAmbisonicsArgs();
  common_args.substream_ids = {0, 1, 2, 3};
  constexpr uint8_t kOutputChannelCount = 4;
  constexpr uint8_t kCoupledSubstreamCount = 0;
  std::vector<int16_t> demixing_matrix(16, 0);
  std::iota(demixing_matrix.begin(), demixing_matrix.end(), 1);
  auto obu = CreateProjectionAmbisonicsAudioElement(
      common_args, kOutputChannelCount, kCoupledSubstreamCount,
      demixing_matrix);
  ASSERT_THAT(obu, IsOk());
  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 45});
  const auto kExpectedPayload =
      std::to_array<uint8_t>({// `audio_element_id`.
                              1,
                              // `audio_element_type (3), reserved (5).
                              AudioElementObu::kAudioElementSceneBased << 5,
                              // `codec_config_id`.
                              2,
                              // `num_substreams`.
                              4,
                              // `audio_substream_ids`
                              0, 1, 2, 3,
                              // `num_parameters`.
                              0,
                              // Start `ambisonics_config`.
                              // `ambisonics_mode`.
                              AmbisonicsConfig::kAmbisonicsModeProjection,
                              // `output_channel_count`.
                              4,
                              // `substream_count`.
                              4,
                              // `coupled_substream_count`.
                              0,
                              // `demixing_matrix`.
                              /*             ACN#:    0,    1,    2,    3 */
                              /* Substream   0: */ 0, 1, 0, 2, 0, 3, 0, 4,
                              /* Substream   1: */ 0, 5, 0, 6, 0, 7, 0, 8,
                              /* Substream   2: */ 0, 9, 0, 10, 0, 11, 0, 12,
                              /* Substream   3: */ 0, 13, 0, 14, 0, 15, 0, 16});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesMaxAmbisonicsProjection) {
  auto common_args = CreateAmbisonicsArgs();
  common_args.substream_ids = std::vector<DecodedUleb128>(225);
  std::iota(common_args.substream_ids.begin(), common_args.substream_ids.end(),
            0);
  constexpr uint8_t kOutputChannelCount = 225;
  constexpr uint8_t kCoupledSubstreamCount = 0;
  std::vector<int16_t> demixing_matrix(50625, 0);
  auto obu = CreateProjectionAmbisonicsAudioElement(
      common_args, kOutputChannelCount, kCoupledSubstreamCount,
      demixing_matrix);

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());
  constexpr auto kExpectedSizeOfObu = 101586;

  EXPECT_EQ(wb.bit_buffer().size(), kExpectedSizeOfObu);
}

CommonAudioElementArgs CreateExtensionConfigAudioElementArgs(
    AudioElementObu::AudioElementType audio_element_type) {
  return {
      .header = ObuHeader(),
      .audio_element_id = 1,
      .audio_element_type = audio_element_type,
      .reserved = 0,
      .codec_config_id = 2,
      .substream_ids = {3},
      .audio_element_params = {AudioElementParam{
          CreateDemixingInfoParamDefinition(
              DemixingInfoParameterData::kDMixPMode1)}},
  };
}

absl::StatusOr<AudioElementObu> CreateExtensionConfigAudioElement(
    const CommonAudioElementArgs& common_args,
    absl::Span<const uint8_t> audio_element_config_bytes) {
  auto obu = AudioElementObu::CreateForExtension(
      common_args.header, common_args.audio_element_id,
      common_args.audio_element_type, common_args.reserved,
      common_args.codec_config_id, common_args.substream_ids,
      audio_element_config_bytes);
  if (!obu.ok()) {
    return obu.status();
  }
  obu->InitializeParams(common_args.audio_element_params.size());
  for (const auto& param : common_args.audio_element_params) {
    obu->audio_element_params_.push_back(param);
  }
  return obu;
}

TEST(ValidateAndWriteObu, WriteExtensionConfigSizeZero) {
  CommonAudioElementArgs common_args = CreateExtensionConfigAudioElementArgs(
      AudioElementObu::kAudioElementBeginReserved);
  auto obu = CreateExtensionConfigAudioElement(common_args, {});

  ASSERT_THAT(obu, IsOk());

  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 15});
  constexpr auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id`.
       1,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementBeginReserved << 5,
       // `codec_config_id`.
       2,
       // `num_substreams`.
       1,
       // `audio_substream_ids`
       3,
       // `num_parameters`.
       1,
       // `audio_element_params[0]`.
       kParameterDefinitionDemixingAsUint8, 4, 5, 0x00, 64, 64, 0, 0,
       // `audio_element_config_size`.
       0});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WriteExtensionConfigSizeMax) {
  CommonAudioElementArgs common_args = CreateExtensionConfigAudioElementArgs(
      AudioElementObu::kAudioElementEndReserved);
  auto obu = CreateExtensionConfigAudioElement(common_args, {});

  ASSERT_THAT(obu, IsOk());

  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 15});
  constexpr auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id`.
       1,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementEndReserved << 5,
       // `codec_config_id`.
       2,
       // `num_substreams`.
       1,
       // `audio_substream_ids`
       3,
       // `num_parameters`.
       1,
       // `audio_element_params[0]`.
       kParameterDefinitionDemixingAsUint8, 4, 5, 0x00, 64, 64, 0, 0,
       // `audio_element_config_size`.
       0});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(ValidateAndWriteObu, WritesMaxNonEmptyExtensionConfig) {
  CommonAudioElementArgs common_args = CreateExtensionConfigAudioElementArgs(
      AudioElementObu::kAudioElementEndReserved);
  auto obu =
      CreateExtensionConfigAudioElement(common_args, {'e', 'x', 't', 'r', 'a'});

  ASSERT_THAT(obu, IsOk());

  constexpr auto kExpectedHeader =
      std::to_array<uint8_t>({kObuIaAudioElement << 3, 20});
  constexpr auto kExpectedPayload = std::to_array<uint8_t>(
      {// `audio_element_id`.
       1,
       // `audio_element_type (3), reserved (5).
       AudioElementObu::kAudioElementEndReserved << 5,
       // `codec_config_id`.
       2,
       // `num_substreams`.
       1,
       // `audio_substream_ids`
       3,
       // `num_parameters`.
       1,
       // `audio_element_params[0]`.
       kParameterDefinitionDemixingAsUint8, 4, 5, 0x00, 64, 64, 0, 0,
       // `audio_element_config_size`.
       5,
       // 'audio_element_config_bytes`.
       'e', 'x', 't', 'r', 'a'});

  WriteBitBuffer wb(kInitialBufferCapacity);
  ASSERT_THAT(obu->ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb, kExpectedHeader, kExpectedPayload);
}

TEST(TestValidateAmbisonicsMono, MappingInAscendingOrder) {
  // Users may map the Ambisonics Channel Number to substreams in numerical
  // order. (e.g. A0 to the zeroth substream, A1 to the first substream, ...).
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3}};
  EXPECT_THAT(ambisonics_mono.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsMono, MappingInArbitraryOrder) {
  // Users may map the Ambisonics Channel Number to substreams in any order.
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/3, /*A1=*/1, /*A2=*/0, /*A3=*/2}};
  EXPECT_THAT(ambisonics_mono.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsMono, MixedOrderAmbisonics) {
  // User may choose to map the Ambisonics Channel Number (ACN) to
  // `255` to drop that ACN (e.g. to drop A0 and A3).
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 2,
      .channel_mapping = {/*A0=*/255, /*A1=*/1, /*A2=*/0, /*A3=*/255}};
  EXPECT_THAT(ambisonics_mono.Validate(2), IsOk());
}

TEST(TestValidateAmbisonicsMono,
     ManyAmbisonicsChannelNumbersMappedToOneSubstream) {
  // User may choose to map several Ambisonics Channel Numbers (ACNs) to
  // one substream (e.g. A0, A1, A2, A3 are all mapped to the zeroth substream).
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 1,
      .channel_mapping = {/*A0=*/0, /*A1=*/0, /*A2=*/0, /*A3=*/0}};
  EXPECT_THAT(ambisonics_mono.Validate(1), IsOk());
}

TEST(TestValidateAmbisonicsMono,
     InvalidWhenObuSubstreamCountDoesNotEqualSubstreamCount) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3}};
  const DecodedUleb128 kInconsistentObuSubstreamCount = 3;
  EXPECT_FALSE(ambisonics_mono.Validate(kInconsistentObuSubstreamCount).ok());
}

TEST(TestValidateAmbisonicsMono,
     InvalidWhenChannelMappingIsLargerThanSubstreamCount) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 2,
      .channel_mapping = {/*A0=*/255 /*A1=*/, 1 /*A2=*/, 0 /*A3=*/}};
  EXPECT_FALSE(ambisonics_mono.Validate(2).ok());
}

TEST(TestValidateAmbisonicsMono, InvalidOutputChannelCount) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 5,
      .substream_count = 5,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3, /*A4=*/4}};
  EXPECT_FALSE(ambisonics_mono.Validate(2).ok());
}

TEST(TestValidateAmbisonicsMono, InvalidWhenSubstreamIndexIsTooLarge) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/4}};
  EXPECT_FALSE(ambisonics_mono.Validate(4).ok());
}

TEST(TestValidateAmbisonicsMono,
     InvalidWhenNoAmbisonicsChannelNumberIsMappedToASubstream) {
  // The OBU claims two associated substreams. But substream 1 is in limbo and
  // has no meaning because there are no Ambisonics Channel Numbers mapped to
  // it.
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 2,
      .channel_mapping = {/*A0=*/0, /*A1=*/0, /*A2=*/0, /*A3=*/0}};
  EXPECT_FALSE(ambisonics_mono.Validate(2).ok());
}

TEST(TestValidateAmbisonicsProjection, FOAWithMainDiagonalMatrix) {
  // Typical users MAY create a matrix with non-zero values on the main
  // diagonal and zeroes in other entries. This results in one Ambisonics
  // Channel Number (ACN) represented per substream.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = {/*           ACN#: 0, 1, 2, 3 */
                          /* Substream 0: */ 1, 0, 0, 0,
                          /* Substream 1: */ 0, 1, 0, 0,
                          /* Substream 2: */ 0, 0, 1, 0,
                          /* Substream 3: */ 0, 0, 0, 1}};
  EXPECT_THAT(ambisonics_projection.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FOAWithArbitraryMatrix) {
  // Users MAY set arbitrary values anywhere in this matrix, but the size MUST
  // comply with the spec. This results in multiple Ambisonics Channel Numbers
  // (ACNs) per substream.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = {/*           ACN#: 0, 1, 2, 3 */
                          /* Substream 0: */ 1, 2, 3, 4,
                          /* Substream 1: */ 2, 3, 4, 5,
                          /* Substream 2: */ 3, 4, 5, 6,
                          /* Substream 3: */ 4, 5, 6, 7}};
  EXPECT_THAT(ambisonics_projection.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsProjection, ZerothOrderAmbisonics) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 1,
      .substream_count = 1,
      .coupled_substream_count = 0,
      .demixing_matrix = {
          /*                                             ACN#: 0, */
          /* Substream 0: */ std::numeric_limits<int16_t>::max()}};
  EXPECT_THAT(ambisonics_projection.Validate(1), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FOAWithOnlyA2) {
  // Fewer substreams than `output_channel_count` are allowed.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 1,
      .coupled_substream_count = 0,
      .demixing_matrix = {/*           ACN#: 0, 1, 2, 3 */
                          /* Substream 0: */ 0, 0, 1, 0}};
  EXPECT_THAT(ambisonics_projection.Validate(1), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FOAOneCoupledStream) {
  // The first `coupled_substream_count` substreams are coupled. Each pair in
  // the coupling has a column in the bitstream (written as a row in this
  // test). The remaining streams are decoupled.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 3,
      .coupled_substream_count = 1,
      .demixing_matrix = {/*             ACN#: 0, 1, 2, 3 */
                          /* Substream 0_a: */ 1, 0, 0, 0,
                          /* Substream 0_b: */ 0, 1, 0, 0,
                          /* Substream   1: */ 0, 0, 1, 0,
                          /* Substream   2: */ 0, 0, 0, 1}};
  EXPECT_THAT(ambisonics_projection.Validate(3), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FourteenthOrderAmbisonicsIsSupported) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 225,
      .substream_count = 225,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(225 * 225, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(225), IsOk());
}

TEST(TestValidateAmbisonicsProjection,
     FourteenthOrderAmbisonicsWithCoupledSubstreamsIsSupported) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 225,
      .substream_count = 113,
      .coupled_substream_count = 112,
      .demixing_matrix = std::vector<int16_t>((113 + 112) * 225, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(113), IsOk());
}

TEST(TestValidateAmbisonicsProjection, InvalidOutputChannelCountMaxValue) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 255,
      .substream_count = 255,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(255 * 255, 1)};
  EXPECT_FALSE(ambisonics_projection.Validate(255).ok());
}

TEST(TestValidateAmbisonicsProjection, InvalidOutputChannelCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 3,
      .substream_count = 3,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(3 * 3, 1)};
  EXPECT_FALSE(ambisonics_projection.Validate(3).ok());
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenSubstreamCountIsGreaterThanOutputChannelCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 5,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(4 * 5, 1)};
  EXPECT_FALSE(ambisonics_projection.Validate(5).ok());
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenObuSubstreamCountDoesNotEqualSubstreamCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(4 * 4, 1)};
  const DecodedUleb128 kInconsistentObuSubstreamCount = 3;

  EXPECT_FALSE(
      ambisonics_projection.Validate(kInconsistentObuSubstreamCount).ok());
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenCoupledSubstreamCountIsGreaterThanSubstreamCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 1,
      .coupled_substream_count = 3,
      .demixing_matrix = std::vector<int16_t>((1 + 3) * 4, 1)};

  EXPECT_FALSE(ambisonics_projection.Validate(1).ok());
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenSubstreamCountPlusCoupledSubstreamCountIsTooLarge) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 3,
      .coupled_substream_count = 2,
      .demixing_matrix = std::vector<int16_t>((3 + 2) * 4, 1)};

  EXPECT_FALSE(ambisonics_projection.Validate(3).ok());
}

TEST(TestGetNextValidCount, ReturnsNextHighestCount) {
  uint8_t next_valid_count;
  EXPECT_THAT(
      AmbisonicsConfig::GetNextValidOutputChannelCount(0, next_valid_count),
      IsOk());
  EXPECT_EQ(next_valid_count, 1);
}

TEST(TestGetNextValidCount, SupportsFirstOrderAmbisonics) {
  uint8_t next_valid_count;
  EXPECT_THAT(
      AmbisonicsConfig::GetNextValidOutputChannelCount(4, next_valid_count),
      IsOk());
  EXPECT_EQ(next_valid_count, 4);
}

TEST(TestGetNextValidCount, SupportsFourteenthOrderAmbisonics) {
  uint8_t next_valid_count;
  EXPECT_THAT(
      AmbisonicsConfig::GetNextValidOutputChannelCount(225, next_valid_count),
      IsOk());
  EXPECT_EQ(next_valid_count, 225);
}

TEST(TestGetNextValidCount, InvalidInputTooLarge) {
  uint8_t unused_next_valid_count;
  EXPECT_FALSE(AmbisonicsConfig::GetNextValidOutputChannelCount(
                   226, unused_next_valid_count)
                   .ok());
}

TEST(AudioElementParamEqualOperator, EqualDemixingParamDefinition) {
  AudioElementParam lhs_a{.param_definition = CreateDemixingInfoParamDefinition(
                              DemixingInfoParameterData::kDMixPMode2)};
  AudioElementParam rhs_a{.param_definition = CreateDemixingInfoParamDefinition(
                              DemixingInfoParameterData::kDMixPMode2)};

  EXPECT_EQ(lhs_a, lhs_a);
}

TEST(AudioElementParamEqualOperator, NotEqualDemixingParamDefinition) {
  const auto kLhsDemixingInfoParamData = DemixingInfoParameterData::kDMixPMode2;
  const auto kRhsDemixingInfoParamData =
      DemixingInfoParameterData::kDMixPMode2_n;
  AudioElementParam lhs_a{.param_definition = CreateDemixingInfoParamDefinition(
                              kLhsDemixingInfoParamData)};
  AudioElementParam rhs_a{.param_definition = CreateDemixingInfoParamDefinition(
                              kRhsDemixingInfoParamData)

  };

  EXPECT_NE(lhs_a, rhs_a);
}

TEST(ReadAudioElementParamTest, ValidReconGainParamDefinition) {
  const uint32_t kAudioElementId = 1;
  std::vector<uint8_t> bitstream = {
      ParamDefinition::kParameterDefinitionReconGain,
      // Parameter ID.
      0x00,
      // Parameter Rate.
      0x01,
      // Parameter Definition Mode (upper bit).
      0x00,
      // Duration.
      64,
      // Constant Subblock Duration.
      64};

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  AudioElementParam param;
  EXPECT_THAT(param.ReadAndValidate(kAudioElementId, *buffer), IsOk());
}

TEST(ReadAudioElementParamTest, RejectMixGainParamDefinition) {
  const uint32_t kAudioElementId = 1;
  std::vector<uint8_t> bitstream = {
      ParamDefinition::kParameterDefinitionMixGain,
      // Parameter ID.
      0x00,
      // Parameter Rate.
      0x01,
      // Parameter Definition Mode (upper bit).
      0x00,
      // Duration.
      64,
      // Constant Subblock Duration.
      64};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  AudioElementParam param;
  EXPECT_FALSE(param.ReadAndValidate(kAudioElementId, *buffer).ok());
}

TEST(ReadAudioElementParamTest, ValidDemixingParamDefinition) {
  const uint32_t kAudioElementId = 1;
  std::vector<uint8_t> bitstream = {
      ParamDefinition::kParameterDefinitionDemixing,
      // Parameter ID.
      0x00,
      // Parameter Rate.
      0x01,
      // Parameter Definition Mode (upper bit).
      0x00,
      // Duration.
      64,
      // Constant Subblock Duration.
      64,
      // `dmixp_mode`.
      DemixingInfoParameterData::kDMixPMode2 << 5,
      // `default_w`.
      0};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  AudioElementParam param;
  EXPECT_THAT(param.ReadAndValidate(kAudioElementId, *buffer), IsOk());

  const auto& param_definition =
      std::get<DemixingParamDefinition>(param.param_definition);
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionDemixing);
  EXPECT_EQ(param_definition.default_demixing_info_parameter_data_.dmixp_mode,
            DemixingInfoParameterData::kDMixPMode2);
}

TEST(AudioElementParam, ReadAndValidateReadsReservedParamDefinition3) {
  constexpr uint32_t kAudioElementId = 1;
  constexpr auto kExpectedParamDefinitionType =
      ParamDefinition::kParameterDefinitionReservedStart;
  const std::vector<uint8_t> kExpectedParamDefinitionBytes = {99};
  std::vector<uint8_t> bitstream = {
      ParamDefinition::kParameterDefinitionReservedStart,
      // param_definition_size.
      0x01,
      // param_definition_bytes.
      99};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(bitstream));
  AudioElementParam param;
  EXPECT_THAT(param.ReadAndValidate(kAudioElementId, *buffer), IsOk());

  const auto& param_definition =
      std::get<ExtendedParamDefinition>(param.param_definition);
  EXPECT_EQ(param_definition.GetType(), kExpectedParamDefinitionType);
  EXPECT_EQ(param_definition.param_definition_bytes_,
            kExpectedParamDefinitionBytes);
}

// --- Begin CreateFromBuffer tests ---
TEST(CreateFromBuffer, InvalidWhenPayloadIsEmpty) {
  std::vector<uint8_t> source;
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  EXPECT_FALSE(AudioElementObu::CreateFromBuffer(header, 0, *buffer).ok());
}

TEST(CreateFromBuffer, ScalableChannelConfigMultipleChannelsNoParams) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementChannelBased << 5,
      // `codec_config_id`.
      2,
      // `num_substreams`.
      2,
      // `audio_substream_ids`
      3, 4,
      // `num_parameters`.
      0,
      // `scalable_channel_layout_config`.
      // `num_layers` (3), reserved (5).
      2 << 5,
      // `channel_audio_layer_config[0]`.
      // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
      // `recon_gain_is_present_flag` (1), `reserved` (2).
      ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
      // `substream_count`.
      1,
      // `coupled_substream_count`.
      1,
      // `output_gain_flags` (6) << reserved.
      1 << 2,
      // `output_gain`.
      0, 1,
      // `channel_audio_layer_config[1]`.
      // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
      // `recon_gain_is_present_flag` (1), `reserved` (2).
      ChannelAudioLayerConfig::kLayout5_1_ch << 4 | (1 << 3) | (1 << 2),
      // `substream_count`.
      1,
      // `coupled_substream_count`.
      1,
      // `output_gain_flags` (6) << reserved.
      1 << 2,
      // `output_gain`.
      0, 1};
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  auto obu = AudioElementObu::CreateFromBuffer(header, payload_size, *buffer);

  // Validate
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetAudioElementId(), 1);
  EXPECT_EQ(obu->GetAudioElementType(),
            AudioElementObu::kAudioElementChannelBased);
  EXPECT_EQ(obu->GetNumSubstreams(), 2);
  EXPECT_EQ(obu->audio_substream_ids_[0], 3);
  EXPECT_EQ(obu->audio_substream_ids_[1], 4);
  EXPECT_EQ(obu->GetNumParameters(), 0);
  EXPECT_TRUE(obu->audio_element_params_.empty());

  ScalableChannelLayoutConfig expected_scalable_channel_layout_config = {
      .channel_audio_layer_configs = {
          ChannelAudioLayerConfig{
              .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
              .output_gain_is_present_flag = true,
              .recon_gain_is_present_flag = true,
              .substream_count = 1,
              .coupled_substream_count = 1,
              .output_gain_flag = 1,
              .reserved_b = 0,
              .output_gain = 1},
          ChannelAudioLayerConfig{
              .loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_ch,
              .output_gain_is_present_flag = true,
              .recon_gain_is_present_flag = true,
              .substream_count = 1,
              .coupled_substream_count = 1,
              .output_gain_flag = 1,
              .reserved_b = 0,
              .output_gain = 1}}};
  EXPECT_EQ(std::get<ScalableChannelLayoutConfig>(obu.value().config_),
            expected_scalable_channel_layout_config);
}

TEST(CreateFromBuffer, InvalidMultipleChannelConfigWithBinauralLayout) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementChannelBased << 5,
      // `codec_config_id`.
      2,
      // `num_substreams`.
      2,
      // `audio_substream_ids`
      3, 4,
      // `num_parameters`.
      0,
      // `scalable_channel_layout_config`.
      // `num_layers` (3), reserved (5).
      2 << 5,
      // `channel_audio_layer_config[0]`.
      // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
      // `recon_gain_is_present_flag` (1), `reserved` (2).
      ChannelAudioLayerConfig::kLayoutStereo << 4 | (1 << 3) | (1 << 2),
      // `substream_count`.
      1,
      // `coupled_substream_count`.
      1,
      // `output_gain_flags` (6) << reserved.
      1 << 2,
      // `output_gain`.
      0, 1,
      // `channel_audio_layer_config[1]`.
      // `loudspeaker_layout` (4), `output_gain_is_present_flag` (1),
      // `recon_gain_is_present_flag` (1), `reserved` (2).
      ChannelAudioLayerConfig::kLayoutBinaural << 4 | (0 << 3) | (0 << 2),
      // `substream_count`.
      1,
      // `coupled_substream_count`.
      1};
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  auto obu = AudioElementObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_FALSE(obu.ok());
}

TEST(CreateFromBuffer, ValidAmbisonicsMonoConfig) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,  // Arbitrary.  Doesn't matter for this test.
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementSceneBased << 5,  // Req. for Ambisonics.
      // `codec_config_id`.
      2,  // Arbitrary.  Doesn't matter for this test.
      // `num_substreams`.
      4,  // Matters for validating the AmbisonicsMonoConfig.
      // `audio_substream_ids`
      3, 4, 5, 6,  // Arbitrary IDs, need one per substream.
      // `num_parameters`.
      0,  // Skip parameters, not part of the tested AmbisonicsMonoConfig.

      // Now we're into the fields of the AmbisonicsMonoConfig.
      static_cast<uint8_t>(
          AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeMono),
      4,          // `output_channel_count`
      4,          // `substream_count`
      0, 1, 2, 3  // `channel_mapping`, one per `output_channel_count`.
  };
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  auto obu = AudioElementObu::CreateFromBuffer(header, payload_size, *buffer);

  // Validate
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu.value().GetAudioElementType(),
            AudioElementObu::kAudioElementSceneBased);
  EXPECT_EQ(obu.value().GetNumSubstreams(), 4);

  AmbisonicsMonoConfig expected_ambisonics_mono_config = {
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {0, 1, 2, 3}};
  AmbisonicsConfig expected_ambisonics_config = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono,
      .ambisonics_config = expected_ambisonics_mono_config};
  EXPECT_EQ(std::get<AmbisonicsConfig>(obu.value().config_),
            expected_ambisonics_config);
}

TEST(CreateFromBuffer, InvalidObjectConfigSizeZero) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementObjectBased << 5,
      // `codec_config_id`.
      2,
      // `num_substreams`.
      1,
      // `audio_substream_ids`
      3,
      // `num_parameters`.
      1,
      // `audio_element_params[0]`.
      kParameterDefinitionDemixingAsUint8,
      4,
      5,
      0x00,
      64,
      64,
      0,
      0,
      // `objects_config`
      // `objects_config_size`.
      0,
  };
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  auto obu = AudioElementObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_THAT(obu, Not(IsOk()));
}

TEST(CreateFromBuffer, OneObjectConfigWithExtensionBytes) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementObjectBased << 5,
      // `codec_config_id`.
      2,
      // `num_substreams`.
      1,
      // `audio_substream_ids`
      3,
      // `num_parameters`.
      1,
      // `audio_element_params[0]`.
      kParameterDefinitionDemixingAsUint8,
      4,
      5,
      0x00,
      64,
      64,
      0,
      0,
      // `objects_config`
      // `objects_config_size`.
      4,
      // `num_objects`.
      1,
      // `objects_config_extension_bytes`.
      0x01,
      0x02,
      0x03,
  };
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  auto obu = AudioElementObu::CreateFromBuffer(header, payload_size, *buffer);

  // Validate
  EXPECT_THAT(obu, IsOk());

  EXPECT_EQ(obu.value().GetAudioElementType(),
            AudioElementObu::kAudioElementObjectBased);
  EXPECT_EQ(obu.value().GetNumSubstreams(), 1);

  ObjectsConfig expected_objects_config =
      ObjectsConfig::Create(1, {0x01, 0x02, 0x03}).value();
  EXPECT_EQ(std::get<ObjectsConfig>(obu.value().config_),
            expected_objects_config);
}

TEST(CreateFromBuffer, InvalidTooManyParameters) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,  // Arbitrary.  Doesn't matter for this test.
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementSceneBased << 5,  // Req. for Ambisonics.
      // `codec_config_id`.
      2,  // Arbitrary.  Doesn't matter for this test.
      // `num_substreams`.
      4,  // Matters for validating the AmbisonicsMonoConfig.
      // `audio_substream_ids`
      3, 4, 5,
      6,  // Arbitrary IDs, need one per substream.
      // `num_parameters`
      0x80, 0x80, 0x80, 0x80, 0x0f};
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));

  EXPECT_THAT(
      AudioElementObu::CreateFromBuffer(ObuHeader(), payload_size, *buffer),
      Not(IsOk()));
}

TEST(CreateFromBuffer, ValidAmbisonicsProjectionConfig) {
  std::vector<uint8_t> source = {
      // `audio_element_id`.
      1,  // Arbitrary.  Doesn't matter for this test.
      // `audio_element_type (3), reserved (5).
      AudioElementObu::kAudioElementSceneBased << 5,  // Req. for Ambisonics.
      // `codec_config_id`.
      2,  // Arbitrary.  Doesn't matter for this test.
      // `num_substreams`.
      4,  // Matters for validating the AmbisonicsMonoConfig.
      // `audio_substream_ids`.  Arbitrary IDs, need one per substream.
      3, 4, 5, 6,
      // `num_parameters`.
      0,  // Skip parameters, not part of the tested AmbisonicsMonoConfig.

      // Now we're into the fields of the AmbisonicsMonoConfig.
      static_cast<uint8_t>(
          AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeProjection),
      4,  // `output_channel_count`
      4,  // `substream_count`
      0,  // `coupled_substream_count`
      // We need (`substream_count` + `coupled_substream_count`) *
      // `output_channel_count` values for `demixing matrix`.
      0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06,
      0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x00, 0x0a, 0x00, 0x0b, 0x00, 0x0c,
      0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00, 0x10};
  const int64_t payload_size = source.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  ObuHeader header;
  auto obu = AudioElementObu::CreateFromBuffer(header, payload_size, *buffer);

  // Validate
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu.value().GetAudioElementType(),
            AudioElementObu::kAudioElementSceneBased);
  EXPECT_EQ(obu.value().GetNumSubstreams(), 4);

  AmbisonicsProjectionConfig expected_ambisonics_projection_config = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                          16}};
  AmbisonicsConfig expected_ambisonics_config = {
      .ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection,
      .ambisonics_config = expected_ambisonics_projection_config};
  AmbisonicsConfig actual_ambisonics_config =
      std::get<AmbisonicsConfig>(obu.value().config_);
  EXPECT_EQ(std::get<AmbisonicsConfig>(obu.value().config_),
            expected_ambisonics_config);
}

}  // namespace
}  // namespace iamf_tools
