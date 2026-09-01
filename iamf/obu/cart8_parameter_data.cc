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
#include "iamf/obu/cart8_parameter_data.h"

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

absl::StatusOr<Cart8ParameterData> Cart8ParameterData::CreateFromBuffer(
    ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));

  AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto read_int8 = [](ReadBitBuffer& r, int8_t& val) {
    return r.ReadSigned8(val);
  };

  auto x = AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
      animation_type, rb, read_int8);
  if (!x.ok()) {
    return x.status();
  }

  auto y = AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
      animation_type, rb, read_int8);
  if (!y.ok()) {
    return y.status();
  }

  auto z = AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
      animation_type, rb, read_int8);
  if (!z.ok()) {
    return z.status();
  }

  return Make(animation_type, *std::move(x), *std::move(y), *std::move(z));
}

Cart8ParameterData Cart8ParameterData::Make(AnimationType animation_type,
                                            AnimatedParameterData<int8_t> x,
                                            AnimatedParameterData<int8_t> y,
                                            AnimatedParameterData<int8_t> z) {
  return Cart8ParameterData(animation_type, x, y, z);
}

absl::Status Cart8ParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_int8 = [](WriteBitBuffer& w, int8_t val) {
    return w.WriteSigned8(val);
  };

  RETURN_IF_NOT_OK(x_.WritePayload(wb, write_int8));
  RETURN_IF_NOT_OK(y_.WritePayload(wb, write_int8));
  RETURN_IF_NOT_OK(z_.WritePayload(wb, write_int8));
  return absl::OkStatus();
}

void Cart8ParameterData::Print() const {
  ABSL_LOG(INFO) << "Cart8ParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools
