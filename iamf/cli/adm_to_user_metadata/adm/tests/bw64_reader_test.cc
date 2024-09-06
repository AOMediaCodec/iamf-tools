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

#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

using ::absl_testing::IsOk;

constexpr int32_t kImportanceThreshold = 10;

constexpr absl::string_view kValidWav(
    "RIFF"
    "\x54\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels
    "\x03\x00\x00\x00"  // Sample Per Second
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
    "\x1b\x00\x00\x00"  // Size of `axml` chunk.
    "<audioObject></audioObject>",
    87);

constexpr absl::string_view kValidWavWithPlatformDependentControlCharacters(
    "RIFF"
    "\x54\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
    "WAVE"
    "fmt "
    "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
    "\x01\x00"          // Format tag.
    "\x02\x00"          // Number of channels
    "\x03\x00\x00\x00"  // Sample Per Second
    "\x04\x00\x00\x00"  // Bytes per second.
    "\x10\x00"          // Block align.
    "\x10\x00"          // Bits per sample.
    "data"
    "\x08\x00\x00\x00"  // Size of `data` chunk.
    "\n\n"              // Sample[0] for channel 0.
    "\r\n"              // Sample[0] for channel 1.
    "\x1a\r"            // Sample[1] for channel 0.
    "\r\r"              // Sample[1] for channel 1.
    "axml"
    "\x1b\x00\x00\x00"  // Size of `axml` chunk.
    "<audioObject></audioObject>",
    87);

constexpr uint32_t kWavHeaderSize = 12;
constexpr uint32_t kFmtChunkSize = 16;
constexpr uint32_t kDataChunkSize = 8;
constexpr uint32_t kAxmlChunkSize = 27;
constexpr uint16_t kExpectedFormatTag = 1;
constexpr uint16_t kExpectedNumChannels = 2;
constexpr uint32_t kExpectedSamplesPerSec = 3;
constexpr uint32_t kExpectedAvgBytesPerSec = 4;
constexpr uint16_t kExpectedBlockAlign = 16;
constexpr uint16_t kExpectedBitsPerSample = 16;

constexpr size_t kExpectedNumObjects = 1;

constexpr int64_t kExpectedTotalSamplesPerChannel = 2;

void ValidateGetChunkInfo(const Bw64Reader& reader,
                          absl::string_view chunk_name, int32_t expected_size,
                          int32_t expected_offset) {
  const auto chunk_info = reader.GetChunkInfo(chunk_name);
  ASSERT_THAT(chunk_info, IsOk());
  EXPECT_EQ(chunk_info->size, expected_size);
  EXPECT_EQ(chunk_info->offset, expected_offset);
}

TEST(BuildFromStream, FailsOnEmptyStream) {
  std::istringstream ss("");

  EXPECT_FALSE(Bw64Reader::BuildFromStream(kImportanceThreshold, ss).ok());
}

TEST(BuildFromStream, PopulatesChunkInfo) {
  std::istringstream ss((std::string(kValidWav)));

  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());

  // Chunk name | Size | Offset
  // -----------|------|-------
  // fmt        | 16   | 12
  // data       | 2    | 28
  // axml       | 27   | 30
  ValidateGetChunkInfo(*reader, "fmt ", kFmtChunkSize, kWavHeaderSize);
  ValidateGetChunkInfo(
      *reader, "data", kDataChunkSize,
      kWavHeaderSize + kFmtChunkSize + Bw64Reader::kChunkHeaderOffset);
  ValidateGetChunkInfo(*reader, "axml", kAxmlChunkSize,
                       kWavHeaderSize + kFmtChunkSize + kDataChunkSize +
                           (Bw64Reader::kChunkHeaderOffset * 2));
}

TEST(BuildFromStream, SucceedsWhenDataHasPlatformDependentControlCharacters) {
  std::istringstream ss(
      (std::string(kValidWavWithPlatformDependentControlCharacters)));

  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);

  ASSERT_THAT(reader, IsOk());
}

TEST(BuildFromStream, PopulatesFormatInfo) {
  std::istringstream ss((std::string(kValidWav)));

  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());

  EXPECT_EQ(reader->format_info_.format_tag, kExpectedFormatTag);
  EXPECT_EQ(reader->format_info_.num_channels, kExpectedNumChannels);
  EXPECT_EQ(reader->format_info_.samples_per_sec, kExpectedSamplesPerSec);
  EXPECT_EQ(reader->format_info_.avg_bytes_per_sec, kExpectedAvgBytesPerSec);
  EXPECT_EQ(reader->format_info_.block_align, kExpectedBlockAlign);
  EXPECT_EQ(reader->format_info_.bits_per_sample, kExpectedBitsPerSample);
}

TEST(BuildFromStream, PopulatesAudioObjects) {
  std::istringstream ss((std::string(kValidWav)));

  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());

  EXPECT_EQ(reader->adm_.audio_objects.size(), kExpectedNumObjects);
}

TEST(BuildFromStream, ReturnsErrorWhenLookingUpUnknownChunkName) {
  std::istringstream ss((std::string(kValidWav)));

  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());

  // Returns error when Looking up an invalid chunk name returns an error.
  EXPECT_FALSE(reader->GetChunkInfo("INVALID_CHUNK").ok());
}

TEST(GetTotalSamplesPerChannel, ReturnsTotalSamplesPerChannel) {
  std::istringstream ss((std::string(kValidWav)));
  auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());

  const auto total_samples_per_channel = reader->GetTotalSamplesPerChannel();
  ASSERT_THAT(total_samples_per_channel, IsOk());

  EXPECT_EQ(*total_samples_per_channel, kExpectedTotalSamplesPerChannel);
}

TEST(GetTotalSamplesPerChannel, ReturnsErrorWhenInvalidNumberOfChannels) {
  constexpr absl::string_view kInvalidNumberOfChannels(
      "RIFF"
      "\x44\x00\x00\x00"  // Size of `RIFF` chunk (the whole file).
      "WAVE"
      "fmt "
      "\x10\x00\x00\x00"  // Size of the `fmt ` chunk.
      "\x01\x00"          // Format tag.
      "\x00\x00"          // Number of channels.
      "\x03\x00\x00\x00"  // Sample Per Second
      "\x04\x00\x00\x00"  // Bytes per second.
      "\x10\x00"          // Block align.
      "\x10\x00"          // Bits per sample.
      "data"
      "\x00\x00\x00\x00"
      "axml"
      "\x1b\x00\x00\x00"  // Size of `axml` chunk.
      "<audioObject></audioObject>",
      79);

  std::istringstream ss((std::string(kInvalidNumberOfChannels)));
  const auto reader = Bw64Reader::BuildFromStream(kImportanceThreshold, ss);
  ASSERT_THAT(reader, IsOk());

  EXPECT_FALSE(reader->GetTotalSamplesPerChannel().ok());
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
