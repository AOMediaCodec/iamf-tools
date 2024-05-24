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

#ifndef CLI_ADM_TO_USER_METADATA_IAMF_TEST_VECTOR_METADATA_HANDLER_H_
#define CLI_ADM_TO_USER_METADATA_IAMF_TEST_VECTOR_METADATA_HANDLER_H_

#include "absl/strings/string_view.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Populates a `TestVectorMetadataHandler` proto.
 *
 * \param file_name_prefix File name prefix.
 * \param test_vector_metadata Data to populate.
 */
void TestVectorMetadataHandler(
    absl::string_view file_name_prefix,
    iamf_tools_cli_proto::TestVectorMetadata& test_vector_metadata);

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_IAMF_TEST_VECTOR_METADATA_HANDLER_H_
