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
#include "iamf/obu/dual_polar_parameter_data.h"

#include <algorithm>
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

int16_t Clip3(int16_t value, int16_t min_val, int16_t max_val) {
  return std::clamp(value, min_val, max_val);
}

template <typename T>
absl::Status ValidateRange(const AnimatedParameterData<T>& anim, T min_val,
                           T max_val, absl::string_view name) {
  const T min = min_val;
  const T max = max_val;
  const std::pair<const T&, const T&> kRange{min, max};

  if (anim.start_point_value().has_value()) {
    RETURN_IF_NOT_OK(ValidateInRange(*anim.start_point_value(), kRange,
                                     absl::StrCat(name, " start_point_value")));
  }
  if (anim.end_point_value().has_value()) {
    RETURN_IF_NOT_OK(ValidateInRange(*anim.end_point_value(), kRange,
                                     absl::StrCat(name, " end_point_value")));
  }
  if (anim.control_point_value().has_value()) {
    RETURN_IF_NOT_OK(
        ValidateInRange(*anim.control_point_value(), kRange,
                        absl::StrCat(name, " control_point_value")));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<DualPolarParameterData> DualPolarParameterData::CreateFromBuffer(
    ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));

  AnimationType animation_type =
      static_cast<AnimationType>(animation_type_uleb);

  auto read_azimuth = [](ReadBitBuffer& r, int16_t& val) -> absl::Status {
    RETURN_IF_NOT_OK(r.ReadSigned9(val));
    val = Clip3(val, -180, 180);
    return absl::OkStatus();
  };
  auto read_elevation = [](ReadBitBuffer& r, int8_t& val) -> absl::Status {
    RETURN_IF_NOT_OK(r.ReadSigned8(val));
    val = Clip3(val, -90, 90);
    return absl::OkStatus();
  };
  auto read_distance = [](ReadBitBuffer& r, uint8_t& val) {
    return r.ReadUnsignedLiteral(7, val);
  };

  auto first_azimuth =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_azimuth);
  if (!first_azimuth.ok()) {
    return first_azimuth.status();
  }

  auto first_elevation =
      AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_elevation);
  if (!first_elevation.ok()) {
    return first_elevation.status();
  }

  auto first_distance =
      AnimatedParameterData<uint8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_distance);
  if (!first_distance.ok()) {
    return first_distance.status();
  }

  auto second_azimuth =
      AnimatedParameterData<int16_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_azimuth);
  if (!second_azimuth.ok()) {
    return second_azimuth.status();
  }

  auto second_elevation =
      AnimatedParameterData<int8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_elevation);
  if (!second_elevation.ok()) {
    return second_elevation.status();
  }

  auto second_distance =
      AnimatedParameterData<uint8_t>::CreateFromBufferGivenAnimationType(
          animation_type, rb, read_distance);
  if (!second_distance.ok()) {
    return second_distance.status();
  }

  return Create(animation_type, *std::move(first_azimuth),
                *std::move(first_elevation), *std::move(first_distance),
                *std::move(second_azimuth), *std::move(second_elevation),
                *std::move(second_distance));
}

absl::StatusOr<DualPolarParameterData> DualPolarParameterData::Create(
    AnimationType animation_type, AnimatedParameterData<int16_t> first_azimuth,
    AnimatedParameterData<int8_t> first_elevation,
    AnimatedParameterData<uint8_t> first_distance,
    AnimatedParameterData<int16_t> second_azimuth,
    AnimatedParameterData<int8_t> second_elevation,
    AnimatedParameterData<uint8_t> second_distance) {
  RETURN_IF_NOT_OK(ValidateEqual(first_azimuth.animation_type(), animation_type,
                                 "first_azimuth animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(first_elevation.animation_type(),
                                 animation_type,
                                 "first_elevation animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(first_distance.animation_type(),
                                 animation_type,
                                 "first_distance animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(second_azimuth.animation_type(),
                                 animation_type,
                                 "second_azimuth animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(second_elevation.animation_type(),
                                 animation_type,
                                 "second_elevation animation_type"));
  RETURN_IF_NOT_OK(ValidateEqual(second_distance.animation_type(),
                                 animation_type,
                                 "second_distance animation_type"));

  RETURN_IF_NOT_OK(
      ValidateRange<int16_t>(first_azimuth, -180, 180, "first_azimuth"));
  RETURN_IF_NOT_OK(
      ValidateRange<int8_t>(first_elevation, -90, 90, "first_elevation"));
  RETURN_IF_NOT_OK(
      ValidateRange<uint8_t>(first_distance, 0, 127, "first_distance"));

  RETURN_IF_NOT_OK(
      ValidateRange<int16_t>(second_azimuth, -180, 180, "second_azimuth"));
  RETURN_IF_NOT_OK(
      ValidateRange<int8_t>(second_elevation, -90, 90, "second_elevation"));
  RETURN_IF_NOT_OK(
      ValidateRange<uint8_t>(second_distance, 0, 127, "second_distance"));

  return DualPolarParameterData(animation_type, first_azimuth, first_elevation,
                                first_distance, second_azimuth,
                                second_elevation, second_distance);
}

absl::Status DualPolarParameterData::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(animation_type_)));

  auto write_azimuth = [](WriteBitBuffer& w, int16_t val) {
    return w.WriteSigned9(Clip3(val, -180, 180));
  };
  auto write_elevation = [](WriteBitBuffer& w, int8_t val) {
    return w.WriteSigned8(Clip3(val, -90, 90));
  };
  auto write_distance = [](WriteBitBuffer& w, uint8_t val) {
    return w.WriteUnsignedLiteral(val, 7);
  };

  RETURN_IF_NOT_OK(first_azimuth_.WritePayload(wb, write_azimuth));
  RETURN_IF_NOT_OK(first_elevation_.WritePayload(wb, write_elevation));
  RETURN_IF_NOT_OK(first_distance_.WritePayload(wb, write_distance));
  RETURN_IF_NOT_OK(second_azimuth_.WritePayload(wb, write_azimuth));
  RETURN_IF_NOT_OK(second_elevation_.WritePayload(wb, write_elevation));
  RETURN_IF_NOT_OK(second_distance_.WritePayload(wb, write_distance));
  return absl::OkStatus();
}

void DualPolarParameterData::Print() const {
  ABSL_LOG(INFO) << "DualPolarParameterData printing is not implemented yet:";
}

}  // namespace iamf_tools
