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
#include "iamf/cli/downmixer_manager.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer.h"
#include "iamf/cli/downmixer_factory.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/substream_frames.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>
CreateConfigMap(DecodedUleb128 id,
                absl::flat_hash_set<ChannelLabel::Label> user_labels,
                SubstreamIdLabelsMap substream_id_to_labels,
                LabelGainMap label_to_output_gain = {}) {
  auto down_mixers =
      *std::move(DownmixerFactory::CreateScalableChannelDownmixers(
          user_labels, substream_id_to_labels));
  DownmixerManager::DownmixingConfig out_config{
      .down_mixers = std::move(down_mixers),
      .substream_id_to_labels = substream_id_to_labels,
      .label_to_output_gain = label_to_output_gain,
  };
  absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig> m;
  m.emplace(id, std::move(out_config));
  return m;
}

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Pointwise;

using enum ChannelLabel::Label;
using AudioElementsById = DescriptorObus::AudioElementsById;
using CodecConfigsById = DescriptorObus::CodecConfigsById;

constexpr DecodedUleb128 kAudioElementId = 137;
constexpr size_t kNumSamplesPerFrame = 4;
constexpr DecodedUleb128 kStereoSubstreamId = 2;

constexpr DownMixingParams kIrrelevantDownMixingParams = {};

// TODO(b/305927287): Test some cases of erroneous input.
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

TEST(Make, EmptyConfigMapIsOk) {
  absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>
      id_to_config_map;
  const auto downmixer_manager =
      DownmixerManager::Make(std::move(id_to_config_map));
  EXPECT_THAT(downmixer_manager, NotNull());
}

