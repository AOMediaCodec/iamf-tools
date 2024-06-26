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
#include "iamf/cli/wav_writer.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_reader.h"

namespace iamf_tools {
namespace {

inline std::string GetTestWavPath() {
  return (std::filesystem::path(::testing::TempDir()) / "test.wav").string();
}

constexpr int kNumChannels = 1;
constexpr int kSampleRateHz = 16000;
constexpr int kBitDepth16 = 16;
constexpr int kBitDepth24 = 24;
constexpr int kBitDepth32 = 32;

TEST(WavWriterTest, Construct16BitWavWriter) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16);
  EXPECT_EQ(wav_writer.bit_depth(), kBitDepth16);
}

TEST(WavWriterTest, Construct16BitWavWriterWithoutHeader) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16, /*write_header=*/false);
  EXPECT_EQ(wav_writer.bit_depth(), kBitDepth16);
}

TEST(WavWriterTest, Construct24BitWavWriter) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth24);
  EXPECT_EQ(wav_writer.bit_depth(), kBitDepth24);
}

TEST(WavWriterTest, Construct32BitWavWriter) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth32);
  EXPECT_EQ(wav_writer.bit_depth(), kBitDepth32);
}

TEST(WavWriterTest, InvalidBitDepthFailsAtWriting) {
  const int kInvalidBitDepth = 13;
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kInvalidBitDepth);

  // `WriteSamples()` returning `false` means writing failed.
  std::vector<uint8_t> samples(10, 0);
  EXPECT_FALSE(wav_writer.WriteSamples(samples));
}

TEST(WavWriterTest, WriteEmptySamplesSucceeds) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth24);

  std::vector<uint8_t> empty_samples(0);
  EXPECT_TRUE(wav_writer.WriteSamples(empty_samples));
}

TEST(WavWriterTest, WriteIntegerSamplesSucceeds) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16);

  // Bit depth = 16, and writing 6 bytes = 48 bits = 3 samples succeeds.
  std::vector<uint8_t> samples(6, 0);
  EXPECT_TRUE(wav_writer.WriteSamples(samples));
}

TEST(WavWriterTest, WriteNonIntegerNumberOfSamplesFails) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16);

  // Bit depth = 16, and writing 3 bytes = 24 bits = 1.5 samples fails.
  std::vector<uint8_t> samples(3, 0);
  EXPECT_FALSE(wav_writer.WriteSamples(samples));
}

TEST(WavWriterTest, WriteIntegerSamplesSucceedsWithoutHeader) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16, /*write_header=*/false);

  // Bit depth = 16, and writing 6 bytes = 48 bits = 3 samples succeeds.
  std::vector<uint8_t> samples(6, 0);
  EXPECT_TRUE(wav_writer.WriteSamples(samples));
}

TEST(WavWriterTest, Write24BitSamplesSucceeds) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth24);

  // Bit depth = 24, and writing 6 bytes = 48 bits = 2 samples succeeds.
  std::vector<uint8_t> samples(6, 0);
  EXPECT_TRUE(wav_writer.WriteSamples(samples));
}

TEST(WavWriterTest, Write32BitSamplesSucceeds) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth32);

  // Bit depth = 32, and writing 8 bytes = 64 bits = 2 samples succeeds.
  std::vector<uint8_t> samples = {1, 0, 0, 0, 2, 0, 0, 0};
  EXPECT_TRUE(wav_writer.WriteSamples(samples));
}

TEST(WavWriterTest, FileExistsAndHasNonZeroSizeWithHeader) {
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth16);
  }

  EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(GetTestWavPath())));
  std::error_code error_code;
  EXPECT_NE(std::filesystem::file_size(std::filesystem::path(GetTestWavPath()),
                                       error_code),
            0);
  EXPECT_FALSE(error_code);
}

TEST(WavWriterTest, EmptyFileExistsAndHasZeroSizeWithoutHeader) {
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth16, /*write_header=*/false);
  }

  EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(GetTestWavPath())));
  std::error_code error_code;
  EXPECT_EQ(std::filesystem::file_size(std::filesystem::path(GetTestWavPath()),
                                       error_code),
            0);
  EXPECT_FALSE(error_code);
}

