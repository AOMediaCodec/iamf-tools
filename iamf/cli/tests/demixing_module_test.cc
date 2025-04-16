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
#include "iamf/cli/demixing_module.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <list>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using enum ChannelLabel::Label;
using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointwise;

constexpr DecodedUleb128 kAudioElementId = 137;
constexpr std::array<uint8_t, 12> kReconGainValues = {
    255, 0, 125, 200, 150, 255, 255, 255, 255, 255, 255, 255};
constexpr uint32_t kZeroSamplesToTrimAtEnd = 0;
constexpr uint32_t kZeroSamplesToTrimAtStart = 0;
constexpr InternalTimestamp kStartTimestamp = 0;
constexpr InternalTimestamp kEndTimestamp = 4;
constexpr DecodedUleb128 kMonoSubstreamId = 0;
constexpr DecodedUleb128 kL2SubstreamId = 1;
constexpr DecodedUleb128 kStereoSubstreamId = 2;

// TODO(b/305927287): Test computation of linear output gains. Test some cases
//                    of erroneous input.

TEST(FindSamplesOrDemixedSamples, FindsMatchingSamples) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{kL2, kSamplesToFind}};

  const std::vector<InternalSampleType>* found_samples;
  EXPECT_THAT(DemixingModule::FindSamplesOrDemixedSamples(kL2, kLabelToSamples,
                                                          &found_samples),
              IsOk());
  EXPECT_THAT(*found_samples, Pointwise(DoubleEq(), kSamplesToFind));
}

TEST(FindSamplesOrDemixedSamples, FindsMatchingDemixedSamples) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{kDemixedR2, kSamplesToFind}};

  const std::vector<InternalSampleType>* found_samples;
  EXPECT_THAT(DemixingModule::FindSamplesOrDemixedSamples(kR2, kLabelToSamples,
                                                          &found_samples),
              IsOk());
  EXPECT_THAT(*found_samples, Pointwise(DoubleEq(), kSamplesToFind));
}

TEST(FindSamplesOrDemixedSamples, InvalidWhenThereIsNoDemixingLabel) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{kDemixedR2, kSamplesToFind}};

  const std::vector<InternalSampleType>* found_samples;
  EXPECT_FALSE(DemixingModule::FindSamplesOrDemixedSamples(kL2, kLabelToSamples,
                                                           &found_samples)
                   .ok());
}

TEST(FindSamplesOrDemixedSamples, RegularSamplesTakePrecedence) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const std::vector<InternalSampleType> kDemixedSamplesToIgnore = {4, 5, 6};
  const LabelSamplesMap kLabelToSamples = {
      {kR2, kSamplesToFind}, {kDemixedR2, kDemixedSamplesToIgnore}};
  const std::vector<InternalSampleType>* found_samples;
  EXPECT_THAT(DemixingModule::FindSamplesOrDemixedSamples(kR2, kLabelToSamples,
                                                          &found_samples),
              IsOk());
  EXPECT_THAT(*found_samples, Pointwise(DoubleEq(), kSamplesToFind));
}

TEST(FindSamplesOrDemixedSamples, ErrorNoMatchingSamples) {
  const std::vector<InternalSampleType> kSamplesToFind = {1, 2, 3};
  const LabelSamplesMap kLabelToSamples = {{kL2, kSamplesToFind}};

  const std::vector<InternalSampleType>* found_samples;
  EXPECT_FALSE(DemixingModule::FindSamplesOrDemixedSamples(kL3, kLabelToSamples,
                                                           &found_samples)
                   .ok());
}

void InitAudioElementWithLabelsAndLayers(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const std::vector<ChannelAudioLayerConfig::LoudspeakerLayout>&
        loudspeaker_layouts,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements) {
  auto [iter, unused_inserted] = audio_elements.emplace(
      kAudioElementId,
      AudioElementWithData{
          .obu = AudioElementObu(ObuHeader(), kAudioElementId,
                                 AudioElementObu::kAudioElementChannelBased,
                                 /*reserved=*/0,
                                 /*codec_config_id=*/0),
          .substream_id_to_labels = substream_id_to_labels,
      });
  auto& obu = iter->second.obu;
  ASSERT_THAT(
      obu.InitializeScalableChannelLayout(loudspeaker_layouts.size(), 0),
      IsOk());
  auto& config = std::get<ScalableChannelLayoutConfig>(obu.config_);
  for (int i = 0; i < loudspeaker_layouts.size(); ++i) {
    config.channel_audio_layer_configs[i].loudspeaker_layout =
        loudspeaker_layouts[i];
  }
}

TEST(CreateForDownMixingAndReconstruction, EmptyConfigMapIsOk) {
  absl::flat_hash_map<DecodedUleb128,
                      DemixingModule::DownmixingAndReconstructionConfig>
      id_to_config_map;
  const auto demixing_module =
      DemixingModule::CreateForDownMixingAndReconstruction(
          std::move(id_to_config_map));
  EXPECT_THAT(demixing_module, IsOk());
}

TEST(CreateForDownMixingAndReconstruction, ValidWithTwoLayerStereo) {
  DecodedUleb128 id = 137;
  DemixingModule::DownmixingAndReconstructionConfig config = {
      .user_labels = {kL2, kR2},
      .substream_id_to_labels = {{0, {kMono}}, {1, {kL2}}},
      .label_to_output_gain = {}};
  absl::flat_hash_map<DecodedUleb128,
                      DemixingModule::DownmixingAndReconstructionConfig>
      id_to_config_map = {{id, config}};
  const auto demixing_module =
      DemixingModule::CreateForDownMixingAndReconstruction(
          std::move(id_to_config_map));
  EXPECT_THAT(demixing_module, IsOk());
}

