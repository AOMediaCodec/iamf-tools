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

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <list>
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
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

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

TEST(SerializeObusExpectOk, SerializesObus) {
  MockObu mock_obu(ObuHeader{}, ObuType::kObuIaCodecConfig);
  constexpr size_t kObuHeaderSize = 2;
  constexpr std::array<uint8_t, 6> kExpectedBytes = {// OBU header.
                                                     0x00, 0x04,
                                                     // OBU payload.
                                                     0x01, 0x02, 0x00, 0x03};

  ON_CALL(mock_obu, ValidateAndWritePayload)
      .WillByDefault([&](WriteBitBuffer& wb) {
        return wb.WriteUint8Span(
            absl::MakeConstSpan(kExpectedBytes).subspan(kObuHeaderSize));
      });
  const std::vector<uint8_t> serialized_obus =
      SerializeObusExpectOk(std::list<const ObuBase*>{&mock_obu});

  EXPECT_EQ(absl::MakeConstSpan(serialized_obus),
            absl::MakeConstSpan(kExpectedBytes));
}

TEST(OneFrameDelayer, ValidatesInputShapeWithTooManyChannels) {
  // Input shape validation is managed by `SampleProcessorBase`.
  constexpr uint32_t kNumSamplesPerFrame = 3;
  constexpr size_t kNumChannels = 1;
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  const std::vector<std::vector<InternalSampleType>>
      kInputFrameWithTooManyChannels(
          kNumSamplesPerFrame,
          std::vector<InternalSampleType>(kNumChannels + 1, 0.0));

  EXPECT_THAT(one_frame_delayer.PushFrame(
                  MakeSpanOfConstSpans(kInputFrameWithTooManyChannels)),
              Not(IsOk()));
}

TEST(OneFrameDelayer, ValidatesInputShapeWithTooManySamplesPerFrame) {
  // Input shape validation is managed by `SampleProcessorBase`.
  constexpr uint32_t kNumSamplesPerFrame = 3;
  constexpr size_t kNumChannels = 1;
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  const std::vector<std::vector<InternalSampleType>>
      kInputFrameWithTooFewSamples(
          kNumSamplesPerFrame + 1,
          std::vector<InternalSampleType>(kNumChannels, 0.0));

  EXPECT_THAT(one_frame_delayer.PushFrame(
                  MakeSpanOfConstSpans(kInputFrameWithTooFewSamples)),
              Not(IsOk()));
}

TEST(OneFrameDelayer, DelaysSamplesByOneFrame) {
  constexpr uint32_t kNumSamplesPerFrame = 5;
  constexpr size_t kNumChannels = 4;
  const std::vector<std::vector<InternalSampleType>> kFirstInputFrame = {
      {0.01, 0.05, 0.09, 0.13, 0.17},
      {0.02, 0.06, 0.10, 0.14, 0.18},
      {0.03, 0.07, 0.11, 0.15, 0.19},
      {0.04, 0.08, 0.12, 0.16, 0.20}};
  const std::vector<std::vector<InternalSampleType>> kSecondInputFrame = {
      {0.21}, {0.22}, {0.23}, {0.24}};
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  // Nothing is available at the start.
  for (const auto& output_channel :
       one_frame_delayer.GetOutputSamplesAsSpan()) {
    EXPECT_TRUE(output_channel.empty());
  }

  EXPECT_THAT(
      one_frame_delayer.PushFrame(MakeSpanOfConstSpans(kFirstInputFrame)),
      IsOk());
  // Still nothing is available because the samples are delayed by a frame.
  for (const auto& output_channel :
       one_frame_delayer.GetOutputSamplesAsSpan()) {
    EXPECT_TRUE(output_channel.empty());
  }

  // Pushing in a new frame will cause the first frame to be available.
  EXPECT_THAT(
      one_frame_delayer.PushFrame(MakeSpanOfConstSpans(kSecondInputFrame)),
      IsOk());
  EXPECT_EQ(one_frame_delayer.GetOutputSamplesAsSpan(),
            MakeSpanOfConstSpans(kFirstInputFrame));
}

TEST(OneFrameDelayer, GetOutputSamplesAsSpanReturnsFinalFrameAfterFlush) {
  constexpr uint32_t kNumSamplesPerFrame = 5;
  constexpr size_t kNumChannels = 4;
  const std::vector<std::vector<InternalSampleType>> kFirstInputFrame = {
      {0.01, 0.05, 0.09, 0.13, 0.17},
      {0.02, 0.06, 0.10, 0.14, 0.18},
      {0.03, 0.07, 0.11, 0.15, 0.19},
      {0.04, 0.08, 0.12, 0.16, 0.20}};
  OneFrameDelayer one_frame_delayer(kNumSamplesPerFrame, kNumChannels);
  EXPECT_THAT(
      one_frame_delayer.PushFrame(MakeSpanOfConstSpans(kFirstInputFrame)),
      IsOk());
  // Nothing is available because the samples are delayed by a frame.
  for (const auto& output_channel :
       one_frame_delayer.GetOutputSamplesAsSpan()) {
    EXPECT_TRUE(output_channel.empty());
  }

  // Flushing will allow access to the final delayed frame.
  EXPECT_THAT(one_frame_delayer.Flush(), IsOk());

  EXPECT_EQ(one_frame_delayer.GetOutputSamplesAsSpan(),
            MakeSpanOfConstSpans(kFirstInputFrame));
}

}  // namespace
}  // namespace iamf_tools
