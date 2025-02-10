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

#include <cmath>
#include <cstdint>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

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
    InternalTimestamp start_time, InternalTimestamp end_time,
    InternalTimestamp target_time, float& target_mix_gain_db) {
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