TEST(InitializeForReconstruction, NeverCreatesDownMixers) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kMono}}, {1, {kL2}}},
                                      {ChannelAudioLayerConfig::kLayoutMono,
                                       ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  const std::list<Demixer>* down_mixers = nullptr;
  EXPECT_THAT(demixing_module->GetDownMixers(kAudioElementId, down_mixers),
              IsOk());
  EXPECT_TRUE(down_mixers->empty());
}

TEST(CreateForReconstruction, CreatesOneDemixerForTwoLayerStereo) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kMono}}, {1, {kL2}}},
                                      {ChannelAudioLayerConfig::kLayoutMono,
                                       ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  const std::list<Demixer>* demixer = nullptr;
  EXPECT_THAT(demixing_module->GetDemixers(kAudioElementId, demixer), IsOk());
  EXPECT_EQ(demixer->size(), 1);
}

TEST(CreateForReconstruction, FailsForReservedLayout14) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{0, {kOmitted}}}, {ChannelAudioLayerConfig::kLayoutReserved14},
      audio_elements);

  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);

  EXPECT_FALSE(demixing_module.ok());
}

TEST(CreateForReconstruction, ValidForExpandedLayoutLFE) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{0, {kLFE}}}, {ChannelAudioLayerConfig::kLayoutExpanded},
      audio_elements);
  std::get<ScalableChannelLayoutConfig>(
      audio_elements.at(kAudioElementId).obu.config_)
      .channel_audio_layer_configs[0]
      .expanded_loudspeaker_layout =
      ChannelAudioLayerConfig::kExpandedLayoutLFE;

  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);

  EXPECT_THAT(demixing_module, IsOk());
}

TEST(CreateForReconstruction, CreatesNoDemixersForSingleLayerChannelBased) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kL2, kR2}}},
                                      {ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  const std::list<Demixer>* demixer = nullptr;
  EXPECT_THAT(demixing_module->GetDemixers(kAudioElementId, demixer), IsOk());
  EXPECT_TRUE(demixer->empty());
}

TEST(CreateForReconstruction, CreatesNoDemixersForAmbisonics) {
  const DecodedUleb128 kCodecConfigId = 0;
  constexpr std::array<DecodedUleb128, 4> kAmbisonicsSubstreamIds{0, 1, 2, 3};
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000, codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                kAmbisonicsSubstreamIds,
                                                codec_configs, audio_elements);

  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  const std::list<Demixer>* demixer = nullptr;
  EXPECT_THAT(demixing_module->GetDemixers(kAudioElementId, demixer), IsOk());
  EXPECT_TRUE(demixer->empty());
}

TEST(DemixOriginalAudioSamples, ReturnsErrorAfterCreateForReconstruction) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  EXPECT_THAT(demixing_module->DemixOriginalAudioSamples({}), Not(IsOk()));
}

TEST(DemixDecodedAudioSamples, OutputContainsOriginalAndDemixedSamples) {
  const std::vector<std::vector<int32_t>> kDecodedSamples = {{0}};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  std::list<DecodedAudioFrame> decoded_audio_frames;
  decoded_audio_frames.push_back(
      DecodedAudioFrame{.substream_id = kMonoSubstreamId,
                        .start_timestamp = kStartTimestamp,
                        .end_timestamp = kEndTimestamp,
                        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                        .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
                        .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(
      DecodedAudioFrame{.substream_id = kL2SubstreamId,
                        .start_timestamp = kStartTimestamp,
                        .end_timestamp = kEndTimestamp,
                        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                        .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
                        .down_mixing_params = DownMixingParams()});
  auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());
  const auto id_to_labeled_decoded_frame =
      demixing_module->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kL2));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kMono));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kDemixedR2));
}

TEST(DemixDecodedAudioSamples, ReturnsErrorWhenChannelCountsMismatch) {
  // Configure a stereo audio element. We'd typically expected audio frames to
  // have two channels.
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{kStereoSubstreamId, {kL2, kR2}}},
                                      {ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());
  std::list<DecodedAudioFrame> decoded_audio_frames;
  // The decoded audio frame has one channel, which is inconsistent with a
  // one-layer stereo audio element.
  const std::vector<int32_t> kErrorOneChannel = {0};
  decoded_audio_frames.push_back(
      DecodedAudioFrame{.substream_id = kStereoSubstreamId,
                        .start_timestamp = kStartTimestamp,
                        .end_timestamp = kEndTimestamp,
                        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                        .decoded_samples = {kErrorOneChannel},
                        .down_mixing_params = DownMixingParams()});

  // Demixing gracefully fails, as we can't determine the missing channel.
  EXPECT_THAT(demixing_module->DemixDecodedAudioSamples(decoded_audio_frames),
              Not(IsOk()));
}

TEST(DemixDecodedAudioSamples, OutputEchoesTimingInformation) {
  // These values are not very sensible, but as long as they are consistent
  // between related frames it is OK.
  const DecodedUleb128 kExpectedStartTimestamp = 99;
  const DecodedUleb128 kExpectedEndTimestamp = 123;
  const DecodedUleb128 kExpectedNumSamplesToTrimAtEnd = 999;
  const DecodedUleb128 kExpectedNumSamplesToTrimAtStart = 9999;
  const DecodedUleb128 kL2SubstreamId = 1;
  const std::vector<std::vector<int32_t>> kDecodedSamples = {{0}};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  std::list<DecodedAudioFrame> decoded_audio_frames;
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kMonoSubstreamId,
      .start_timestamp = kExpectedStartTimestamp,
      .end_timestamp = kExpectedEndTimestamp,
      .samples_to_trim_at_end = kExpectedNumSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kExpectedNumSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kL2SubstreamId,
      .start_timestamp = kExpectedStartTimestamp,
      .end_timestamp = kExpectedEndTimestamp,
      .samples_to_trim_at_end = kExpectedNumSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kExpectedNumSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  const auto id_to_labeled_decoded_frame =
      demixing_module->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  EXPECT_EQ(labeled_frame.end_timestamp, kExpectedEndTimestamp);
  EXPECT_EQ(labeled_frame.samples_to_trim_at_end,
            kExpectedNumSamplesToTrimAtEnd);
  EXPECT_EQ(labeled_frame.samples_to_trim_at_start,
            kExpectedNumSamplesToTrimAtStart);
}

