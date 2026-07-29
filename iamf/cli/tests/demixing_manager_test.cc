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
#include "iamf/cli/demixing_manager.h"

#include <array>
#include <cstdint>
#include <list>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixer.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointwise;

using enum ChannelLabel::Label;
using AudioElementsById = DescriptorObus::AudioElementsById;
using CodecConfigsById = DescriptorObus::CodecConfigsById;
using absl::MakeConstSpan;

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

void InitAudioElementWithLabelsAndScalableChannelLayout(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const ScalableChannelLayoutConfig& config,
    AudioElementsById& audio_elements) {
  std::vector<DecodedUleb128> substream_ids;
  substream_ids.reserve(substream_id_to_labels.size());
  for (const auto& [substream_id, _] : substream_id_to_labels) {
    substream_ids.push_back(substream_id);
  }
  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kAudioElementId, /*reserved=*/0, /*codec_config_id=*/0,
      substream_ids, config);
  ASSERT_THAT(obu, IsOk());

  audio_elements.emplace(kAudioElementId,
                         AudioElementWithData{
                             .obu = *std::move(obu),
                             .substream_id_to_labels = substream_id_to_labels,
                         });
}

const ScalableChannelLayoutConfig kOneLayerStereoConfig = {
    .channel_audio_layer_configs = {
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
         .substream_count = 1,
         .coupled_substream_count = 1}}};

const ScalableChannelLayoutConfig kTwoLayerStereoConfig = {
    .channel_audio_layer_configs = {
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
         .substream_count = 1},
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
         .substream_count = 1}}};

TEST(Create, CreatesOneDemixerForTwoLayerStereo) {
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{0, {kMono}}, {1, {kL2}}}, kTwoLayerStereoConfig, audio_elements);
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  absl::StatusOr<const std::list<Demixer>*> demixer =
      demixing_manager->GetDemixers(kAudioElementId);
  EXPECT_THAT(demixer, IsOk());
  EXPECT_EQ((*demixer)->size(), 1);
}

TEST(Create, FailsForReservedLayout14) {
  const ScalableChannelLayoutConfig kReserved14Config = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutReserved14,
           .substream_count = 1}}};

  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{0, {kOmitted}}}, kReserved14Config, audio_elements);

  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));

  EXPECT_THAT(demixing_manager, Not(IsOk()));
}

TEST(Create, ValidForExpandedLayoutLFE) {
  const ScalableChannelLayoutConfig kExpandedLayoutLFEConfig = {
      .channel_audio_layer_configs = {
          {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutExpanded,
           .substream_count = 1,
           .expanded_loudspeaker_layout =
               ChannelAudioLayerConfig::kExpandedLayoutLFE}}};

  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{0, {kLFE}}}, kExpandedLayoutLFEConfig, audio_elements);

  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));

  EXPECT_THAT(demixing_manager, IsOk());
}

TEST(Create, CreatesNoDemixersForSingleLayerChannelBased) {
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{0, {kL2, kR2}}}, kOneLayerStereoConfig, audio_elements);
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  absl::StatusOr<const std::list<Demixer>*> demixer =
      demixing_manager->GetDemixers(kAudioElementId);
  EXPECT_THAT(demixer, IsOk());
  EXPECT_TRUE((*demixer)->empty());
}

TEST(Create, CreatesNoDemixersForAmbisonics) {
  const DecodedUleb128 kCodecConfigId = 0;
  constexpr std::array<DecodedUleb128, 4> kAmbisonicsSubstreamIds{0, 1, 2, 3};
  CodecConfigsById codec_configs;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000, codec_configs);
  AudioElementsById audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                kAmbisonicsSubstreamIds,
                                                codec_configs, audio_elements);

  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  absl::StatusOr<const std::list<Demixer>*> demixer =
      demixing_manager->GetDemixers(kAudioElementId);
  EXPECT_THAT(demixer, IsOk());
  EXPECT_TRUE((*demixer)->empty());
}

TEST(DemixDecodedAudioSamples, OutputContainsOriginalAndDemixedSamples) {
  const std::vector<std::vector<int32_t>> kDecodedSamplesInt = {{0}};
  const auto kDecodedSamples = Int32ToInternalSampleType2D(kDecodedSamplesInt);
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      kTwoLayerStereoConfig, audio_elements);
  std::list<AudioFrameWithData> decoded_audio_frames;
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                    .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
          kMonoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                    .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
          kL2SubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());
  const auto id_to_labeled_decoded_frame =
      demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames);
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
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kStereoSubstreamId, {kL2, kR2}}}, kOneLayerStereoConfig,
      audio_elements);
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());
  std::list<AudioFrameWithData> decoded_audio_frames;
  // The decoded audio frame has one channel, which is inconsistent with a
  // one-layer stereo audio element.
  const std::vector<std::vector<int32_t>> kErrorOneChannelInt = {{0}};
  const auto kErrorOneChannel =
      Int32ToInternalSampleType2D(kErrorOneChannelInt);
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                    .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
          kStereoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kErrorOneChannel),
      .down_mixing_params = DownMixingParams()});

  // Demixing gracefully fails, as we can't determine the missing channel.
  EXPECT_THAT(demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames),
              Not(IsOk()));
}

