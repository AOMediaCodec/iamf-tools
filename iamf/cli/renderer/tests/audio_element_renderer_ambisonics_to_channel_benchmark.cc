/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <variant>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/renderer/audio_element_renderer_ambisonics_to_channel.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using enum ChannelLabel::Label;
using enum LoudspeakersSsConventionLayout::SoundSystem;
using enum AmbisonicsConfig::AmbisonicsMode;
using AmbisonicsMode = AmbisonicsConfig::AmbisonicsMode;

constexpr DecodedUleb128 kAudioElementId = 0;
constexpr uint8_t kReserved = 0;
constexpr DecodedUleb128 kCodecConfigId = 0;
constexpr uint8_t kCoupledSubstreamCount = 0;

const Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout =
        LoudspeakersSsConventionLayout{.sound_system = kSoundSystemA_0_2_0}};

static std::vector<int16_t> CreateIdentityMatrix(size_t size) {
  std::vector<int16_t> matrix(size * size, 0);
  for (size_t i = 0; i < size; ++i) {
    matrix[i * size + i] = std::numeric_limits<int16_t>::max();
  }
  return matrix;
}

// Creates a 7x16 matrix for mixed-order (horizontal only) TOA projection. Some
// ambisonics channel numbers (ACNs) are set to 0, indicating this channel is
// dropped.
static std::vector<int16_t> CreateHorizontalToaProjectionMatrix() {
  std::vector<int16_t> matrix(7 * 16, 0);
  const std::vector<int> active_acns = {0, 1, 3, 4, 8, 9, 15};
  for (size_t i = 0; i < 7; ++i) {
    matrix[i * 16 + active_acns[i]] = std::numeric_limits<int16_t>::max();
  }
  return matrix;
}

// Creates a `LabeledFrame` with `num_ticks` samples for each label in the
// `substream_id_to_labels` map. The samples are set to the index of the label
// in the map.
static LabeledFrame CreateLabeledFrameFromMap(
    const SubstreamIdLabelsMap& substream_id_to_labels, int num_ticks) {
  LabeledFrame frame{.samples_to_trim_at_end = 0,
                     .samples_to_trim_at_start = 0};
  int i = 0;
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    for (const auto& label : labels) {
      const InternalSampleType sample_value =
          Int32ToNormalizedFloatingPoint<InternalSampleType>(i);
      frame.label_to_samples[label] =
          std::vector<InternalSampleType>(num_ticks, sample_value);
      i++;
    }
  }
  return frame;
}

// Creates an Audio Element OBU for the given `Order` and `AmbisonicsMode`.
template <int Order, AmbisonicsMode Mode>
static absl::StatusOr<AudioElementObu> CreateFullOrderAudioElement() {
  const size_t num_channels = (Order + 1) * (Order + 1);
  std::vector<DecodedUleb128> audio_substream_ids(num_channels);
  std::iota(audio_substream_ids.begin(), audio_substream_ids.end(), 0);

  if (Mode == kAmbisonicsModeMono) {
    std::vector<uint8_t> channel_mapping(num_channels);
    std::iota(channel_mapping.begin(), channel_mapping.end(), 0);
    return AudioElementObu::CreateForMonoAmbisonics(
        ObuHeader{}, kAudioElementId, kReserved, kCodecConfigId,
        audio_substream_ids, channel_mapping);
  }
  return AudioElementObu::CreateForProjectionAmbisonics(
      ObuHeader{}, kAudioElementId, kReserved, kCodecConfigId,
      audio_substream_ids, num_channels, kCoupledSubstreamCount,
      CreateIdentityMatrix(num_channels));
}

// Creates an Audio Element OBU for the given `AmbisonicsMode` using third-order
// ambisonics (TOA) with only the horizontal channels.
template <AmbisonicsMode Mode>
static absl::StatusOr<AudioElementObu> CreateHorizontalToaAudioElement() {
  constexpr std::array<DecodedUleb128, 7> substream_ids = {0, 1, 2, 3, 4, 5, 6};
  if (Mode == kAmbisonicsModeMono) {
    // Horizontal-only TOA has 7 active channels. The others are inactive.
    constexpr auto kInactiveChannel =
        AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber;
    const std::array<uint8_t, 16> channel_mapping = {0,
                                                     1,
                                                     kInactiveChannel,
                                                     2,
                                                     3,
                                                     kInactiveChannel,
                                                     kInactiveChannel,
                                                     kInactiveChannel,
                                                     4,
                                                     5,
                                                     kInactiveChannel,
                                                     kInactiveChannel,
                                                     kInactiveChannel,
                                                     kInactiveChannel,
                                                     kInactiveChannel,
                                                     6};
    return AudioElementObu::CreateForMonoAmbisonics(
        ObuHeader{}, kAudioElementId, kReserved, kCodecConfigId, substream_ids,
        channel_mapping);
  }
  constexpr uint8_t kOutputChannelCount = 16;
  return AudioElementObu::CreateForProjectionAmbisonics(
      ObuHeader{}, kAudioElementId, kReserved, kCodecConfigId, substream_ids,
      kOutputChannelCount, kCoupledSubstreamCount,
      CreateHorizontalToaProjectionMatrix());
}

