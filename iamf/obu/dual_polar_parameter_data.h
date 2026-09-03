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
#ifndef OBU_DUAL_POLAR_PARAMETER_DATA_H_
#define OBU_DUAL_POLAR_PARAMETER_DATA_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct DualPolarParameterData : public ParameterData {
 public:
  DualPolarParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~DualPolarParameterData() override = default;

  /*!\brief Creates a `DualPolarParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `DualPolarParameterData` or error.
   */
  static absl::StatusOr<DualPolarParameterData> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Creates a `DualPolarParameterData` and validates coordinate bounds.
   *
   * \param animation_type Animation type.
   * \param first_azimuth Animated first azimuth.
   * \param first_elevation Animated first elevation.
   * \param first_distance Animated first distance.
   * \param second_azimuth Animated second azimuth.
   * \param second_elevation Animated second elevation.
   * \param second_distance Animated second distance.
   * \return `DualPolarParameterData` or error if coordinates out of bounds.
   */
  static absl::StatusOr<DualPolarParameterData> Create(
      AnimationType animation_type,
      AnimatedParameterData<int16_t> first_azimuth,
      AnimatedParameterData<int8_t> first_elevation,
      AnimatedParameterData<uint8_t> first_distance,
      AnimatedParameterData<int16_t> second_azimuth,
      AnimatedParameterData<int8_t> second_elevation,
      AnimatedParameterData<uint8_t> second_distance);

  bool friend operator==(const DualPolarParameterData& lhs,
                         const DualPolarParameterData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the polar parameter data.
   */
  void Print() const override;

  // Getters
  AnimationType animation_type() const { return animation_type_; }
  const AnimatedParameterData<int16_t>& first_azimuth() const {
    return first_azimuth_;
  }
  const AnimatedParameterData<int8_t>& first_elevation() const {
    return first_elevation_;
  }
  const AnimatedParameterData<uint8_t>& first_distance() const {
    return first_distance_;
  }
  const AnimatedParameterData<int16_t>& second_azimuth() const {
    return second_azimuth_;
  }
  const AnimatedParameterData<int8_t>& second_elevation() const {
    return second_elevation_;
  }
  const AnimatedParameterData<uint8_t>& second_distance() const {
    return second_distance_;
  }

 private:
  DualPolarParameterData(AnimationType input_animation_type,
                         AnimatedParameterData<int16_t> input_first_azimuth,
                         AnimatedParameterData<int8_t> input_first_elevation,
                         AnimatedParameterData<uint8_t> input_first_distance,
                         AnimatedParameterData<int16_t> input_second_azimuth,
                         AnimatedParameterData<int8_t> input_second_elevation,
                         AnimatedParameterData<uint8_t> input_second_distance)
      : ParameterData(),
        animation_type_(input_animation_type),
        first_azimuth_(input_first_azimuth),
        first_elevation_(input_first_elevation),
        first_distance_(input_first_distance),
        second_azimuth_(input_second_azimuth),
        second_elevation_(input_second_elevation),
        second_distance_(input_second_distance) {}

  AnimationType animation_type_ = AnimationType::kStep;
  AnimatedParameterData<int16_t> first_azimuth_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int8_t> first_elevation_ =
      AnimatedParameterData<int8_t>::MakeStep(0);
  AnimatedParameterData<uint8_t> first_distance_ =
      AnimatedParameterData<uint8_t>::MakeStep(0);
  AnimatedParameterData<int16_t> second_azimuth_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int8_t> second_elevation_ =
      AnimatedParameterData<int8_t>::MakeStep(0);
  AnimatedParameterData<uint8_t> second_distance_ =
      AnimatedParameterData<uint8_t>::MakeStep(0);
};
}  // namespace iamf_tools

#endif  // OBU_DUAL_POLAR_PARAMETER_DATA_H_
