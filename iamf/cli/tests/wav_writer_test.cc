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

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <numeric>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_reader.h"

using ::absl_testing::IsOk;

namespace iamf_tools {
namespace {

constexpr int kNumChannels = 1;
constexpr int kTwoChannels = 2;
constexpr int kSampleRateHz = 16000;
constexpr int kBitDepth16 = 16;
constexpr int kBitDepth24 = 24;
constexpr int kBitDepth32 = 32;
constexpr size_t kMaxInputSamplesPerFrame = 960;

constexpr int32_t kSampleValue = 0x00000000;

TEST(WavWriterTest, Construct16BitWavWriter) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);
  EXPECT_EQ(wav_writer->bit_depth(), kBitDepth16);
}

TEST(WavWriterTest, Construct16BitWavWriterWithoutHeader) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame,
                        /*write_header=*/false);
  ASSERT_NE(wav_writer, nullptr);
  EXPECT_EQ(wav_writer->bit_depth(), kBitDepth16);
}

TEST(WavWriterTest, Construct24BitWavWriter) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth24, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);
  EXPECT_EQ(wav_writer->bit_depth(), kBitDepth24);
}

TEST(WavWriterTest, Construct32BitWavWriter) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth32, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);
  EXPECT_EQ(wav_writer->bit_depth(), kBitDepth32);
}

TEST(WavWriterTest, InvalidBitDepthFailsAtCreation) {
  const int kInvalidBitDepth = 13;
  auto wav_writer = WavWriter::Create(
      GetAndCleanupOutputFileName(".wav"), kNumChannels, kSampleRateHz,
      kInvalidBitDepth, kMaxInputSamplesPerFrame);
  EXPECT_EQ(wav_writer, nullptr);
}

TEST(DeprecatedWritePcmSamples, WriteEmptySamplesSucceeds) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth24, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  const std::vector<uint8_t> kEmptySamples;
  EXPECT_THAT(wav_writer->WritePcmSamples(kEmptySamples), IsOk());
}

TEST(PushFrame, WriteEmptySamplesSucceeds) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth24, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  const std::vector<std::vector<int32_t>> kEmptySamples;
  EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(kEmptySamples)),
              IsOk());
}

TEST(DeprecatedWritePcmSamples, WriteIntegerSamplesSucceeds) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  // Bit depth = 16, and writing 6 bytes = 48 bits = 3 samples succeeds.
  std::vector<uint8_t> samples(6, 0);
  EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
}

TEST(PushFrame, WriteIntegerSamplesSucceeds) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  constexpr int kNumSamples = 3;
  const std::vector<std::vector<int32_t>> samples(
      kNumSamples, std::vector<int32_t>(kNumChannels, kSampleValue));
  EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(samples)), IsOk());
}

TEST(PushFrame, WriteManyChannelsSucceeds) {
  // The wav writer samples supports many channels. For example, it could
  // represent a 9.1.6 channel layout using 16 channels.
  constexpr int kNumChannels = 16;
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  constexpr int kNumSamples = 3;
  const std::vector<std::vector<int32_t>> samples(
      kNumSamples, std::vector<int32_t>(kNumChannels, kSampleValue));
  EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(samples)), IsOk());
}

TEST(DeprecatedWritePcmSamples, WriteMoreSamplesThanConfiguredFails) {
  constexpr size_t kMaxOneInputSample = 1;
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxOneInputSample);
  ASSERT_NE(wav_writer, nullptr);

  // Bit depth = 16, and writing 4 bytes = 32 bits = 2 samples fails.
  std::vector<uint8_t> samples(4, 0);
  EXPECT_FALSE(wav_writer->WritePcmSamples(samples).ok());
}

TEST(PushFrame, WriteMoreSamplesThanConfiguredFails) {
  constexpr size_t kMaxOneInputSample = 1;
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxOneInputSample);
  ASSERT_NE(wav_writer, nullptr);

  constexpr int kTooManySamples = 2;
  std::vector<std::vector<int32_t>> samples(
      kNumChannels, std::vector<int32_t>(kTooManySamples, kSampleValue));
  EXPECT_FALSE(wav_writer->PushFrame(samples).ok());
}

TEST(DeprecatedWritePcmSamples, DeprecatedWriteNonIntegerNumberOfSamplesFails) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  // Bit depth = 16, and writing 3 bytes = 24 bits = 1.5 samples fails.
  std::vector<uint8_t> samples(3, 0);
  EXPECT_FALSE(wav_writer->WritePcmSamples(samples).ok());
}

TEST(PushFrame, WriteChannelWithTooFewSamplesFails) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kTwoChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  // The second tick is missing a channel.
  const std::vector<std::vector<int32_t>> samples = {
      {kSampleValue, kSampleValue}, {kSampleValue}};
  EXPECT_FALSE(wav_writer->PushFrame(absl::MakeConstSpan(samples)).ok());
}

