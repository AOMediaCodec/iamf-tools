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
#include "iamf/obu/extension_parameter_data.h"

#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

absl::Status ExtensionParameterData::ReadAndValidate(
    const PerIdParameterMetadata&, ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_data_size));
  parameter_data_bytes.resize(parameter_data_size);
  return rb.ReadUint8Span(absl::MakeSpan(parameter_data_bytes));
}

absl::Status ExtensionParameterData::Write(
    const PerIdParameterMetadata& /*per_id_metadata*/,
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_data_size));
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "parameter_data_bytes", parameter_data_bytes, parameter_data_size));
  RETURN_IF_NOT_OK(
      wb.WriteUint8Span(absl::MakeConstSpan(parameter_data_bytes)));
  return absl::OkStatus();
}

void ExtensionParameterData::Print() const {
  LOG(INFO) << "    parameter_data_size= " << absl::StrCat(parameter_data_size);
  LOG(INFO) << "    // parameter_data_bytes.size()= "
            << absl::StrCat(parameter_data_bytes.size());
}

}  // namespace iamf_tools
