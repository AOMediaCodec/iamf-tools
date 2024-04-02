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

#include "iamf/cli/adm_to_user_metadata/iamf/test_vector_metadata_handler.h"

#include <string>

#include "gtest/gtest.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

TEST(TestVectorMetadataHandler, GeneratesValidTestVectorMetadata) {
  iamf_tools_cli_proto::TestVectorMetadata test_vector_metadata;

  const std::string kFilePrefix = "prefix";
  TestVectorMetadataHandler(kFilePrefix, test_vector_metadata);

  EXPECT_TRUE(test_vector_metadata.is_valid());
  EXPECT_EQ(test_vector_metadata.file_name_prefix(), kFilePrefix);
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
