/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/iamf_components.h"

#include <cstdint>
#include <filesystem>
#include <optional>

#include "gtest/gtest.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

constexpr bool kValidateLoudness = true;

TEST(IamfComponentsTest, CreateMixPresentationFinalizerReturnsNonNull) {
  EXPECT_NE(CreateMixPresentationFinalizer(
                {}, /*file_name_prefix=*/"",
                /*output_wav_file_bit_depth_override=*/std::nullopt,
                kValidateLoudness),
            nullptr);
}

TEST(IamfComponentsTest,
     CreateObuSequencersReturnsNonNullAndNonZeroObuSequencers) {
  auto obu_sequencers =
      CreateObuSequencers({}, std::filesystem::temp_directory_path(), false);

  EXPECT_FALSE(obu_sequencers.empty());
  for (auto& obu_sequencer : obu_sequencers) {
    EXPECT_NE(obu_sequencer, nullptr);
  }
}

TEST(IamfComponentsTest, CanBeConfiguredWithFixedSizeLebGenerator) {
  iamf_tools_cli_proto::UserMetadata user_metadata;

  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_FIXED_SIZE fixed_size: 5
                )pb",
                user_metadata.mutable_test_vector_metadata()
                    ->mutable_leb_generator()),
            true);

  auto obu_sequencers = CreateObuSequencers(
      user_metadata, std::filesystem::temp_directory_path(), false);

  EXPECT_FALSE(obu_sequencers.empty());
  for (auto& obu_sequencer : obu_sequencers) {
    EXPECT_NE(obu_sequencer, nullptr);
  }
}

TEST(IamfComponentsTest, CanBeConfiguredWithOutputWavFileBitDepthOverride) {
  const uint8_t kOutputWavFileBitDepthOverride = 16;
  EXPECT_NE(CreateMixPresentationFinalizer(
                {}, "", kOutputWavFileBitDepthOverride, kValidateLoudness),
            nullptr);
}

TEST(IamfComponentsTest, ReturnsEmptyListWhenLebGeneratorIsInvalid) {
  iamf_tools_cli_proto::UserMetadata user_metadata;

  ASSERT_EQ(google::protobuf::TextFormat::ParseFromString(
                R"pb(
                  mode: GENERATE_LEB_FIXED_SIZE fixed_size: 0
                )pb",
                user_metadata.mutable_test_vector_metadata()
                    ->mutable_leb_generator()),
            true);

  auto obu_sequencers = CreateObuSequencers(
      user_metadata, std::filesystem::temp_directory_path(), false);

  EXPECT_TRUE(obu_sequencers.empty());
}

}  // namespace
}  // namespace iamf_tools
