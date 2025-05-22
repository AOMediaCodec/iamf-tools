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

#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "iamf/api/conversion/channel_reorderer.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum LoudspeakersSsConventionLayout::SoundSystem;

static int32_t GetNumberOfChannels(
    LoudspeakersSsConventionLayout::SoundSystem sound_system) {
  int32_t num_channels = 0;
  absl::Status status = MixPresentationObu::GetNumChannelsFromLayout(
      {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
       .specific_layout =
           LoudspeakersSsConventionLayout{.sound_system = sound_system}},
      num_channels);
  CHECK_OK(status);
  return num_channels;
}

static std::vector<std::vector<InternalSampleType>> CreateAudioSamples(
    LoudspeakersSsConventionLayout::SoundSystem sound_system, int num_ticks) {
  std::vector<std::vector<InternalSampleType>> samples(
      GetNumberOfChannels(sound_system),
      std::vector<InternalSampleType>(num_ticks));
  int32_t i = 0;
  const InternalSampleType denominator = num_ticks * samples.size();
  for (auto& channel : samples) {
    for (auto& sample : channel) {
      sample = static_cast<InternalSampleType>(i++) / denominator;
    }
  }
  return samples;
}

static void BM_ReorderForAndroid(
    LoudspeakersSsConventionLayout::SoundSystem sound_system,
    benchmark::State& state) {
  // Create a channel reorderer.
  // We do not benchmark the NoOp scheme, since it should be trivial.
  const auto scheme = ChannelReorderer::RearrangementScheme::kReorderForAndroid;
  auto reorderer = ChannelReorderer::Create(sound_system, scheme);

  // Create input samples and a vector of spans pointing to the channels.
  const int num_ticks = state.range(0);
  auto samples = CreateAudioSamples(sound_system, num_ticks);
  std::vector<absl::Span<const InternalSampleType>> sample_spans(
      samples.size());
  for (int c = 0; c < samples.size(); c++) {
    sample_spans[c] = absl::MakeConstSpan(samples[c]);
  }

  // Measure the calls to `ChannelReorderer::Reorder()`.
  for (auto _ : state) {
    reorderer.Reorder(sample_spans);
  }
}

static void BM_ReorderForAndroid_SoundSystemF(benchmark::State& state) {
  BM_ReorderForAndroid(kSoundSystemF_3_7_0, state);
}

static void BM_ReorderForAndroid_SoundSystemG(benchmark::State& state) {
  BM_ReorderForAndroid(kSoundSystemG_4_9_0, state);
}

static void BM_ReorderForAndroid_SoundSystemH(benchmark::State& state) {
  BM_ReorderForAndroid(kSoundSystemH_9_10_3, state);
}

static void BM_ReorderForAndroid_SoundSystemI(benchmark::State& state) {
  BM_ReorderForAndroid(kSoundSystemI_0_7_0, state);
}

static void BM_ReorderForAndroid_SoundSystemJ(benchmark::State& state) {
  BM_ReorderForAndroid(kSoundSystemJ_4_7_0, state);
}

static void BM_ReorderForAndroid_SoundSystem10(benchmark::State& state) {
  BM_ReorderForAndroid(kSoundSystem10_2_7_0, state);
}

// Benchmark for sound systems that require reordering for Android, which means
// to exclude Sound system A, B, C, D, E, 11, 12, and 13.
BENCHMARK(BM_ReorderForAndroid_SoundSystemF)
    ->Args({1 << 4})
    ->Args({1 << 8})
    ->Args({1 << 12});

BENCHMARK(BM_ReorderForAndroid_SoundSystemG)
    ->Args({1 << 4})
    ->Args({1 << 8})
    ->Args({1 << 12});

BENCHMARK(BM_ReorderForAndroid_SoundSystemH)
    ->Args({1 << 4})
    ->Args({1 << 8})
    ->Args({1 << 12});

BENCHMARK(BM_ReorderForAndroid_SoundSystemI)
    ->Args({1 << 4})
    ->Args({1 << 8})
    ->Args({1 << 12});

BENCHMARK(BM_ReorderForAndroid_SoundSystemJ)
    ->Args({1 << 4})
    ->Args({1 << 8})
    ->Args({1 << 12});

BENCHMARK(BM_ReorderForAndroid_SoundSystem10)
    ->Args({1 << 4})
    ->Args({1 << 8})
    ->Args({1 << 12});

}  // namespace
}  // namespace iamf_tools
