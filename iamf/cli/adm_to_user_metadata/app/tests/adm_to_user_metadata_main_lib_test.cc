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

#include "iamf/cli/adm_to_user_metadata/app/adm_to_user_metadata_main_lib.h"

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/test_vector_metadata.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

using enum iamf_tools::ProfileVersion;

constexpr int32_t kImportanceThreshold = 10;
constexpr int32_t kMaxFrameDurationMs = 10;
constexpr absl::string_view kFilePrefix = "file_prefix";

constexpr absl::string_view kAdmWithOneStereoObject(
    "RIFF"
    "\x8b\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels
    "\x80\xbb\x00\x00"  // Sample Per Second
    "\x04\x00\x00\x00"  // Bytes per second.
    "\x10\x00"          // Block align.
    "\x10\x00"          // Bits per sample.
    "data"
    "\x08\x00\x00\x00"  // Size of `data` chunk.
    "\x01\x23"          // Sample[0] for channel 0.
    "\x45\x67"          // Sample[0] for channel 1.
    "\x89\xab"          // Sample[1] for channel 0.
    "\xcd\xef"          // Sample[1] for channel 1.
    "axml"
    "\x53\x00\x00\x00"  // Size of `axml` chunk.
    "<audioObject>"
    "<audioPackFormatIDRef>AP_00010001</audioPackFormatIDRef>"
    "</audioObject>",
    143);

constexpr absl::string_view kInvalidAdmWithoutDataChunk(
    "RIFF"
    "\x7b\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels
    "\x80\xbb\x00\x00"  // Sample Per Second
    "\x04\x00\x00\x00"  // Bytes per second.
    "\x10\x00"          // Block align.
    "\x10\x00"          // Bits per sample.
    "axml"
    "\x53\x00\x00\x00"  // Size of `axml` chunk.
    "<audioObject>"
    "<audioPackFormatIDRef>AP_00010001</audioPackFormatIDRef>"
    "</audioObject>",
    127);

constexpr absl::string_view kInvalidAdmWithoutAxmlChunk(
    "RIFF"
    "\x30\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels
    "\x80\xbb\x00\x00"  // Sample Per Second
    "\x04\x00\x00\x00"  // Bytes per second.
    "\x10\x00"          // Block align.
    "\x10\x00"          // Bits per sample.
    "data"
    "\x08\x00\x00\x00"  // Size of `data` chunk.
    "\x01\x23"          // Sample[0] for channel 0.
    "\x45\x67"          // Sample[0] for channel 1.
    "\x89\xab"          // Sample[1] for channel 0.
    "\xcd\xef",         // Sample[1] for channel 1.
    52);

iamf_tools_cli_proto::UserMetadata
GenerateUserMetadataAndSpliceWavFilesExpectOk(
    absl::string_view input_adm, const std::filesystem::path& output_path) {
  std::istringstream ss((std::string(input_adm)));
  const auto& user_metadata = GenerateUserMetadataAndSpliceWavFiles(
      kFilePrefix, kMaxFrameDurationMs, kImportanceThreshold, output_path, ss,
      kIamfBaseProfile);

  EXPECT_THAT(user_metadata, IsOk());

  return *user_metadata;
}

TEST(GenerateUserMetadataAndSpliceWavFiles,
     WavFileNameAndAudioFrameMetadataAreConsistent) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));
  const auto& user_metadata = GenerateUserMetadataAndSpliceWavFilesExpectOk(
      kAdmWithOneStereoObject, output_path);

  const std::filesystem::path expected_wav_path =
      output_path / user_metadata.audio_frame_metadata(0).wav_filename();
  EXPECT_TRUE(std::filesystem::exists(expected_wav_path));
}

TEST(GenerateUserMetadataAndSpliceWavFiles,
     SetsTestVectorMetadataFileNamePrefix) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));
  const auto& user_metadata = GenerateUserMetadataAndSpliceWavFilesExpectOk(
      kAdmWithOneStereoObject, output_path);

  EXPECT_EQ(user_metadata.test_vector_metadata().file_name_prefix(),
            kFilePrefix);
}

TEST(GenerateUserMetadataAndSpliceWavFiles, CreatesDescriptorObuMetadata) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));
  const auto& user_metadata = GenerateUserMetadataAndSpliceWavFilesExpectOk(
      kAdmWithOneStereoObject, output_path);

  EXPECT_EQ(user_metadata.ia_sequence_header_metadata().size(), 1);
  EXPECT_EQ(user_metadata.codec_config_metadata().size(), 1);
  EXPECT_EQ(user_metadata.audio_element_metadata().size(), 1);
  EXPECT_EQ(user_metadata.mix_presentation_metadata().size(), 1);
  EXPECT_EQ(user_metadata.audio_frame_metadata().size(), 1);
}

TEST(GenerateUserMetadataAndSpliceWavFiles, InvalidWithoutAxmlChunk) {
  std::istringstream ss((std::string(kInvalidAdmWithoutAxmlChunk)));
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_FALSE(GenerateUserMetadataAndSpliceWavFiles(
                   kFilePrefix, kMaxFrameDurationMs, kImportanceThreshold,
                   output_path, ss, kIamfBaseProfile)
                   .ok());
  EXPECT_TRUE(std::filesystem::is_empty(output_path));
}

TEST(GenerateUserMetadataAndSpliceWavFiles, InvalidWithoutDataChunk) {
  std::istringstream ss((std::string(kInvalidAdmWithoutDataChunk)));
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_FALSE(GenerateUserMetadataAndSpliceWavFiles(
                   kFilePrefix, kMaxFrameDurationMs, kImportanceThreshold,
                   output_path, ss, kIamfBaseProfile)
                   .ok());
  EXPECT_TRUE(std::filesystem::is_empty(output_path));
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
