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

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "fuzztest/fuzztest.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/include/iamf_tools/iamf_encoder_factory.h"
#include "src/google/protobuf/io/zero_copy_stream_impl.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::iamf_tools_cli_proto::UserMetadata;

constexpr absl::string_view kTestdataPath = "iamf/cli/testdata/";

std::vector<std::tuple<UserMetadata>> LoadTextprotoSeedsFromDirectory(
    const std::string& directory_path) {
  std::vector<std::tuple<UserMetadata>> seeds;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(directory_path)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".textproto") {
      continue;
    }
    std::ifstream ifs(entry.path());
    if (!ifs.good()) {
      continue;
    }

    google::protobuf::io::IstreamInputStream input_stream(&ifs);
    UserMetadata msg;
    if (google::protobuf::TextFormat::Parse(&input_stream, &msg)) {
      seeds.push_back(std::make_tuple(msg));
    }
  }

  return seeds;
}

void CreateIamfEncoderNeverCrashes(const UserMetadata& user_metadata) {
  std::string user_metadata_string;
  user_metadata.SerializeToString(&user_metadata_string);
  auto iamf_encoder =
      api::IamfEncoderFactory::CreateIamfEncoder(user_metadata_string);
}

FUZZ_TEST(SeededWithTestSuite, CreateIamfEncoderNeverCrashes)
    .WithSeeds(LoadTextprotoSeedsFromDirectory(GetRunfilesPath(kTestdataPath)));

}  // namespace
}  // namespace iamf_tools
