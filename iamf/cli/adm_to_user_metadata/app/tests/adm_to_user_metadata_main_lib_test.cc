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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
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
using ::absl_testing::StatusIs;
using ::testing::Not;

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

  EXPECT_THAT(GenerateUserMetadataAndSpliceWavFiles(
                  kFilePrefix, kMaxFrameDurationMs, kImportanceThreshold,
                  output_path, ss, kIamfBaseProfile),
              Not(IsOk()));
  EXPECT_TRUE(std::filesystem::is_empty(output_path));
}

TEST(GenerateUserMetadataAndSpliceWavFiles, InvalidWithoutDataChunk) {
  std::istringstream ss((std::string(kInvalidAdmWithoutDataChunk)));
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_THAT(GenerateUserMetadataAndSpliceWavFiles(
                  kFilePrefix, kMaxFrameDurationMs, kImportanceThreshold,
                  output_path, ss, kIamfBaseProfile),
              Not(IsOk()));
  EXPECT_TRUE(std::filesystem::is_empty(output_path));
}

// A Dolby ADM BWF routes `GenerateUserMetadataAndSpliceWavFiles` through
// `ModifyAdmToPanObjectsTo3OAAndSeparateLfe`, which rewrites the first
// `audioPackFormatIDRef` of the first audioObject. `ParseXmlToAdm` returns OK
// after dropping audioObjects that fall below the importance threshold or that
// it could not validate, so that list can be empty, or its first entry can
// carry no `audioPackFormatIDRef`, on a file that parsed successfully.

constexpr size_t kNumSamplesPerChannel = 6005;  // 0.125104166... s at 48 kHz.
constexpr size_t kBytesPerSample = 3;           // Dolby ADM files are 24-bit.
constexpr uint32_t kSamplesPerSecond = 48000;

void AppendLittleEndian(uint32_t value, int num_bytes, std::string& output) {
  for (int i = 0; i < num_bytes; ++i) {
    output.push_back(static_cast<char>((value >> (8 * i)) & 0xff));
  }
}

// Assembles a RIFF chunk: the four-character ID, the little-endian payload
// size, the payload, and a pad byte when the payload is odd-sized.
std::string RiffChunk(absl::string_view fourcc, absl::string_view payload) {
  std::string chunk(fourcc);
  AppendLittleEndian(static_cast<uint32_t>(payload.size()), 4, chunk);
  absl::StrAppend(&chunk, payload);
  if (payload.size() % 2 == 1) {
    chunk.push_back('\0');
  }
  return chunk;
}

// A `fmt ` payload for 48 kHz / 24-bit PCM, which is what `Bw64Reader` accepts
// for a Dolby ADM file.
std::string DolbyFmtPayload(uint32_t num_channels) {
  const uint32_t block_align =
      num_channels * static_cast<uint32_t>(kBytesPerSample);
  std::string payload;
  AppendLittleEndian(1, 2, payload);  // Format tag: PCM.
  AppendLittleEndian(num_channels, 2, payload);
  AppendLittleEndian(kSamplesPerSecond, 4, payload);
  AppendLittleEndian(kSamplesPerSecond * block_align, 4, payload);
  AppendLittleEndian(block_align, 2, payload);
  AppendLittleEndian(8 * static_cast<uint32_t>(kBytesPerSample), 2, payload);
  return payload;
}

// Assembles a Dolby ADM BWF. The `dbmd` chunk is what marks the file as Dolby
// ADM and routes it down the objects-to-3OA path.
std::string DolbyAdmBwf(uint32_t num_channels, absl::string_view axml) {
  return absl::StrCat(
      "RIFF????WAVE",  // The reader does not depend on the RIFF size field.
      RiffChunk("fmt ", DolbyFmtPayload(num_channels)),
      RiffChunk("dbmd", absl::string_view("\x00\x00\x00\x00", 4)),
      RiffChunk("axml", axml),
      RiffChunk("data", std::string(kNumSamplesPerChannel * kBytesPerSample *
                                        num_channels,
                                    '\0')));
}

// One mono object. `importance` is omitted, so it defaults to the highest
// value and the object survives `kImportanceThreshold`.
constexpr absl::string_view kAxmlWithOneObject =
    R"xml(<?xml version="1.0" encoding="UTF-8"?>