TEST(DemixDecodedAudioSamples, OutputEchoesOriginalLabels) {
  const std::vector<std::vector<int32_t>> kDecodedMonoSamples = {{1}, {2}, {3}};
  const std::vector<std::vector<int32_t>> kDecodedL2Samples = {{9}, {10}, {11}};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  std::list<DecodedAudioFrame> decoded_audio_frames;
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kMonoSubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedMonoSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kL2SubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedL2Samples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  IdLabeledFrameMap unused_id_labeled_frame;
  const auto id_to_labeled_decoded_frame =
      demixing_module->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  // Examine the demixed frame.
  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  constexpr std::array<int32_t, 3> kExpectedMonoSamples = {1, 2, 3};
  constexpr std::array<int32_t, 3> kExpectedL2Samples = {9, 10, 11};
  EXPECT_THAT(
      labeled_frame.label_to_samples.at(kMono),
      Pointwise(InternalSampleMatchesIntegralSample(), kExpectedMonoSamples));
  EXPECT_THAT(
      labeled_frame.label_to_samples.at(kL2),
      Pointwise(InternalSampleMatchesIntegralSample(), kExpectedL2Samples));
}

TEST(DemixDecodedAudioSamples, OutputHasReconstructedLayers) {
  const std::vector<std::vector<int32_t>> kDecodedMonoSamples = {{750}};
  const std::vector<std::vector<int32_t>> kDecodedL2Samples = {{1000}};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;

  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  std::list<DecodedAudioFrame> decoded_audio_frames;
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kMonoSubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedMonoSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kL2SubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedL2Samples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());

  const auto id_to_labeled_decoded_frame =
      demixing_module->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  // Examine the demixed frame.
  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  EXPECT_THAT(labeled_frame.label_to_samples.at(kDemixedR2),
              Pointwise(InternalSampleMatchesIntegralSample(), {500}));
}

TEST(DemixDecodedAudioSamples, OutputContainsReconGainAndLayerInfo) {
  const std::vector<std::vector<int32_t>> kDecodedSamples = {{0}};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  std::list<DecodedAudioFrame> decoded_audio_frames;
  ReconGainInfoParameterData recon_gain_info_parameter_data;
  recon_gain_info_parameter_data.recon_gain_elements.push_back(ReconGainElement{
      .recon_gain_flag = DecodedUleb128(1), .recon_gain = kReconGainValues});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kMonoSubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams(),
      .recon_gain_info_parameter_data = recon_gain_info_parameter_data,
      .audio_element_with_data = &audio_elements.at(kAudioElementId)});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kL2SubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams(),
      .recon_gain_info_parameter_data = recon_gain_info_parameter_data,
      .audio_element_with_data = &audio_elements.at(kAudioElementId)});
  const auto demixing_module =
      DemixingModule::CreateForReconstruction(audio_elements);
  ASSERT_THAT(demixing_module, IsOk());
  const auto id_to_labeled_decoded_frame =
      demixing_module->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kL2));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kMono));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kDemixedR2));

  EXPECT_EQ(
      labeled_frame.recon_gain_info_parameter_data.recon_gain_elements.size(),
      1);
  const auto& recon_gain_element =
      labeled_frame.recon_gain_info_parameter_data.recon_gain_elements.at(0);
  ASSERT_TRUE(recon_gain_element.has_value());
  EXPECT_EQ(recon_gain_element->recon_gain_flag, DecodedUleb128(1));
  EXPECT_THAT(recon_gain_element->recon_gain,
              testing::ElementsAreArray(kReconGainValues));
  EXPECT_EQ(labeled_frame.loudspeaker_layout_per_layer.size(), 2);
  EXPECT_THAT(labeled_frame.loudspeaker_layout_per_layer,
              testing::ElementsAre(ChannelAudioLayerConfig::kLayoutMono,
                                   ChannelAudioLayerConfig::kLayoutStereo));
}

class DemixingModuleTestBase {
 public:
  DemixingModuleTestBase() {
    audio_frame_metadata_.set_audio_element_id(kAudioElementId);
  }

  void CreateDemixingModuleExpectOk() {
    iamf_tools_cli_proto::UserMetadata user_metadata;
    *user_metadata.add_audio_frame_metadata() = audio_frame_metadata_;
    audio_elements_.emplace(
        kAudioElementId,
        AudioElementWithData{
            .obu = AudioElementObu(ObuHeader(), kAudioElementId,
                                   AudioElementObu::kAudioElementChannelBased,
                                   /*reserved=*/0,
                                   /*codec_config_id=*/0),
            .substream_id_to_labels = substream_id_to_labels_,
        });
    const absl::StatusOr<absl::flat_hash_map<
        DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
        audio_element_id_to_demixing_metadata =
            CreateAudioElementIdToDemixingMetadata(user_metadata,
                                                   audio_elements_);
    ASSERT_THAT(audio_element_id_to_demixing_metadata.status(), IsOk());
    auto demixing_module = DemixingModule::CreateForDownMixingAndReconstruction(
        std::move(audio_element_id_to_demixing_metadata.value()));
    ASSERT_THAT(demixing_module, IsOk());
    demixing_module_.emplace(*std::move(demixing_module));
  }

