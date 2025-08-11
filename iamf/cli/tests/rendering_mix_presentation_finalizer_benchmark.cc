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

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum ChannelLabel::Label;
using enum LoudspeakersSsConventionLayout::SoundSystem;

constexpr uint32_t kAudioElementId = 59;
constexpr uint32_t kCodecConfigId = 42;
constexpr uint32_t kMixPresentationId = 13;
constexpr uint32_t kBitDepth = 16;
constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kCommonParameterId = 999;
constexpr uint32_t kCommonParameterRate = kSampleRate;
constexpr std::array<DecodedUleb128, 1> kStereoSubstreamIds = {1};
constexpr std::array<DecodedUleb128, 4> kFoaSubstreamIds = {2, 3, 4, 5};

static LabelSamplesMap GetLabelToSamples(bool ambisonics_input, int num_ticks) {
  if (ambisonics_input) {
    return LabelSamplesMap{
        {kA0, std::vector<InternalSampleType>(num_ticks, 0.3)},
        {kA1, std::vector<InternalSampleType>(num_ticks, 0.4)},
        {kA2, std::vector<InternalSampleType>(num_ticks, 0.5)},
        {kA3, std::vector<InternalSampleType>(num_ticks, 0.6)}};
  } else {
    return LabelSamplesMap{
        {kL2, std::vector<InternalSampleType>(num_ticks, 0.1)},
        {kR2, std::vector<InternalSampleType>(num_ticks, 0.9)}};
  }
}

static IdLabeledFrameMap CreateInput(bool ambisonics_input, int num_ticks) {
  IdLabeledFrameMap id_to_labeled_frame;
  id_to_labeled_frame[kAudioElementId] = {
      .samples_to_trim_at_end = 0,
      .samples_to_trim_at_start = 0,
      .label_to_samples = GetLabelToSamples(ambisonics_input, num_ticks)};
  return id_to_labeled_frame;
}

static RenderingMixPresentationFinalizer
CreateRenderingMixPresentationFinalizer(
    bool ambisonics_input,
    iamf_tools::LoudspeakersSsConventionLayout::SoundSystem sound_system_layout,
    int num_ticks,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfig(kCodecConfigId, num_ticks, kBitDepth, kSampleRate,
                     codec_config_obus);
  if (ambisonics_input) {
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kAudioElementId, kCodecConfigId, kFoaSubstreamIds, codec_config_obus,
        audio_elements);
  } else {
    AddScalableAudioElementWithSubstreamIds(
        IamfInputLayout::kStereo, kAudioElementId, kCodecConfigId,
        kStereoSubstreamIds, codec_config_obus, audio_elements);
  }

  AddMixPresentationObuWithConfigurableLayouts(
      kMixPresentationId, {kAudioElementId}, kCommonParameterId,
      kCommonParameterRate, {sound_system_layout}, mix_presentation_obus);

  auto renderer_factory = std::make_unique<RendererFactory>();
  auto finalizer = RenderingMixPresentationFinalizer::Create(
      renderer_factory.get(),
      /*loudness_calculator_factory=*/nullptr, audio_elements,
      RenderingMixPresentationFinalizer::ProduceNoSampleProcessors,
      mix_presentation_obus);
  CHECK_OK(finalizer);
  return std::move(*finalizer);
}

static void BM_PushTemporalUnit(
    bool ambisonics_input,
    iamf_tools::LoudspeakersSsConventionLayout::SoundSystem sound_system_layout,
    benchmark::State& state) {
  // Set up the input.
  const int num_ticks = state.range(0);
  IdLabeledFrameMap id_to_labeled_frame =
      CreateInput(ambisonics_input, num_ticks);

  // Create a rendering mix presentation finalizer using prerequisite OBUs.
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  auto finalizer = CreateRenderingMixPresentationFinalizer(
      ambisonics_input, sound_system_layout, num_ticks, codec_config_obus,
      audio_elements, mix_presentation_obus);

  // Measure the calls to
  // `RenderingMixPresentationFinalizer::PushTemporalUnit()`, which will
  // render the samples to layouts.
  std::list<ParameterBlockWithData> empty_parameter_blocks;
  for (auto _ : state) {
    CHECK_OK(finalizer.PushTemporalUnit(id_to_labeled_frame,
                                        /*start_timestamp=*/0,
                                        /*end_timestamp=*/num_ticks,
                                        empty_parameter_blocks));
  }
}

static void BM_PushTemporalUnitFoaToStereo(benchmark::State& state) {
  BM_PushTemporalUnit(true, kSoundSystemA_0_2_0, state);
}

static void BM_PushTemporalUnitFoaTo5_1_2(benchmark::State& state) {
  BM_PushTemporalUnit(true, kSoundSystemC_2_5_0, state);
}

static void BM_PushTemporalUnitFoaTo7_1_4(benchmark::State& state) {
  BM_PushTemporalUnit(true, kSoundSystemJ_4_7_0, state);
}

static void BM_PushTemporalUnitStereoToStereo(benchmark::State& state) {
  BM_PushTemporalUnit(false, kSoundSystemA_0_2_0, state);
}

static void BM_PushTemporalUnitStereoTo5_1_2(benchmark::State& state) {
  BM_PushTemporalUnit(false, kSoundSystemC_2_5_0, state);
}

static void BM_PushTemporalUnitStereoTo7_1_4(benchmark::State& state) {
  BM_PushTemporalUnit(false, kSoundSystemJ_4_7_0, state);
}

// Benchmark with different number of samples per frame.
// From FOA inputs.
BENCHMARK(BM_PushTemporalUnitFoaToStereo)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});
BENCHMARK(BM_PushTemporalUnitFoaTo5_1_2)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});
BENCHMARK(BM_PushTemporalUnitFoaTo7_1_4)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});

// From stereo inputs.
BENCHMARK(BM_PushTemporalUnitStereoToStereo)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});
BENCHMARK(BM_PushTemporalUnitStereoTo5_1_2)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});
BENCHMARK(BM_PushTemporalUnitStereoTo7_1_4)
    ->Args({1 << 8})
    ->Args({1 << 10})
    ->Args({1 << 12});

}  // namespace
}  // namespace iamf_tools