TEST(PushFrame, ConsumesInputSamples) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);
  constexpr int kNumSamples = 3;
  const std::vector<std::vector<int32_t>> samples(
      kNumSamples, std::vector<int32_t>(kNumChannels, kSampleValue));

  EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(samples)), IsOk());

  // The writer consumes all input samples, so
  // `SampleProcessorBase::GetOutputSamplesAsSpan` will always return an empty
  // span.
  EXPECT_TRUE(wav_writer->GetOutputSamplesAsSpan().empty());
}

TEST(DeprecatedWritePcmSamples,
     DeprecatedWriteIntegerSamplesSucceedsWithoutHeader) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame,
                        /*write_header=*/false);
  ASSERT_NE(wav_writer, nullptr);

  // Bit depth = 16, and writing 6 bytes = 48 bits = 3 samples succeeds.
  std::vector<uint8_t> samples(6, 0);
  EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
}

TEST(DeprecatedWritePcmSamples, DeprecatedWrite24BitSamplesSucceeds) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth24, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  // Bit depth = 24, and writing 6 bytes = 48 bits = 2 samples succeeds.
  std::vector<uint8_t> samples(6, 0);
  EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
}

TEST(DeprecatedWritePcmSamples, DeprecatedWrite32BitSamplesSucceeds) {
  auto wav_writer =
      WavWriter::Create(GetAndCleanupOutputFileName(".wav"), kNumChannels,
                        kSampleRateHz, kBitDepth32, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);

  // Bit depth = 32, and writing 8 bytes = 64 bits = 2 samples succeeds.
  std::vector<uint8_t> samples = {1, 0, 0, 0, 2, 0, 0, 0};
  EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
}

TEST(DeprecatedWritePcmSamples, FileExistsAndHasNonZeroSizeWithHeader) {
  const std::filesystem::path output_file_path(
      GetAndCleanupOutputFileName(".wav"));
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path.string(), kNumChannels,
                          kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame);
  }

  EXPECT_TRUE(std::filesystem::exists(output_file_path));
  std::error_code error_code;
  EXPECT_NE(std::filesystem::file_size(output_file_path, error_code), 0);
  EXPECT_FALSE(error_code);
}

TEST(WavWriterTest, EmptyFileExistsAndHasZeroSizeWithoutHeader) {
  const std::filesystem::path output_file_path(
      GetAndCleanupOutputFileName(".wav"));
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path.string(), kNumChannels,
                          kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame,
                          /*write_header=*/false);
  }

  EXPECT_TRUE(std::filesystem::exists(output_file_path));

  std::error_code error_code;
  EXPECT_EQ(std::filesystem::file_size(output_file_path, error_code), 0)
      << output_file_path;
  EXPECT_FALSE(error_code);
}

TEST(WavWriterTest, OutputFileHasCorrectSizeWithoutHeader) {
  const std::filesystem::path output_file_path(
      GetAndCleanupOutputFileName(".wav"));
  const int kInputBytes = 10;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path.string(), kNumChannels,
                          kSampleRateHz, kBitDepth16, kMaxInputSamplesPerFrame,
                          /*write_header=*/false);
    ASSERT_NE(wav_writer, nullptr);
    std::vector<uint8_t> samples(kInputBytes, 0);
    EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
  }

  std::error_code error_code;
  EXPECT_EQ(std::filesystem::file_size(output_file_path, error_code),
            kInputBytes);
  EXPECT_FALSE(error_code);
}

