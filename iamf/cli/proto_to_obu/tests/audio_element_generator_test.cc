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
#include "iamf/cli/proto_to_obu/audio_element_generator.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "src/google/protobuf/repeated_ptr_field.h"
#include "src/google/protobuf/text_format.h"

// TODO(b/296171268): Add more tests for `AudioElementGenerator`.

namespace iamf_tools {
namespace {
using ::absl_testing::IsOk;
using enum ChannelLabel::Label;

constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;

// Based on `output_gain_flags` in
// https://aomediacodec.github.io/iamf/#syntax-scalable-channel-layout-config.
constexpr uint8_t kApplyOutputGainToLeftChannel = 0x20;

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForOneLayerStereo) {
  const std::vector<DecodedUleb128> kSubstreamIds = {99};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {kSubstreamIds[0], {kL2, kR2}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 2, .lfe = 0, .height = 0}};
  const ScalableChannelLayoutConfig kOneLayerStereoConfig{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kOneLayerStereoConfig, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     InvalidWhenSubstreamCountIsInconsistent) {
  constexpr uint8_t kInvalidOneLayerStereoSubstreamCount = 2;
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig
      kInvalidOneLayerStereoWithoutCoupledSubstreams{
          .num_layers = 1,
          .channel_audio_layer_configs = {
              {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
               .output_gain_is_present_flag = false,
               .substream_count = kInvalidOneLayerStereoSubstreamCount,
               .coupled_substream_count = 1}}};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds,
                   kInvalidOneLayerStereoWithoutCoupledSubstreams,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

TEST(FinalizeScalableChannelLayoutConfig,
     InvalidWhenCoupledSubstreamCountIsInconsistent) {
  constexpr uint8_t kInvalidOneLayerStereoCoupledSubstreamCount = 0;
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig
      kInvalidOneLayerStereoWithoutCoupledSubstreams{
          .num_layers = 1,
          .channel_audio_layer_configs = {
              {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
               .output_gain_is_present_flag = false,
               .substream_count = 1,
               .coupled_substream_count =
                   kInvalidOneLayerStereoCoupledSubstreamCount}}};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds,
                   kInvalidOneLayerStereoWithoutCoupledSubstreams,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForTwoLayerMonoStereo) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{0, {kMono}},
                                                             {1, {kL2}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 1, .lfe = 0, .height = 0},
      {.surround = 2, .lfe = 0, .height = 0}};
  const ScalableChannelLayoutConfig kTwoLayerMonoStereoConfig{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0},
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kSubstreamIds, kTwoLayerMonoStereoConfig,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     InvalidWhenSubsequenceLayersAreLower) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const ScalableChannelLayoutConfig kInvalidWithMonoLayerAfterStereo{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0},
      }};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds, kInvalidWithMonoLayerAfterStereo,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