  void TestCreateDemixingModule(int expected_number_of_down_mixers) {
    CreateDemixingModuleExpectOk();
    const std::list<Demixer>* down_mixers = nullptr;
    const std::list<Demixer>* demixers = nullptr;

    ASSERT_THAT(demixing_module_->GetDownMixers(kAudioElementId, down_mixers),
                IsOk());
    ASSERT_THAT(demixing_module_->GetDemixers(kAudioElementId, demixers),
                IsOk());
    EXPECT_EQ(down_mixers->size(), expected_number_of_down_mixers);
    EXPECT_EQ(demixers->size(), expected_number_of_down_mixers);
  }

 protected:
  void ConfigureAudioFrameMetadata(
      absl::Span<const ChannelLabel::Label> labels) {
    for (const auto& label : labels) {
      auto proto_label = ChannelLabelUtils::LabelToProto(label);
      ASSERT_TRUE(proto_label.ok());
      audio_frame_metadata_.add_channel_metadatas()->set_channel_label(
          *proto_label);
    }
  }

  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  SubstreamIdLabelsMap substream_id_to_labels_;

  // Held in `std::optional` for delayed construction.
  std::optional<DemixingModule> demixing_module_;
};

class DownMixingModuleTest : public DemixingModuleTestBase,
                             public ::testing::Test {
 protected:
  void TestDownMixing(const DownMixingParams& down_mixing_params,
                      int expected_number_of_down_mixers) {
    TestCreateDemixingModule(expected_number_of_down_mixers);

    EXPECT_THAT(demixing_module_->DownMixSamplesToSubstreams(
                    kAudioElementId, down_mixing_params,
                    input_label_to_samples_, substream_id_to_substream_data_),
                IsOk());

    for (const auto& [substream_id, substream_data] :
         substream_id_to_substream_data_) {
      // Copy the output queue to a vector for comparison.
      std::vector<std::vector<int32_t>> output_samples;
      std::copy(substream_data.samples_obu.begin(),
                substream_data.samples_obu.end(),
                std::back_inserter(output_samples));
      EXPECT_EQ(output_samples,
                substream_id_to_expected_samples_[substream_id]);
    }
  }

  void ConfigureInputChannel(ChannelLabel::Label label,
                             absl::Span<const int32_t> input_samples) {
    ConfigureAudioFrameMetadata({label});

    auto [iter, inserted] = input_label_to_samples_.emplace(
        label, std::vector<InternalSampleType>(input_samples.size(), 0));
    Int32ToInternalSampleType(input_samples, absl::MakeSpan(iter->second));
    // This function should not be called with the same label twice.
    ASSERT_TRUE(inserted);
  }

  void ConfigureOutputChannel(
      const std::list<ChannelLabel::Label>& requested_output_labels,
      const std::vector<std::vector<int32_t>>& expected_output_smples) {
    // The substream ID itself does not matter. Generate a unique one.
    const uint32_t substream_id = substream_id_to_labels_.size();

    substream_id_to_labels_[substream_id] = requested_output_labels;
    substream_id_to_substream_data_[substream_id] = {.substream_id =
                                                         substream_id};

    substream_id_to_expected_samples_[substream_id] = expected_output_smples;
  }

  LabelSamplesMap input_label_to_samples_;

  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data_;

  absl::flat_hash_map<uint32_t, std::vector<std::vector<int32_t>>>
      substream_id_to_expected_samples_;
};

TEST_F(DownMixingModuleTest, OneLayerStereoHasNoDownMixers) {
  ConfigureInputChannel(kL2, {});
  ConfigureInputChannel(kR2, {});

  ConfigureOutputChannel({kL2, kR2}, {{}});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, OneLayer7_1_4HasNoDownMixers) {
  // Initialize arguments for single layer 7.1.4.
  ConfigureInputChannel(kL7, {});
  ConfigureInputChannel(kR7, {});
  ConfigureInputChannel(kCentre, {});
  ConfigureInputChannel(kLFE, {});
  ConfigureInputChannel(kLss7, {});
  ConfigureInputChannel(kRss7, {});
  ConfigureInputChannel(kLrs7, {});
  ConfigureInputChannel(kRrs7, {});
  ConfigureInputChannel(kLtf4, {});
  ConfigureInputChannel(kRtf4, {});
  ConfigureInputChannel(kLtb4, {});
  ConfigureInputChannel(kRtb4, {});

  ConfigureOutputChannel({kCentre}, {{}});
  ConfigureOutputChannel({kL7, kR7}, {});
  ConfigureOutputChannel({kLss7, kRss7}, {});
  ConfigureOutputChannel({kLrs7, kRrs7}, {});
  ConfigureOutputChannel({kLtf4, kRtf4}, {});
  ConfigureOutputChannel({kLtb4, kRtb4}, {});
  ConfigureOutputChannel({kLFE}, {});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, AmbisonicsHasNoDownMixers) {
  ConfigureInputChannel(kA0, {});
  ConfigureInputChannel(kA1, {});
  ConfigureInputChannel(kA2, {});
  ConfigureInputChannel(kA3, {});

  ConfigureOutputChannel({kA0}, {{}});
  ConfigureOutputChannel({kA1}, {{}});
  ConfigureOutputChannel({kA2}, {{}});
  ConfigureOutputChannel({kA3}, {{}});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, OneLayerStereo) {
  ConfigureInputChannel(kL2, {0, 1, 2, 3});
  ConfigureInputChannel(kR2, {100, 101, 102, 103});

  // Down-mix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kL2, kR2}, {{0, 100}, {1, 101}, {2, 102}, {3, 103}});

  TestDownMixing({}, 0);
}

