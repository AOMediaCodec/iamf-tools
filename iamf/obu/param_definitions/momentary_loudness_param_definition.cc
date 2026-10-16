/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/momentary_loudness_parameter_data.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {

absl::Status MomentaryLoudnessParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // No sub-class specific part for Momentary Loudness Parameter Definition.
  return absl::OkStatus();
}

absl::Status MomentaryLoudnessParamDefinition::ReadAndValidate(
    ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // No sub-class specific part for Momentary Loudness Parameter Definition.
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<ParameterData>>
MomentaryLoudnessParamDefinition::CreateParameterDataFromBuffer(
    ReadBitBuffer& rb) const {
  auto data = MomentaryLoudnessParameterData::CreateFromBuffer(rb);
  if (!data.ok()) {
    return data.status();
  }
  return std::make_unique<MomentaryLoudnessParameterData>(*std::move(data));
}

std::string MomentaryLoudnessParamDefinition::ToString() const {
  return absl::StrCat("MomentaryLoudnessParamDefinition:\n",
                      ParamDefinition::ToString());
}

}  // namespace iamf_tools
