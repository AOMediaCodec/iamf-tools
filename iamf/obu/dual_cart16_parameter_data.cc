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
#include "iamf/obu/dual_cart16_parameter_data.h"

#include <cstdint>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<DualCart16ParameterData>
DualCart16ParameterData::CreateFromBuffer(ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));

  AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto read_int16 = [](ReadBitBuffer& r, int16_t& val) {
    return r.ReadSigned16(val);
  };

  auto first_x =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_int16);
  if (!first_x.ok()) {
    return first_x.status();
  }

  auto first_y =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_int16);
  if (!first_y.ok()) {
    return first_y.status();
  }

  auto first_z =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_int16);
  if (!first_z.ok()) {
    return first_z.status();
  }

  auto second_x =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_int16);
  if (!second_x.ok()) {
    return second_x.status();
  }

  auto second_y =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_int16);
  if (!second_y.ok()) {
    return second_y.status();
  }

  auto second_z =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_int16);
  if (!second_z.ok()) {
    return second_z.status();
  }

  return Make(animation_type, *std::move(first_x), *std::move(first_y),
              *std::move(first_z), *std::move(second_x), *std::move(second_y),
              *std::move(second_z));
}

DualCart16ParameterData DualCart16ParameterData::Make(
    AnimationType animation_type, AnimatedParameterData<int16_t> first_x,
    AnimatedParameterData<int16_t> first_y,
    AnimatedParameterData<int16_t> first_z,
    AnimatedParameterData<int16_t> second_x,
    AnimatedParameterData<int16_t> second_y,
    AnimatedParameterData<int16_t> second_z) {
  return DualCart16ParameterData(animation_type, first_x, first_y, first_z,
                                 second_x, second_y, second_z);
}

absl::Status DualCart16ParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_int16 = [](WriteBitBuffer& w, int16_t val) {
    return w.WriteSigned16(val);
  };

  RETURN_IF_NOT_OK(first_x_.WritePayload(wb, write_int16));
  RETURN_IF_NOT_OK(first_y_.WritePayload(wb, write_int16));
  RETURN_IF_NOT_OK(first_z_.WritePayload(wb, write_int16));
  RETURN_IF_NOT_OK(second_x_.WritePayload(wb, write_int16));
  RETURN_IF_NOT_OK(second_y_.WritePayload(wb, write_int16));
  RETURN_IF_NOT_OK(second_z_.WritePayload(wb, write_int16));
  return absl::OkStatus();
}

void DualCart16ParameterData::Print() const {
  ABSL_LOG(INFO) << "DualCart16ParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools
