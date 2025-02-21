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
#include "iamf/cli/tests/cli_test_utils.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <numeric>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

TEST(GetLogSpectralDistance, ReturnsCorrectValue) {
  std::vector<double> first_log_spectrum(10);
  std::iota(first_log_spectrum.begin(), first_log_spectrum.end(), 0);
  std::vector<double> second_log_spectrum(10);
  std::iota(second_log_spectrum.begin(), second_log_spectrum.end(), 1);
  EXPECT_EQ(GetLogSpectralDistance(absl::MakeConstSpan(first_log_spectrum),
                                   absl::MakeConstSpan(second_log_spectrum)),
            10.0);
}

TEST(ExpectLogSpectralDistanceBelowThreshold, ReturnsZeroWhenEqual) {
  std::vector<double> first_log_spectrum(10);
  std::iota(first_log_spectrum.begin(), first_log_spectrum.end(), 1);
  std::vector<double> second_log_spectrum(10);
  std::iota(second_log_spectrum.begin(), second_log_spectrum.end(), 1);
  EXPECT_EQ(GetLogSpectralDistance(absl::MakeConstSpan(first_log_spectrum),
                                   absl::MakeConstSpan(second_log_spectrum)),
            0.0);
}

TEST(ReadFileToBytes, FailsIfFileDoesNotExist) {
  const std::filesystem::path file_path_does_not_exist(
      GetAndCleanupOutputFileName(".bin"));

  ASSERT_FALSE(std::filesystem::exists(file_path_does_not_exist));

  std::vector<uint8_t> bytes;
  EXPECT_FALSE(ReadFileToBytes(file_path_does_not_exist, bytes).ok());
}

void WriteVectorToFile(const std::filesystem::path filename,
                       const std::vector<uint8_t>& bytes) {
  std::filesystem::remove(filename);
  WriteBitBuffer wb(0);

  ASSERT_THAT(wb.WriteUint8Span(absl::MakeConstSpan(bytes)), IsOk());
  auto output_file = std::make_optional<std::fstream>(
      filename.string(), std::ios::binary | std::ios::out);
  ASSERT_THAT(wb.FlushAndWriteToFile(output_file), IsOk());
  output_file->close();
}

TEST(ReadFileToBytes, ReadsFileContents) {
  // Prepare a file to read back.
  const std::filesystem::path file_to_read(GetAndCleanupOutputFileName(".bin"));
  const std::vector<uint8_t> kExpectedBytes = {0x01, 0x02, 0x00, 0x03, 0x04};
  WriteVectorToFile(file_to_read, kExpectedBytes);

  std::vector<uint8_t> bytes;
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());

  EXPECT_EQ(bytes, kExpectedBytes);
}

TEST(ReadFileToBytes, AppendsFileContents) {
  // Prepare a file to read back.
  const std::filesystem::path file_to_read(GetAndCleanupOutputFileName(".bin"));
  const std::vector<uint8_t> kExpectedBytes = {0x01, 0x02, 0x00, 0x03, 0x04};
  WriteVectorToFile(file_to_read, kExpectedBytes);

  std::vector<uint8_t> bytes;
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());
  EXPECT_EQ(bytes.size(), kExpectedBytes.size());
  // The vector grows with each read.
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());
  EXPECT_EQ(bytes.size(), kExpectedBytes.size() * 2);
}

TEST(ReadFileToBytes, ReadsBinaryFileWithPlatformDependentControlCharacters) {
  // Prepare a file to read back.
  const std::filesystem::path file_to_read(GetAndCleanupOutputFileName(".bin"));
  const std::vector<uint8_t> kBinaryDataWithPlatformDependentControlCharacters =
      {'\n', '\r', '\n', '\r', '\x1a', '\r', '\n', '\n', ' ', '\n'};
  WriteVectorToFile(file_to_read,
                    kBinaryDataWithPlatformDependentControlCharacters);

  std::vector<uint8_t> bytes;
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());

  EXPECT_THAT(bytes, kBinaryDataWithPlatformDependentControlCharacters);
}

TEST(OneFrameDelayer, ValidatesInputShapeWithTooManyChannels) {
  // Input shape validation is managed by `SampleProcessorBase`.
  constexpr uint32_t kNumSamplesPerFrame = 3;
  constexpr size_t kNumChannels = 1;
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  const std::vector<std::vector<int32_t>> kInputFrameWithTooManyChannels(
      kNumSamplesPerFrame, std::vector<int32_t>(kNumChannels + 1, 0));

  EXPECT_THAT(one_frame_delayer.PushFrame(kInputFrameWithTooManyChannels),
              Not(IsOk()));
}

TEST(OneFrameDelayer, ValidatesInputShapeWithTooManySamplesPerFrame) {
  // Input shape validation is managed by `SampleProcessorBase`.
  constexpr uint32_t kNumSamplesPerFrame = 3;
  constexpr size_t kNumChannels = 1;
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  const std::vector<std::vector<int32_t>> kInputFrameWithTooFewSamples(
      kNumSamplesPerFrame + 1, std::vector<int32_t>(kNumChannels, 0));

  EXPECT_THAT(one_frame_delayer.PushFrame(kInputFrameWithTooFewSamples),
              Not(IsOk()));
}

TEST(OneFrameDelayer, DelaysSamplesByOneFrame) {
  constexpr uint32_t kNumSamplesPerFrame = 5;
  constexpr size_t kNumChannels = 4;
  const std::vector<std::vector<int32_t>> kFirstInputFrame = {
      {{1, 2, 3, 4},
       {5, 6, 7, 8},
       {9, 10, 11, 12},
       {13, 14, 15, 16},
       {17, 18, 19, 20}}};
  const std::vector<std::vector<int32_t>> kSecondInputFrame = {
      {{21, 22, 23, 24}}};
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  // Nothing is available at the start.
  EXPECT_TRUE(one_frame_delayer.GetOutputSamplesAsSpan().empty());
  EXPECT_THAT(one_frame_delayer.PushFrame(kFirstInputFrame), IsOk());
  // Still nothing is available because the samples are delayed by a frame.
  EXPECT_TRUE(one_frame_delayer.GetOutputSamplesAsSpan().empty());

  // Pushing in a new frame will cause the first frame to be available.
  EXPECT_THAT(one_frame_delayer.PushFrame(kSecondInputFrame), IsOk());

  EXPECT_THAT(one_frame_delayer.GetOutputSamplesAsSpan(), kFirstInputFrame);
}

TEST(OneFrameDelayer, GetOutputSamplesAsSpanReturnsFinalFrameAfterFlush) {
  constexpr uint32_t kNumSamplesPerFrame = 5;
  constexpr size_t kNumChannels = 4;
  const std::vector<std::vector<int32_t>> kFirstInputFrame = {
      {{1, 2, 3, 4},
       {5, 6, 7, 8},
       {9, 10, 11, 12},
       {13, 14, 15, 16},
       {17, 18, 19, 20}}};
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  EXPECT_THAT(one_frame_delayer.PushFrame(kFirstInputFrame), IsOk());
  // Nothing is available because the samples are delayed by a frame.
  EXPECT_TRUE(one_frame_delayer.GetOutputSamplesAsSpan().empty());

  // Flushing will allow access to the final delayed frame.
  EXPECT_THAT(one_frame_delayer.Flush(), IsOk());

  EXPECT_THAT(one_frame_delayer.GetOutputSamplesAsSpan(), kFirstInputFrame);
}

}  // namespace
}  // namespace iamf_tools
