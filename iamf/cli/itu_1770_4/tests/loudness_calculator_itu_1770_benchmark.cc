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
#include "iamf/cli/itu_1770_4/loudness_calculator_itu_1770_4.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum LoudspeakersSsConventionLayout::SoundSystem;

constexpr uint32_t kSampleRate = 48000;

static MixPresentationLayout GetSoundSystemLayout(
    LoudspeakersSsConventionLayout::SoundSystem sound_system,
    bool measure_true_peak) {
  return {.loudness_layout = {.layout_type =
                                  Layout::kLayoutTypeLoudspeakersSsConvention,
                              .specific_layout =
                                  LoudspeakersSsConventionLayout{
                                      .sound_system = sound_system}},
          .loudness = {
              .info_type = static_cast<uint8_t>(
                  measure_true_peak ? LoudnessInfo::kTruePeak : 0),
          }};
}

static std::vector<std::vector<InternalSampleType>> CreateAudioSamples(
    int32_t num_channels, int num_ticks) {
  std::vector<std::vector<InternalSampleType>> samples(
      num_channels, std::vector<InternalSampleType>(num_ticks));
  int32_t i = 0;
  const InternalSampleType denominator = num_ticks * samples.size();
  for (auto& channel : samples) {
    for (auto& sample : channel) {
      sample = static_cast<InternalSampleType>(i++) / denominator;
    }
  }
  return samples;
}

static void BM_LoudnessCalculatorItu1770_4(
    LoudspeakersSsConventionLayout::SoundSystem sound_system,
    benchmark::State& state) {
  const bool measure_true_peak = state.range(0);
  const int num_samples_per_frame = state.range(1);
  const int bit_depth = state.range(2);
  const auto layout = GetSoundSystemLayout(sound_system, measure_true_peak);
  // Derive the number of channels from the layout.
  int32_t num_channels;
  absl::Status status = MixPresentationObu::GetNumChannelsFromLayout(
      layout.loudness_layout, num_channels);
  CHECK_OK(status);

  // Create a loudness calculator.
  auto loudness_calculator = LoudnessCalculatorItu1770_4::CreateForLayout(
      layout, num_samples_per_frame, kSampleRate, bit_depth);

  // Create input samples and a vector of spans pointing to the channels.
  auto samples = CreateAudioSamples(num_channels, num_samples_per_frame);
  const auto sample_spans = MakeSpanOfConstSpans(samples);

  // Measure the calls to
  // `LoudnessCalculatorItu1770_4::AccumulateLoudnessForSamples()`.
  for (auto _ : state) {
    CHECK_OK(loudness_calculator->AccumulateLoudnessForSamples(sample_spans));
  }
}

static void BM_LoudnessCalculatorItu1770_4_ForSoundSystemA(
    benchmark::State& state) {
  BM_LoudnessCalculatorItu1770_4(kSoundSystemA_0_2_0, state);
}

static void BM_LoudnessCalculatorItu1770_4_ForSoundSystemB(
    benchmark::State& state) {
  BM_LoudnessCalculatorItu1770_4(kSoundSystemB_0_5_0, state);
}

static void BM_LoudnessCalculatorItu1770_4_ForSoundSystemJ(
    benchmark::State& state) {
  BM_LoudnessCalculatorItu1770_4(kSoundSystemJ_4_7_0, state);
}

static void BM_LoudnessCalculatorItu1770_4_ForSoundSystem13(
    benchmark::State& state) {
  BM_LoudnessCalculatorItu1770_4(kSoundSystem13_6_9_0, state);
}

// Benchmark common and high-channel count sound systems for ITU 1770-4. We
// expect the main impact from the specific sound system to be on the number of
// channels.
BENCHMARK(BM_LoudnessCalculatorItu1770_4_ForSoundSystemA)
    ->Args({0, 480, 16})
    ->Args({1, 480, 16})
    ->Args({1, 480, 24})
    ->Args({1, 480, 32})
    ->Args({0, 960, 32})
    ->Args({1, 960, 32})
    ->Args({0, 1920, 32})
    ->Args({1, 1920, 32});

BENCHMARK(BM_LoudnessCalculatorItu1770_4_ForSoundSystemB)
    ->Args({0, 480, 16})
    ->Args({1, 480, 16})
    ->Args({1, 480, 24})
    ->Args({1, 480, 32})
    ->Args({0, 960, 32})
    ->Args({1, 960, 32})
    ->Args({0, 1920, 32})
    ->Args({1, 1920, 32});

BENCHMARK(BM_LoudnessCalculatorItu1770_4_ForSoundSystemJ)
    ->Args({0, 480, 16})
    ->Args({1, 480, 16})
    ->Args({1, 480, 24})
    ->Args({1, 480, 32})
    ->Args({0, 960, 32})
    ->Args({1, 960, 32})
    ->Args({0, 1920, 32})
    ->Args({1, 1920, 32});

BENCHMARK(BM_LoudnessCalculatorItu1770_4_ForSoundSystem13)
    ->Args({0, 480, 16})
    ->Args({1, 480, 16})
    ->Args({1, 480, 24})
    ->Args({1, 480, 32})
    ->Args({0, 960, 32})
    ->Args({1, 960, 32})
    ->Args({0, 1920, 32})
    ->Args({1, 1920, 32});

}  // namespace
}  // namespace iamf_tools