TEST(DemixDecodedAudioSamples,
     ReturnsErrorWhenSampleCountsMismatchAcrossChannels) {
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kStereoSubstreamId, {kL2, kR2}}}, kOneLayerStereoConfig,
      audio_elements);
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());
  // Configure two channels, with different sample counts.
  const std::vector<std::vector<InternalSampleType>> kErrorMismatchedSamples = {
      {0.0}, {0.0, 0.0}};
  std::list<AudioFrameWithData> decoded_audio_frames = {(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                    .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
          kStereoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = MakeConstSpan(kErrorMismatchedSamples),
      .down_mixing_params = DownMixingParams()})};

  EXPECT_THAT(demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames),
              Not(IsOk()));
}

TEST(DemixDecodedAudioSamples,
     ReturnsErrorWhenSampleCountsMismatchAcrossSubstreams) {
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{0, {kMono}}, {1, {kL2}}}, kTwoLayerStereoConfig, audio_elements);
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());
  const std::vector<std::vector<InternalSampleType>> kSubstream0Samples = {
      {0.0}};
  const std::vector<std::vector<InternalSampleType>> kSubstream1Samples = {
      {0.0, 0.1}};
  std::list<AudioFrameWithData> decoded_audio_frames = {
      // Substream 0 has one sample.
      {.obu = AudioFrameObu(
           ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                     .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
           0, {}),
       .start_timestamp = kStartTimestamp,
       .end_timestamp = kEndTimestamp,
       .decoded_samples = MakeConstSpan(kSubstream0Samples),
       .down_mixing_params = DownMixingParams()},
      // Substream 1 has two samples.
      {.obu = AudioFrameObu(
           ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                     .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
           1, {}),
       .start_timestamp = kStartTimestamp,
       .end_timestamp = kEndTimestamp,
       .decoded_samples = MakeConstSpan(kSubstream1Samples),
       .down_mixing_params = DownMixingParams()}};

  EXPECT_THAT(demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames),
              Not(IsOk()));
}

TEST(DemixDecodedAudioSamples, OutputEchoesTimingInformation) {
  // These values are not very sensible, but as long as they are consistent
  // between related frames it is OK.
  const DecodedUleb128 kStartTimestamp = 99;
  const DecodedUleb128 kEndTimestamp = 123;
  const DecodedUleb128 kExpectedNumSamplesToTrimAtEnd = 999;
  const DecodedUleb128 kExpectedNumSamplesToTrimAtStart = 9999;
  const DecodedUleb128 kL2SubstreamId = 1;
  const std::vector<std::vector<int32_t>> kDecodedSamplesInt = {{0}};
  const auto kDecodedSamples = Int32ToInternalSampleType2D(kDecodedSamplesInt);
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      kTwoLayerStereoConfig, audio_elements);
  std::list<AudioFrameWithData> decoded_audio_frames;
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kExpectedNumSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kExpectedNumSamplesToTrimAtStart,
          },
          kMonoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kExpectedNumSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kExpectedNumSamplesToTrimAtStart,
          },
          kL2SubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  const auto id_to_labeled_decoded_frame =
      demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  EXPECT_EQ(labeled_frame.samples_to_trim_at_end,
            kExpectedNumSamplesToTrimAtEnd);
  EXPECT_EQ(labeled_frame.samples_to_trim_at_start,
            kExpectedNumSamplesToTrimAtStart);
}