TEST(Make, ValidWithTwoLayerStereo) {
  DecodedUleb128 id = 137;
  auto id_to_config_map =
      CreateConfigMap(id, {kL2, kR2}, {{0, {kMono}}, {1, {kL2}}}, {});
  const auto downmixer_manager =
      DownmixerManager::Make(std::move(id_to_config_map));
  EXPECT_THAT(downmixer_manager, NotNull());
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

void ExpectHasNumDownMixers(
    const std::unique_ptr<DownmixerManager>& downmixer_manager,
    int expected_number_of_down_mixers) {
  absl::StatusOr<const std::list<DownMixer>*> down_mixers =
      downmixer_manager->GetDownMixers(kAudioElementId);
  ASSERT_THAT(down_mixers, IsOk());
  EXPECT_EQ((*down_mixers)->size(), expected_number_of_down_mixers);
}

void DownMixAndExpectOutput(
    const std::unique_ptr<DownmixerManager>& downmixer_manager,
    const DownMixingParams& down_mixing_params,
    const absl::flat_hash_map<uint32_t, std::vector<std::vector<int32_t>>>&
        substream_id_to_expected_samples,
    LabelSamplesMap input_label_to_samples,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) {
  EXPECT_THAT(downmixer_manager->DownMixSamplesToSubstreams(
                  kAudioElementId, down_mixing_params, input_label_to_samples,
                  substream_id_to_substream_data),
              IsOk());

  for (auto& [substream_id, substream_data] : substream_id_to_substream_data) {
    const auto& output_samples = substream_data.frames_in_obu.Front();

    ASSERT_TRUE(substream_id_to_expected_samples.contains(substream_id));
    ASSERT_EQ(substream_id_to_expected_samples.at(substream_id).size(),
              output_samples.size());
    for (int c = 0; c < output_samples.size(); c++) {
      EXPECT_THAT(
          output_samples[c],
          Pointwise(InternalSampleMatchesIntegralSample(),
                    substream_id_to_expected_samples.at(substream_id).at(c)));
    }
  }
}

class DownMixingModuleTest : public ::testing::Test {
 protected:
  void ConfigureInputChannel(ChannelLabel::Label label,
                             absl::Span<const int32_t> input_samples) {
    input_labels_.insert(label);

    auto [iter, inserted] = input_label_to_samples_.emplace(
        label, std::vector<InternalSampleType>(input_samples.size(), 0));
    Int32ToInternalSampleType(input_samples, absl::MakeSpan(iter->second));
    // This function should not be called with the same label twice.
    ASSERT_TRUE(inserted);
  }

  void ConfigureOutputChannel(
      const std::list<ChannelLabel::Label>& requested_output_labels,
      const std::vector<std::vector<int32_t>>& expected_output_samples) {
    // The substream ID itself does not matter. Generate a unique one.
    const uint32_t substream_id = substream_id_to_labels_.size();

    substream_id_to_labels_[substream_id] = requested_output_labels;
    const auto num_channels = requested_output_labels.size();
    substream_id_to_substream_data_.emplace(
        substream_id, SubstreamData{
                          .substream_id = substream_id,
                          .frames_in_obu = SubstreamFrames<InternalSampleType>(
                              num_channels, kNumSamplesPerFrame),
                          .frames_to_encode = SubstreamFrames<int32_t>(
                              num_channels, kNumSamplesPerFrame),
                      });
    substream_id_to_expected_samples_[substream_id] = expected_output_samples;
  }

  absl::flat_hash_set<ChannelLabel::Label> input_labels_;
  LabelSamplesMap input_label_to_samples_;

  SubstreamIdLabelsMap substream_id_to_labels_;

  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data_;

  absl::flat_hash_map<uint32_t, std::vector<std::vector<int32_t>>>
      substream_id_to_expected_samples_;
};

TEST_F(DownMixingModuleTest, OneLayerStereo) {
  ConfigureInputChannel(kL2, {0, 1, 2, 3});
  ConfigureInputChannel(kR2, {100, 101, 102, 103});
  // Down-mix to stereo as the highest layer. The highest layer always matches
  // the original input.
  ConfigureOutputChannel({kL2, kR2}, {{0, 1, 2, 3}, {100, 101, 102, 103}});

  auto downmixer_manager = DownmixerManager::Make(
      CreateConfigMap(kAudioElementId, input_labels_, substream_id_to_labels_));
  ASSERT_THAT(downmixer_manager, NotNull());
  ExpectHasNumDownMixers(downmixer_manager, 0);

  DownMixAndExpectOutput(downmixer_manager, kIrrelevantDownMixingParams,
                         substream_id_to_expected_samples_,
                         input_label_to_samples_,
                         substream_id_to_substream_data_);
}

TEST(DownMixSamplesToSubstreams, AppliesInverseOutputGainsToFramesToEncode) {
  const DecodedUleb128 kSubstreamId = 0;
  const double kHalfGainDb = 20.0 * std::log10(0.5);
  const double kDoubleGainDb = 20.0 * std::log10(2.0);
  auto downmixer_manager = DownmixerManager::Make(
      CreateConfigMap(kAudioElementId, {kL2, kR2}, {{kSubstreamId, {kL2, kR2}}},
                      {{kL2, kHalfGainDb}, {kR2, kDoubleGainDb}}));
  ASSERT_THAT(downmixer_manager, NotNull());
  LabelSamplesMap input_label_to_samples = {
      {kL2, Int32ToInternalSampleType({1000})},
      {kR2, Int32ToInternalSampleType({5000})},
  };
  constexpr int kTwoChannels = 2;
  constexpr size_t kOneSamplePerFrame = 1;
  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data;
  substream_id_to_substream_data.emplace(
      kSubstreamId, SubstreamData{
                        .substream_id = 0,
                        .frames_in_obu = SubstreamFrames<InternalSampleType>(
                            kTwoChannels, kOneSamplePerFrame),
                        .frames_to_encode = SubstreamFrames<int32_t>(
                            kTwoChannels, kOneSamplePerFrame),
                    });

  EXPECT_THAT(downmixer_manager->DownMixSamplesToSubstreams(
                  kAudioElementId, kIrrelevantDownMixingParams,
                  input_label_to_samples, substream_id_to_substream_data),
              IsOk());

  // Compare in the floating point domain to allow for some tolerance in the
  // expected outputs.
  ASSERT_TRUE(substream_id_to_substream_data.contains(kSubstreamId));
  const auto& downmixed_frame =
      substream_id_to_substream_data.at(kSubstreamId).frames_to_encode.Front();
  ASSERT_THAT(downmixed_frame, Has2DShape(kTwoChannels, kOneSamplePerFrame));
  constexpr double kEquivalenceTolerance = 1e-6;
  // The downmixer divides by the output gains to compensate for the downstream
  // decoder's multiplication.
  // - L2 is divided by 0.5
  // - R2 is divided by 2.0.
  constexpr InternalSampleType kExpectedL2Sample =
      Int32ToNormalizedFloatingPoint<InternalSampleType>(2000);
  constexpr InternalSampleType kExpectedR2Sample =
      Int32ToNormalizedFloatingPoint<InternalSampleType>(2500);
  EXPECT_THAT(
      Int32ToInternalSampleType2D(downmixed_frame),
      ElementsAre(
          ElementsAre(DoubleNear(kExpectedL2Sample, kEquivalenceTolerance)),
          ElementsAre(DoubleNear(kExpectedR2Sample, kEquivalenceTolerance))));
}

TEST_F(DownMixingModuleTest, ReturnsErrorWhenMissingInputChannels) {
  // Configure a two-layer stereo down-mixer.
  ConfigureInputChannel(kL2, {0});
  ConfigureInputChannel(kR2, {0});
  ConfigureOutputChannel({kL2}, {{0}});
  ConfigureOutputChannel({kMono}, {{0}});
  auto downmixer_manager = DownmixerManager::Make(
      CreateConfigMap(kAudioElementId, input_labels_, substream_id_to_labels_));
  ASSERT_THAT(downmixer_manager, NotNull());

  // Later, the L2 channel was missing.
  input_label_to_samples_.erase(kL2);
  EXPECT_THAT(downmixer_manager->DownMixSamplesToSubstreams(
                  kAudioElementId, kIrrelevantDownMixingParams,
                  input_label_to_samples_, substream_id_to_substream_data_),
              Not(IsOk()));
}

TEST(DownMixSamplesToSubstreams, ReturnsErrorWhenAudioElementIdNotFound) {
  const absl::flat_hash_set<ChannelLabel::Label> kStereoInputLabels = {kL2,
                                                                       kR2};
  const SubstreamIdLabelsMap kOneLayerStereoOutputIdToLabels = {
      {kStereoSubstreamId, {kL2, kR2}}};
  auto downmixer_manager = DownmixerManager::Make(CreateConfigMap(
      kAudioElementId, kStereoInputLabels, kOneLayerStereoOutputIdToLabels));
  ASSERT_THAT(downmixer_manager, NotNull());
  const DecodedUleb128 kUnconfiguredAudioElementId = kAudioElementId + 1;
  LabelSamplesMap empty_input_label_to_samples;
  absl::flat_hash_map<uint32_t, SubstreamData>
      empty_substream_id_to_substream_data;

  EXPECT_THAT(
      downmixer_manager->DownMixSamplesToSubstreams(
          kUnconfiguredAudioElementId, kIrrelevantDownMixingParams,
          empty_input_label_to_samples, empty_substream_id_to_substream_data),
      Not(IsOk()));
}

TEST(GetDownMixers, ReturnsErrorWhenAudioElementIdNotFound) {
  const absl::flat_hash_set<ChannelLabel::Label> kStereoInputLabels = {kL2,
                                                                       kR2};
  const SubstreamIdLabelsMap kOneLayerStereoOutputIdToLabels = {
      {kStereoSubstreamId, {kL2, kR2}}};
  auto downmixer_manager = DownmixerManager::Make(CreateConfigMap(
      kAudioElementId, kStereoInputLabels, kOneLayerStereoOutputIdToLabels));
  ASSERT_THAT(downmixer_manager, NotNull());
  // Call with a non-existent audio element ID.
  const DecodedUleb128 kUnconfiguredAudioElementId = kAudioElementId + 1;

  EXPECT_THAT(downmixer_manager->GetDownMixers(kUnconfiguredAudioElementId),
              Not(IsOk()));
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
  ConfigureOutputChannel({kLtb4, kRtb4}, {{1000}, {2000}});

  // Down-mix to 7.1.2 as the fifth layer.
  ConfigureOutputChannel({kLrs7, kRrs7}, {{3000}, {4000}});

  // Down-mix to 5.1.2 as the fourth layer.
  // Ls5 = Lss7 * alpha + Lrs7 * beta.
  ConfigureOutputChannel({kLs5, kRs5}, {{3598}, {5464}});

  // Down-mix to 3.1.2 as the third layer.
  ConfigureOutputChannel({kCentre}, {{1000}});
  // Ltf2 = Ltf4 + Ltb4 * gamma.
  // Ltf3 = Ltf2 + Ls5 * w * delta.
  ConfigureOutputChannel({kLtf3, kRtf3}, {{2644}, {4914}});
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

  auto downmixer_manager = DownmixerManager::Make(
      CreateConfigMap(kAudioElementId, input_labels_, substream_id_to_labels_));
  ASSERT_THAT(downmixer_manager, NotNull());
  ExpectHasNumDownMixers(downmixer_manager, 6);

  DownMixAndExpectOutput(
      downmixer_manager,
      {.alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25},
      substream_id_to_expected_samples_, input_label_to_samples_,
      substream_id_to_substream_data_);
}

TEST(MakeForPassthrough, EmptyMapIsOk) {
  DescriptorObus::AudioElementsById audio_elements;

  const auto downmixer_manager =
      DownmixerManager::MakeForPassthrough(audio_elements);

  EXPECT_THAT(downmixer_manager, NotNull());
}

TEST(MakeForPassthrough, TwoLayerStereoHasNoDownMixers) {
  const SubstreamIdLabelsMap substream_id_to_labels = {{0, {kMono}},
                                                       {1, {kL2}}};
  DescriptorObus::AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      substream_id_to_labels, kTwoLayerStereoConfig, audio_elements);

  auto downmixer_manager = DownmixerManager::MakeForPassthrough(audio_elements);
  ASSERT_THAT(downmixer_manager, NotNull());

  auto down_mixers = downmixer_manager->GetDownMixers(kAudioElementId);
  EXPECT_THAT(down_mixers, IsOkAndHolds(Pointee(IsEmpty())));
}

}  // namespace
}  // namespace iamf_tools