TEST(FinalizeScalableChannelLayoutConfig, FillsOutputGainMap) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{0, {kMono}},
                                                             {1, {kL2}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 1, .lfe = 0, .height = 0},
      {.surround = 2, .lfe = 0, .height = 0}};
  const ScalableChannelLayoutConfig kTwoLayerStereoConfig{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0},
          {
              .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
              .output_gain_is_present_flag = true,
              .substream_count = 1,
              .coupled_substream_count = 0,
              .output_gain_flag = kApplyOutputGainToLeftChannel,
              .reserved_b = 0,
              .output_gain = std::numeric_limits<int16_t>::min(),
          },
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kTwoLayerStereoConfig, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  ASSERT_TRUE(output_label_to_output_gain.contains(kL2));
  EXPECT_FLOAT_EQ(output_label_to_output_gain.at(kL2), -128.0);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForTwoLayerStereo3_1_2) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1, 2, 3};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kL2, kR2}},
      {1, {kLtf3, kRtf3}},
      {2, {kCentre}},
      {3, {kLFE}},
  };
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 2, .lfe = 0, .height = 0},
      {.surround = 3, .lfe = 1, .height = 2}};
  const ScalableChannelLayoutConfig kTwoLayerStereo3_1_2Config{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout3_1_2_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 3,
           .coupled_substream_count = 1},
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kSubstreamIds, kTwoLayerStereo3_1_2Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForTwoLayer3_1_2And5_1_2) {
  const std::vector<DecodedUleb128> kSubstreamIds = {300, 301, 302, 303, 514};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {300, {kL3, kR3}}, {301, {kLtf3, kRtf3}}, {302, {kCentre}},
      {303, {kLFE}},     {514, {kL5, kR5}},
  };
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 3, .lfe = 1, .height = 2},
      {.surround = 5, .lfe = 1, .height = 2}};
  const ScalableChannelLayoutConfig kTwoLayer3_1_2_and_5_1_2Config{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout3_1_2_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 4,
           .coupled_substream_count = 2},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_2_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1},
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kSubstreamIds, kTwoLayer3_1_2_and_5_1_2Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForTwoLayer5_1_0And7_1_0) {
  const std::vector<DecodedUleb128> kSubstreamIds = {500, 501, 502, 503, 704};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {500, {kL5, kR5}}, {501, {kLs5, kRs5}},   {502, {kCentre}},
      {503, {kLFE}},     {704, {kLss7, kRss7}},
  };
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 5, .lfe = 1, .height = 0},
      {.surround = 7, .lfe = 1, .height = 0}};
  const ScalableChannelLayoutConfig kTwoLayer5_1_0_and_7_1_0Config{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 4,
           .coupled_substream_count = 2},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout7_1_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1},
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kSubstreamIds, kTwoLayer5_1_0_and_7_1_0Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForOneLayer5_1_4) {
  const std::vector<DecodedUleb128> kSubstreamIds = {55, 77, 66, 11, 22, 88};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {55, {kL5, kR5}},     {77, {kLs5, kRs5}}, {66, {kLtf4, kRtf4}},
      {11, {kLtb4, kRtb4}}, {22, {kCentre}},    {88, {kLFE}}};

  const LabelGainMap kExpectedLabelToOutputGain = {};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 5, .lfe = 1, .height = 4}};
  const std::vector<DecodedUleb128> kAudioSubstreamIds = kSubstreamIds;
  const ScalableChannelLayoutConfig kOneLayer5_1_4Config{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_4_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 6,
           .coupled_substream_count = 4}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kAudioSubstreamIds, kOneLayer5_1_4Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForTwoLayer5_1_2And5_1_4) {
  const std::vector<DecodedUleb128> kSubstreamIds = {520, 521, 522,
                                                     523, 524, 540};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {520, {kL5, kR5}}, {521, {kLs5, kRs5}}, {522, {kLtf2, kRtf2}},
      {523, {kCentre}},  {524, {kLFE}},       {540, {kLtf4, kRtf4}},
  };
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 5, .lfe = 1, .height = 2},
      {.surround = 5, .lfe = 1, .height = 4}};
  const ScalableChannelLayoutConfig kTwoLayer5_1_2_and_5_1_4Config{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_2_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 5,
           .coupled_substream_count = 3},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout5_1_4_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1},
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kSubstreamIds, kTwoLayer5_1_2_and_5_1_4Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForTwoLayer7_1_0And7_1_4) {
  const std::vector<DecodedUleb128> kSubstreamIds = {700, 701, 702, 703,
                                                     704, 740, 741};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {700, {kL7, kR7}},     {701, {kLss7, kRss7}}, {702, {kLrs7, kRrs7}},
      {703, {kCentre}},      {704, {kLFE}},         {740, {kLtf4, kRtf4}},
      {741, {kLtb4, kRtb4}},
  };
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 7, .lfe = 1, .height = 0},
      {.surround = 7, .lfe = 1, .height = 4}};
  const ScalableChannelLayoutConfig kTwoLayer7_1_0_and_7_1_4Config{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout7_1_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 5,
           .coupled_substream_count = 3},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout7_1_4_ch,
           .output_gain_is_present_flag = false,

           .substream_count = 2,
           .coupled_substream_count = 2},
      }};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kSubstreamIds, kTwoLayer7_1_0_and_7_1_4Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForForOneLayer7_1_4) {
  const std::vector<DecodedUleb128> kSubstreamIds = {6, 5, 4, 3, 2, 1, 0};
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {6, {kL7, kR7}},     {5, {kLss7, kRss7}}, {4, {kLrs7, kRrs7}},
      {3, {kLtf4, kRtf4}}, {2, {kLtb4, kRtb4}}, {1, {kCentre}},
      {0, {kLFE}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 7, .lfe = 1, .height = 4}};
  const std::vector<DecodedUleb128> kAudioSubstreamIds = kSubstreamIds;
  const ScalableChannelLayoutConfig kOneLayer7_1_4Config{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayout7_1_4_ch,
           .output_gain_is_present_flag = false,
           .substream_count = 7,
           .coupled_substream_count = 5}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                  kAudioSubstreamIds, kOneLayer7_1_4Config,
                  output_substream_id_to_labels, output_label_to_output_gain,
                  output_channel_numbers_for_layer),
              IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig, InvalidWithReservedLayout14) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kOneLayerReserved14Layout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved14,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1}}};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds, kOneLayerReserved14Layout,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutLFE) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{0, {kLFE}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 0, .lfe = 1, .height = 0}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kLFELayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutLFE}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kLFELayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutStereoS) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{0, {kLs5, kRs5}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 2, .lfe = 0, .height = 0}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kStereoSSLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutStereoS}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kStereoSSLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutStereoSS) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kLss7, kRss7}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 2, .lfe = 0, .height = 0}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kStereoSSLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutStereoSS}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kStereoSSLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutStereoTf) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kLtf4, kRtf4}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 0, .lfe = 0, .height = 2}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kStereoTfLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutStereoTF}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kStereoTfLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutStereoTB) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kLtb4, kRtb4}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 0, .lfe = 0, .height = 2}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kStereoTBLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutStereoTB}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kStereoTBLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutTop4Ch) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kLtf4, kRtf4}}, {1, {kLtb4, kRtb4}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 0, .lfe = 0, .height = 4}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const ScalableChannelLayoutConfig kTop4ChLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 2,
           .coupled_substream_count = 2,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutTop4Ch}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kTop4ChLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayout3_0_Ch) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{0, {kL7, kR7}},
                                                             {1, {kCentre}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 3, .lfe = 0, .height = 0}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const ScalableChannelLayoutConfig k3_0_ChLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 2,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayout3_0_ch}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, k3_0_ChLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayout9_1_6) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kFLc, kFRc}},   {1, {kFL, kFR}},     {2, {kSiL, kSiR}},
      {3, {kBL, kBR}},     {4, {kTpFL, kTpFR}}, {5, {kTpSiL, kTpSiR}},
      {6, {kTpBL, kTpBR}}, {7, {kFC}},          {8, {kLFE}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 9, .lfe = 1, .height = 6}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const ScalableChannelLayoutConfig k9_1_6Layout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 9,
           .coupled_substream_count = 7,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayout9_1_6_ch}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, k9_1_6Layout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutStereoTpSi) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kTpSiL, kTpSiR}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 0, .lfe = 0, .height = 2}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0};
  const ScalableChannelLayoutConfig kTpSiLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutStereoTpSi}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kTpSiLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     FillsExpectedOutputForExpandedLoudspeakerLayoutTop6_Ch) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {
      {0, {kTpFL, kTpFR}}, {1, {kTpSiL, kTpSiR}}, {2, {kTpBL, kTpBR}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 0, .lfe = 0, .height = 6}};
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1, 2};
  const ScalableChannelLayoutConfig kTop6ChLayout{
      .num_layers = 1,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 3,
           .coupled_substream_count = 3,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutTop6Ch}}};
  SubstreamIdLabelsMap output_substream_id_to_labels;
  LabelGainMap output_label_to_output_gain;
  std::vector<ChannelNumbers> output_channel_numbers_for_layer;

  EXPECT_THAT(
      AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
          kSubstreamIds, kTop6ChLayout, output_substream_id_to_labels,
          output_label_to_output_gain, output_channel_numbers_for_layer),
      IsOk());

  EXPECT_EQ(output_substream_id_to_labels, kExpectedSubstreamIdToLabels);
  EXPECT_TRUE(output_label_to_output_gain.empty());
  EXPECT_EQ(output_channel_numbers_for_layer, kExpectedChannelNumbersForLayer);
}