TEST(DemixDecodedAudioSamples, OutputEchoesOriginalLabels) {
  const std::vector<std::vector<int32_t>> kDecodedMonoSamplesInt = {{1, 2, 3}};
  const std::vector<std::vector<int32_t>> kDecodedL2SamplesInt = {{9, 10, 11}};
  const auto kDecodedMonoSamples =
      Int32ToInternalSampleType2D(kDecodedMonoSamplesInt);
  const auto kDecodedL2Samples =
      Int32ToInternalSampleType2D(kDecodedL2SamplesInt);
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      kTwoLayerStereoConfig, audio_elements);
  std::list<AudioFrameWithData> decoded_audio_frames;
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
          },
          kMonoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedMonoSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{.num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                    .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart},
          kL2SubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedL2Samples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  IdLabeledFrameMap unused_id_labeled_frame;
  const auto id_to_labeled_decoded_frame =
      demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames);
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
  const std::vector<std::vector<int32_t>> kDecodedMonoSamplesInt = {{750}};
  const std::vector<std::vector<int32_t>> kDecodedL2SamplesInt = {{1000}};
  const auto kDecodedMonoSamples =
      Int32ToInternalSampleType2D(kDecodedMonoSamplesInt);
  const auto kDecodedL2Samples =
      Int32ToInternalSampleType2D(kDecodedL2SamplesInt);
  AudioElementsById audio_elements;

  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      kTwoLayerStereoConfig, audio_elements);
  std::list<AudioFrameWithData> decoded_audio_frames;
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
          },
          kMonoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedMonoSamples),
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
          },
          kL2SubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedL2Samples),
      .down_mixing_params = DownMixingParams()});
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  const auto id_to_labeled_decoded_frame =
      demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames);
  ASSERT_THAT(id_to_labeled_decoded_frame, IsOk());
  ASSERT_TRUE(id_to_labeled_decoded_frame->contains(kAudioElementId));

  // Examine the demixed frame.
  const auto& labeled_frame = id_to_labeled_decoded_frame->at(kAudioElementId);
  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  EXPECT_THAT(labeled_frame.label_to_samples.at(kDemixedR2),
              Pointwise(InternalSampleMatchesIntegralSample(), {500}));
}

TEST(DemixDecodedAudioSamples, OutputContainsReconGainAndLayerInfo) {
  const std::vector<std::vector<int32_t>> kDecodedSamplesInt = {{0}};
  const auto kDecodedSamples = Int32ToInternalSampleType2D(kDecodedSamplesInt);
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      kTwoLayerStereoConfig, audio_elements);
  std::list<AudioFrameWithData> decoded_audio_frames;
  ReconGainInfoParameterData recon_gain_info_parameter_data;
  recon_gain_info_parameter_data.recon_gain_elements.push_back(
      ReconGainElement{.recon_gain_flag = 1, .recon_gain = kReconGainValues});
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
          },
          kMonoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams(),
      .recon_gain_info_parameter_data = recon_gain_info_parameter_data,
      .audio_element_with_data = &audio_elements.at(kAudioElementId)});
  decoded_audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(
          ObuHeader{
              .num_samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
              .num_samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
          },
          kL2SubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .decoded_samples = absl::MakeConstSpan(kDecodedSamples),
      .down_mixing_params = DownMixingParams(),
      .recon_gain_info_parameter_data = recon_gain_info_parameter_data,
      .audio_element_with_data = &audio_elements.at(kAudioElementId)});
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());
  const auto id_to_labeled_decoded_frame =
      demixing_manager->DemixDecodedAudioSamples(decoded_audio_frames);
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
  EXPECT_EQ(recon_gain_element->recon_gain_flag, 1);
  EXPECT_THAT(recon_gain_element->recon_gain,
              testing::ElementsAreArray(kReconGainValues));
  EXPECT_EQ(labeled_frame.loudspeaker_layout_per_layer.size(), 2);
  EXPECT_THAT(labeled_frame.loudspeaker_layout_per_layer,
              testing::ElementsAre(ChannelAudioLayerConfig::kLayoutMono,
                                   ChannelAudioLayerConfig::kLayoutStereo));
}

TEST(DemixingManager, DemixingOriginalAudioSamplesSucceedsWithEmptyInputs) {
  const auto demixing_manager = DemixingManager::Create({});
  ASSERT_THAT(demixing_manager, IsOk());

  EXPECT_THAT(demixing_manager->DemixOriginalAudioSamples({}),
              IsOkAndHolds(IsEmpty()));
}

TEST(DemixingManager, DemixingDecodedAudioSamplesSucceedsWithEmptyInputs) {
  const auto demixing_manager = DemixingManager::Create({});
  ASSERT_THAT(demixing_manager, IsOk());

  EXPECT_THAT(demixing_manager->DemixDecodedAudioSamples({}),
              IsOkAndHolds(IsEmpty()));
}

TEST(DemixOriginalAudioSamples, ReturnsErrorIfAudioFrameIsMissingPcmSamples) {
  AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      kTwoLayerStereoConfig, audio_elements);
  const auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ASSERT_THAT(demixing_manager, IsOk());

  std::list<AudioFrameWithData> audio_frames;
  // Push a frame that is missing `encoded_samples`.
  audio_frames.push_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), kMonoSubstreamId, {}),
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .encoded_samples = std::nullopt,
      .decoded_samples = {},
  });

  EXPECT_THAT(demixing_manager->DemixOriginalAudioSamples(audio_frames),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
