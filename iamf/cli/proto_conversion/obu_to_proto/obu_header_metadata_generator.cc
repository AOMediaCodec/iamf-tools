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
#include "iamf/cli/proto_conversion/obu_to_proto/obu_header_metadata_generator.h"

#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

absl::StatusOr<iamf_tools_cli_proto::ObuHeaderMetadata>
ObuHeaderMetadataGenerator::Generate(const ObuHeader& obu_header) {
  iamf_tools_cli_proto::ObuHeaderMetadata result;
  result.set_obu_redundant_copy(obu_header.obu_redundant_copy);
  result.set_obu_trimming_status_flag(obu_header.obu_trimming_status_flag);
  result.set_obu_extension_flag(obu_header.GetExtensionHeaderFlag());
  result.set_num_samples_to_trim_at_end(obu_header.num_samples_to_trim_at_end);
  result.set_num_samples_to_trim_at_start(
      obu_header.num_samples_to_trim_at_start);
  result.mutable_extension_header_bytes()->resize(
      obu_header.GetExtensionHeaderSize());
  if (obu_header.GetExtensionHeaderFlag()) {
    RETURN_IF_NOT_OK(StaticCastSpanIfInRange(
        "extension_header_bytes",
        absl::MakeConstSpan(*obu_header.extension_header_bytes),
        absl::MakeSpan(*result.mutable_extension_header_bytes())));
  }

  return result;
}

}  // namespace iamf_tools