TEST(FinalizeScalableChannelLayoutConfig,
     InvalidWhenThereAreTwoLayersWithExpandedLoudspeakerLayout) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const ScalableChannelLayoutConfig
      kInvalidWithFirstLayerExpandedAndAnotherSecondLayer{
          .num_layers = 2,
          .channel_audio_layer_configs = {
              {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
               .output_gain_is_present_flag = false,
               .substream_count = 1,
               .coupled_substream_count = 0,
               .expanded_loudspeaker_layout =
                   ChannelAudioLayerConfig::kExpandedLayoutLFE},
              {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
               .output_gain_is_present_flag = false,
               .substream_count = 1,
               .coupled_substream_count = 1}}};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds,
                   kInvalidWithFirstLayerExpandedAndAnotherSecondLayer,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

TEST(FinalizeScalableChannelLayoutConfig,
     InvalidWhenSecondLayerIsExpandedLayout) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1};
  const ScalableChannelLayoutConfig kInvalidWithSecondLayerExpandedLayout{
      .num_layers = 2,
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 1},
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .output_gain_is_present_flag = false,
           .substream_count = 1,
           .coupled_substream_count = 0,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutLFE}}};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds, kInvalidWithSecondLayerExpandedLayout,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

TEST(FinalizeScalableChannelLayoutConfig,
     InvalidWithExpandedLoudspeakerLayoutIsInconsistent) {
  const std::vector<DecodedUleb128> kSubstreamIds = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  const ScalableChannelLayoutConfig
      kInvaliWithInconsistentExpandedLoudspeakerLayout{
          .num_layers = 1,
          .channel_audio_layer_configs = {
              {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
               .output_gain_is_present_flag = false,
               .substream_count = 9,
               .coupled_substream_count = 7,
               .expanded_loudspeaker_layout = std::nullopt}}};
  SubstreamIdLabelsMap unused_substream_id_to_labels;
  LabelGainMap unused_label_to_output_gain;
  std::vector<ChannelNumbers> unused_channel_numbers_for_layer;

  EXPECT_FALSE(AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
                   kSubstreamIds,
                   kInvaliWithInconsistentExpandedLoudspeakerLayout,
                   unused_substream_id_to_labels, unused_label_to_output_gain,
                   unused_channel_numbers_for_layer)
                   .ok());
}

