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
#include "iamf/obu/polar_parameter_data.h"

#include <cstdint>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

template <typename T>
absl::Status ValidateRange(const AnimatedParameterData<T>& anim, T min_val,
                           T max_val, absl::string_view name) {
  if (anim.start_point_value().has_value()) {
    RETURN_IF_NOT_OK(ValidateInRange(*anim.start_point_value(),
                                     {min_val, max_val},
                                     absl::StrCat(name, " start_point_value")));
  }
  if (anim.end_point_value().has_value()) {
    RETURN_IF_NOT_OK(ValidateInRange(*anim.end_point_value(),
                                     {min_val, max_val},
                                     absl::StrCat(name, " end_point_value")));
  }
  if (anim.control_point_value().has_value()) {
    RETURN_IF_NOT_OK(
        ValidateInRange(*anim.control_point_value(), {min_val, max_val},
                        absl::StrCat(name, " control_point_value")));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<PolarParameterData> PolarParameterData::CreateFromBuffer(
    ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));

  AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto read_azimuth = [](ReadBitBuffer& r, int16_t& val) {
    return r.ReadSigned9(val);
  };
  auto read_elevation = [](ReadBitBuffer& r, int8_t& val) {
    return r.ReadSigned8(val);
  };
  auto read_distance = [](ReadBitBuffer& r, uint8_t& val) {
    return r.ReadUnsignedLiteral(7, val);
  };

  auto azimuth =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_azimuth);
  if (!azimuth.ok()) {
    return azimuth.status();
  }

  auto elevation =
      AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_elevation);
  if (!elevation.ok()) {
    return elevation.status();
  }

  auto distance =
      AnimatedParameterData<uint8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_distance);
  if (!distance.ok()) {
    return distance.status();
  }

  return Create(animation_type, *std::move(azimuth), *std::move(elevation),
                *std::move(distance));
}

absl::StatusOr<PolarParameterData> PolarParameterData::Create(
    AnimationType animation_type, AnimatedParameterData<int16_t> azimuth,
    AnimatedParameterData<int8_t> elevation,
    AnimatedParameterData<uint8_t> distance) {
  RETURN_IF_NOT_OK(ValidateEqual(azimuth.animation_type(), animation_type,
                                 "azimuth animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(elevation.animation_type(), animation_type,
                                 "elevation animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(distance.animation_type(), animation_type,
                                 "distance animation_type"));

  // azimuth is 9 bits signed -> [-256, 255]
  RETURN_IF_NOT_OK(ValidateRange<int16_t>(azimuth, -256, 255, "azimuth"));
  // elevation is 8 bits signed -> [-128, 127]
  RETURN_IF_NOT_OK(ValidateRange<int8_t>(elevation, -128, 127, "elevation"));
  // distance is 7 bits unsigned -> [0, 127]
  RETURN_IF_NOT_OK(ValidateRange<uint8_t>(distance, 0, 127, "distance"));

  return PolarParameterData(animation_type, azimuth, elevation, distance);
}

absl::Status PolarParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_azimuth = [](WriteBitBuffer& w, int16_t val) {
    return w.WriteSigned9(val);
  };
  auto write_elevation = [](WriteBitBuffer& w, int8_t val) {
    return w.WriteSigned8(val);
  };
  auto write_distance = [](WriteBitBuffer& w, uint8_t val) {
    return w.WriteUnsignedLiteral(val, 7);
  };

  RETURN_IF_NOT_OK(azimuth_.WritePayload(wb, write_azimuth));
  RETURN_IF_NOT_OK(elevation_.WritePayload(wb, write_elevation));
  RETURN_IF_NOT_OK(distance_.WritePayload(wb, write_distance));
  return absl::OkStatus();
}

void PolarParameterData::Print() const {
  ABSL_LOG(INFO) << "PolarParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools
