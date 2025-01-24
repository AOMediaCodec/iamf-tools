/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_UTILS_OBU_UTIL_H_
#define COMMON_UTILS_OBU_UTIL_H_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/common/utils/numeric_utils.h"

namespace iamf_tools {

/*!\brief Writes the input PCM sample to a buffer.
 *
 * Writes the most significant `sample_size` bits of `sample` starting at
 * `buffer[write_position]`. It is up to the user to ensure the buffer is valid.
 *
 * \param sample Sample to write the upper `sample_size` bits of.
 * \param sample_size Sample size in bits. MUST be one of {8, 16, 24, 32}.
 * \param big_endian `true` to write the sample as big endian. `false` to write
 *        it as little endian.
 * \param buffer Start of the buffer to write to.
 * \param write_position Offset of the buffer to write to. Incremented to one
 *        after the last byte written on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 *         `sample_size` is invalid.
 */
absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* buffer,
                            int& write_position);

/*!\brief Arranges the input samples by time and channel.
 *
 * \param samples Interleaved samples to arrange.
 * \param num_channels Number of channels.
 * \param transform_samples Function to transform each sample to the output
 *        type.
 * \param output Output vector to write the samples to. The size is not
 *        modified in this function even if the number of input samples do
 *        not fill the entire output vector. In that case, only the first
 *        `num_ticks` are filled.
 * \param num_ticks Number of ticks (time samples) of the output vector that
 *        are filled in this function.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *         number of samples is not a multiple of the number of channels. An
 *         error propagated from `transform_samples` if it fails.
 */
template <typename InputType, typename OutputType>
absl::Status ConvertInterleavedToTimeChannel(
    absl::Span<const InputType> samples, size_t num_channels,
    const absl::AnyInvocable<absl::Status(InputType, OutputType&) const>&
        transform_samples,
    std::vector<std::vector<OutputType>>& output, size_t& num_ticks) {
  if (samples.size() % num_channels != 0) [[unlikely]] {
    return absl::InvalidArgumentError(absl::StrCat(
        "Number of samples must be a multiple of the number of "
        "channels. Found ",
        samples.size(), " samples and ", num_channels, " channels."));
  }

  num_ticks = samples.size() / num_channels;
  if (num_ticks > output.size()) [[unlikely]] {
    return absl::InvalidArgumentError(absl::StrCat(
        "Number of ticks does not fit into the output vector: (num_ticks= ",
        num_ticks, " > output.size()= ", output.size(), ")"));
  }

  for (int t = 0; t < num_ticks; ++t) {
    if (output[t].size() != num_channels) [[unlikely]] {
      return absl::InvalidArgumentError(absl::StrCat(
          "Number of channels is not equal to the output vector at tick ", t,
          ": (", num_channels, " != ", output[t].size(), ")"));
    }
    for (int c = 0; c < num_channels; ++c) {
      const auto status =
          transform_samples(samples[t * num_channels + c], output[t][c]);
      if (!status.ok()) [[unlikely]] {
        return status;
      }
    }
  }
  return absl::OkStatus();
}

/*!\brief Interleaves the input samples.
 *
 * \param samples Samples in (time, channel) axes to arrange.
 * \param transform_samples Function to transform each sample to the output
 *        type.
 * \param output Output vector to write the interleaved samples to.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *         input has an inconsistent number of channels. An error propagated
 *         from `transform_samples` if it fails.
 */
template <typename InputType, typename OutputType>
absl::Status ConvertTimeChannelToInterleaved(
    absl::Span<const std::vector<InputType>> input,
    const absl::AnyInvocable<absl::Status(InputType, OutputType&) const>&
        transform_samples,
    std::vector<OutputType>& output) {
  const size_t num_channels = input.empty() ? 0 : input[0].size();
  if (!std::all_of(input.begin(), input.end(), [&](const auto& tick) {
        return tick.size() == num_channels;
      })) {
    return absl::InvalidArgumentError(
        "All ticks must have the same number of channels.");
  }

  // TODO(b/382197581): avoid resizing inside this function.
  output.clear();
  output.reserve(input.size() * num_channels);
  for (const auto& tick : input) {
    for (const auto& sample : tick) {
      OutputType transformed_sample;
      auto status = transform_samples(sample, transformed_sample);
      if (!status.ok()) {
        return status;
      }
      output.emplace_back(transformed_sample);
    }
  }
  return absl::OkStatus();
}

/*!\brief Gets the duration of a parameter subblock.
 *
 * The Spec defines a complex logic of getting the final subblock duration from
 * multiple potential sources, including:
 *   - The constant subblock duration recorded in the parameter block.
 *   - The duration recorded in the parameter block's subblock at index i.
 *   - The constant subblock duration recorded in the parameter definition.
 *   - The subblock duration at index i recorded in the parameter definition.
 *
 * \param subblock_index Index of the subblock to get the duration of.
 * \param num_subblocks Number of subblocks.
 * \param constant_subblock_duration Constant subblock duration.
 * \param subblock_duration_getter_from_parameter_block Getter function
 *        that returns the subblock duration recorded inside a parameter block,
 *        indexed at `subblock_index`.
 * \param subblock_duration_getter_from_parameter_definition Getter function
 *        that returns the subblock duration recorded inside a parameter
 *        definition, indexed at `subblock_index`.
 * \return Duration of the subblock or `absl::InvalidArgumentError()` on
 *         failure.
 */
