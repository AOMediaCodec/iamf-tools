/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/renderer/loudspeakers_renderer.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/renderer/precomputed_gains.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// TODO(b/382197581): Reduce the number of allocations, this function allocates
//                    a new vector on every call. Callsites never cache it and
//                    will call every frame.
std::optional<std::vector<std::vector<double>>> ComputeGains(
    absl::string_view input_layout_string,
    absl::string_view output_layout_string,
    const DownMixingParams& down_mixing_params) {
  const auto alpha = down_mixing_params.alpha;
  const auto beta = down_mixing_params.beta;
  const auto gamma = down_mixing_params.gamma;
  const auto delta = down_mixing_params.delta;
  const auto w = down_mixing_params.w;
  // TODO(b/292174366): Strictly follow IAMF spec logic of when to use demixers
  //                    vs. libear renderer.
  ABSL_LOG_FIRST_N(INFO, 5)
      << "Rendering  may be buggy or not follow the spec "
         "recommendations. Computing gains based on demixing params: "
      << input_layout_string << " --> " << output_layout_string;
  if (input_layout_string == "4+7+0" && output_layout_string == "3.1.2") {
    // Values checked; fixed.
    return std::vector<std::vector<double>>{
        {1, 0, 0, 0, 0, 0},
        {0, 1, 0, 0, 0, 0},
        {0, 0, 1, 0, 0, 0},
        {0, 0, 0, 1, 0, 0},
        // Lss7
        {alpha * delta, 0, 0, 0, alpha * w * delta, 0},
        // Rss7
        {0, alpha * delta, 0, 0, 0, alpha * w * delta},
        {beta * delta, 0, 0, 0, beta * w * delta, 0},
        {0, beta * delta, 0, 0, 0, beta * w * delta},
        {0, 0, 0, 0, 1, 0},
        {0, 0, 0, 0, 0, 1},
        {0, 0, 0, 0, gamma, 0},
        {0, 0, 0, 0, 0, gamma}};
  } else if (input_layout_string == "4+7+0" &&
             output_layout_string == "7.1.2") {
    // clang-format off
    return std::vector<std::vector<double>>{
    /*       L, R, C, LFE, Lss, Rss, Lrs, Rrs,   Ltf,   Rtf */
    /*  L*/ {1, 0, 0,   0,   0,   0,   0,   0,     0,     0},
    /*  R*/ {0, 1, 0,   0,   0,   0,   0,   0,     0,     0},
    /*  C*/ {0, 0, 1,   0,   0,   0,   0,   0,     0,     0},
    /*LFE*/ {0, 0, 0,   1,   0,   0,   0,   0,     0,     0},
    /*Lss*/ {0, 0, 0,   0,   1,   0,   0,   0,     0,     0},
    /*Rss*/ {0, 0, 0,   0,   0,   1,   0,   0,     0,     0},
    /*Lrs*/ {0, 0, 0,   0,   0,   0,   1,   0,     0,     0},
    /*Rrs*/ {0, 0, 0,   0,   0,   0,   0,   1,     0,     0},
    /*Ltf*/ {0, 0, 0,   0,   0,   0,   0,   0,     1,     0},
    /*Rtf*/ {0, 0, 0,   0,   0,   0,   0,   0,     0,     1},
    /*Ltb*/ {0, 0, 0,   0,   0,   0,   0,   0, gamma,     0},
    /*Rtb*/ {0, 0, 0,   0,   0,   0,   0,   0,     0, gamma},
    };
    // clang-format on
  } else {
    // Ok, gain matrices are not defined or implemented. But it is reasonable
    // for the caller to try to use the precomputed gains.
    ABSL_LOG(WARNING) << "The encoder did not implement matrices for "
                      << input_layout_string << " to " << output_layout_string
                      << " yet.";

    return std::nullopt;
  }
}

bool LayoutStringHasHeightChannels(absl::string_view layout_string) {
  // TODO(b/292174366): Fill in all possible layouts or determine this in a
  //                    better way.
  if (layout_string == "4+7+0" || layout_string == "7.1.2" ||
      layout_string == "4+5+0" || layout_string == "2+5+0" ||
      layout_string == "3.1.2") {
    return true;
  } else if (layout_string == "0+7+0" || layout_string == "0+5+0" ||
             layout_string == "0+2+0" || layout_string == "0+1+0") {
    return false;

  } else {
    ABSL_LOG(WARNING) << "Unknown if " << layout_string
                      << " has height channels.";
    return false;
  }
}

double Q15ToSignedDouble(const int16_t input) {
  return static_cast<double>(input) / 32768.0;
}

void ProjectSamplesToRender(
    absl::Span<const absl::Span<const InternalSampleType>> input_samples,
    const int16_t* demixing_matrix, const int num_output_channels,
    std::vector<std::vector<InternalSampleType>>& projected_samples) {
  ABSL_CHECK_NE(demixing_matrix, nullptr);
  const auto num_in_channels = input_samples.size();
  const auto num_ticks = input_samples.empty() ? 0 : input_samples[0].size();
  projected_samples.resize(num_output_channels);
  for (int out_channel = 0; out_channel < num_output_channels; out_channel++) {
    auto& projected_samples_for_channel = projected_samples[out_channel];
    projected_samples_for_channel.assign(num_ticks, 0.0);
    for (int in_channel = 0; in_channel < num_in_channels; in_channel++) {
      const auto& input_sample_for_channel = input_samples[in_channel];
      const auto demixing_value = Q15ToSignedDouble(
          demixing_matrix[in_channel * num_in_channels + out_channel]);
      for (int t = 0; t < num_ticks; t++) {
        // Project with `demixing_matrix`, which is encoded as Q15 and stored
        // in column major.
        projected_samples_for_channel[t] +=
            demixing_value * input_sample_for_channel[t];
      }
    }
  }
}