TEST_F(DownMixingModuleTest, S2ToS1DownMixer) {
  ConfigureInputChannel(kL2, {0, 100, 500, 1000});
  ConfigureInputChannel(kR2, {100, 0, 500, 500});

  // Down-mix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kL2}, {{0}, {100}, {500}, {1000}});

  // Down-mix to mono as the lowest layer.
  // M = (L2 - 6 dB) + (R2 - 6 dB).
  ConfigureOutputChannel({kMono}, {{50}, {50}, {500}, {750}});

  TestDownMixing({}, 1);
}

TEST_F(DownMixingModuleTest, S3ToS2DownMixer) {
  ConfigureInputChannel(kL3, {0, 100});
  ConfigureInputChannel(kR3, {0, 100});
  ConfigureInputChannel(kCentre, {100, 100});
  ConfigureInputChannel(kLtf3, {99999, 99999});
  ConfigureInputChannel(kRtf3, {99998, 99998});

  // Down-mix to 3.1.2 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kCentre}, {{100}, {100}});
  ConfigureOutputChannel({kLtf3, kRtf3}, {{99999, 99998}, {99999, 99998}});

  // Down-mix to stereo as the lowest layer.
  // L2 = L3 + (C - 3 dB).
  // R2 = R3 + (C - 3 dB).
  ConfigureOutputChannel({kL2, kR2}, {{70, 70}, {170, 170}});

  TestDownMixing({}, 1);
}

TEST_F(DownMixingModuleTest, S5ToS3ToS2DownMixer) {
  ConfigureInputChannel(kL5, {100});
  ConfigureInputChannel(kR5, {200});
  ConfigureInputChannel(kCentre, {1000});
  ConfigureInputChannel(kLs5, {2000});
  ConfigureInputChannel(kRs5, {3000});
  ConfigureInputChannel(kLFE, {6});

  // Down-mix to 5.1 as the highest layer. The highest layer always matches the
  // original input.
  ConfigureOutputChannel({kCentre}, {{1000}});
  ConfigureOutputChannel({kLs5, kRs5}, {{2000, 3000}});
  ConfigureOutputChannel({kLFE}, {{6}});

  // Down-mix to stereo as the lowest layer.
  // L3 = L5 + Ls5 * delta.
  // L2 = L3 + (C - 3 dB).
  ConfigureOutputChannel({kL2, kR2}, {{2221, 3028}});

  // Internally there is a down-mixer to L3/R3 then another for L2/R2.
  TestDownMixing({.delta = .707}, 2);
}

TEST_F(DownMixingModuleTest, S5ToS3ToDownMixer) {
  ConfigureInputChannel(kL5, {1000});
  ConfigureInputChannel(kR5, {2000});
  ConfigureInputChannel(kCentre, {3});
  ConfigureInputChannel(kLs5, {4000});
  ConfigureInputChannel(kRs5, {8000});
  ConfigureInputChannel(kLtf2, {1000});
  ConfigureInputChannel(kRtf2, {2000});
  ConfigureInputChannel(kLFE, {8});

  // Down-mix to 5.1.2 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kLs5, kRs5}, {{4000, 8000}});

  // Down-mix to 3.1.2 as the lowest layer.
  // L3 = L5 + Ls5 * delta.
  ConfigureOutputChannel({kL3, kR3}, {{3828, 7656}});
  ConfigureOutputChannel({kCentre}, {{3}});
  // Ltf3 = Ltf2 + Ls5 * w * delta.
  ConfigureOutputChannel({kLtf3, kRtf3}, {{1707, 3414}});
  ConfigureOutputChannel({kLFE}, {{8}});

  // Internally there is a down-mixer for the height and another for the
  // surround.
  TestDownMixing({.delta = .707, .w = 0.25}, 2);
}

TEST_F(DownMixingModuleTest, T4ToT2DownMixer) {
  ConfigureInputChannel(kL5, {1});
  ConfigureInputChannel(kR5, {2});
  ConfigureInputChannel(kCentre, {3});
  ConfigureInputChannel(kLs5, {4});
  ConfigureInputChannel(kRs5, {5});
  ConfigureInputChannel(kLtf4, {1000});
  ConfigureInputChannel(kRtf4, {2000});
  ConfigureInputChannel(kLtb4, {1000});
  ConfigureInputChannel(kRtb4, {2000});
  ConfigureInputChannel(kLFE, {10});

  // Down-mix to 5.1.4 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kLtb4, kRtb4}, {{1000, 2000}});

  // Down-mix to 5.1.2 as the lowest layer.
  ConfigureOutputChannel({kL5, kR5}, {{1, 2}});
  ConfigureOutputChannel({kCentre}, {{3}});
  ConfigureOutputChannel({kLs5, kRs5}, {{4, 5}});
  // Ltf2 = Ltf4 + Ltb4 * gamma.
  ConfigureOutputChannel({kLtf2, kRtf2}, {{1707, 3414}});
  ConfigureOutputChannel({kLFE}, {{10}});

  TestDownMixing({.gamma = .707}, 1);
}

TEST_F(DownMixingModuleTest, S7ToS5DownMixerWithoutT0) {
  ConfigureInputChannel(kL7, {1});
  ConfigureInputChannel(kR7, {2});
  ConfigureInputChannel(kCentre, {3});
  ConfigureInputChannel(kLss7, {1000});
  ConfigureInputChannel(kRss7, {2000});
  ConfigureInputChannel(kLrs7, {3000});
  ConfigureInputChannel(kRrs7, {4000});
  ConfigureInputChannel(kLFE, {8});

  // Down-mix to 7.1.0 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kLrs7, kRrs7}, {{3000, 4000}});

  // Down-mix to 5.1.0 as the lowest layer.
  ConfigureOutputChannel({kL5, kR5}, {{1, 2}});
  ConfigureOutputChannel({kCentre}, {{3}});
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({kLs5, kRs5}, {{3598, 5464}});
  ConfigureOutputChannel({kLFE}, {{8}});

  TestDownMixing({.alpha = 1, .beta = .866}, 1);
}

