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
#include "iamf/obu/demixing_param_definition.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

absl::Status DemixingParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(
      default_demixing_info_parameter_data_.Write(/*per_id_metadata=*/{}, wb));

  return absl::OkStatus();
}

absl::Status DemixingParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ReadAndValidate(rb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(default_demixing_info_parameter_data_.ReadAndValidate(
      /*per_id_metadata=*/{}, rb));

  return absl::OkStatus();
}

void DemixingParamDefinition::Print() const {
  LOG(INFO) << "DemixingParamDefinition:";
  ParamDefinition::Print();
  default_demixing_info_parameter_data_.Print();
}

}  // namespace iamf_tools
