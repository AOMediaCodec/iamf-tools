#include "iamf/cli/encoder_main_lib.h"

#include <cstdint>
#include <filesystem>
#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

void AddIaSequenceHeader(iamf_tools_cli_proto::UserMetadata& user_metadata) {
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        primary_profile: PROFILE_VERSION_SIMPLE
        additional_profile: PROFILE_VERSION_SIMPLE
      )pb",
      user_metadata.add_ia_sequence_header_metadata()));
}

TEST(EncoderMainLibTest, EmptyUserMetadataTestMainFails) {
  EXPECT_FALSE(TestMain(iamf_tools_cli_proto::UserMetadata(), "", "", "").ok());
}

TEST(EncoderMainLibTest, IaSequenceHeaderOnlySucceeds) {
  // Populate the user metadata with only an IA Sequence Header, leaving
  // everything else empty, which would still succeed.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  EXPECT_TRUE(TestMain(user_metadata, "", "",
                       std::filesystem::temp_directory_path().string())
                  .ok());
}

TEST(EncoderMainLibTest, ConfigureOutputWavFileBitDepthOverrideSucceeds) {
  // Initialize prerequisites.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
  // Configure a reasonable bit-depth to output to.
  user_metadata.mutable_test_vector_metadata()
      ->set_output_wav_file_bit_depth_override(16);

  EXPECT_TRUE(TestMain(user_metadata, "", "",
                       std::filesystem::temp_directory_path().string())
                  .ok());
}

TEST(EncoderMainLibTest, ConfigureOutputWavFileBitDepthOverrideTooHighFails) {
  // Initialize prerequisites.
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
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

  // Setting a filename prefix makes the function output a .iamf file.
  user_metadata.mutable_test_vector_metadata()->set_file_name_prefix("empty");

  const auto output_iamf_directory = std::filesystem::temp_directory_path();

  EXPECT_TRUE(
      TestMain(user_metadata, "", "", output_iamf_directory.string()).ok());

  EXPECT_TRUE(std::filesystem::exists(output_iamf_directory / "empty.iamf"));
}

TEST(EncoderMainLibTest, CreatesAndWritesToOutputIamfDirectory) {
  iamf_tools_cli_proto::UserMetadata user_metadata;
  AddIaSequenceHeader(user_metadata);
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

  EXPECT_TRUE(
      TestMain(user_metadata, "", "", output_iamf_directory.string()).ok());

  EXPECT_TRUE(std::filesystem::exists(output_iamf_directory / "empty.iamf"));
}
// TODO(b/308385831): Add more tests.

}  // namespace
}  // namespace iamf_tools