TEST(WavWriterTest, OutputFileHasCorrectSizeWithoutHeader) {
  const int kInputBytes = 10;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth16, /*write_header=*/false);
    std::vector<uint8_t> samples(kInputBytes, 0);
    EXPECT_TRUE(wav_writer.WriteSamples(samples));
  }

  std::error_code error_code;
  EXPECT_EQ(std::filesystem::file_size(std::filesystem::path(GetTestWavPath()),
                                       error_code),
            kInputBytes);
  EXPECT_FALSE(error_code);
}

TEST(WavWriterTest, OutputWavFileHasCorrectNumberOfSamples16Bit) {
  const int kInputBytes = 12;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth16);
    std::vector<uint8_t> samples(kInputBytes, 0);
    EXPECT_TRUE(wav_writer.WriteSamples(samples));
  }

  const auto wav_reader = CreateWavReaderExpectOk(GetTestWavPath());
  EXPECT_EQ(wav_reader.remaining_samples(), 6);
}

TEST(WavWriterTest, OutputWavFileHasCorrectNumberOfSamples24Bit) {
  const int kInputBytes = 12;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth24);
    std::vector<uint8_t> samples(kInputBytes, 0);
    EXPECT_TRUE(wav_writer.WriteSamples(samples));
  }

  const auto wav_reader = CreateWavReaderExpectOk(GetTestWavPath());
  EXPECT_EQ(wav_reader.remaining_samples(), 4);
}

TEST(WavWriterTest, OutputWavFileHasCorrectNumberOfSamples32Bit) {
  const int kInputBytes = 12;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth32);
    std::vector<uint8_t> samples(kInputBytes, 0);
    EXPECT_TRUE(wav_writer.WriteSamples(samples));
  }

  const auto wav_reader = CreateWavReaderExpectOk(GetTestWavPath());
  EXPECT_EQ(wav_reader.remaining_samples(), 3);
}

TEST(WavWriterTest, OutputWavFileHasCorrectProperties) {
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth32);
  }

  auto wav_reader = CreateWavReaderExpectOk(GetTestWavPath());
  EXPECT_EQ(wav_reader.sample_rate_hz(), kSampleRateHz);
  EXPECT_EQ(wav_reader.num_channels(), kNumChannels);
  EXPECT_EQ(wav_reader.bit_depth(), kBitDepth32);
}

TEST(WavWriterTest, OutputWavFileHasCorrectPropertiesAfterMoving) {
  {
    WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                         kBitDepth32);
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    WavWriter new_wav_writer = std::move(wav_writer);
  }

  const auto wav_reader = CreateWavReaderExpectOk(GetTestWavPath());
  EXPECT_EQ(wav_reader.sample_rate_hz(), kSampleRateHz);
  EXPECT_EQ(wav_reader.num_channels(), kNumChannels);
  EXPECT_EQ(wav_reader.bit_depth(), kBitDepth32);
}

TEST(WavWriterTest, AbortDeletesOutputFile) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16);
  wav_writer.Abort();

  // Expect that the output file is deleted.
  EXPECT_FALSE(
      std::filesystem::exists(std::filesystem::path(GetTestWavPath())));
}

TEST(WavWriterTest, MovingSucceeds) {
  WavWriter wav_writer(GetTestWavPath(), kNumChannels, kSampleRateHz,
                       kBitDepth16);

  // Move.
  WavWriter new_wav_writer = std::move(wav_writer);

  // Expect that the new `wav_writer` has the same attributes as the original.
  EXPECT_EQ(new_wav_writer.bit_depth(), kBitDepth16);

  // Expect that the output file still exists.
  EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(GetTestWavPath())));

  // Expect the new `wav_writer` can write samples.
  std::vector<uint8_t> samples(10, 0);
  EXPECT_TRUE(new_wav_writer.WriteSamples(samples));

  // Expect that aborting after moving deletes the output file.
  new_wav_writer.Abort();
  EXPECT_FALSE(
      std::filesystem::exists(std::filesystem::path(GetTestWavPath())));
}

// TODO(b/307692452): Add tests that actually checks the content of the
//                    output WAV files written by `WriteSamples()`.

}  // namespace
}  // namespace iamf_tools