<ebuCoreMain xmlns="urn:ebu:metadata-schema:ebuCore_2016"><coreMetadata><format><audioFormatExtended version="ITU-R_BS.2076-2">
<audioProgramme audioProgrammeID="APR_1001" audioProgrammeName="fixture" start="00:00:00.00000" end="00:00:00.12510"><audioContentIDRef>ACO_1001</audioContentIDRef><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioProgramme>
<audioContent audioContentID="ACO_1001" audioContentName="All"><audioObjectIDRef>AO_1001</audioObjectIDRef></audioContent>
<audioObject audioObjectID="AO_1001" audioObjectName="Obj1" start="00:00:00.00000" duration="00:00:00.12510"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef><audioTrackUIDRef>ATU_00000001</audioTrackUIDRef></audioObject>
<audioPackFormat audioPackFormatID="AP_00031001" audioPackFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioChannelFormatIDRef>AC_00031001</audioChannelFormatIDRef></audioPackFormat>
<audioChannelFormat audioChannelFormatID="AC_00031001" audioChannelFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioBlockFormat audioBlockFormatID="AB_00031001_00000001" rtime="00:00:00.00000" duration="00:00:00.12510"><cartesian>1</cartesian><position coordinate="X">-0.000000</position><position coordinate="Y">1.000000</position><position coordinate="Z">0.000000</position></audioBlockFormat></audioChannelFormat>
<audioTrackUID UID="ATU_00000001" sampleRate="48000" bitDepth="24"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioTrackUID>
</audioFormatExtended></format></coreMetadata></ebuCoreMain>
)xml";

// The same file, but the only audioObject is below `kImportanceThreshold`, so
// `ParseXmlToAdm` drops it and returns an ADM with no audioObjects.
constexpr absl::string_view kAxmlWithOneLowImportanceObject =
    R"xml(<?xml version="1.0" encoding="UTF-8"?>
<ebuCoreMain xmlns="urn:ebu:metadata-schema:ebuCore_2016"><coreMetadata><format><audioFormatExtended version="ITU-R_BS.2076-2">
<audioProgramme audioProgrammeID="APR_1001" audioProgrammeName="fixture" start="00:00:00.00000" end="00:00:00.12510"><audioContentIDRef>ACO_1001</audioContentIDRef><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioProgramme>
<audioContent audioContentID="ACO_1001" audioContentName="All"><audioObjectIDRef>AO_1001</audioObjectIDRef></audioContent>
<audioObject audioObjectID="AO_1001" audioObjectName="Obj1" importance="0" start="00:00:00.00000" duration="00:00:00.12510"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef><audioTrackUIDRef>ATU_00000001</audioTrackUIDRef></audioObject>
<audioPackFormat audioPackFormatID="AP_00031001" audioPackFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioChannelFormatIDRef>AC_00031001</audioChannelFormatIDRef></audioPackFormat>
<audioChannelFormat audioChannelFormatID="AC_00031001" audioChannelFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioBlockFormat audioBlockFormatID="AB_00031001_00000001" rtime="00:00:00.00000" duration="00:00:00.12510"><cartesian>1</cartesian><position coordinate="X">-0.000000</position><position coordinate="Y">1.000000</position><position coordinate="Z">0.000000</position></audioBlockFormat></audioChannelFormat>
<audioTrackUID UID="ATU_00000001" sampleRate="48000" bitDepth="24"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioTrackUID>
</audioFormatExtended></format></coreMetadata></ebuCoreMain>
)xml";

// The same file, but the only audioObject carries no `audioPackFormatIDRef`.
// `ParseXmlToAdm` skips such an object rather than rejecting it, so it
// survives into the ADM with an empty `audio_pack_format_id_refs`.
constexpr absl::string_view kAxmlWithObjectWithoutAudioPackFormatIdRef =
    R"xml(<?xml version="1.0" encoding="UTF-8"?>
<ebuCoreMain xmlns="urn:ebu:metadata-schema:ebuCore_2016"><coreMetadata><format><audioFormatExtended version="ITU-R_BS.2076-2">
<audioProgramme audioProgrammeID="APR_1001" audioProgrammeName="fixture" start="00:00:00.00000" end="00:00:00.12510"><audioContentIDRef>ACO_1001</audioContentIDRef><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioProgramme>
<audioContent audioContentID="ACO_1001" audioContentName="All"><audioObjectIDRef>AO_1001</audioObjectIDRef></audioContent>
<audioObject audioObjectID="AO_1001" audioObjectName="Obj1" start="00:00:00.00000" duration="00:00:00.12510"><audioTrackUIDRef>ATU_00000001</audioTrackUIDRef></audioObject>
<audioPackFormat audioPackFormatID="AP_00031001" audioPackFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioChannelFormatIDRef>AC_00031001</audioChannelFormatIDRef></audioPackFormat>
<audioChannelFormat audioChannelFormatID="AC_00031001" audioChannelFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioBlockFormat audioBlockFormatID="AB_00031001_00000001" rtime="00:00:00.00000" duration="00:00:00.12510"><cartesian>1</cartesian><position coordinate="X">-0.000000</position><position coordinate="Y">1.000000</position><position coordinate="Z">0.000000</position></audioBlockFormat></audioChannelFormat>
<audioTrackUID UID="ATU_00000001" sampleRate="48000" bitDepth="24"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioTrackUID>
</audioFormatExtended></format></coreMetadata></ebuCoreMain>
)xml";

