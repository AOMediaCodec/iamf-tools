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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum ChannelLabel::Label;
using testing::DoubleEq;
using testing::DoubleNear;
using testing::Pointwise;

constexpr DecodedUleb128 kAudioElementId = 137;
constexpr std::array<uint8_t, 12> kReconGainValues = {
    255, 0, 125, 200, 150, 255, 255, 255, 255, 255, 255, 255};
const uint32_t kZeroSamplesToTrimAtEnd = 0;
const uint32_t kZeroSamplesToTrimAtStart = 0;
const int kStartTimestamp = 0;
const int kEndTimestamp = 4;
const DecodedUleb128 kMonoSubstreamId = 0;
const DecodedUleb128 kL2SubstreamId = 1;

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

TEST(InitializeForDownMixingAndReconstruction,
     ValidWhenCalledOncePerAudioElement) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 137
        channel_labels: [ "L2", "R2" ]
      )pb",
      user_metadata.add_audio_frame_metadata()));
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kMono}}, {1, {kL2}}},
                                      {ChannelAudioLayerConfig::kLayoutMono,
                                       ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);

  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForDownMixingAndReconstruction(
                  user_metadata, audio_elements),
              IsOk());
  // Each audio element can only be added once.
  EXPECT_FALSE(demixing_module
                   .InitializeForDownMixingAndReconstruction(user_metadata,
                                                             audio_elements)
                   .ok());
}

TEST(InitializeForDownMixingAndReconstruction, InvalidWhenMissingAudioElement) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        audio_element_id: 137
        channel_labels: [ "L2", "R2" ]
      )pb",
      user_metadata.add_audio_frame_metadata()));
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kNoMatchingAudioElement;

  DemixingModule demixing_module;
  EXPECT_FALSE(demixing_module
                   .InitializeForDownMixingAndReconstruction(
                       user_metadata, kNoMatchingAudioElement)
                   .ok());
}

TEST(InitializeForReconstruction, NeverCreatesDownMixers) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kMono}}, {1, {kL2}}},
                                      {ChannelAudioLayerConfig::kLayoutMono,
                                       ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  const std::list<Demixer>* down_mixers = nullptr;
  EXPECT_THAT(demixing_module.GetDownMixers(kAudioElementId, down_mixers),
              IsOk());
  EXPECT_TRUE(down_mixers->empty());
}

TEST(InitializeForReconstruction, InvalidWhenCalledTwicePerAudioElement) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kL2, kR2}}},
                                      {ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  EXPECT_FALSE(
      demixing_module.InitializeForReconstruction(audio_elements).ok());
}

TEST(InitializeForReconstruction, CreatesOneDemixerForTwoLayerStereo) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kMono}}, {1, {kL2}}},
                                      {ChannelAudioLayerConfig::kLayoutMono,
                                       ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  const std::list<Demixer>* demixer = nullptr;
  EXPECT_THAT(demixing_module.GetDemixers(kAudioElementId, demixer), IsOk());
  EXPECT_EQ(demixer->size(), 1);
}

TEST(InitializeForReconstruction, InvalidForReservedLoudspeakerLayout14) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{0, {kOmitted}}}, {ChannelAudioLayerConfig::kLayoutReserved14},
      audio_elements);
  DemixingModule demixing_module;

  EXPECT_FALSE(
      demixing_module.InitializeForReconstruction(audio_elements).ok());
}

TEST(InitializeForReconstruction, InvalidForExpandedLayout) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{0, {kLFE}}}, {ChannelAudioLayerConfig::kLayoutExpanded},
      audio_elements);
  std::get<ScalableChannelLayoutConfig>(
      audio_elements.at(kAudioElementId).obu.config_)
      .channel_audio_layer_configs[0]
      .expanded_loudspeaker_layout =
      ChannelAudioLayerConfig::kExpandedLayoutLFE;
  DemixingModule demixing_module;

  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());
}

TEST(InitializeForReconstruction, CreatesNoDemixersForSingleLayerChannelBased) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers({{0, {kL2, kR2}}},
                                      {ChannelAudioLayerConfig::kLayoutStereo},
                                      audio_elements);
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  const std::list<Demixer>* demixer = nullptr;
  EXPECT_THAT(demixing_module.GetDemixers(kAudioElementId, demixer), IsOk());
  EXPECT_TRUE(demixer->empty());
}

TEST(InitializeForReconstruction, CreatesNoDemixersForAmbisonics) {
  const DecodedUleb128 kCodecConfigId = 0;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000, codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(kAudioElementId, kCodecConfigId,
                                                {0, 1, 2, 3}, codec_configs,
                                                audio_elements);
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  const std::list<Demixer>* demixer = nullptr;
  EXPECT_THAT(demixing_module.GetDemixers(kAudioElementId, demixer), IsOk());
  EXPECT_TRUE(demixer->empty());
}

TEST(DemixAudioSamples, OutputContainsOriginalAndDemixedSamples) {
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
                        .decoded_samples = {{0}},
                        .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(
      DecodedAudioFrame{.substream_id = kL2SubstreamId,
                        .start_timestamp = kStartTimestamp,
                        .end_timestamp = kEndTimestamp,
                        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                        .decoded_samples = {{0}},
                        .down_mixing_params = DownMixingParams()});
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());
  IdLabeledFrameMap id_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(demixing_module.DemixAudioSamples({}, decoded_audio_frames,
                                                id_labeled_frame,
                                                id_to_labeled_decoded_frame),
              IsOk());

  const auto& labeled_frame = id_to_labeled_decoded_frame.at(kAudioElementId);
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kL2));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kMono));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kDemixedR2));
  // When being used for reconstruction the original audio frames are not
  // output.
  EXPECT_FALSE(id_labeled_frame.contains(kAudioElementId));
}

TEST(DemixAudioSamples, OutputEchoesTimingInformation) {
  // These values are not very sensible, but as long as they are consistent
  // between related frames it is OK.
  const DecodedUleb128 kExpectedStartTimestamp = 99;
  const DecodedUleb128 kExpectedEndTimestamp = 123;
  const DecodedUleb128 kExpectedNumSamplesToTrimAtEnd = 999;
  const DecodedUleb128 kExpectedNumSamplesToTrimAtStart = 9999;
  const DecodedUleb128 kL2SubstreamId = 1;
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
      .decoded_samples = {{0}},
      .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kL2SubstreamId,
      .start_timestamp = kExpectedStartTimestamp,
      .end_timestamp = kExpectedEndTimestamp,
      .samples_to_trim_at_end = kExpectedNumSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kExpectedNumSamplesToTrimAtStart,
      .decoded_samples = {{0}},
      .down_mixing_params = DownMixingParams()});
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());
  IdLabeledFrameMap unused_id_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(demixing_module.DemixAudioSamples({}, decoded_audio_frames,
                                                unused_id_labeled_frame,
                                                id_to_labeled_decoded_frame),
              IsOk());

  const auto& labeled_frame = id_to_labeled_decoded_frame.at(kAudioElementId);
  EXPECT_EQ(labeled_frame.end_timestamp, kExpectedEndTimestamp);
  EXPECT_EQ(labeled_frame.samples_to_trim_at_end,
            kExpectedNumSamplesToTrimAtEnd);
  EXPECT_EQ(labeled_frame.samples_to_trim_at_start,
            kExpectedNumSamplesToTrimAtStart);
}

