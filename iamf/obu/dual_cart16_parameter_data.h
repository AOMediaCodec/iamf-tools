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
#ifndef OBU_DUAL_CART16_PARAMETER_DATA_H_
#define OBU_DUAL_CART16_PARAMETER_DATA_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct DualCart16ParameterData : public ParameterData {
 public:
  DualCart16ParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~DualCart16ParameterData() override = default;

  /*!\brief Creates a `DualCart16ParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `DualCart16ParameterData` or error.
   */
  static absl::StatusOr<DualCart16ParameterData> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Makes a `DualCart16ParameterData`.
   *
   * \param animation_type Animation type.
   * \param first_x Animated first Cartesian coordinate x.
   * \param first_y Animated first Cartesian coordinate y.
   * \param first_z Animated first Cartesian coordinate z.
   * \param second_x Animated second Cartesian coordinate x.
   * \param second_y Animated second Cartesian coordinate y.
   * \param second_z Animated second Cartesian coordinate z.
   * \return `DualCart16ParameterData` object.
   */
  static DualCart16ParameterData Make(AnimationType animation_type,
                                      AnimatedParameterData<int16_t> first_x,
                                      AnimatedParameterData<int16_t> first_y,
                                      AnimatedParameterData<int16_t> first_z,
                                      AnimatedParameterData<int16_t> second_x,
                                      AnimatedParameterData<int16_t> second_y,
                                      AnimatedParameterData<int16_t> second_z);

  bool friend operator==(const DualCart16ParameterData& lhs,
                         const DualCart16ParameterData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the DualCart16 parameter data.
   */
  void Print() const override;

  // Getters
  AnimationType animation_type() const { return animation_type_; }
  const AnimatedParameterData<int16_t>& first_x() const { return first_x_; }
  const AnimatedParameterData<int16_t>& first_y() const { return first_y_; }
  const AnimatedParameterData<int16_t>& first_z() const { return first_z_; }
  const AnimatedParameterData<int16_t>& second_x() const { return second_x_; }
  const AnimatedParameterData<int16_t>& second_y() const { return second_y_; }
  const AnimatedParameterData<int16_t>& second_z() const { return second_z_; }

 private:
  DualCart16ParameterData(AnimationType input_animation_type,
                          AnimatedParameterData<int16_t> input_first_x,
                          AnimatedParameterData<int16_t> input_first_y,
                          AnimatedParameterData<int16_t> input_first_z,
                          AnimatedParameterData<int16_t> input_second_x,
                          AnimatedParameterData<int16_t> input_second_y,
                          AnimatedParameterData<int16_t> input_second_z)
      : ParameterData(),
        animation_type_(input_animation_type),
        first_x_(input_first_x),
        first_y_(input_first_y),
        first_z_(input_first_z),
        second_x_(input_second_x),
        second_y_(input_second_y),
        second_z_(input_second_z) {}

  AnimationType animation_type_ = AnimationType::kStep;
  AnimatedParameterData<int16_t> first_x_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> first_y_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> first_z_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> second_x_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> second_y_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> second_z_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
};
}  // namespace iamf_tools

#endif  // OBU_DUAL_CART16_PARAMETER_DATA_H_