TEST_F(DownMixingModuleTest, S7ToS5DownMixerWithT2) {
  ConfigureInputChannel(kL7, {1});
  ConfigureInputChannel(kR7, {2});
  ConfigureInputChannel(kCentre, {3});
  ConfigureInputChannel(kLss7, {1000});
  ConfigureInputChannel(kRss7, {2000});
  ConfigureInputChannel(kLrs7, {3000});
  ConfigureInputChannel(kRrs7, {4000});
  ConfigureInputChannel(kLtf2, {8});
  ConfigureInputChannel(kRtf2, {9});
  ConfigureInputChannel(kLFE, {10});

  // Down-mix to 7.1.2 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kLrs7, kRrs7}, {{3000, 4000}});

  // Down-mix to 5.1.2 as the lowest layer.
  ConfigureOutputChannel({kL5, kR5}, {{1, 2}});
  ConfigureOutputChannel({kCentre}, {{3}});
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({kLs5, kRs5}, {{3598, 5464}});
  ConfigureOutputChannel({kLtf2, kRtf2}, {{8, 9}});
  ConfigureOutputChannel({kLFE}, {{10}});

  TestDownMixing({.alpha = 1, .beta = .866}, 1);
}

TEST_F(DownMixingModuleTest, S7ToS5DownMixerWithT4) {
  ConfigureInputChannel(kL7, {1});
  ConfigureInputChannel(kR7, {2});
  ConfigureInputChannel(kCentre, {3});
  ConfigureInputChannel(kLss7, {1000});
  ConfigureInputChannel(kRss7, {2000});
  ConfigureInputChannel(kLrs7, {3000});
  ConfigureInputChannel(kRrs7, {4000});
  ConfigureInputChannel(kLtf4, {8});
  ConfigureInputChannel(kRtf4, {9});
  ConfigureInputChannel(kLtb4, {10});
  ConfigureInputChannel(kRtb4, {11});
  ConfigureInputChannel(kLFE, {12});

  // Down-mix to 7.1.4 as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kLrs7, kRrs7}, {{3000, 4000}});

  // Down-mix to 5.1.4 as the lowest layer.
  ConfigureOutputChannel({kL5, kR5}, {{1, 2}});
  ConfigureOutputChannel({kCentre}, {{3}});
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({kLs5, kRs5}, {{3598, 5464}});
  ConfigureOutputChannel({kLtf4, kRtf4}, {{8, 9}});
  ConfigureOutputChannel({kLtb4, kRtb4}, {{10, 11}});
  ConfigureOutputChannel({kLFE}, {{12}});

  TestDownMixing({.alpha = 1, .beta = .866}, 1);
}

TEST_F(DownMixingModuleTest, SixLayer7_1_4) {
  ConfigureInputChannel(kL7, {1000});
  ConfigureInputChannel(kR7, {2000});
  ConfigureInputChannel(kCentre, {1000});
  ConfigureInputChannel(kLss7, {1000});
  ConfigureInputChannel(kRss7, {2000});
  ConfigureInputChannel(kLrs7, {3000});
  ConfigureInputChannel(kRrs7, {4000});
  ConfigureInputChannel(kLtf4, {1000});
  ConfigureInputChannel(kRtf4, {2000});
  ConfigureInputChannel(kLtb4, {1000});
  ConfigureInputChannel(kRtb4, {2000});
  ConfigureInputChannel(kLFE, {12});

  // There are different paths to have six-layers, choose 7.1.2, 5.1.2, 3.1.2,
  // stereo, mono to avoid dropping the height channels for as many steps as
  // possible.

  // Down-mix to 7.1.4 as the sixth layer.
  ConfigureOutputChannel({kLtb4, kRtb4}, {{1000, 2000}});

  // Down-mix to 7.1.2 as the fifth layer.
  ConfigureOutputChannel({kLrs7, kRrs7}, {{3000, 4000}});

  // Down-mix to 5.1.2 as the fourth layer.
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({kLs5, kRs5}, {{3598, 5464}});

  // Down-mix to 3.1.2 as the third layer.
  ConfigureOutputChannel({kCentre}, {{1000}});
  // Ltf2 = Ltf4 + Ltb4 * gamma.
  // Ltf3 = Ltf2 + Ls5 * w * delta.
  ConfigureOutputChannel({kLtf3, kRtf3}, {{2644, 4914}});
  ConfigureOutputChannel({kLFE}, {{12}});

  // Down-mix to stereo as the second layer.
  // L5 = L7.
  // L3 = L5 + Ls5 * delta.
  // L2 = L3 + (C - 3 dB).
  ConfigureOutputChannel({kL2}, {{4822}});

  // Down=mix to mono as the first layer.
  // R5 = R7.
  // R3 = R5 + Rs5 * delta.
  // R2 = R3 + (C - 3 dB).
  // M = (L2 - 6 dB) + (R2 - 6 dB).
  ConfigureOutputChannel({kMono}, {{6130}});

  TestDownMixing(
      {.alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25}, 6);
}