TEST(WavWriterTest,
     Output16BitWavFileHasCorrectDataWithDeprecatedWritePcmSamples) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x01000000}, {0x03020000}, {0x05040000},
      {0x07060000}, {0x09080000}, {0x0b0a0000}};
  constexpr int kNumSamplesPerFrame = 6;
  const int kInputBytes = kNumSamplesPerFrame * 2;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth16, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
    std::vector<uint8_t> samples(kInputBytes, 0);
    std::iota(samples.begin(), samples.end(), 0);
    EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamplesPerFrame);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest,
     Output16BitWavFileHasCorrectDataWithPushFrameAfterDestruction) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x01000000}, {0x03020000}, {0x05040000},
      {0x07060000}, {0x09080000}, {0x0b0a0000}};
  constexpr int kNumSamplesPerFrame = 6;
  {
    // Create the writer in a small scope. The user can safely omit the call the
    // `Flush()` method, but then they must wait until the writer is destroyed,
    // to read the finalized header.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth16, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
    EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(kExpectedSamples)),
                IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamplesPerFrame);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest, Output16BitWavFileHasCorrectDataWithPushFrameAfterFlush) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x01000000}, {0x03020000}, {0x05040000},
      {0x07060000}, {0x09080000}, {0x0b0a0000}};
  constexpr int kNumSamplesPerFrame = 6;

  auto wav_writer =
      WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                        kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);
  EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(kExpectedSamples)),
              IsOk());
  // Instead of waiting for the destructor to call `Flush()`, the user can call
  // `Flush()` explicitly, to signal the wav header (including the total number
  // of samples) to be finalized.
  EXPECT_THAT(wav_writer->Flush(), IsOk());

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamplesPerFrame);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest,
     Output24BitWavFileHasCorrectDataWithDeprecatedWritePcmSamples) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x02010000}, {0x05040300}, {0x08070600}, {0x0b0a0900}};
  constexpr int kNumSamplesPerFrame = 4;
  constexpr int kInputBytes = kNumSamplesPerFrame * 3;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth24, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
    std::vector<uint8_t> samples(kInputBytes, 0);
    std::iota(samples.begin(), samples.end(), 0);
    EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamplesPerFrame);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest, Output24BitWavFileHasCorrectData) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x02010000}, {0x05040300}, {0x08070600}, {0x0b0a0900}};
  constexpr int kNumSamplesPerFrame = 4;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth24, kMaxInputSamplesPerFrame);
    EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(kExpectedSamples)),
                IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), kNumSamplesPerFrame);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest,
     Output32BitWavFileHasCorrectDataWithDeprecatedWritePcmSamples) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x03020100}, {0x07060504}, {0x0b0a0908}};
  constexpr int kNumSamplesPerFrame = 3;
  constexpr int kInputBytes = kNumSamplesPerFrame * 4;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth32, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
    std::vector<uint8_t> samples(kInputBytes, 0);
    std::iota(samples.begin(), samples.end(), 0);
    EXPECT_THAT(wav_writer->WritePcmSamples(samples), IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), 3);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest, Output32BitWavFileHasCorrectData) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x03020100}, {0x07060504}, {0x0b0a0908}};
  constexpr int kNumSamplesPerFrame = 3;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth32, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
    EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(kExpectedSamples)),
                IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), 3);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest, OutputWithManyChannelsHasCorrectData) {
  constexpr int kNumChannels = 6;
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  const std::vector<std::vector<int32_t>> kExpectedSamples = {
      {0x01010101, 0x02010101, 0x03010101, 0x04010101, 0x05010101, 0x06010101},
      {0x01020202, 0x02020202, 0x03020202, 0x04020202, 0x05020202, 0x06020202}};
  constexpr int kNumSamplesPerFrame = 2;
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth32, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
    EXPECT_THAT(wav_writer->PushFrame(absl::MakeConstSpan(kExpectedSamples)),
                IsOk());
  }

  auto wav_reader =
      CreateWavReaderExpectOk(output_file_path, kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), 12);
  EXPECT_TRUE(wav_reader.ReadFrame());
  EXPECT_EQ(wav_reader.buffers_, kExpectedSamples);
}

TEST(WavWriterTest, OutputWavFileHasCorrectProperties) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  {
    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth32, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);
  }

  auto wav_reader = CreateWavReaderExpectOk(output_file_path);
  EXPECT_EQ(wav_reader.sample_rate_hz(), kSampleRateHz);
  EXPECT_EQ(wav_reader.num_channels(), kNumChannels);
  EXPECT_EQ(wav_reader.bit_depth(), kBitDepth32);
}

TEST(WavWriterTest, OutputWavFileHasCorrectPropertiesAfterMoving) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  {
    auto wav_writer =
        WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                          kBitDepth32, kMaxInputSamplesPerFrame);
    ASSERT_NE(wav_writer, nullptr);

    // Create the writer in a small scope. It should be destroyed before
    // checking the results.
    auto new_wav_writer = std::move(wav_writer);
    ASSERT_NE(new_wav_writer, nullptr);
  }

  const auto wav_reader = CreateWavReaderExpectOk(output_file_path);
  EXPECT_EQ(wav_reader.sample_rate_hz(), kSampleRateHz);
  EXPECT_EQ(wav_reader.num_channels(), kNumChannels);
  EXPECT_EQ(wav_reader.bit_depth(), kBitDepth32);
}

TEST(WavWriterTest, AbortDeletesOutputFile) {
  const std::string output_file_path(GetAndCleanupOutputFileName(".wav"));
  auto wav_writer =
      WavWriter::Create(output_file_path, kNumChannels, kSampleRateHz,
                        kBitDepth16, kMaxInputSamplesPerFrame);
  ASSERT_NE(wav_writer, nullptr);
  wav_writer->Abort();

  // Expect that the output file is deleted.
  EXPECT_FALSE(
      std::filesystem::exists(std::filesystem::path(output_file_path)));
}

}  // namespace
}  // namespace iamf_tools
