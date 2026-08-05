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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/cli/adm_to_user_metadata/adm/panner.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/ia_sequence_header.h"

// TODO(b/384048095): Add better tests for spliced wav files with LFE channels.

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

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

  EXPECT_THAT(SpliceWavFilesFromAdm(::testing::TempDir(), "prefix",
                                    kIamfBaseProfile, *reader, ss, lfe_count),
              Not(IsOk()));

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

// Verifies that ADM timecode quantization rounding drift is safely absorbed to
// produce sample-exact output.
TEST(SpliceWavFilesFromAdm, AbsorbsOneSampleRoundingDriftInObjectsMode) {
  constexpr size_t kNumSamples = 6005;  // 0.125104166... s at 48 kHz.
  constexpr size_t kBytesPerSample =
      3;  // `kAdmFileTypeDolby` files are 24-bit.
  constexpr size_t kNumInputChannels = 1;  // One audio object.
  constexpr absl::string_view kDriftAxml =
      R"xml(<?xml version="1.0" encoding="UTF-8"?>
<ebuCoreMain xmlns="urn:ebu:metadata-schema:ebuCore_2016"><coreMetadata><format><audioFormatExtended version="ITU-R_BS.2076-2">
<audioProgramme audioProgrammeID="APR_1001" audioProgrammeName="drift_fixture" start="00:00:00.00000" end="00:00:00.12510"><audioContentIDRef>ACO_1001</audioContentIDRef><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioProgramme>
<audioContent audioContentID="ACO_1001" audioContentName="All"><audioObjectIDRef>AO_1001</audioObjectIDRef></audioContent>
<audioObject audioObjectID="AO_1001" audioObjectName="Obj1" start="00:00:00.00000" duration="00:00:00.12510"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef><audioTrackUIDRef>ATU_00000001</audioTrackUIDRef></audioObject>
<audioPackFormat audioPackFormatID="AP_00031001" audioPackFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioChannelFormatIDRef>AC_00031001</audioChannelFormatIDRef></audioPackFormat>
<audioChannelFormat audioChannelFormatID="AC_00031001" audioChannelFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioBlockFormat audioBlockFormatID="AB_00031001_00000001" rtime="00:00:00.00000" duration="00:00:00.12510"><cartesian>1</cartesian><position coordinate="X">-0.000000</position><position coordinate="Y">1.000000</position><position coordinate="Z">0.000000</position></audioBlockFormat></audioChannelFormat>
<audioStreamFormat audioStreamFormatID="AS_00031001" audioStreamFormatName="PCM_AC_00031001" formatLabel="0001" formatDefinition="PCM"><audioChannelFormatIDRef>AC_00031001</audioChannelFormatIDRef></audioStreamFormat>
<audioTrackFormat audioTrackFormatID="AT_00031001_01" audioTrackFormatName="PCM_AC_00031001" formatLabel="0001" formatDefinition="PCM"><audioStreamFormatIDRef>AS_00031001</audioStreamFormatIDRef></audioTrackFormat>
<audioTrackUID UID="ATU_00000001" sampleRate="48000" bitDepth="24"><audioTrackFormatIDRef>AT_00031001_01</audioTrackFormatIDRef><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioTrackUID>
</audioFormatExtended></format></coreMetadata></ebuCoreMain>
)xml";

  // Assemble the BWF: `fmt ` (mono / 48 kHz / 24-bit), `dbmd` (presence
  // routes the file down the objects-to-3OA path), `axml`, and a
  // `data` chunk of exactly `kNumSamples` samples.
  const auto chunk = [](absl::string_view fourcc, absl::string_view payload) {
    const uint32_t size = static_cast<uint32_t>(payload.size());
    std::string out(fourcc);
    for (int shift : {0, 8, 16, 24}) {
      out.push_back(static_cast<char>((size >> shift) & 0xff));
    }
    absl::StrAppend(&out, payload);
    if (payload.size() % 2 == 1) out.push_back('\0');
    return out;
  };
  constexpr absl::string_view kFmtPayload(
      "\x01\x00"          // Format tag.
      "\x01\x00"          // Number of channels.
      "\x80\xbb\x00\x00"  // Samples per second (48000).
      "\x80\x32\x02\x00"  // Bytes per second (144000).
      "\x03\x00"          // Block align.
      "\x18\x00",         // Bits per sample (24).
      16);
  const std::string wav_bytes = absl::StrCat(
      "RIFF????WAVE",  // The reader does not depend on the RIFF size field.
      chunk("fmt ", kFmtPayload),
      chunk("dbmd", absl::string_view("\x00\x00\x00\x00", 4)),
      chunk("axml", kDriftAxml),
      chunk("data",
            std::string(kNumSamples * kBytesPerSample * kNumInputChannels,
                        '\0')));

  std::istringstream ss(wav_bytes);
  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());
  const std::string directory = GetAndCreateOutputDirectory("");
  int lfe_count = 0;

  EXPECT_THAT(SpliceWavFilesFromAdm(directory, "prefix", kIamfBaseProfile,
                                    *reader, ss, lfe_count),
              IsOk());

  // The panned 3OA output must exist and be sample-exact: `kNumSamples`
  // samples across `kOutputWavChannels` channels, with no samples dropped to
  // rounding.
  const auto output_path =
      std::filesystem::path(directory) / "prefix_converted1.wav";
  const auto wav_reader = CreateWavReaderExpectOk(output_path.string());
  EXPECT_EQ(wav_reader.num_channels(), kOutputWavChannels);
  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamples * kOutputWavChannels);
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
