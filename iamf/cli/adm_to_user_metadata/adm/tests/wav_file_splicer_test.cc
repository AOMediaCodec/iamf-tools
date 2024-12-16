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

#include "iamf/cli/adm_to_user_metadata/adm/wav_file_splicer.h"

#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

using enum iamf_tools::ProfileVersion;

constexpr int32_t kImportanceThreshold = 10;

constexpr absl::string_view kAdmBwfWithOneStereoObject(
    "RIFF"
    "\xb8\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels.
    "\x01\x00\x00\x00"  // Samples per second.
    "\x04\x00\x00\x00"  // Bytes per second = [number_of_channels *
                        // ceil(bits_per_sample / 8) * sample_per_second].
    "\x04\x00"          // Block align = [number_of_channels * bits_per_sample].
    "\x10\x00"          // Bits per sample.
    "data"
    "\x08\x00\x00\x00"  // Size of `data` chunk.
    "\x01\x23"          // Sample[0] for channel 0.
    "\x45\x67"          // Sample[0] for channel 1.
    "\x89\xab"          // Sample[1] for channel 0.
    "\xcd\xef"          // Sample[1] for channel 1.80 decimal to hexadecimal
    "axml"
    "\x7c\x00\x00\x00"  // Size of `axml` chunk.
    "<topLevel><audioObject><audioTrackUIDRef>L</"
    "audioTrackUIDRef><audioTrackUIDRef>R</audioTrackUIDRef></"
    "audioObject></topLevel>",
    184);

constexpr absl::string_view
    kAdmBwfWithDataWithPlatformDependentControlCharacters(
        "RIFF"
        "\xb8\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
        "WAVE"
        "fmt "
        "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
        "\x01\x00"          // Format tag.
        "\x02\x00"          // Number of channels.
        "\x01\x00\x00\x00"  // Samples per second.
        "\x04\x00\x00\x00"  // Bytes per second = [number_of_channels *
                            // ceil(bits_per_sample / 8) * sample_per_second].
        "\x04\x00"  // Block align = [number_of_channels * bits_per_sample].
        "\x10\x00"  // Bits per sample.
        "data"
        "\x08\x00\x00\x00"  // Size of `data` chunk.
        "\n\n"              // Sample[0] for channel 0.
        "\r\n"              // Sample[0] for channel 1.
        "\x1a\r"            // Sample[1] for channel 0.
        "\r\r"              // Sample[1] for channel 1.
        "axml"
        "\x7c\x00\x00\x00"  // Size of `axml` chunk.
        "<topLevel><audioObject><audioTrackUIDRef>L</"
        "audioTrackUIDRef><audioTrackUIDRef>R</audioTrackUIDRef></"
        "audioObject></topLevel>",
        184);

// When there is one object the output wav file is the same as the input wav
// file with sizes adjusted and any extra chunks removed (e.g. "axml").
constexpr absl::string_view kExpectedOutputForStereoObject(
    "RIFF"
    "\x2c\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels.
    "\x01\x00\x00\x00"  // Samples per second.
    "\x04\x00\x00\x00"  // Bytes per second = [number_of_channels *
                        // ceil(bits_per_sample / 8) * sample_per_second].
    "\x04\x00"          // Block align = [number_of_channels * bits_per_sample].
    "\x10\x00"          // Bits per sample.
    "data"
    "\x08\x00\x00\x00"  // Size of `data` chunk.
    "\x01\x23"          // Sample[0] for channel 0.
    "\x45\x67"          // Sample[0] for channel 1.
    "\x89\xab"          // Sample[1] for channel 0.
    "\xcd\xef",         // Sample[1] for channel 1.
    52);

constexpr absl::string_view kInvalidWavFileWithInconsistentDataChunkSize(
    "RIFF"
    "\xb8\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels.
    "\x01\x00\x00\x00"  // Samples per second.
    "\x04\x00\x00\x00"  // Bytes per second.
    "\x04\x00"          // Block align.
    "\x10\x00"          // Bits per sample.
    "axml"
    "\x7c\x00\x00\x00"  // Size of `axml` chunk.
    "<topLevel><audioObject><audioTrackUIDRef>L</"
    "audioTrackUIDRef><audioTrackUIDRef>R</audioTrackUIDRef></"
    "audioObject></topLevel>"
    "data"
    "\x0a\x00\x00\x00"  // Size of `data` chunk. Note that it is inconsistent -
                        // it calls for 10 bytes, but there are 8 bytes of audio
                        // data below.
    "\x01\x23"          // Sample[0] for channel 0.
    "\x45\x67"          // Sample[0] for channel 1.
    "\x89\xab"          // Sample[1] for channel 0.
    "\xcd\xef",         // Sample[1] for channel 0.
    184);

