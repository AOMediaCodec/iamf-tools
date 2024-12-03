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
#ifndef COMMON_OBU_UTIL_H_
#define COMMON_OBU_UTIL_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/common/macros.h"

namespace iamf_tools {

/*!\brief Sums the input values and checks for overflow.
 *
 * \param x_1 First summand.
 * \param x_2 Second summand.
 * \param result Sum of the inputs on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` when
 *         the sum would cause an overflow in a `uint32_t`.
 */
absl::Status AddUint32CheckOverflow(uint32_t x_1, uint32_t x_2,
                                    uint32_t& result);

/*!\brief Converts float input to Q7.8 format.
 *
 * \param value Value to convert.
 * \param result Converted value if successful. The result is floored to the
 *        nearest Q7.8 value.
 * \return `absl::OkStatus()` if successful. `absl::UnknownError()` if the input
 *         is not valid in Q7.8 format.
 */
absl::Status FloatToQ7_8(float value, int16_t& result);

/*!\brief Converts Q7.8 input to float output.
 *
 * \param value Value to convert.
 * \return Converted value.
 */
float Q7_8ToFloat(int16_t value);

// TODO(b/283281856): Consider removing `FloatToQ0_8()` if it is still an unused
//                    function after the encoder supports resampling parameter
//                    blocks.
/*!\brief Converts float input to Q0.8 format.
 *
 * \param value Value to convert.
 * \param result Converted value if successful. The result is floored to the
 *        nearest Q0.8 value.
 * \return `absl::OkStatus()` if successful. `absl::UnknownError()` if the input
 *         is not valid in Q0.8 format.
 */
absl::Status FloatToQ0_8(float value, uint8_t& result);

/*!\brief Converts Q0.8 input to float output.
 *
 * \param value Value to convert.
 * \return Converted value.
 */
float Q0_8ToFloat(uint8_t value);

/*!\brief Typecasts the input value and writes to the output argument if valid.
 *
 * The custom `field_name` is used to create a more descriptive error message.
 * This is inserted surrounded by backticks. When this refers to a user facing
 * field (i.e. related to `UserMetadata` protos) this should refer to the
 * user-facing field name.
 *
 * \param field_name Field name to insert into the error message.
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the input is outside the expected range.
 */
template <typename T, typename U>
absl::Status StaticCastIfInRange(absl::string_view field_name, T input,
                                 U& output) {
  constexpr U kMinOutput = std::numeric_limits<U>::min();
  constexpr U kMaxOutput = std::numeric_limits<U>::max();
  if (input < kMinOutput || kMaxOutput < input) [[unlikely]] {
    std::string message =
        absl::StrCat(field_name, " is outside the expected range of ");
    if constexpr (std::is_same_v<U, char> || std::is_same_v<U, unsigned char>) {
      absl::StrAppend(&message, "[0, 255]");
    } else {
      absl::StrAppend(&message, "[", kMinOutput, ", ", kMaxOutput, "]");
    }
    return absl::InvalidArgumentError(message);
  }
  output = static_cast<U>(input);
  return absl::OkStatus();
}

/*!\brief Creates a 32-bit signed integer from the [1, 4] input `bytes`.
 *
 * \param bytes Bytes to convert.
 * \param output Converted value if successful. The result is left-justified;
 *        the upper `bytes.size()` bytes are set based on the input and the
 *        remaining lower bytes are 0.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the number of bytes is not in the range of [1, 4].
 */
absl::Status LittleEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                      int32_t& output);

/*!\brief Creates a 32-bit signed integer from the [1, 4] input `bytes`.
 *
 * \param bytes Bytes to convert.
 * \param output Converted value if successful. The result is left-justified;
 *        the upper `bytes.size()` bytes are set based on the input and the
 *        remaining lower bytes are 0.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the number of bytes is not in the range of [1, 4].
 */
absl::Status BigEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                   int32_t& output);

/*!\brief Clips and typecasts the input value and writes to the output argument.
 *
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the input is NaN.
 */
absl::Status ClipDoubleToInt32(double input, int32_t& output);

