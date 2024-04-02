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

#include "iamf/cli/adm_to_user_metadata/iamf/ia_sequence_header_obu_metadata_handler.h"

#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

// Sets the required textproto fields for `ia_sequence_header_metadata`.
void PopulateBaseProfileIaSequenceHeaderObuMetadata(
    iamf_tools_cli_proto::IASequenceHeaderObuMetadata&
        ia_sequence_header_obu_metadata) {
  ia_sequence_header_obu_metadata.set_ia_code(IASequenceHeaderObu::kIaCode);
  ia_sequence_header_obu_metadata.set_primary_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE);
  ia_sequence_header_obu_metadata.set_additional_profile(
      iamf_tools_cli_proto::PROFILE_VERSION_BASE);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
