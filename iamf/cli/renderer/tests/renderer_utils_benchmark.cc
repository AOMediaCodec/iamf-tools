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

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace renderer_utils {
namespace {

using enum ChannelLabel::Label;

static std::vector<ChannelLabel::Label> CreateLabels(int num_channels) {
  // This list includes all non-demixed labels.
  static const std::vector<ChannelLabel::Label> kLabelsToPick = {
      kOmitted,
      // Mono channels.
      kMono,
      // Stereo or binaural channels.
      kL2,
      kR2,
      // Centre channel common to several layouts (e.g. 3.1.2, 5.x.y, 7.x.y).
      kCentre,
      // LFE channel common to several layouts
      // (e.g. 3.1.2, 5.1.y, 7.1.y, 9.1.6).
      kLFE,
      // 3.1.2 surround channels.
      kL3,
      kR3,
      kLtf3,
      kRtf3,
      // 5.x.y surround channels.
      kL5,
      kR5,
      kLs5,
      kRs5,
      // Common channels between 5.1.2 and 7.1.2.
      kLtf2,
      kRtf2,
      // Common channels between 5.1.4 and 7.1.4.
      kLtf4,
      kRtf4,
      kLtb4,
      kRtb4,
      // 7.x.y surround channels.
      kL7,
      kR7,
      kLss7,
      kRss7,
      kLrs7,
      kRrs7,
      // 9.1.6 surround channels.
      kFLc,
      kFC,
      kFRc,
      kFL,
      kFR,
      kSiL,
      kSiR,
      kBL,
      kBR,
      kTpFL,
      kTpFR,
      kTpSiL,
      kTpSiR,
      kTpBL,
      kTpBR,
      // Ambisonics channels.
      kA0,
      kA1,
      kA2,
      kA3,
      kA4,
      kA5,
      kA6,
      kA7,
      kA8,
      kA9,
      kA10,
      kA11,
      kA12,
      kA13,
      kA14,
      kA15,
      kA16,
      kA17,
      kA18,
      kA19,
      kA20,
      kA21,
      kA22,
      kA23,
      kA24,
  };

  // We cannot pick more labels than available ones.
  CHECK_LE(num_channels, kLabelsToPick.size());

  // Randomly pick `num_channels` from the list.
  auto shuffled_labels = kLabelsToPick;
  absl::BitGen gen;
  for (int i = 0; i < num_channels; ++i) {
    int j = absl::Uniform<int>(gen, i, num_channels);
    std::swap(shuffled_labels[i], shuffled_labels[j]);
  }
  shuffled_labels.resize(num_channels);
  return shuffled_labels;
}

static void BM_ArrangeSamplesToRender(benchmark::State& state) {
  const int num_channels = state.range(0);
  const int num_ticks = state.range(1);

  // Create input ordered labels.
  const std::vector<ChannelLabel::Label> ordered_labels =
      CreateLabels(num_channels);

  // Create input labeled frames.
  LabeledFrame labeled_frame;
  for (const auto label : ordered_labels) {
    labeled_frame.samples_to_trim_at_start = 0;
    labeled_frame.samples_to_trim_at_end = 0;
    labeled_frame.label_to_samples[label] =
        std::vector<InternalSampleType>(num_ticks);
  }

  // Create an input empty channel.
  const std::vector<InternalSampleType> kEmptyChannel(num_ticks, 0.0);

  // Placeholder for outputs.
  std::vector<absl::Span<const InternalSampleType>> samples_to_render(
      num_channels);
  size_t num_valid_samples = 0;

  // Measure the calls to `ArrangeSamplesToRender()`.
  for (auto _ : state) {
    auto status =
        ArrangeSamplesToRender(labeled_frame, ordered_labels, kEmptyChannel,
                               samples_to_render, num_valid_samples);
    CHECK_OK(status);
  }
}

// Benchmark various combinations of (#channels, #ticks).
BENCHMARK(BM_ArrangeSamplesToRender)
    ->Args({2, 1 << 4})
    ->Args({2, 1 << 8})
    ->Args({2, 1 << 12})
    ->Args({8, 1 << 4})
    ->Args({8, 1 << 8})
    ->Args({8, 1 << 12})
    ->Args({32, 1 << 4})
    ->Args({32, 1 << 8})
    ->Args({32, 1 << 12});

}  // namespace
}  // namespace renderer_utils
}  // namespace iamf_tools