TEST(DemixAudioSamples, OutputEchoesOriginalLabels) {
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
                        .decoded_samples = {{1}, {2}, {3}},
                        .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(
      DecodedAudioFrame{.substream_id = kL2SubstreamId,
                        .start_timestamp = kStartTimestamp,
                        .end_timestamp = kEndTimestamp,
                        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                        .decoded_samples = {{9}, {10}, {11}},
                        .down_mixing_params = DownMixingParams()});
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  IdLabeledFrameMap unused_id_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(demixing_module.DemixAudioSamples({}, decoded_audio_frames,
                                                unused_id_labeled_frame,
                                                id_to_labeled_decoded_frame),
              IsOk());

  // Examine the demixed frame.
  const auto& labeled_frame = id_to_labeled_decoded_frame.at(kAudioElementId);
  EXPECT_THAT(labeled_frame.label_to_samples.at(kMono),
              Pointwise(DoubleEq(), {1.0, 2.0, 3.0}));
  EXPECT_THAT(labeled_frame.label_to_samples.at(kL2),
              Pointwise(DoubleEq(), {9.0, 10.0, 11.0}));
}

TEST(DemixAudioSamples, OutputHasReconstructedLayers) {
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
                        .decoded_samples = {{750}},
                        .down_mixing_params = DownMixingParams()});
  decoded_audio_frames.push_back(
      DecodedAudioFrame{.substream_id = kL2SubstreamId,
                        .start_timestamp = kStartTimestamp,
                        .end_timestamp = kEndTimestamp,
                        .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                        .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                        .decoded_samples = {{1000}},
                        .down_mixing_params = DownMixingParams()});
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());

  IdLabeledFrameMap unused_id_time_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(demixing_module.DemixAudioSamples({}, decoded_audio_frames,
                                                unused_id_time_labeled_frame,
                                                id_to_labeled_decoded_frame),
              IsOk());

  // Examine the demixed frame.
  const auto& labeled_frame = id_to_labeled_decoded_frame.at(kAudioElementId);
  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  EXPECT_THAT(labeled_frame.label_to_samples.at(kDemixedR2),
              Pointwise(DoubleEq(), {500}));
}

TEST(DemixAudioSamples, OutputContainsReconGainAndLayerInfo) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  InitAudioElementWithLabelsAndLayers(
      {{kMonoSubstreamId, {kMono}}, {kL2SubstreamId, {kL2}}},
      {ChannelAudioLayerConfig::kLayoutMono,
       ChannelAudioLayerConfig::kLayoutStereo},
      audio_elements);
  std::list<DecodedAudioFrame> decoded_audio_frames;
  ReconGainInfoParameterData recon_gain_parameter_data;
  recon_gain_parameter_data.recon_gain_elements.push_back(ReconGainElement{
      .recon_gain_flag = DecodedUleb128(1), .recon_gain = kReconGainValues});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kMonoSubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = {{0}},
      .down_mixing_params = DownMixingParams(),
      .recon_gain_info_param_data = recon_gain_parameter_data,
      .audio_element_with_data = &audio_elements.at(kAudioElementId)});
  decoded_audio_frames.push_back(DecodedAudioFrame{
      .substream_id = kL2SubstreamId,
      .start_timestamp = kStartTimestamp,
      .end_timestamp = kEndTimestamp,
      .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
      .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
      .decoded_samples = {{0}},
      .down_mixing_params = DownMixingParams(),
      .recon_gain_info_param_data = recon_gain_parameter_data,
      .audio_element_with_data = &audio_elements.at(kAudioElementId)});
  DemixingModule demixing_module;
  EXPECT_THAT(demixing_module.InitializeForReconstruction(audio_elements),
              IsOk());
  IdLabeledFrameMap id_labeled_frame;
  IdLabeledFrameMap id_to_labeled_decoded_frame;
  EXPECT_THAT(demixing_module.DemixAudioSamples({}, decoded_audio_frames,
                                                id_labeled_frame,
                                                id_to_labeled_decoded_frame),
              IsOk());

  const auto& labeled_frame = id_to_labeled_decoded_frame.at(kAudioElementId);
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kL2));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kMono));
  EXPECT_TRUE(labeled_frame.label_to_samples.contains(kDemixedR2));

  EXPECT_EQ(labeled_frame.recon_gain_parameters.recon_gain_elements.size(), 1);
  const auto& recon_gain_element =
      labeled_frame.recon_gain_parameters.recon_gain_elements.at(0);
  EXPECT_EQ(recon_gain_element.recon_gain_flag, DecodedUleb128(1));
  EXPECT_THAT(recon_gain_element.recon_gain,
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

  void TestCreateDemixingModule(int expected_number_of_down_mixers) {
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

    ASSERT_THAT(demixing_module_.InitializeForDownMixingAndReconstruction(
                    user_metadata, audio_elements_),
                IsOk());

    const std::list<Demixer>* down_mixers = nullptr;
    const std::list<Demixer>* demixers = nullptr;

    ASSERT_THAT(demixing_module_.GetDownMixers(kAudioElementId, down_mixers),
                IsOk());
    ASSERT_THAT(demixing_module_.GetDemixers(kAudioElementId, demixers),
                IsOk());
    EXPECT_EQ(down_mixers->size(), expected_number_of_down_mixers);
    EXPECT_EQ(demixers->size(), expected_number_of_down_mixers);
  }

 protected:
  void ConfigureAudioFrameMetadata(absl::string_view label) {
    audio_frame_metadata_.add_channel_labels(label);
  }

  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
  SubstreamIdLabelsMap substream_id_to_labels_;

  DemixingModule demixing_module_;
};