// Two channels, the second of which is an LFE, but only one audioObject. IA
// Base Enhanced Profile keeps the LFE as a separate audio element, so it needs
// one audioObject for the ambisonics element and one more per LFE channel.
constexpr absl::string_view kAxmlWithOneObjectAndAnLfeChannel =
    R"xml(<?xml version="1.0" encoding="UTF-8"?>
<ebuCoreMain xmlns="urn:ebu:metadata-schema:ebuCore_2016"><coreMetadata><format><audioFormatExtended version="ITU-R_BS.2076-2">
<audioProgramme audioProgrammeID="APR_1001" audioProgrammeName="fixture" start="00:00:00.00000" end="00:00:00.12510"><audioContentIDRef>ACO_1001</audioContentIDRef><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioProgramme>
<audioContent audioContentID="ACO_1001" audioContentName="All"><audioObjectIDRef>AO_1001</audioObjectIDRef></audioContent>
<audioObject audioObjectID="AO_1001" audioObjectName="Obj1" start="00:00:00.00000" duration="00:00:00.12510"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef><audioTrackUIDRef>ATU_00000001</audioTrackUIDRef></audioObject>
<audioPackFormat audioPackFormatID="AP_00031001" audioPackFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioChannelFormatIDRef>AC_00031001</audioChannelFormatIDRef></audioPackFormat>
<audioChannelFormat audioChannelFormatID="AC_00031001" audioChannelFormatName="Obj1" typeLabel="0003" typeDefinition="Objects"><audioBlockFormat audioBlockFormatID="AB_00031001_00000001" rtime="00:00:00.00000" duration="00:00:00.12510"><cartesian>1</cartesian><position coordinate="X">-0.000000</position><position coordinate="Y">1.000000</position><position coordinate="Z">0.000000</position></audioBlockFormat></audioChannelFormat>
<audioChannelFormat audioChannelFormatID="AC_00031002" audioChannelFormatName="RoomCentricLFE" typeLabel="0003" typeDefinition="Objects"><audioBlockFormat audioBlockFormatID="AB_00031002_00000001" rtime="00:00:00.00000" duration="00:00:00.12510"><cartesian>1</cartesian><position coordinate="X">0.000000</position><position coordinate="Y">1.000000</position><position coordinate="Z">-1.000000</position></audioBlockFormat></audioChannelFormat>
<audioTrackUID UID="ATU_00000001" sampleRate="48000" bitDepth="24"><audioPackFormatIDRef>AP_00031001</audioPackFormatIDRef></audioTrackUID>
</audioFormatExtended></format></coreMetadata></ebuCoreMain>
)xml";

absl::StatusOr<iamf_tools_cli_proto::UserMetadata> GenerateFromDolbyAdm(
    uint32_t num_channels, absl::string_view axml,
    iamf_tools::ProfileVersion profile_version,
    const std::filesystem::path& output_path) {
  std::istringstream ss(DolbyAdmBwf(num_channels, axml));
  return GenerateUserMetadataAndSpliceWavFiles(
      kFilePrefix, kMaxFrameDurationMs, kImportanceThreshold, output_path, ss,
      profile_version);
}

TEST(GenerateUserMetadataAndSpliceWavFiles, DolbyAdmWithOneObjectIsOk) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_THAT(GenerateFromDolbyAdm(1, kAxmlWithOneObject, kIamfBaseProfile,
                                   output_path),
              IsOk());
}

TEST(GenerateUserMetadataAndSpliceWavFiles,
     InvalidWhenDolbyAdmHasNoAudioObjectLeftAfterFiltering) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_THAT(GenerateFromDolbyAdm(1, kAxmlWithOneLowImportanceObject,
                                   kIamfBaseProfile, output_path),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(GenerateUserMetadataAndSpliceWavFiles,
     InvalidWhenTheFirstAudioObjectHasNoAudioPackFormatIdRef) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_THAT(
      GenerateFromDolbyAdm(1, kAxmlWithObjectWithoutAudioPackFormatIdRef,
                           kIamfBaseProfile, output_path),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(GenerateUserMetadataAndSpliceWavFiles,
     InvalidWhenBaseEnhancedHasTooFewAudioObjectsForTheLfeCount) {
  const std::filesystem::path output_path(GetAndCreateOutputDirectory(""));

  EXPECT_THAT(GenerateFromDolbyAdm(2, kAxmlWithOneObjectAndAnLfeChannel,
                                   kIamfBaseEnhancedProfile, output_path),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