constexpr absl::string_view kAdmBwfWithOneStereoAndOneMonoObject(
    "RIFF"
    "\xf5\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x03\x00"          // Number of channels.
    "\x01\x00\x00\x00"  // Sample per second
    "\x06\x00\x00\x00"  // Bytes per second = [number_of_channels *
                        // ceil(bits_per_sample / 8) * sample_per_second].
    "\x06\x00"          // Block align = [number of channels * bits per sample]
    "\x10\x00"          // Bits per sample.
    "data"
    "\x0c\x00\x00\x00"  // Size of `data` chunk.
    "\x01\x23"          // Sample[0] for object[0] L.
    "\x45\x67"          // Sample[0] for object[0] R.
    "\xaa\xbb"          // Sample[0] for object[1] M.
    "\x89\xab"          // Sample[1] for object[0] L.
    "\xcd\xef"          // Sample[1] for object[0] R.
    "\xcc\xdd"          // Sample[1] for object[1] M.
    "axml"
    "\xbd\x00\x00\x00"  // Size of `axml` chunk.
    "<topLevel>"
    "<audioObject>"
    "<audioTrackUIDRef>L</audioTrackUIDRef>"
    "<audioTrackUIDRef>R</audioTrackUIDRef>"
    "</audioObject>"
    "<audioObject>"
    "<audioTrackUIDRef>M</audioTrackUIDRef>"
    "</audioObject>"
    "</topLevel>",
    253);

// When there are two objects each will correspond to an output wav file. The
// number of channels of each output wav file will be the same as the number of
// audio tracks in the corresponding ADM object. Some fields (i.e. "number of
// channels", "bytes per second", "block align", and the sizes of chunks) must
// be recalculated to maintain self-consistency. Extra chunks will be removed
// (e.g. "axml").
constexpr absl::string_view kExpectedOutputForMonoObject(
    "RIFF"
    "\x28\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x01\x00"          // Number of channels.
    "\x01\x00\x00\x00"  // Samples per second.
    "\x02\x00\x00\x00"  // Bytes per second = [number_of_channels *
                        // ceil(bits_per_sample / 8) * sample_per_second].
    "\x02\x00"          // Block align = [number of channels * bits per sample]
    "\x10\x00"          // Bits per sample.
    "data"
    "\x04\x00\x00\x00"  // Size of `data` chunk.
    "\xaa\xbb"          // Sample[0] for object[1] M.
    "\xcc\xdd",         // Sample[1] for object[1] M.
    48);

void ValidateFileContents(std::filesystem::path file_path,
                          absl::string_view expected_contents) {
  // Read back in the output wav file and compare it to the expected output.
  std::vector<uint8_t> actual_contents;
  EXPECT_THAT(ReadFileToBytes(file_path, actual_contents), IsOk());
  absl::string_view actual_contents_view(
      reinterpret_cast<const char*>(actual_contents.data()),
      actual_contents.size());

  EXPECT_EQ(actual_contents_view, expected_contents);
}

TEST(SpliceWavFilesFromAdm, CreatesWavFiles) {
  std::istringstream ss((std::string(kAdmBwfWithOneStereoObject)));
  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());
  const std::string directory = GetAndCreateOutputDirectory("");
  int lfe_count = 0;

  EXPECT_THAT(SpliceWavFilesFromAdm(directory, "prefix", kIamfBaseProfile,
                                    *reader, ss, lfe_count),
              IsOk());
  EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(directory) /
                                      "prefix_converted1.wav"));
}

TEST(SpliceWavFilesFromAdm,
     SucceedsWhenDataHasPlatformDependentControlCharacters) {
  std::istringstream ss(
      (std::string(kAdmBwfWithDataWithPlatformDependentControlCharacters)));

  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);

  ASSERT_THAT(reader, IsOk());
}

TEST(SpliceWavFilesFromAdm,
     InvalidAndDoesNotCreateWavFilehenDataChunkIsInconsistent) {
  std::istringstream ss(
      (std::string(kInvalidWavFileWithInconsistentDataChunkSize)));
  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());
  const std::string directory = GetAndCreateOutputDirectory("");
  int lfe_count = 0;

  EXPECT_FALSE(SpliceWavFilesFromAdm(::testing::TempDir(), "prefix",
                                     kIamfBaseProfile, *reader, ss, lfe_count)
                   .ok());

  EXPECT_TRUE(std::filesystem::is_empty(directory));
}

TEST(SpliceWavFilesFromAdm, StripsAxmlChunkAndUpdatesChunkSizes) {
  std::istringstream ss((std::string(kAdmBwfWithOneStereoObject)));
  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());
  const std::string directory = GetAndCreateOutputDirectory("");
  int lfe_count = 0;

  ASSERT_THAT(SpliceWavFilesFromAdm(directory, "prefix", kIamfBaseProfile,
                                    *reader, ss, lfe_count),
              IsOk());

  ValidateFileContents(
      std::filesystem::path(directory) / "prefix_converted1.wav",
      kExpectedOutputForStereoObject);
}

TEST(SpliceWavFilesFromAdm, OutputsOneWavFilePerObject) {
  std::istringstream ss((std::string(kAdmBwfWithOneStereoAndOneMonoObject)));
  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());
  const std::string directory = GetAndCreateOutputDirectory("");
  int lfe_count = 0;

  EXPECT_THAT(SpliceWavFilesFromAdm(directory, "prefix", kIamfBaseProfile,
                                    *reader, ss, lfe_count),
              IsOk());

  ValidateFileContents(
      std::filesystem::path(directory) / ("prefix_converted1.wav"),
      kExpectedOutputForStereoObject);

  ValidateFileContents(
      std::filesystem::path(directory) / ("prefix_converted2.wav"),
      kExpectedOutputForMonoObject);
}
}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
