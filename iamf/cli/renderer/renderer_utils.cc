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
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace renderer_utils {

namespace {

// Returns the common number of time ticks to be rendered for the requested
// labels or associated demixed label in `labeled_frame`. This represents the
// number of time ticks in the rendered audio after trimming.
absl::StatusOr<size_t> GetCommonNumTrimmedTimeTicks(
    const LabeledFrame& labeled_frame,
    const std::vector<ChannelLabel::Label>& ordered_labels) {
  std::optional<size_t> num_raw_time_ticks;
  for (const auto& label : ordered_labels) {
    if (label == ChannelLabel::kOmitted) {
      continue;
    }

    const std::vector<InternalSampleType>* samples_to_render = nullptr;
    RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
        label, labeled_frame.label_to_samples, &samples_to_render));

    if (samples_to_render == nullptr) {
      return absl::InvalidArgumentError(
          absl::StrCat("Label ", label, " or D_", label, " not found."));
    } else if (!num_raw_time_ticks.has_value()) {
      num_raw_time_ticks = samples_to_render->size();
    } else if (*num_raw_time_ticks != samples_to_render->size()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "All labels must have the same number of samples ", label, " (",
          samples_to_render->size(), " vs. ", *num_raw_time_ticks, ")"));
    }
  }
  if (*num_raw_time_ticks < (labeled_frame.samples_to_trim_at_start +
                             labeled_frame.samples_to_trim_at_end)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Not enough samples to render samples",
        ". #Raw samples: ", *num_raw_time_ticks,
        ", samples to trim at start: ", labeled_frame.samples_to_trim_at_start,
        ", samples to trim at end: ", labeled_frame.samples_to_trim_at_end));
  }

  return *num_raw_time_ticks - labeled_frame.samples_to_trim_at_start -
         labeled_frame.samples_to_trim_at_end;
}

}  // namespace

absl::Status ArrangeSamplesToRender(
    const LabeledFrame& labeled_frame,
    const std::vector<ChannelLabel::Label>& ordered_labels,
    std::vector<std::vector<InternalSampleType>>& samples_to_render,
    size_t& num_valid_samples) {
  if (ordered_labels.empty()) {
    return absl::OkStatus();
  }

  const auto common_num_trimmed_time_ticks =
      GetCommonNumTrimmedTimeTicks(labeled_frame, ordered_labels);
  if (!common_num_trimmed_time_ticks.ok()) {
    return common_num_trimmed_time_ticks.status();
  }
  num_valid_samples = *common_num_trimmed_time_ticks;

  const auto num_channels = ordered_labels.size();
  if (num_valid_samples > samples_to_render.size()) [[unlikely]] {
    return absl::InvalidArgumentError(absl::StrCat(
        "Number of time samples to render= ", num_valid_samples,
        "does not fit into the output of size ", samples_to_render.size()));
  }

  for (int channel = 0; channel < num_channels; ++channel) {
    const auto& channel_label = ordered_labels[channel];
    if (channel_label == ChannelLabel::kOmitted) {
      // Missing channels for mixed-order ambisonics representation will not be
      // updated and will thus have the initialized zeros.
      continue;
    }

    const std::vector<InternalSampleType>* channel_samples = nullptr;
    RETURN_IF_NOT_OK(DemixingModule::FindSamplesOrDemixedSamples(
        channel_label, labeled_frame.label_to_samples, &channel_samples));
    // The lookup should not fail because its presence was already checked in
    // `GetCommonNumTrimmedTimeTicks`.
    CHECK_NE(channel_samples, nullptr);

    // Grab the entire time axes for this label, Skip over any samples that
    // should be trimmed.
    for (int time = 0; time < num_valid_samples; ++time) {
      samples_to_render[time][channel] =
          (*channel_samples)[time + labeled_frame.samples_to_trim_at_start];
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> LookupOutputKeyFromPlaybackLayout(
    const Layout& output_layout) {
  switch (output_layout.layout_type) {
    using enum Layout::LayoutType;
    case kLayoutTypeLoudspeakersSsConvention: {
      auto sound_system = std::get<LoudspeakersSsConventionLayout>(
                              output_layout.specific_layout)
                              .sound_system;
      using enum LoudspeakersSsConventionLayout::SoundSystem;
      static const absl::NoDestructor<absl::flat_hash_map<
          LoudspeakersSsConventionLayout::SoundSystem, std::string>>
          kSoundSystemToOutputKey({
              {kSoundSystemA_0_2_0, "0+2+0"},
              {kSoundSystemB_0_5_0, "0+5+0"},
              {kSoundSystemC_2_5_0, "2+5+0"},
              {kSoundSystemD_4_5_0, "4+5+0"},
              {kSoundSystemE_4_5_1, "4+5+1"},
              {kSoundSystemF_3_7_0, "3+7+0"},
              {kSoundSystemG_4_9_0, "4+9+0"},
              {kSoundSystemH_9_10_3, "9+10+3"},
              {kSoundSystemI_0_7_0, "0+7+0"},
              {kSoundSystemJ_4_7_0, "4+7+0"},
              {kSoundSystem10_2_7_0, "7.1.2"},
              {kSoundSystem11_2_3_0, "3.1.2"},
              {kSoundSystem12_0_1_0, "0+1+0"},
              {kSoundSystem13_6_9_0, "9.1.6"},
          });

      return LookupInMap(*kSoundSystemToOutputKey, sound_system,
                         "Output key for `SoundSystem`");
    }

    case kLayoutTypeBinaural:
      return absl::UnimplementedError(
          "Loudness layout key for BINAURAL not supported "
          "yet.");
    case kLayoutTypeReserved0:
    case kLayoutTypeReserved1:
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unsupported layout_type= ", output_layout.layout_type));
  }
  return absl::OkStatus();
}

}  // namespace renderer_utils

}  // namespace iamf_tools
