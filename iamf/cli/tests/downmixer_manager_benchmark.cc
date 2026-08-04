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

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/downmixer_factory.h"
#include "iamf/cli/downmixer_manager.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/substream_frames.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum ChannelLabel::Label;
constexpr DecodedUleb128 kAudioElementId = 591;
constexpr DownMixingParams kDownMixingParams = {
    .alpha = 1, .beta = .866, .gamma = .866, .delta = .866, .w = 0.25};

static void ConfigureInputChannel(ChannelLabel::Label label, int num_ticks,
                                  LabelSamplesMap& input_label_to_samples) {
  auto [iter, inserted] = input_label_to_samples.emplace(
      label, std::vector<InternalSampleType>(num_ticks, 0.0));

  // This function should not be called with the same label twice, so the
  // insertion should succeed.
  ABSL_CHECK(inserted);
}

static void ConfigureOutputChannel(
    const std::list<ChannelLabel::Label>& requested_output_labels,
    const size_t num_samples_per_frame,
    SubstreamIdLabelsMap& substream_id_to_labels,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) {
  // The substream ID itself does not matter. Generate a unique one.
  const uint32_t substream_id = substream_id_to_labels.size();
  substream_id_to_labels[substream_id] = requested_output_labels;
  const auto num_channels = requested_output_labels.size();
  substream_id_to_substream_data.emplace(
      substream_id, SubstreamData{
                        .substream_id = substream_id,
                        .frames_in_obu = SubstreamFrames<InternalSampleType>(
                            num_channels, num_samples_per_frame),
                        .frames_to_encode = SubstreamFrames<int32_t>(
                            num_channels, num_samples_per_frame),
                    });
}

static std::unique_ptr<DownmixerManager> CreateDownmixerManager(
    const SubstreamIdLabelsMap& substream_id_to_labels) {
  auto downmixers = DownmixerFactory::CreateScalableChannelDownmixers(
      {kL2, kR2}, substream_id_to_labels);
  ABSL_CHECK_OK(downmixers);
  absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig> map;
  map.emplace(kAudioElementId,
              DownmixerManager::DownmixingConfig{
                  .down_mixers = *std::move(downmixers),
                  .substream_id_to_labels = substream_id_to_labels,
                  .label_to_output_gain = {{kMono, 0}, {kR2, 0}}});
  return DownmixerManager::Make(std::move(map));
}

// Currently benchmarking down-mixing from stereo to mono.
// Down-mixing between other layouts should take time proportional to the number
// of units of operations.
static void BM_DownMixing(benchmark::State& state) {
  // Set up the input.
  const int num_ticks = state.range(0);
  LabelSamplesMap input_label_to_samples;
  ConfigureInputChannel(kL2, num_ticks, input_label_to_samples);
  ConfigureInputChannel(kR2, num_ticks, input_label_to_samples);

  // Placeholder for the output.
  SubstreamIdLabelsMap substream_id_to_labels;
  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data;
  ConfigureOutputChannel({kMono}, num_ticks, substream_id_to_labels,
                         substream_id_to_substream_data);

  // Create a downmixer manager.
  auto downmixer_manager = CreateDownmixerManager(substream_id_to_labels);

  // Measure the calls to `DownmixerManager::DownMixSamplesToSubstreams()`.
  for (auto _ : state) {
    auto status = downmixer_manager->DownMixSamplesToSubstreams(
        kAudioElementId, kDownMixingParams, input_label_to_samples,
        substream_id_to_substream_data);

    // Simulate consuming the substream data by popping the samples.
    for (auto& [unused_id, substream_data] : substream_id_to_substream_data) {
      substream_data.frames_to_encode.PopFront();
      substream_data.frames_in_obu.PopFront();
    }
  }
}

// Benchmark with different number of samples per frame.
BENCHMARK(BM_DownMixing)->Args({1 << 8})->Args({1 << 10})->Args({1 << 12});

}  // namespace
}  // namespace iamf_tools
