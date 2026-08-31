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
#include "iamf/obu/cart16_parameter_data.h"

#include <cstdint>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<Cart16ParameterData> Cart16ParameterData::CreateFromBuffer(
    ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));

  AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto read_int16 = [](ReadBitBuffer& r, int16_t& val) {
    return r.ReadSigned16(val);
  };

  auto parse_animated_fields = [&](AnimationType type)
      -> absl::StatusOr<AnimatedParameterData<int16_t>> {
    switch (type) {
      case AnimationType::kStep: {
        int16_t start_val;
        RETURN_IF_NOT_OK(read_int16(rb, start_val));
        return AnimatedParameterData<int16_t>::MakeStep(start_val);
      }
      case AnimationType::kLinear: {
        int16_t start_val, end_val;
        RETURN_IF_NOT_OK(read_int16(rb, start_val));
        RETURN_IF_NOT_OK(read_int16(rb, end_val));
        return AnimatedParameterData<int16_t>::MakeLinear(start_val, end_val);
      }
      case AnimationType::kBezier: {
        int16_t start_val, end_val, control_val;
        uint8_t rel_time;
        RETURN_IF_NOT_OK(read_int16(rb, start_val));
        RETURN_IF_NOT_OK(read_int16(rb, end_val));
        RETURN_IF_NOT_OK(read_int16(rb, control_val));
        RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, rel_time));
        return AnimatedParameterData<int16_t>::MakeBezier(
            start_val, end_val, control_val, rel_time);
      }
      case AnimationType::kInterLinear: {
        int16_t end_val;
        RETURN_IF_NOT_OK(read_int16(rb, end_val));
        return AnimatedParameterData<int16_t>::MakeInterLinear(end_val);
      }
      case AnimationType::kInterBezier: {
        int16_t end_val, control_val;
        uint8_t rel_time;
        RETURN_IF_NOT_OK(read_int16(rb, end_val));
        RETURN_IF_NOT_OK(read_int16(rb, control_val));
        RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, rel_time));
        return AnimatedParameterData<int16_t>::MakeInterBezier(
            end_val, control_val, rel_time);
      }
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "Invalid animation type: ", static_cast<uint32_t>(type)));
    }
  };

  auto x = parse_animated_fields(animation_type);
  if (!x.ok()) {
    return x.status();
  }

  auto y = parse_animated_fields(animation_type);
  if (!y.ok()) {
    return y.status();
  }

  auto z = parse_animated_fields(animation_type);
  if (!z.ok()) {
    return z.status();
  }

  return Make(animation_type, *std::move(x), *std::move(y), *std::move(z));
}

Cart16ParameterData Cart16ParameterData::Make(
    AnimationType animation_type, AnimatedParameterData<int16_t> x,
    AnimatedParameterData<int16_t> y, AnimatedParameterData<int16_t> z) {
  return Cart16ParameterData(animation_type, x, y, z);
}

absl::Status Cart16ParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_int16 = [](WriteBitBuffer& w, int16_t val) {
    return w.WriteSigned16(val);
  };

  auto write_animated_data =
      [&](const AnimatedParameterData<int16_t>& anim) -> absl::Status {
    switch (anim.animation_type()) {
      case AnimationType::kStep:
        return write_int16(wb, *anim.start_point_value());
      case AnimationType::kLinear:
        RETURN_IF_NOT_OK(write_int16(wb, *anim.start_point_value()));
        return write_int16(wb, *anim.end_point_value());
      case AnimationType::kBezier:
        RETURN_IF_NOT_OK(write_int16(wb, *anim.start_point_value()));
        RETURN_IF_NOT_OK(write_int16(wb, *anim.end_point_value()));
        RETURN_IF_NOT_OK(write_int16(wb, *anim.control_point_value()));
        return wb.WriteUnsignedLiteral(*anim.control_point_relative_time(), 8);
      case AnimationType::kInterLinear:
        return write_int16(wb, *anim.end_point_value());
      case AnimationType::kInterBezier:
        RETURN_IF_NOT_OK(write_int16(wb, *anim.end_point_value()));
        RETURN_IF_NOT_OK(write_int16(wb, *anim.control_point_value()));
        return wb.WriteUnsignedLiteral(*anim.control_point_relative_time(), 8);
    }
    return absl::InvalidArgumentError("Unknown animation type");
  };

  RETURN_IF_NOT_OK(write_animated_data(x_));
  RETURN_IF_NOT_OK(write_animated_data(y_));
  RETURN_IF_NOT_OK(write_animated_data(z_));
  return absl::OkStatus();
}

void Cart16ParameterData::Print() const {
  ABSL_LOG(INFO) << "Cart16ParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools
