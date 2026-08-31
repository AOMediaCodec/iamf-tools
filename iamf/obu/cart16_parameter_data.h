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
#ifndef OBU_CART16_PARAMETER_DATA_H_
#define OBU_CART16_PARAMETER_DATA_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

struct Cart16ParameterData : public ParameterData {
 public:
  Cart16ParameterData() = default;

  /*!\brief Overridden destructor.
   */
  ~Cart16ParameterData() override = default;

  /*!\brief Creates a `Cart16ParameterData` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `Cart16ParameterData` or error.
   */
  static absl::StatusOr<Cart16ParameterData> CreateFromBuffer(
      ReadBitBuffer& rb);

  /*!\brief Makes a `Cart16ParameterData`.
   *
   * \param animation_type Animation type.
   * \param x Animated coordinate x.
   * \param y Animated coordinate y.
   * \param z Animated coordinate z.
   * \return `Cart16ParameterData` object.
   */
  static Cart16ParameterData Make(AnimationType animation_type,
                                  AnimatedParameterData<int16_t> x,
                                  AnimatedParameterData<int16_t> y,
                                  AnimatedParameterData<int16_t> z);

  bool friend operator==(const Cart16ParameterData& lhs,
                         const Cart16ParameterData& rhs) = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the cart16 parameter data.
   */
  void Print() const override;

  // Getters
  AnimationType animation_type() const { return animation_type_; }
  const AnimatedParameterData<int16_t>& x() const { return x_; }
  const AnimatedParameterData<int16_t>& y() const { return y_; }
  const AnimatedParameterData<int16_t>& z() const { return z_; }

 private:
  Cart16ParameterData(AnimationType input_animation_type,
                      AnimatedParameterData<int16_t> input_x,
                      AnimatedParameterData<int16_t> input_y,
                      AnimatedParameterData<int16_t> input_z)
      : ParameterData(),
        animation_type_(input_animation_type),
        x_(input_x),
        y_(input_y),
        z_(input_z) {}

  AnimationType animation_type_ = AnimationType::kStep;
  AnimatedParameterData<int16_t> x_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> y_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
  AnimatedParameterData<int16_t> z_ =
      AnimatedParameterData<int16_t>::MakeStep(0);
};
}  // namespace iamf_tools

#endif  // OBU_CART16_PARAMETER_DATA_H_
