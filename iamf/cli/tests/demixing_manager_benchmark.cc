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

#include <list>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum ChannelLabel::Label;
constexpr DecodedUleb128 kAudioElementId = 591;
constexpr DownMixingParams kDownMixingParams = {
    .alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25};
constexpr InternalTimestamp kStartTimestamp = 0;

static void ConfigureLosslessAudioFrame(
    const std::list<ChannelLabel::Label>& labels, const int num_ticks,
    SubstreamIdLabelsMap& substream_id_to_labels,
    std::list<AudioFrameWithData>& frames) {
  static std::vector<std::vector<InternalSampleType>> samples(1);
  samples[0].resize(num_ticks);

  // The substream ID itself does not matter. Generate a unique one.
  const DecodedUleb128 substream_id = substream_id_to_labels.size();
  substream_id_to_labels[substream_id] = labels;
  // A lossless audio frame would have the same encoded and decoded samples.
  frames.emplace_back(
      AudioFrameWithData{.obu = AudioFrameObu(ObuHeader(), substream_id, {}),
                         .start_timestamp = kStartTimestamp,
                         .end_timestamp = kStartTimestamp + num_ticks,
                         .encoded_samples = samples,
                         .decoded_samples = absl::MakeConstSpan(samples),
                         .down_mixing_params = kDownMixingParams});
}

static void InitAudioElementWithLabelsAndScalableChannelLayout(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const ScalableChannelLayoutConfig& config,
    DescriptorObus::AudioElementsById& audio_elements) {
  constexpr DecodedUleb128 kCodecConfigId = 0;
  std::vector<DecodedUleb128> substream_ids;
  substream_ids.reserve(substream_id_to_labels.size());
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    substream_ids.push_back(substream_id);
  }

  auto obu = AudioElementObu::CreateForScalableChannelLayout(
      ObuHeader(), kAudioElementId, /*reserved=*/0, kCodecConfigId,
      substream_ids, config);
  ABSL_CHECK_OK(obu.status());

  audio_elements.emplace(kAudioElementId,
                         AudioElementWithData{
                             .obu = *std::move(obu),
                             .substream_id_to_labels = substream_id_to_labels,
                         });
}

const ScalableChannelLayoutConfig kTwoLayerStereoConfig = {
    .channel_audio_layer_configs = {
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
         .substream_count = 1},
        {.loudspeaker_layout = ChannelAudioLayerConfig::kLayoutStereo,
         .substream_count = 1}}};

static DemixingManager CreateDemixingManager(
    const SubstreamIdLabelsMap& substream_id_to_labels) {
  DescriptorObus::AudioElementsById audio_elements;
  InitAudioElementWithLabelsAndScalableChannelLayout(
      substream_id_to_labels, kTwoLayerStereoConfig, audio_elements);

  auto demixing_manager = DemixingManager::Create(
      DemixingManager::CreateIdToReconstructionConfig(audio_elements));
  ABSL_CHECK_OK(demixing_manager);

  return *demixing_manager;
}

absl::StatusOr<IdLabeledFrameMap> CallDemixing(
    bool use_original_samples, const std::list<AudioFrameWithData>& frames,
    DemixingManager& demixing_manager) {
  if (use_original_samples) {
    return demixing_manager.DemixOriginalAudioSamples(frames);
  } else {
    return demixing_manager.DemixDecodedAudioSamples(frames);
  }
}

void BM_Demixing(bool use_original_samples, benchmark::State& state) {
  // Set up the input.
  const int num_ticks = state.range(0);
  SubstreamIdLabelsMap substream_id_to_labels;
  std::list<AudioFrameWithData> audio_frames;

  // Mono is the lowest layer.
  ConfigureLosslessAudioFrame({kMono}, num_ticks, substream_id_to_labels,
                              audio_frames);

  // Stereo is the next layer. One additional channel (L2) is provided.
  ConfigureLosslessAudioFrame({kL2}, num_ticks, substream_id_to_labels,
                              audio_frames);

  // Create a demixing manager.
  auto demixing_manager = CreateDemixingManager(substream_id_to_labels);

  // Measure the calls to either `DemixingManager::DemixOriginalAudioSamples()`
  // or `DemixingManager::DemixDecodedAudioSamples()`.
  for (auto _ : state) {
    auto id_to_labeled_frame =
        CallDemixing(use_original_samples, audio_frames, demixing_manager);
    ABSL_CHECK_OK(id_to_labeled_frame);
  }
}

static void BM_DemixingOriginal(benchmark::State& state) {
  BM_Demixing(true, state);
}

static void BM_DemixingDecoded(benchmark::State& state) {
  BM_Demixing(false, state);
}

// Benchmark with different number of samples per frame.
BENCHMARK(BM_DemixingOriginal)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});
BENCHMARK(BM_DemixingDecoded)->Args({1 << 8})->Args({1 << 10})->Args({1 << 12});

}  // namespace
}  // namespace iamf_tools