namespace obu_util_internal {

constexpr double kMaxInt32PlusOneAsDouble =
    static_cast<double>(std::numeric_limits<int32_t>::max()) + 1.0;

}  // namespace obu_util_internal

/*!\brief Normalizes the input value to a floating point in the range [-1, +1].
 *
 * Normalizes the input from [std::numeric_limits<int32_t>::min(),
 * std::numeric_limits<int32_t>::max() + 1] to [-1, +1].
 *
 * \param value Value to normalize.
 * \return Normalized value.
 */
template <typename T>
constexpr T Int32ToNormalizedFloatingPoint(int32_t value) {
  using obu_util_internal::kMaxInt32PlusOneAsDouble;
  static_assert(std::is_floating_point_v<T>);

  // Perform calculations in double. The final cast to the output type, e.g.
  // `float` could result in loss of precision. Note that casting `int32_t` to
  // `double` is lossless; every `int32_t` can be exactly represented.
  return static_cast<T>(static_cast<double>(value) / kMaxInt32PlusOneAsDouble);
}

/*!\brief Converts normalized floating point input to an `int32_t`.
 *
 * Transforms the input from the range of [-1, +1] to the range of
 * [std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() +
 * 1].
 *
 * Input is clamped to [-1, +1] before processing. Output is clamped to the
 * full range of an `int32_t`.
 *
 * \param value Normalized floating point value to convert.
 * \param result Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *         the input is any type of NaN or infinity.
 */
template <typename T>
absl::Status NormalizedFloatingPointToInt32(T value, int32_t& result) {
  using obu_util_internal::kMaxInt32PlusOneAsDouble;
  static_assert(std::is_floating_point_v<T>);
  if (std::isnan(value) || std::isinf(value)) {
    return absl::InvalidArgumentError("Input is NaN or infinity.");
  }

  const double clamped_input =
      std::clamp(static_cast<double>(value), -1.0, 1.0);
  // Clip the result to be safe. Although only values near
  // `std::numeric_limits<int32_t>::max() + 1` will be out of range.
  return ClipDoubleToInt32(clamped_input * kMaxInt32PlusOneAsDouble, result);
}

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

/*!\brief Gets the native byte order of the runtime system.
 *
 * \return `true` if the runtime system natively uses big endian, `false`
 *         otherwise.
 */
bool IsNativeBigEndian();

/*!\brief Returns an error if the size arguments are not equivalent.
 *
 * Intended to be used in OBUs to ensure the reported and actual size of
 * containers are equivalent.
 *
 * \param field_name Field name of the container to insert into the error
 *                   message.
 * \param container Container to check the size of.
 * \param reported_size Size reported by associated fields (e.g. "*_size" fields
 *                      in the OBU).
 * \return `absl::OkStatus()` if the size arguments are equivalent.
 *         `absl::InvalidArgumentError()` otherwise.
 */
template <typename Container, typename ReportedSize>
absl::Status ValidateContainerSizeEqual(absl::string_view field_name,
                                        const Container& container,
                                        ReportedSize reported_size) {
  const auto actual_size = container.size();
  if (actual_size == reported_size) [[likely]] {
    return absl::OkStatus();
  }
  return absl::InvalidArgumentError(absl::StrCat(
      "Found inconsistency with `", field_name, ".size()`= ", actual_size,
      ". Expected a value of ", reported_size, "."));
}

/*!\brief Casts and copies the input span to the output span.
 *
 * \param field_name Field name of the vector to insert into the error message.
 * \param vector_size Size of the vector.
 * \param reported_size Size reported by associated fields (e.g. "*_size" fields
 *                      in the OBU).
 * \return `absl::OkStatus()` if the size arguments are equivalent.
 *         `absl::InvalidArgumentError()` otherwise.
 */
template <typename T, typename U>
absl::Status StaticCastSpanIfInRange(absl::string_view field_name,
                                     absl::Span<const T> input_data,
                                     absl::Span<U> output_data) {
  if (const auto status = ValidateContainerSizeEqual(field_name, input_data,
                                                     output_data.size());
      !status.ok()) [[unlikely]] {
    return status;
  }

  for (int i = 0; i < input_data.size(); ++i) {
    const auto status =
        StaticCastIfInRange(field_name, input_data[i], output_data[i]);
    if (!status.ok()) [[unlikely]] {
      return status;
    }
  }
  return absl::OkStatus();
}

