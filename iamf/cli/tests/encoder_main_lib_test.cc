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
#include "iamf/cli/encoder_main_lib.h"

#include <cstdint>
#include <filesystem>
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

void AddIaSequenceHeader(iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        primary_profile: PROFILE_VERSION_SIMPLE
        additional_profile: PROFILE_VERSION_SIMPLE
      )pb",
      user_metadata.add_ia_sequence_header_metadata()));
}

void AddCodecConfig(iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        codec_config_id: 200
        codec_config {
          codec_id: CODEC_ID_LPCM
          num_samples_per_frame: 64
          audio_roll_distance: 0
          decoder_config_lpcm {
            sample_format_flags: LPCM_LITTLE_ENDIAN
            sample_size: 16
            sample_rate: 48000
          }
        }
      )pb",
      user_metadata.add_codec_config_metadata()));
}

TEST(EncoderMainLibTest, EmptyUserMetadataTestMainFails) {
  EXPECT_FALSE(TestMain(iamf_tools_cli_proto::UserMetadata(), "", "", "").ok());
}

TEST(EncoderMainLibTest, IaSequenceHeaderOnly) {
  // Populate the user metadata with only an IA Sequence Header, leaving
  // everything else empty. This will fail if
  // `partition_mix_gain_parameter_blocks` is left true (the default value).
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  EXPECT_FALSE(TestMain(user_metadata, "", "",
                        std::filesystem::temp_directory_path().string())
                   .ok());

  // After setting `partition_mix_gain_parameter_blocks` to false, `TestMain()`
  // will succeed.
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);
  EXPECT_THAT(TestMain(user_metadata, "", "",
                       std::filesystem::temp_directory_path().string()),
              IsOk());
}

TEST(EncoderMainLibTest, IaSequenceHeaderAndCodecConfigSucceeds) {
  // Populate the user metadata with an IA Sequence Header AND a Codec Config,
  // leaving everything else empty. This will succeed.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  AddCodecConfig(user_metadata);
  EXPECT_THAT(TestMain(user_metadata, "", "",
                       std::filesystem::temp_directory_path().string()),
              IsOk());
}

TEST(EncoderMainLibTest, ConfigureOutputWavFileBitDepthOverrideSucceeds) {
  // Initialize prerequisites.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  // Configure a reasonable bit-depth to output to.
  user_metadata.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(16);

  EXPECT_THAT(TestMain(user_metadata, "", "",
                       std::filesystem::temp_directory_path().string()),
              IsOk());
}

TEST(EncoderMainLibTest, ConfigureOutputWavFileBitDepthOverrideTooHighFails) {
  // Initialize prerequisites.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  const uint32_t kBitDepthTooHigh = 256;
  user_metadata.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(kBitDepthTooHigh);

  EXPECT_FALSE(TestMain(user_metadata, "", "",
                        std::filesystem::temp_directory_path().string())
                   .ok());
}

TEST(EncoderMainLibTest, SettingPrefixOutputsFile) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  // Setting a filename prefix makes the function output a .iamf file.
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix("empty");

  const auto output_iamf_directory = std::filesystem::temp_directory_path();

  EXPECT_THAT(TestMain(user_metadata, "", "", output_iamf_directory.string()),
              IsOk());

  EXPECT_TRUE(std::filesystem::exists(output_iamf_directory / "empty.iamf"));
}

TEST(EncoderMainLibTest, CreatesAndWritesToOutputIamfDirectory) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  user_metadata.mutable_test_vector_metadata()
      ->set_partition_mix_gain_parameter_blocks(false);

  // Setting a filename prefix makes the function output a .iamf file.
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix("empty");

  // The encoder will create and write the file based on a (nested)
  // `output_iamf_directory` argument.
  const auto test_directory_root =
      std::filesystem::temp_directory_path() /
      std::filesystem::path("encoder_main_lib_test");

  // Clean up any previously created file and directories.
  std::filesystem::remove_all(test_directory_root.c_str());
  ASSERT_FALSE(std::filesystem::exists(test_directory_root));

  const auto output_iamf_directory =
      test_directory_root / std::filesystem::path("EncoderMainLibTest") /
      std::filesystem::path("CreatesAndWritesToOutputIamfDirectory");

  EXPECT_THAT(TestMain(user_metadata, "", "", output_iamf_directory.string()),
              IsOk());

  EXPECT_TRUE(std::filesystem::exists(output_iamf_directory / "empty.iamf"));
}
// TODO(b/308385831): Add more tests.

}  // namespace
}  // namespace iamf_tools
