/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/mix_gain_parameter_data.h"

#include <cstdint>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

using enum AnimationType;

absl::Status MixGainParameterData::ReadAndValidate(ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));
  const auto animation_type = static_cast<AnimationType>(animation_type_uleb);
  switch (animation_type) {
    case kStep: {
      int16_t start_val;
      RETURN_IF_NOT_OK(rb.ReadSigned16(start_val));
      param_data = AnimatedParameterData<int16_t>::MakeStep(start_val);
      return absl::OkStatus();
    }
    case kLinear: {
      int16_t start_val;
      RETURN_IF_NOT_OK(rb.ReadSigned16(start_val));
      int16_t end_val;
      RETURN_IF_NOT_OK(rb.ReadSigned16(end_val));
      param_data =
          AnimatedParameterData<int16_t>::MakeLinear(start_val, end_val);
      return absl::OkStatus();
    }
    case kBezier: {
      int16_t start_val;
      RETURN_IF_NOT_OK(rb.ReadSigned16(start_val));
      int16_t end_val;
      RETURN_IF_NOT_OK(rb.ReadSigned16(end_val));
      int16_t control_val;
      RETURN_IF_NOT_OK(rb.ReadSigned16(control_val));
      uint8_t rel_time;
      RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, rel_time));
      param_data = AnimatedParameterData<int16_t>::MakeBezier(
          start_val, end_val, control_val, rel_time);
      return absl::OkStatus();
    }
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unknown animation type= ", animation_type_uleb));
  }
}

absl::Status MixGainParameterData::Write(WriteBitBuffer& wb) const {
  auto write_int16 = [](WriteBitBuffer& w, int16_t val) {
    return w.WriteSigned16(val);
  };
  return param_data.Write(wb, write_int16);
}

AnimationType MixGainParameterData::GetAnimationType() const {
  return param_data.animation_type();
}

void MixGainParameterData::Print() const {
  ABSL_LOG(INFO) << "    animation_type= " << absl::StrCat(GetAnimationType());
  // TODO(b/552563464): Implement and use AnimatedParameterData::Print().
  if (param_data.animation_type() == kStep) {
    ABSL_LOG(INFO) << "     // Step";
    ABSL_LOG(INFO) << "     start_point_value= "
                   << *param_data.start_point_value();
  } else if (param_data.animation_type() == kLinear) {
    ABSL_LOG(INFO) << "     // Linear";
    ABSL_LOG(INFO) << "     start_point_value= "
                   << *param_data.start_point_value();
    ABSL_LOG(INFO) << "     end_point_value= " << *param_data.end_point_value();
  } else if (param_data.animation_type() == kBezier) {
    ABSL_LOG(INFO) << "     // Bezier";
    ABSL_LOG(INFO) << "     start_point_value= "
                   << *param_data.start_point_value();
    ABSL_LOG(INFO) << "     end_point_value= " << *param_data.end_point_value();
    ABSL_LOG(INFO) << "     control_point_value= "
                   << *param_data.control_point_value();
    ABSL_LOG(INFO) << "     control_point_relative_time= "
                   << absl::StrCat(*param_data.control_point_relative_time());
  }
}

}  // namespace iamf_tools