/*!\brief Looks up a key in a map and returns a status or value.
 *
 * When lookup fails the error message will contain the `context` string
 * followed by "= $KEY", where $KEY is the stringified `key`.
 *
 * Some mapping have sufficient context in the typenames, for example:
 *   - Input Map: A map of `PersonName` to `Birthday`.
 *   - Typename-based context: "`Birthday` for `PersonName`".
 *   - Output message: "`Birthday` for `PersonName`= John was not found in the
 *                       map.".
 *
 * Some mappings provide insufficient context in the typenames. Or the typenames
 * would be easily confused. Variable names or phrases should be used as
 * context:
 *   - Input Map: A map of `absl::string_view` names to `int` ages.
 *   - Variable-based context: "`age` for `name`".
 *   - Phrase-based context: or "Age for name".
 * Or:
 *   - Input Map: A map of `proto::Type` to `iamf_tools::Type`.
 *   - Phrase-based context: "Internal version of proto `Type`".
 *
 * \param map Map to search.
 * \param key Key to search for.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return Associated value if lookup is successful. `absl::NotFoundError()`
 *         when lookup fails.
 */
template <typename T, typename U>
absl::StatusOr<U> LookupInMap(const absl::flat_hash_map<T, U>& map,
                              const T& key, absl::string_view context) {
  auto iter = map.find(key);
  if (iter != map.end()) [[likely]] {
    return iter->second;
  }

  return absl::NotFoundError(absl::StrCat(
      context, "= ", key, " was not found in the map.",
      map.empty() ? " The map is empty. Did initialization fail?" : ""));
}

/*!\brief Looks up a key in a map and copies the value to the output argument.
 *
 * \param map Map to search.
 * \param key Key to search for.
 * \param context Context to insert into the error message for debugging
 *        purposes. Forwared to `LookupInMap` which has detailed documentation
 *        on usage.
 * \param value Output argument to write the value to.
 * \return `absl::OkStatus()` if lookup is successful. `absl::NotFoundError()`
 *         when lookup fails.
 */
template <typename T, typename U>
absl::Status CopyFromMap(const absl::flat_hash_map<T, U>& map, const T& key,
                         absl::string_view context, U& value) {
  const auto& result = LookupInMap(map, key, context);
  if (result.ok()) [[likely]] {
    value = *result;
  }
  return result.status();
}

/*!\brief Returns `absl::OkStatus()` if the arguments are equal.
 *
 * \param lhs First value to compare.
 * \param rhs Second value to compare.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments are equal
 *         `absl::InvalidArgumentError()` if the arguments are not equal.
 */
template <typename T>
absl::Status ValidateEqual(const T& lhs, const T& rhs,
                           absl::string_view context) {
  if (lhs == rhs) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected ", lhs, " == ", rhs, "."));
}

/*!\brief Returns `absl::OkStatus()` if the arguments are not equal.
 *
 * \param lhs First value to compare.
 * \param rhs Second value to compare.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments are not equal
 *         `absl::InvalidArgumentError()` if the arguments are equal.
 */
template <typename T>
absl::Status ValidateNotEqual(const T& lhs, const T& rhs,
                              absl::string_view context) {
  if (lhs != rhs) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected ", lhs, " != ", rhs, "."));
}

/*!\brief Returns `absl::OkStatus()` if the argument has a value.
 *
 * \param argument Argument to check.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if the arguments has a value.
 *        `absl::InvalidArgumentError()` if the argument does not have a value.
 */