void RenderSamplesUsingGains(
    absl::Span<const absl::Span<const InternalSampleType>> input_samples,
    const std::vector<std::vector<double>>& gains,
    const int16_t* demixing_matrix,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  // Project with `demixing_matrix` when in projection mode.
  absl::Span<const absl::Span<const InternalSampleType>> samples_to_render;

  // TODO(b/382197581): Avoid re-allocating vectors in each function call.
  std::vector<std::vector<InternalSampleType>> projected_samples;
  std::vector<absl::Span<const InternalSampleType>> projected_spans;
  if (demixing_matrix != nullptr) {
    ProjectSamplesToRender(input_samples, demixing_matrix, gains.size(),
                           projected_samples);
    projected_spans.resize(projected_samples.size());
    for (int c = 0; c < projected_samples.size(); c++) {
      projected_spans[c] = absl::MakeConstSpan(projected_samples[c]);
    }
    samples_to_render = absl::MakeConstSpan(projected_spans);
  } else {
    samples_to_render = input_samples;
  }

  const auto num_ticks = input_samples.empty() ? 0 : input_samples[0].size();
  const auto num_in_channels = input_samples.size();
  const auto num_out_channels = gains[0].size();
  rendered_samples.resize(num_out_channels);
  for (int out_channel = 0; out_channel < num_out_channels; out_channel++) {
    auto& rendered_samples_for_channel = rendered_samples[out_channel];
    rendered_samples_for_channel.assign(num_ticks, 0.0);
    for (int in_channel = 0; in_channel < num_in_channels; in_channel++) {
      const auto& samples_to_render_for_channel = samples_to_render[in_channel];
      const auto gain_value = gains[in_channel][out_channel];
      for (int t = 0; t < num_ticks; t++) {
        rendered_samples_for_channel[t] +=
            samples_to_render_for_channel[t] * gain_value;
      }
    }
  }
}

}  // namespace

// TODO(b/382197581): Avoid returning newly constructed vectors. Store the
// results in a pre-allocated data structure.
absl::StatusOr<std::vector<std::vector<double>>> LookupPrecomputedGains(
    absl::string_view input_key, absl::string_view output_key) {
  static const absl::NoDestructor<PrecomputedGains> precomputed_gains(
      InitPrecomputedGains());

  const std::string input_key_debug_message =
      absl::StrCat("Precomputed gains not found for input_key= ", input_key);
  // Search throughs two layers of maps. We want to find the gains associated
  // with `[input_key][output_key]`.
  auto input_key_it = precomputed_gains->find(input_key);
  if (input_key_it == precomputed_gains->end()) [[unlikely]] {
    return absl::NotFoundError(input_key_debug_message);
  }

  return LookupInMap(input_key_it->second, std::string(output_key),
                     absl::StrCat(input_key_debug_message, " and output_key"));
}

std::optional<std::vector<std::vector<double>>> MaybeComputeDynamicGains(
    const DownMixingParams& down_mixing_params,
    absl::string_view input_layout_string,
    absl::string_view output_layout_string) {
  if (!down_mixing_params.in_bitstream) {
    // There are no dynamic gains in the bitstream, use the precomputed gains.
    return std::nullopt;
  }

  // TODO(b/292174366): Remove hacks. Updates logic of when to use demixers vs
  //                    libear renderer.
  const bool input_layout_has_height_channels =
      LayoutStringHasHeightChannels(input_layout_string);
  const bool playback_has_height_channels =
      LayoutStringHasHeightChannels(output_layout_string);
  if (!playback_has_height_channels && input_layout_has_height_channels) {
    // The spec says to use the precomputed gains.
    return std::nullopt;
  }

  // The bitstream tells us how to compute the gains. Use those.
  return ComputeGains(input_layout_string, output_layout_string,
                      down_mixing_params);
}

absl::Status RenderChannelLayoutToLoudspeakers(
    absl::Span<const absl::Span<const InternalSampleType>> input_samples,
    const std::vector<std::vector<double>>& gains,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  RenderSamplesUsingGains(input_samples, gains,
                          /*demixing_matrix=*/nullptr, rendered_samples);
  return absl::OkStatus();
}

absl::Status RenderAmbisonicsToLoudspeakers(
    absl::Span<const absl::Span<const InternalSampleType>> input_samples,
    const AmbisonicsConfig& ambisonics_config,
    const std::vector<std::vector<double>>& gains,
    std::vector<std::vector<InternalSampleType>>& rendered_samples) {
  // Exclude unsupported mode first, and deal with only mono or projection
  // in the rest of the code.
  const auto mode = ambisonics_config.ambisonics_mode;
  if (mode != AmbisonicsConfig::kAmbisonicsModeMono &&
      mode != AmbisonicsConfig::kAmbisonicsModeProjection) {
    return absl::UnimplementedError(
        absl::StrCat("Unsupported ambisonics mode. mode= ", mode));
  }
  const bool is_mono = mode == AmbisonicsConfig::kAmbisonicsModeMono;

  // Input key for ambisonics is "A{ambisonics_order}".
  const uint8_t output_channel_count =
      is_mono
          ? std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config)
                .output_channel_count
          : std::get<AmbisonicsProjectionConfig>(
                ambisonics_config.ambisonics_config)
                .output_channel_count;

  RETURN_IF_NOT_OK(
      ValidateContainerSizeEqual("gains", gains, output_channel_count));

  RenderSamplesUsingGains(input_samples, gains,
                          is_mono ? nullptr
                                  : std::get<AmbisonicsProjectionConfig>(
                                        ambisonics_config.ambisonics_config)
                                        .demixing_matrix.data(),
                          rendered_samples);

  return absl::OkStatus();
}

}  // namespace iamf_tools