class DemixingModuleTest : public DemixingModuleTestBase,
                           public ::testing::Test {
 public:
  void ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      const std::list<ChannelLabel::Label>& labels,
      const std::vector<std::vector<int32_t>>& pcm_samples,
      DownMixingParams down_mixing_params = {
          .alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25}) {
    // Copy the samples to the buffer so the
    // `DecodedAudioFrame::decoded_samples` can point to them.
    pcm_samples_buffer_.push_back(pcm_samples);

    // The substream ID itself does not matter. Generate a unique one.
    const DecodedUleb128 substream_id = substream_id_to_labels_.size();
    substream_id_to_labels_[substream_id] = labels;

    // Configure a pair of audio frames and decoded audio frames. They share a
    // lot of the same information for a lossless codec.
    audio_frames_.push_back(AudioFrameWithData{
        .obu = AudioFrameObu(ObuHeader(), substream_id, {}),
        .start_timestamp = kStartTimestamp,
        .end_timestamp = kEndTimestamp,
        .pcm_samples = pcm_samples,
        .down_mixing_params = down_mixing_params,
    });

    decoded_audio_frames_.push_back(DecodedAudioFrame{
        .substream_id = substream_id,
        .start_timestamp = kStartTimestamp,
        .end_timestamp = kEndTimestamp,
        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
        .decoded_samples = absl::MakeConstSpan(pcm_samples_buffer_.back()),
        .down_mixing_params = down_mixing_params});

    auto& expected_label_to_samples =
        expected_id_to_labeled_decoded_frame_[kAudioElementId].label_to_samples;
    // `raw_samples` is arranged in (time, channel axes). Arrange the samples
    // associated with each channel by time. The demixing process never changes
    // data for the input labels.
    auto labels_iter = labels.begin();
    for (int channel = 0; channel < labels.size(); ++channel) {
      auto& samples_for_channel = expected_label_to_samples[*labels_iter];

      samples_for_channel.reserve(pcm_samples.size());
      for (auto tick : pcm_samples) {
        samples_for_channel.push_back(
            Int32ToNormalizedFloatingPoint<InternalSampleType>(tick[channel]));
      }
      labels_iter++;
    }
  }

  void ConfiguredExpectedDemixingChannelFrame(
      ChannelLabel::Label label,
      const std::vector<int32_t>& expected_demixed_samples) {
    std::vector<InternalSampleType> expected_demixed_samples_as_internal_type;
    expected_demixed_samples_as_internal_type.reserve(
        expected_demixed_samples.size());
    for (int32_t sample : expected_demixed_samples) {
      expected_demixed_samples_as_internal_type.push_back(
          Int32ToNormalizedFloatingPoint<InternalSampleType>(sample));
    }

    // Configure the expected demixed channels. Typically the input `label`
    // should have a "D_" prefix.
    expected_id_to_labeled_decoded_frame_[kAudioElementId]
        .label_to_samples[label] = expected_demixed_samples_as_internal_type;
  }

  void TestLosslessDemixing(int expected_number_of_down_mixers) {
    TestCreateDemixingModule(expected_number_of_down_mixers);

    const auto id_to_labeled_decoded_frame =
        demixing_module_->DemixDecodedAudioSamples(decoded_audio_frames_);
    ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
    ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

    // Check that the demixed samples have the correct values.
    const auto& actual_label_to_samples =
        id_to_labeled_decoded_frame->at(kAudioElementId).label_to_samples;

    const auto& expected_label_to_samples =
        expected_id_to_labeled_decoded_frame_[kAudioElementId].label_to_samples;
    EXPECT_EQ(actual_label_to_samples.size(), expected_label_to_samples.size());
    for (const auto& [label, samples] : actual_label_to_samples) {
      // Use `DoubleNear` with a tolerance because floating-point arithmetic
      // introduces errors larger than allowed by `DoubleEq`.
      constexpr double kErrorTolerance = 1e-14;
      EXPECT_THAT(samples, Pointwise(DoubleNear(kErrorTolerance),
                                     expected_label_to_samples.at(label)));
    }

    // Also, since this is lossless, we expect demixing the original samples
    // should give the same result.
    const auto id_to_labeled_frame =
        demixing_module_->DemixOriginalAudioSamples(audio_frames_);
    ASSERT_THAT(id_to_labeled_frame, IsOk());
    ASSERT_TRUE(id_to_labeled_frame->contains(kAudioElementId));
    EXPECT_EQ(id_to_labeled_frame->at(kAudioElementId).label_to_samples,
              actual_label_to_samples);
  }

 protected:
  std::list<AudioFrameWithData> audio_frames_;
  std::list<DecodedAudioFrame> decoded_audio_frames_;

  // Memory for the samples, so that the span in `DecodedAudioFrame` points
  // to valid memory addresses.
  std::list<std::vector<std::vector<int32_t>>> pcm_samples_buffer_;

  IdLabeledFrameMap expected_id_to_labeled_decoded_frame_;

};  // namespace

TEST(DemixingModule, DemixingOriginalAudioSamplesSucceedsWithEmptyInputs) {
  const auto demixing_module =
      DemixingModule::CreateForDownMixingAndReconstruction({});
  ASSERT_THAT(demixing_module, IsOk());

  EXPECT_THAT(demixing_module->DemixOriginalAudioSamples({}),
              IsOkAndHolds(IsEmpty()));
}

TEST(DemixingModule, DemixingDecodedAudioSamplesSucceedsWithEmptyInputs) {
  const auto demixing_module =
      DemixingModule::CreateForDownMixingAndReconstruction({});
  ASSERT_THAT(demixing_module, IsOk());

  EXPECT_THAT(demixing_module->DemixDecodedAudioSamples({}),
              IsOkAndHolds(IsEmpty()));
}

TEST_F(DemixingModuleTest, AmbisonicsHasNoDemixers) {
  ConfigureAudioFrameMetadata({kA0, kA1, kA2, kA3});

  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA0}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA1}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA2}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA3}, {{1}});

  TestLosslessDemixing(0);
}

TEST_F(DemixingModuleTest, S1ToS2Demixer) {
  // The highest layer is stereo.
  ConfigureAudioFrameMetadata({kL2, kR2});

  // Mono is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kMono}, {{750}, {1500}});
  // Stereo is the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL2}, {{1000}, {2000}});

  // Demixing recovers kDemixedR2
  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  ConfiguredExpectedDemixingChannelFrame(kDemixedR2, {500, 1000});

  TestLosslessDemixing(1);
}