class AudioElementGeneratorTest : public ::testing::Test {
 public:
  AudioElementGeneratorTest() {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000,
                                          codec_config_obus_);
  }

  void InitAndTestGenerate() {
    AudioElementGenerator generator(audio_element_metadata_);

    EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

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

TEST_F(AudioElementGeneratorTest, FillsAudioElementWithDataFields) {
  const SubstreamIdLabelsMap kExpectedSubstreamIdToLabels = {{99, {kMono}},
                                                             {100, {kL2}}};
  const std::vector<ChannelNumbers> kExpectedChannelNumbersForLayer = {
      {.surround = 1, .lfe = 0, .height = 0},
      {.surround = 2, .lfe = 0, .height = 0}};
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 300
        audio_element_type: AUDIO_ELEMENT_CHANNEL_BASED
        reserved: 0
        codec_config_id: 200
        num_substreams: 2
        audio_substream_ids: [ 99, 100 ]
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
      audio_element_metadata_.Add()));
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

  EXPECT_FALSE(generator.Generate(codec_config_obus_, output_obus_).ok());
}

TEST_F(AudioElementGeneratorTest, DefaultLoudspeakerLayoutIsNotSupported) {
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
  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

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
  EXPECT_THAT(generator.Generate(codec_config_obus_, output_obus_), IsOk());

  const auto& obu = output_obus_.at(kAudioElementId).obu;
  EXPECT_EQ(obu.audio_element_params_.size(), 1);
  ASSERT_FALSE(obu.audio_element_params_.empty());
  EXPECT_EQ(output_obus_.at(kAudioElementId).obu.audio_element_params_.front(),
            kExpectedAudioElementParam);
}

}  // namespace
}  // namespace iamf_tools
