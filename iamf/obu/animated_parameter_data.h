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

#ifndef OBU_ANIMATED_PARAMETER_DATA_H_
#define OBU_ANIMATED_PARAMETER_DATA_H_

#include <cstdint>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief The type of interpolation to use for the animation. */
enum class AnimationType : uint32_t {
  kStep = 0,
  kLinear = 1,
  kBezier = 2,
  kInterLinear = 3,
  kInterBezier = 4,
};

/* The AnimatedParameterData class template provides information
 * required for animating a set of parameter values of type `T`.
 */
template <typename T>
class AnimatedParameterData {
  using enum AnimationType;

 public:
  /*!\brief Creates an AnimatedParameterData object with step animation.
   *
   * \param start_val Start value of the parameter.
   * \return AnimatedParameterData with step animation.
   */
  static AnimatedParameterData MakeStep(T start_val) {
    return AnimatedParameterData(AnimationType::kStep, start_val, std::nullopt,
                                 std::nullopt, std::nullopt);
  }

  /*!\brief Creates an AnimatedParameterData object with linear animation.
   *
   * \param start_val Start value of the parameter.
   * \param end_val End value of the parameter.
   * \return AnimatedParameterData with linear animation.
   */
  static AnimatedParameterData MakeLinear(T start_val, T end_val) {
    return AnimatedParameterData(kLinear, start_val, end_val, std::nullopt,
                                 std::nullopt);
  }

  /*!\brief Creates an AnimatedParameterData object with bezier animation.
   *
   * \param start_val Start value of the parameter.
   * \param end_val End value of the parameter.
   * \param control_val Control point value of the parameter.
   * \param rel_time Relative time of the control point.
   * \return AnimatedParameterData with bezier animation.
   */
  static AnimatedParameterData MakeBezier(T start_val, T end_val, T control_val,
                                          uint8_t rel_time) {
    return AnimatedParameterData(kBezier, start_val, end_val, control_val,
                                 rel_time);
  }

  /*!\brief Creates an AnimatedParameterData from a ReadBitBuffer.
   *
   * \param rb Buffer to read from.
   * \param read_value_func Callable with signature
   *        `absl::Status(ReadBitBuffer&, T&)` to read the start/end/control
   *        values.
   * \return Deserialized AnimatedParameterData, or an error status if reading
   *         fails.
   */
  template <typename ReadValueFunc>
  static absl::StatusOr<AnimatedParameterData> CreateFromBuffer(
      ReadBitBuffer& rb, ReadValueFunc read_value_func) {
    DecodedUleb128 animation_type;
    RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type));

    switch (static_cast<AnimationType>(animation_type)) {
      case kStep: {
        T start_val;
        RETURN_IF_NOT_OK(read_value_func(rb, start_val));
        return MakeStep(start_val);
      }
      case kLinear: {
        T start_val, end_val;
        RETURN_IF_NOT_OK(read_value_func(rb, start_val));
        RETURN_IF_NOT_OK(read_value_func(rb, end_val));
        return MakeLinear(start_val, end_val);
      }
      case kBezier: {
        T start_val, end_val, control_val;
        uint8_t rel_time;
        RETURN_IF_NOT_OK(read_value_func(rb, start_val));
        RETURN_IF_NOT_OK(read_value_func(rb, end_val));
        RETURN_IF_NOT_OK(read_value_func(rb, control_val));
        RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, rel_time));
        return MakeBezier(start_val, end_val, control_val, rel_time);
      }
      case kInterLinear:
      case kInterBezier:
        return absl::UnimplementedError(absl::StrCat(
            "Animation type ", animation_type, " is not implemented yet."));
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("Invalid animation type: ", animation_type));
    }
  }

  /*!\brief Gets the animation type.
   *
   * \return The animation type.
   */
  AnimationType animation_type() const { return animation_type_; }

  /*!\brief Gets the start point value.
   *
   * \return The start point value.
   */
  const std::optional<T>& start_point_value() const {
    return start_point_value_;
  }

  /*!\brief Gets the end point value.
   *
   * \return The end point value.
   */
  const std::optional<T>& end_point_value() const { return end_point_value_; }

  /*!\brief Gets the control point value.
   *
   * \return The control point value.
   */
  const std::optional<T>& control_point_value() const {
    return control_point_value_;
  }

  /*!\brief Gets the control point relative time.
   *
   * \return The control point relative time.
   */
  const std::optional<uint8_t>& control_point_relative_time() const {
    return control_point_relative_time_;
  }

  friend bool operator==(const AnimatedParameterData& lhs,
                         const AnimatedParameterData& rhs) = default;

 private:
  /*!\brief Private constructor.
   *
   * \param type Animation type.
   * \param start_val Value of the parameter at the start of the interval.
   * \param end_val Value of the parameter at the end of the interval.
   * \param control_val Value of the parameter at the control point.
   * \param control_time Relative time of the control point.
   */
  AnimatedParameterData(AnimationType type, std::optional<T> start_val,
                        std::optional<T> end_val, std::optional<T> control_val,
                        std::optional<uint8_t> control_time)
      : animation_type_(type),
        start_point_value_(start_val),
        end_point_value_(end_val),
        control_point_value_(control_val),
        control_point_relative_time_(control_time) {}

  AnimationType animation_type_;
  std::optional<T> start_point_value_;
  std::optional<T> end_point_value_;
  std::optional<T> control_point_value_;
  std::optional<uint8_t> control_point_relative_time_;
};

}  // namespace iamf_tools

#endif  // OBU_ANIMATED_PARAMETER_DATA_H_