class DownMixingModuleTest : public DemixingModuleTestBase,
                             public ::testing::Test {
 protected:
  void TestDownMixing(const DownMixingParams& down_mixing_params,
                      int expected_number_of_down_mixers) {
    TestCreateDemixingModule(expected_number_of_down_mixers);

    EXPECT_THAT(demixing_module_.DownMixSamplesToSubstreams(
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

  void ConfigureInputChannel(
      absl::string_view label_string,
      const std::vector<InternalSampleType>& input_samples) {
    ConfigureAudioFrameMetadata(label_string);
    auto label = ChannelLabel::StringToLabel(label_string);
    ASSERT_TRUE(label.ok());

    auto [unused_iter, inserted] =
        input_label_to_samples_.emplace(*label, input_samples);

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
  ConfigureInputChannel("L2", {});
  ConfigureInputChannel("R2", {});

  ConfigureOutputChannel({kL2, kR2}, {{}});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, OneLayer7_1_4HasNoDownMixers) {
  // Initialize arguments for single layer 7.1.4.
  ConfigureInputChannel("L7", {});
  ConfigureInputChannel("R7", {});
  ConfigureInputChannel("C", {});
  ConfigureInputChannel("LFE", {});
  ConfigureInputChannel("Lss7", {});
  ConfigureInputChannel("Rss7", {});
  ConfigureInputChannel("Lrs7", {});
  ConfigureInputChannel("Rrs7", {});
  ConfigureInputChannel("Ltf4", {});
  ConfigureInputChannel("Rtf4", {});
  ConfigureInputChannel("Ltb4", {});
  ConfigureInputChannel("Rtb4", {});

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
  ConfigureInputChannel("A0", {});
  ConfigureInputChannel("A1", {});
  ConfigureInputChannel("A2", {});
  ConfigureInputChannel("A3", {});

  ConfigureOutputChannel({kA0}, {{}});
  ConfigureOutputChannel({kA1}, {{}});
  ConfigureOutputChannel({kA2}, {{}});
  ConfigureOutputChannel({kA3}, {{}});

  TestCreateDemixingModule(0);
}

TEST_F(DownMixingModuleTest, OneLayerStereo) {
  ConfigureInputChannel("L2", {0, 1, 2, 3});
  ConfigureInputChannel("R2", {100, 101, 102, 103});

  // Down-mix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kL2, kR2}, {{0, 100}, {1, 101}, {2, 102}, {3, 103}});

  TestDownMixing({}, 0);
}

TEST_F(DownMixingModuleTest, S2ToS1DownMixer) {
  ConfigureInputChannel("L2", {0, 100, 500, 1000});
  ConfigureInputChannel("R2", {100, 0, 500, 500});

  // Down-mix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kL2}, {{0}, {100}, {500}, {1000}});

  // Down-mix to mono as the lowest layer.
  // M = (L2 - 6 dB) + (R2 - 6 dB).
  ConfigureOutputChannel({kMono}, {{50}, {50}, {500}, {750}});

  TestDownMixing({}, 1);
}

TEST_F(DownMixingModuleTest, S3ToS2DownMixer) {
  ConfigureInputChannel("L3", {0, 100});
  ConfigureInputChannel("R3", {0, 100});
  ConfigureInputChannel("C", {100, 100});
  ConfigureInputChannel("Ltf3", {99999, 99999});
  ConfigureInputChannel("Rtf3", {99998, 99998});

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
  ConfigureInputChannel("L5", {100});
  ConfigureInputChannel("R5", {200});
  ConfigureInputChannel("C", {1000});
  ConfigureInputChannel("Ls5", {2000});
  ConfigureInputChannel("Rs5", {3000});
  ConfigureInputChannel("LFE", {6});

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
  ConfigureInputChannel("L5", {1000});
  ConfigureInputChannel("R5", {2000});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Ls5", {4000});
  ConfigureInputChannel("Rs5", {8000});
  ConfigureInputChannel("Ltf2", {1000});
  ConfigureInputChannel("Rtf2", {2000});
  ConfigureInputChannel("LFE", {8});

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
  ConfigureInputChannel("L5", {1});
  ConfigureInputChannel("R5", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Ls5", {4});
  ConfigureInputChannel("Rs5", {5});
  ConfigureInputChannel("Ltf4", {1000});
  ConfigureInputChannel("Rtf4", {2000});
  ConfigureInputChannel("Ltb4", {1000});
  ConfigureInputChannel("Rtb4", {2000});
  ConfigureInputChannel("LFE", {10});

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
  ConfigureInputChannel("L7", {1});
  ConfigureInputChannel("R7", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("LFE", {8});

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
  ConfigureInputChannel("L7", {1});
  ConfigureInputChannel("R7", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("Ltf2", {8});
  ConfigureInputChannel("Rtf2", {9});
  ConfigureInputChannel("LFE", {10});

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
  ConfigureInputChannel("L7", {1});
  ConfigureInputChannel("R7", {2});
  ConfigureInputChannel("C", {3});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("Ltf4", {8});
  ConfigureInputChannel("Rtf4", {9});
  ConfigureInputChannel("Ltb4", {10});
  ConfigureInputChannel("Rtb4", {11});
  ConfigureInputChannel("LFE", {12});

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
  ConfigureInputChannel("L7", {1000});
  ConfigureInputChannel("R7", {2000});
  ConfigureInputChannel("C", {1000});
  ConfigureInputChannel("Lss7", {1000});
  ConfigureInputChannel("Rss7", {2000});
  ConfigureInputChannel("Lrs7", {3000});
  ConfigureInputChannel("Rrs7", {4000});
  ConfigureInputChannel("Ltf4", {1000});
  ConfigureInputChannel("Rtf4", {2000});
  ConfigureInputChannel("Ltb4", {1000});
  ConfigureInputChannel("Rtb4", {2000});
  ConfigureInputChannel("LFE", {12});

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
      const std::vector<std::vector<int32_t>>& raw_samples,
      DownMixingParams down_mixing_params = {
          .alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25}) {
    // The substream ID itself does not matter. Generate a unique one.
    const DecodedUleb128 substream_id = substream_id_to_labels_.size();
    substream_id_to_labels_[substream_id] = labels;

    // Configure a pair of audio frames and decoded audio frames. They share a
    // lot of the same information for a lossless codec.
    audio_frames_.push_back(AudioFrameWithData{
        .obu = AudioFrameObu(ObuHeader(), substream_id, {}),
        .start_timestamp = kStartTimestamp,
        .end_timestamp = kEndTimestamp,
        .raw_samples = raw_samples,
        .down_mixing_params = down_mixing_params,
    });

    decoded_audio_frames_.push_back(
        DecodedAudioFrame{.substream_id = substream_id,
                          .start_timestamp = kStartTimestamp,
                          .end_timestamp = kEndTimestamp,
                          .samples_to_trim_at_end = kZeroSamplesToTrimAtEnd,
                          .samples_to_trim_at_start = kZeroSamplesToTrimAtStart,
                          .decoded_samples = raw_samples,
                          .down_mixing_params = down_mixing_params});

    auto& expected_label_to_samples =
        expected_id_to_labeled_decoded_frame_[kAudioElementId].label_to_samples;
    // `raw_samples` is arranged in (time, channel axes). Arrange the samples
    // associated with each channel by time. The demixing process never changes
    // data for the input labels.
    auto labels_iter = labels.begin();
    for (int channel = 0; channel < labels.size(); ++channel) {
      auto& samples_for_channel = expected_label_to_samples[*labels_iter];

      samples_for_channel.reserve(raw_samples.size());
      for (auto tick : raw_samples) {
        samples_for_channel.push_back(tick[channel]);
      }
      labels_iter++;
    }
  }

  void ConfiguredExpectedDemixingChannelFrame(
      ChannelLabel::Label label,
      std::vector<InternalSampleType> expected_demixed_samples) {
    // Configure the expected demixed channels. Typically the input `label`
    // should have a "D_" prefix.
    expected_id_to_labeled_decoded_frame_[kAudioElementId]
        .label_to_samples[label] = expected_demixed_samples;
  }

  void TestDemixing(int expected_number_of_down_mixers) {
    IdLabeledFrameMap unused_id_to_labeled_frame, id_to_labeled_decoded_frame;

    TestCreateDemixingModule(expected_number_of_down_mixers);

    EXPECT_THAT(demixing_module_.DemixAudioSamples(
                    audio_frames_, decoded_audio_frames_,
                    unused_id_to_labeled_frame, id_to_labeled_decoded_frame),
                IsOk());

    // Check that the demixed samples have the correct values.
    const auto& actual_label_to_samples =
        id_to_labeled_decoded_frame[kAudioElementId].label_to_samples;
    const auto& expected_label_to_samples =
        expected_id_to_labeled_decoded_frame_[kAudioElementId].label_to_samples;
    EXPECT_EQ(actual_label_to_samples.size(), expected_label_to_samples.size());
    for (const auto [label, samples] : actual_label_to_samples) {
      // Use `DoubleNear` with a tolerance because floating-point arithmetic
      // introduces errors larger than allowed by `DoubleEq`.
      constexpr double kErrorTolerance = 1e-14;
      EXPECT_THAT(samples, Pointwise(DoubleNear(kErrorTolerance),
                                     expected_label_to_samples.at(label)));
    }
  }

 protected:
  std::list<AudioFrameWithData> audio_frames_;
  std::list<DecodedAudioFrame> decoded_audio_frames_;

  IdLabeledFrameMap expected_id_to_labeled_decoded_frame_;
};  // namespace

TEST_F(DemixingModuleTest, DemixingAudioSamplesSucceedsWithEmptyInputs) {
  iamf_tools_cli_proto::UserMetadata user_metadata;

  // Clear the inputs.
  audio_elements_.clear();
  ASSERT_THAT(demixing_module_.InitializeForDownMixingAndReconstruction(
                  user_metadata, audio_elements_),
              IsOk());

  // Call `DemixAudioSamples()`.
  IdLabeledFrameMap id_to_labeled_frame, id_to_labeled_decoded_frame;
  EXPECT_THAT(demixing_module_.DemixAudioSamples(
                  /*audio_frames=*/{},
                  /*decoded_audio_frames=*/{}, id_to_labeled_frame,
                  id_to_labeled_decoded_frame),
              IsOk());

  // Expect empty outputs.
  EXPECT_TRUE(id_to_labeled_frame.empty());
  EXPECT_TRUE(id_to_labeled_decoded_frame.empty());
}

TEST_F(DemixingModuleTest, AmbisonicsHasNoDemixers) {
  ConfigureAudioFrameMetadata("A0");
  ConfigureAudioFrameMetadata("A1");
  ConfigureAudioFrameMetadata("A2");
  ConfigureAudioFrameMetadata("A3");

  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA0}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA1}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA2}, {{1}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kA3}, {{1}});

  TestDemixing(0);
}

TEST_F(DemixingModuleTest, S1ToS2Demixer) {
  // The highest layer is stereo.
  ConfigureAudioFrameMetadata("L2");
  ConfigureAudioFrameMetadata("R2");

  // Mono is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kMono}, {{750}, {1500}});
  // Stereo is the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL2}, {{1000}, {2000}});

  // Demixing recovers kDemixedR2
  // D_R2 =  M - (L2 - 6 dB)  + 6 dB.
  ConfiguredExpectedDemixingChannelFrame(kDemixedR2, {500, 1000});

  TestDemixing(1);
}

TEST_F(DemixingModuleTest, S2ToS3Demixer) {
  // The highest layer is 3.1.2.
  ConfigureAudioFrameMetadata("L3");
  ConfigureAudioFrameMetadata("R3");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Ltf3");
  ConfigureAudioFrameMetadata("Rtf3");

  // Stereo is the lowest layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kL2, kR2},
                                                  {{70, 70}, {1700, 1700}});

  // 3.1.2 as the next layer.
  ConfigureLosslessAudioFrameAndDecodedAudioFrame({kCentre}, {{100}, {1000}});
  ConfigureLosslessAudioFrameAndDecodedAudioFrame(
      {kLtf3, kRtf3}, {{99999, 99998}, {99999, 99998}});

  // L3/R3 get demixed from the lower layers.
  // L3 = L2 - (C - 3 dB).
  // R3 = R2 - (C - 3 dB).
  ConfiguredExpectedDemixingChannelFrame(kDemixedL3, {-0.7, 993});
  ConfiguredExpectedDemixingChannelFrame(kDemixedR3, {-0.7, 993});

  TestDemixing(1);
}

TEST_F(DemixingModuleTest, S3ToS5AndTf2ToT2Demixers) {
  // Adding a (valid) layer on top of 3.1.2 will always result in both S3ToS5
  // and Tf2ToT2 demixers.
  // The highest layer is 5.1.2.
  ConfigureAudioFrameMetadata("L5");
  ConfigureAudioFrameMetadata("R5");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Ltf2");
  ConfigureAudioFrameMetadata("Rtf2");

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

  TestDemixing(2);
}

TEST_F(DemixingModuleTest, S5ToS7Demixer) {
  // The highest layer is 7.1.0.
  ConfigureAudioFrameMetadata("L7");
  ConfigureAudioFrameMetadata("R7");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Lss7");
  ConfigureAudioFrameMetadata("Rss7");
  ConfigureAudioFrameMetadata("Lrs7");
  ConfigureAudioFrameMetadata("Rrs7");

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

  TestDemixing(1);
}

TEST_F(DemixingModuleTest, T2ToT4Demixer) {
  // The highest layer is 5.1.4.
  ConfigureAudioFrameMetadata("L5");
  ConfigureAudioFrameMetadata("R5");
  ConfigureAudioFrameMetadata("C");
  ConfigureAudioFrameMetadata("Ltf4");
  ConfigureAudioFrameMetadata("Rtf4");

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

  TestDemixing(1);
}

}  // namespace
}  // namespace iamf_tools
