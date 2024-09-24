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

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

void AnimationStepInt16::Print() const {
  LOG(INFO) << "     // Step";
  LOG(INFO) << "     start_point_value= " << start_point_value;
}

absl::Status AnimationStepInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  return absl::OkStatus();
}

absl::Status AnimationStepInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  return absl::OkStatus();
}

void AnimationLinearInt16::Print() const {
  LOG(INFO) << "     // Linear";
  LOG(INFO) << "     start_point_value= " << start_point_value;
  LOG(INFO) << "     end_point_value= " << end_point_value;
}

absl::Status AnimationLinearInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(end_point_value));
  return absl::OkStatus();
}

absl::Status AnimationLinearInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(end_point_value));
  return absl::OkStatus();
}

void AnimationBezierInt16::Print() const {
  LOG(INFO) << "     // Bezier";
  LOG(INFO) << "     start_point_value= " << start_point_value;
  LOG(INFO) << "     end_point_value= " << end_point_value;
  LOG(INFO) << "     control_point_value= " << control_point_value;
  LOG(INFO) << "     control_point_relative_time= "
            << control_point_relative_time;
}

absl::Status AnimationBezierInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(end_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(control_point_value));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(control_point_relative_time, 8));
  return absl::OkStatus();
}

absl::Status AnimationBezierInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(end_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(control_point_value));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, control_point_relative_time));

  return absl::OkStatus();
}

absl::Status MixGainParameterData::ReadAndValidate(
    const PerIdParameterMetadata&, ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));
  animation_type =
      static_cast<MixGainParameterData::AnimationType>(animation_type_uleb);
  switch (animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep:
      AnimationStepInt16 step_param_data;
      RETURN_IF_NOT_OK(step_param_data.ReadAndValidate(rb));
      param_data = step_param_data;
      return absl::OkStatus();
    case kAnimateLinear:
      AnimationLinearInt16 linear_param_data;
      RETURN_IF_NOT_OK(linear_param_data.ReadAndValidate(rb));
      param_data = linear_param_data;
      return absl::OkStatus();
    case kAnimateBezier:
      AnimationBezierInt16 bezier_param_data;
      RETURN_IF_NOT_OK(bezier_param_data.ReadAndValidate(rb));
      param_data = bezier_param_data;
      return absl::OkStatus();
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unknown animation type= ", animation_type_uleb));
  }

  return absl::OkStatus();
}

absl::Status MixGainParameterData::Write(const PerIdParameterMetadata&,
                                         WriteBitBuffer& wb) const {
  // Write the `animation_type` field.
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type)));

  // Write the fields dependent on the `animation_type` field.
  switch (animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep:
      RETURN_IF_NOT_OK(
          std::get<AnimationStepInt16>(param_data).ValidateAndWrite(wb));
      break;
    case kAnimateLinear:
      RETURN_IF_NOT_OK(
          std::get<AnimationLinearInt16>(param_data).ValidateAndWrite(wb));
      break;
    case kAnimateBezier:
      RETURN_IF_NOT_OK(
          std::get<AnimationBezierInt16>(param_data).ValidateAndWrite(wb));
      break;
  }
  return absl::OkStatus();
}

void MixGainParameterData::Print() const {
  LOG(INFO) << "    animation_type= " << absl::StrCat(animation_type);
  switch (animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep:
      std::get<AnimationStepInt16>(param_data).Print();
      break;
    case kAnimateLinear:
      std::get<AnimationLinearInt16>(param_data).Print();
      break;
    case kAnimateBezier:
      std::get<AnimationBezierInt16>(param_data).Print();
      break;
    default:
      LOG(ERROR) << "Unknown animation type: " << absl::StrCat(animation_type);
  }
}

}  // namespace iamf_tools