template <typename T>
absl::Status ValidateHasValue(const std::optional<T>& argument,
                              absl::string_view context) {
  if (argument.has_value()) [[likely]] {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(
      absl::StrCat("Invalid ", context, ". Expected to have a value."));
}

/*!\brief Validates that all values in the range [first, last) are unique.
 *
 * \param first Iterator to start from.
 * \param last Iterator to stop before.
 * \param context Context to insert into the error message for debugging
 *        purposes.
 * \return `absl::OkStatus()` if no duplicates are found while iterating.
 *        `absl::InvalidArgumentError()` if duplicates are found.
 */
template <class InputIt>
absl::Status ValidateUnique(InputIt first, InputIt last,
                            absl::string_view context) {
  absl::flat_hash_set<typename InputIt::value_type> seen_values;

  for (auto iter = first; iter != last; ++iter) {
    if (const auto& [unused_iter, inserted] = seen_values.insert(*iter);
        !inserted) {
      return absl::InvalidArgumentError(
          absl::StrCat(context, " must be unique. Found duplicate: ", *iter));
    }
  }
  return absl::OkStatus();
}

/*!\brief Returns a map for static storage from a container of pairs.
 *
 * \param pairs Container of pairs to convert to a map. The first value must be
 *              unique among all pairs.
 * \return Map suitable for static storage. Or an empty map if the first value
 *         of a pair is not unique.
 */
template <class InputContainer>
auto BuildStaticMapFromPairs(const InputContainer& pairs) {
  typedef absl::flat_hash_map<typename InputContainer::value_type::first_type,
                              typename InputContainer::value_type::second_type>
      MapFromPairs;
  return absl::NoDestructor<MapFromPairs>([&]() {
    MapFromPairs map_from_pairs;
    for (const auto& [key, value] : pairs) {
      const auto& [unused_iter, inserted] = map_from_pairs.insert({key, value});
      if (!inserted) [[unlikely]] {
        LOG(ERROR) << "Failed building map from pairs. Duplicate key= "
                   << absl::StrCat(key) << ". Key type= " << typeid(key).name()
                   << " value type= " << typeid(value).name();
        return MapFromPairs{};
      }
    }
    return map_from_pairs;
  }());
}

/*!\brief Returns a map for static storage from a container of inverted pairs.
 *
 * \param pairs Container of pairs to invert and to convert to a map. The second
 *              value must be unique among all pairs.
 * \return Map suitable for static storage. Or an empty map if the second value
 *         of a pair is not unique.
 */
template <class InputContainer>
auto BuildStaticMapFromInvertedPairs(const InputContainer& pairs) {
  typedef absl::flat_hash_map<typename InputContainer::value_type::second_type,
                              typename InputContainer::value_type::first_type>
      MapFromInvertedPairs;
  return absl::NoDestructor<MapFromInvertedPairs>([&]() {
    MapFromInvertedPairs map_from_inverted_pairs;
    for (const auto& [value, key] : pairs) {
      const auto& [unused_iter, inserted] =
          map_from_inverted_pairs.insert({key, value});
      if (!inserted) [[unlikely]] {
        LOG(ERROR) << "Failed building map from pairs. Duplicate key= "
                   << absl::StrCat(key) << ". Key type: " << typeid(key).name()
                   << " value type: " << typeid(value).name();
        return MapFromInvertedPairs{};
      }
    }
    return map_from_inverted_pairs;
  }());
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

/*!\brief Interpolates a mix gain value.
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
 * \param target_mix_gain Output argument for the inteprolated value.
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
    int16_t& target_mix_gain) {
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
    target_mix_gain = step_start_point_getter();
  } else if (animation_type == linear_enum) {
    // Interpolate using the exact formula from the spec.
    const float a = (float)n / (float)n_2;
    const float p_0 = Q7_8ToFloat(linear_start_point_getter());
    const float p_2 = Q7_8ToFloat(linear_end_point_getter());
    RETURN_IF_NOT_OK(FloatToQ7_8((1 - a) * p_0 + a * p_2, target_mix_gain));
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
    const float target_mix_gain_float =
        (1 - a) * (1 - a) * p_0 + 2 * (1 - a) * a * p_1 + a * a * p_2;
    RETURN_IF_NOT_OK(FloatToQ7_8(target_mix_gain_float, target_mix_gain));
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown animation_type = ", animation_type));
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools

#endif  // COMMON_OBU_UTIL_H_