TEST_F(DemixingModuleTest,
       DemixOriginalAudioSamplesReturnsErrorIfAudioFrameIsMissingPcmSamples) {
  ConfigureAudioFrameMetadata({kL2, kR2});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kMono}, {{750}, {1500}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL2}, {{1000}, {2000}});
  IdLabeledFrameMap unused_id_to_labeled_frame, id_to_labeled_decoded_frame;
  TestCreateDemixingModule(1);
  // Destroy the raw samples.
  audio_frames_.back().pcm_samples = std::nullopt;

  EXPECT_THAT(demixing_module_->DemixOriginalAudioSamples(audio_frames_),
              Not(IsOk()));
}

TEST_F(DemixingModuleTest, S2ToS3Demixer) {
  // The highest layer is 3.1.2.
  ConfigureAudioFrameMetadata({kL3, kR3, kCentre, kLtf3, kRtf3});

  // Stereo is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL2, kR2},
                                                  {{70, 70}, {1700, 1700}});

  // 3.1.2 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kCentre}, {{2000}, {1000}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {kLtf3, kRtf3}, {{99999, 99998}, {99999, 99998}});

  // L3/R3 get demixed from the lower layers.
  // L3 = L2 - (C - 3 dB).
  // R3 = R2 - (C - 3 dB).
  ConfiguredExpectedDemixingChannelFrame(kDemixedL3, {-1344, 993});
  ConfiguredExpectedDemixingChannelFrame(kDemixedR3, {-1344, 993});

  TestLosslessDemixing(1);
}

TEST_F(DemixingModuleTest, S3ToS5AndTf2ToT2Demixers) {
  // Adding a (valid) layer on top of 3.1.2 will always result in both S3ToS5
  // and Tf2ToT2 demixers.
  // The highest layer is 5.1.2.
  ConfigureAudioFrameMetadata({kL5, kR5, kCentre, kLtf2, kRtf2});

  const DownMixingParams kDownMixingParams = {.delta = .866, .w = 0.25};

  // 3.1.2 is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL3, kR3}, {{18660, 28660}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kCentre}, {{100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {kLtf3, kRtf3}, {{1000, 2000}}, kDownMixingParams);

  // 5.1.2 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL5, kR5}, {{10000, 20000}},
                                                  kDownMixingParams);

  // S3ToS5: Ls5/Rs5 get demixed from the lower layers.
  // Ls5 = (1 / delta) * (L3 - L5).
  // Rs5 = (1 / delta) * (R3 - R5).
  ConfiguredExpectedDemixingChannelFrame(kDemixedLs5, {10000});
  ConfiguredExpectedDemixingChannelFrame(kDemixedRs5, {10000});

  // Tf2ToT2: Ltf2/Rtf2 get demixed from the lower layers.
  // Ltf2 = Ltf3 - w * (L3 - L5).
  // Rtf2 = Rtf3 - w * (R3 - R5).
  ConfiguredExpectedDemixingChannelFrame(kDemixedLtf2, {-1165});
  ConfiguredExpectedDemixingChannelFrame(kDemixedRtf2, {-165});

  TestLosslessDemixing(2);
}

TEST_F(DemixingModuleTest, S5ToS7Demixer) {
  // The highest layer is 7.1.0.
  ConfigureAudioFrameMetadata({kL7, kR7, kCentre, kLss7, kRss7, kLrs7, kRrs7});

  const DownMixingParams kDownMixingParams = {.alpha = 0.866, .beta = .866};

  // 5.1.0 is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL5, kR5}, {{100, 100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kLs5, kRs5}, {{7794, 7794}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kCentre}, {{100}},
                                                  kDownMixingParams);

  // 7.1.0 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {kLss7, kRss7}, {{1000, 2000}}, kDownMixingParams);

  // L7/R7 get demixed from the lower layers.
  // L7 = R5.
  // R7 = R5.
  ConfiguredExpectedDemixingChannelFrame(kDemixedL7, {100});
  ConfiguredExpectedDemixingChannelFrame(kDemixedR7, {100});

  // Lrs7/Rrs7 get demixed from the lower layers.
  // Lrs7 = (1 / beta) * (Ls5 - alpha * Lss7).
  // Rrs7 = (1 / beta) * (Rs5 - alpha * Rss7).
  ConfiguredExpectedDemixingChannelFrame(kDemixedLrs7, {8000});
  ConfiguredExpectedDemixingChannelFrame(kDemixedRrs7, {7000});

  TestLosslessDemixing(1);
}

TEST_F(DemixingModuleTest, T2ToT4Demixer) {
  // The highest layer is 5.1.4.
  ConfigureAudioFrameMetadata({kL5, kR5, kCentre, kLtf4, kRtf4});

  const DownMixingParams kDownMixingParams = {.gamma = .866};

  // 5.1.2 is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL5, kR5}, {{100, 100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kLs5, kRs5}, {{100, 100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kCentre}, {{100}},
                                                  kDownMixingParams);
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {kLtf2, kRtf2}, {{8660, 17320}}, kDownMixingParams);

  // 5.1.4 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kLtf4, kRtf4}, {{866, 1732}},
                                                  kDownMixingParams);

  // Ltb4/Rtb4 get demixed from the lower layers.
  // Ltb4 = (1 / gamma) * (Ltf2 - Ltf4).
  // Ttb4 = (1 / gamma) * (Ttf2 - Rtf4).
  ConfiguredExpectedDemixingChannelFrame(kDemixedLtb4, {9000});
  ConfiguredExpectedDemixingChannelFrame(kDemixedRtb4, {18000});

  TestLosslessDemixing(1);
}

}  // namespace
}  // namespace iamf_tools
