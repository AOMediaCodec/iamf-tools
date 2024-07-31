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

#include "absl/strings/string_view.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

// Sets the required textproto fields for test_vector_metadata.
void TestVectorMetadataHandler(
    absl::string_view file_name_prefix,
    iamf_tools_cli_proto::TestVectorMetadata& test_vector_metadata) {
  constexpr absl::string_view kHumanReadableDescription =
      "ADM to IAMF Conversion";
  test_vector_metadata.set_human_readable_description(
      kHumanReadableDescription);
  test_vector_metadata.set_file_name_prefix(file_name_prefix);
  test_vector_metadata.set_is_valid(true);
  test_vector_metadata.set_is_valid_to_decode(true);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