template <typename T>
absl::StatusOr<T> GetParameterSubblockDuration(
    int subblock_index, T num_subblocks, T constant_subblock_duration,
    T total_duration, uint8_t param_definition_mode,
    absl::AnyInvocable<absl::StatusOr<T>(int)>
        subblock_duration_getter_from_parameter_block,
    absl::AnyInvocable<absl::StatusOr<T>(int)>
        subblock_duration_getter_from_parameter_definition) {
  if (subblock_index > num_subblocks) {
    return absl::InvalidArgumentError("subblock_index > num_subblocks");
  }

  if (constant_subblock_duration == 0) {
    if (param_definition_mode == 1) {
      // The durations are explicitly specified in the parameter block.
      return subblock_duration_getter_from_parameter_block(subblock_index);
    } else {
      // The durations are explicitly specified in the parameter definition.
      return subblock_duration_getter_from_parameter_definition(subblock_index);
    }
  }

  // Otherwise the duration is implicit.
  if (subblock_index == num_subblocks - 1 &&
      num_subblocks * constant_subblock_duration > total_duration) {
    // Sometimes the last subblock duration is shorter. The spec describes how
    // to calculate the special case: "If NS x CSD > D, the actual duration of
    // the last subblock SHALL be D - (NS - 1) x CSD."
    return (total_duration - (num_subblocks - 1) * constant_subblock_duration);
  } else {
    // Otherwise the duration is based on `constant_subblock_duration`.
    return constant_subblock_duration;
  }
}

/*!\brief Interpolates a mix gain value in dB.
 *
 * The logic is used to partition parameter block protocol buffers as well as
 * to query the gain value at a specific timestamp during mixing.
 *
 * \param animation_type Type of animation applied to the mix gain values.
 * \param step_enum Enum value representing a step animation.
 * \param linear_enum Enum value representing a linear animation.
 * \param bezier_enum Enum value representing a Bezier animation.
 * \param step_start_point_getter Getter function of the start point value
 *        of a step animation.
 * \param linear_start_point_getter Getter function of the start point value
 *        of a linear animation.
 * \param linear_end_point_getter Getter function of the end point value
 *        of a linear animation.
 * \param bezier_start_point_getter Getter function of the start point value
 *        of a Bezier animation.
 * \param bezier_end_point_getter Getter function of the end point value
 *        of a Bezier animation.
 * \param bezier_control_point_getter Getter function of the middle control
 *        point value of a Bezier animation.
 * \param bezier_control_point_relative_time_getter Getter function of the
 *        time of the middle control point of a Bezier animation.
 * \param start_time Start time of the `MixGainParameterData`.
 * \param end_time End time of the `MixGainParameterData`.
 * \param target_time Target time to the get interpolated value of.
 * \param target_mix_gain_db Output inteprolated mix gain value in dB.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
template <typename AnimationEnumType>
absl::Status InterpolateMixGainValue(
    AnimationEnumType animation_type, AnimationEnumType step_enum,
    AnimationEnumType linear_enum, AnimationEnumType bezier_enum,
    absl::AnyInvocable<int16_t()> step_start_point_getter,
    absl::AnyInvocable<int16_t()> linear_start_point_getter,
    absl::AnyInvocable<int16_t()> linear_end_point_getter,
    absl::AnyInvocable<int16_t()> bezier_start_point_getter,
    absl::AnyInvocable<int16_t()> bezier_end_point_getter,
    absl::AnyInvocable<int16_t()> bezier_control_point_getter,
    absl::AnyInvocable<int16_t()> bezier_control_point_relative_time_getter,
    int32_t start_time, int32_t end_time, int32_t target_time,
    float& target_mix_gain_db) {
  if (target_time < start_time || target_time > end_time ||
      start_time > end_time) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Cannot interpolate mix gain value with start time = ", start_time,
        ", target_time = ", target_time, " and end_time = ", end_time));
  }

  // Shift times so start_time=0 to simplify calculations.
  end_time -= start_time;
  target_time -= start_time;
  start_time = 0;

  // TODO(b/283281856): Support resampling parameter blocks.
  const int sample_rate_ratio = 1;
  const int n_0 = start_time * sample_rate_ratio;
  const int n = target_time * sample_rate_ratio;
  const int n_2 = end_time * sample_rate_ratio;

  if (animation_type == step_enum) {
    // No interpolation is needed for step.
    target_mix_gain_db = Q7_8ToFloat(step_start_point_getter());
  } else if (animation_type == linear_enum) {
    // Interpolate using the exact formula from the spec.
    const float a = (float)n / (float)n_2;
    const float p_0 = Q7_8ToFloat(linear_start_point_getter());
    const float p_2 = Q7_8ToFloat(linear_end_point_getter());
    target_mix_gain_db = (1 - a) * p_0 + a * p_2;
  } else if (animation_type == bezier_enum) {
    const float control_point_float =
        Q0_8ToFloat(bezier_control_point_relative_time_getter());
    // Using the definition of `round` in the IAMF spec.
    const int n_1 = std::floor((end_time * control_point_float) + 0.5);

    const float p_0 = Q7_8ToFloat(bezier_start_point_getter());
    const float p_1 = Q7_8ToFloat(bezier_control_point_getter());
    const float p_2 = Q7_8ToFloat(bezier_end_point_getter());

    const float alpha = n_0 - 2 * n_1 + n_2;
    const float beta = 2 * (n_1 - n_0);
    const float gamma = n_0 - n;
    const float a = alpha == 0
                        ? -gamma / beta
                        : (-beta + std::sqrt(beta * beta - 4 * alpha * gamma)) /
                              (2 * alpha);
    target_mix_gain_db =
        (1 - a) * (1 - a) * p_0 + 2 * (1 - a) * a * p_1 + a * a * p_2;
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown animation_type = ", animation_type));
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools

#endif  // COMMON_UTILS_OBU_UTIL_H_