static void RunRenderAmbisonicsBenchmark(const AudioElementObu& audio_element,
                                         int num_ticks,
                                         benchmark::State& state) {
  // Prepare the `AudioElementWithData`, which contains various settings to help
  // configure the renderer.
  AudioElementWithData audio_element_with_data = {.obu = audio_element};
  ABSL_CHECK_OK(ObuWithDataGenerator::FinalizeAmbisonicsConfig(
      audio_element_with_data.obu,
      audio_element_with_data.substream_id_to_labels));

  const LabeledFrame frame = CreateLabeledFrameFromMap(
      audio_element_with_data.substream_id_to_labels, num_ticks);
  const auto* ambisonics_config =
      std::get_if<AmbisonicsConfig>(&audio_element_with_data.obu.config_);
  ABSL_CHECK_NE(ambisonics_config, nullptr);
  // Create the renderer.
  std::unique_ptr<AudioElementRendererAmbisonicsToChannel> renderer =
      AudioElementRendererAmbisonicsToChannel::CreateFromAmbisonicsConfig(
          *ambisonics_config, audio_element_with_data.obu.audio_substream_ids_,
          audio_element_with_data.substream_id_to_labels, kStereoLayout,
          num_ticks);
  ABSL_CHECK_NE(renderer, nullptr);

  // Loop rendering all frames, we are carful to avoid any extra work
  // (allocations) in the benchmark loop.
  std::vector<std::vector<InternalSampleType>> flush_buffer;
  for (auto _ : state) {
    auto status = renderer->RenderLabeledFrame(frame);
    ABSL_CHECK_EQ(*status, num_ticks);
    renderer->Flush(flush_buffer);
  }
}

template <int Order, AmbisonicsMode Mode>
static void BM_RenderFullOrderToStereo(benchmark::State& state) {
  const int num_ticks = state.range(0);

  const auto audio_element = CreateFullOrderAudioElement<Order, Mode>();
  ABSL_CHECK_OK(audio_element);

  RunRenderAmbisonicsBenchmark(*audio_element, num_ticks, state);
}

template <AmbisonicsMode Mode>
static void BM_RenderHorizontalToaToStereo(benchmark::State& state) {
  const int num_ticks = state.range(0);

  const auto audio_element = CreateHorizontalToaAudioElement<Mode>();
  ABSL_CHECK_OK(audio_element);

  RunRenderAmbisonicsBenchmark(*audio_element, num_ticks, state);
}

// Test orders (1, 3, 4) x (mono, projection) x (1024, 2048 ticks).
BENCHMARK_TEMPLATE(BM_RenderFullOrderToStereo, /*Order=*/1,
                   AmbisonicsConfig::kAmbisonicsModeMono)
    ->Arg(1024)
    ->Arg(2048);
BENCHMARK_TEMPLATE(BM_RenderFullOrderToStereo, /*Order=*/1,
                   AmbisonicsConfig::kAmbisonicsModeProjection)
    ->Arg(1024)
    ->Arg(2048);

BENCHMARK_TEMPLATE(BM_RenderFullOrderToStereo, /*Order=*/3,
                   AmbisonicsConfig::kAmbisonicsModeMono)
    ->Arg(1024)
    ->Arg(2048);
BENCHMARK_TEMPLATE(BM_RenderFullOrderToStereo, /*Order=*/3,
                   AmbisonicsConfig::kAmbisonicsModeProjection)
    ->Arg(1024)
    ->Arg(2048);

BENCHMARK_TEMPLATE(BM_RenderFullOrderToStereo, /*Order=*/4,
                   AmbisonicsConfig::kAmbisonicsModeMono)
    ->Arg(1024)
    ->Arg(2048);
BENCHMARK_TEMPLATE(BM_RenderFullOrderToStereo, /*Order=*/4,
                   AmbisonicsConfig::kAmbisonicsModeProjection)
    ->Arg(1024)
    ->Arg(2048);

// Test horizontal-only TOA x (mono, projection) x (1024, 2048 ticks).
BENCHMARK_TEMPLATE(BM_RenderHorizontalToaToStereo,
                   AmbisonicsConfig::kAmbisonicsModeMono)
    ->Arg(1024)
    ->Arg(2048);
BENCHMARK_TEMPLATE(BM_RenderHorizontalToaToStereo,
                   AmbisonicsConfig::kAmbisonicsModeProjection)
    ->Arg(1024)
    ->Arg(2048);

}  // namespace
}  // namespace iamf_tools
