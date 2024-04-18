/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
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
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/common/macros.h"

namespace iamf_tools {

namespace renderer_utils {

namespace {

// Returns the common number of time ticks to be rendered for the requested
// labels or associated demixed label in `labeled_frame`. This represents the
// number of time ticks in the rendered audio after trimming.
absl::StatusOr<int> GetCommonNumTrimmedTimeTicks(
    const LabeledFrame& labeled_frame,
    const std::vector<std::string>& ordered_labels) {
  const int kUnknownNumTimeTicks = -1;
  int num_raw_time_ticks = kUnknownNumTimeTicks;
  for (const auto& label : ordered_labels) {
    if (label.empty()) {
      continue;
    }

    const std::vector<int32_t>* samples_to_render = nullptr;
    RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
        label, labeled_frame.label_to_samples, &samples_to_render));

    if (samples_to_render == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrCat("Label ", label, " or D_", label, " not found."));
    } else if (num_raw_time_ticks == kUnknownNumTimeTicks) {
      num_raw_time_ticks = samples_to_render->size();
    } else if (num_raw_time_ticks != samples_to_render->size()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "All labels must have the same number of samples ", label, " (",
          samples_to_render->size(), " vs. ", num_raw_time_ticks, ")"));
    }
  }

  const int num_trimmed_time_ticks = num_raw_time_ticks -
                                     labeled_frame.samples_to_trim_at_start -
                                     labeled_frame.samples_to_trim_at_end;
  if (num_trimmed_time_ticks < 0) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Not enough samples to render ", num_trimmed_time_ticks,
        " samples. Raw samples: ", num_raw_time_ticks,
        ", samples to trim at start: ", labeled_frame.samples_to_trim_at_start,
        ", samples to trim at end: ", labeled_frame.samples_to_trim_at_end));
  }
  return num_trimmed_time_ticks;
}

}  // namespace

absl::Status ArrangeSamplesToRender(
    const LabeledFrame& labeled_frame,
    const std::vector<std::string>& ordered_labels,
    std::vector<std::vector<int32_t>>& samples_to_render) {
  samples_to_render.clear();
  if (ordered_labels.empty()) {
    return absl::OkStatus();
  }

  const auto num_trimmed_time_ticks =
      GetCommonNumTrimmedTimeTicks(labeled_frame, ordered_labels);
  if (!num_trimmed_time_ticks.ok()) {
    return num_trimmed_time_ticks.status();
  }

  const auto num_channels = ordered_labels.size();
  samples_to_render.resize(*num_trimmed_time_ticks,
                           std::vector<int32_t>(num_channels, 0));

  for (int channel = 0; channel < num_channels; ++channel) {
    const auto& channel_label = ordered_labels[channel];
    if (channel_label.empty()) {
      // Missing channels for mixed-order ambisonics representation will not be
      // updated and will thus have the initialized zeros.
      continue;
    }

    const std::vector<int32_t>* channel_samples = nullptr;
    RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
        channel_label, labeled_frame.label_to_samples, &channel_samples));
    // The lookup should not fail because its presence was already checked in
    // `GetCommonNumTrimmedTimeTicks`.
    CHECK_NE(channel_samples, nullptr);

    // Grab the entire time axes for this label, Skip over any samples that
    // should be trimmed.
    for (int time = 0; time < *num_trimmed_time_ticks; ++time) {
      samples_to_render[time][channel] =
          (*channel_samples)[time + labeled_frame.samples_to_trim_at_start];
    }
  }

  return absl::OkStatus();
}
}  // namespace renderer_utils

}  // namespace iamf_tools
