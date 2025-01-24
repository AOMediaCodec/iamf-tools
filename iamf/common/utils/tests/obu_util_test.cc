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
#include "iamf/common/utils/obu_util.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <optional>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

constexpr absl::string_view kOmitContext = "";
constexpr absl::string_view kCustomUserContext = "Custom User Context";

TEST(WritePcmSample, LittleEndian32Bits) {
  std::vector<uint8_t> buffer(4, 0);
  int write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345678, 32, /*big_endian=*/false,
                             buffer.data(), write_position),
              IsOk());
  EXPECT_EQ(write_position, 4);
  std::vector<uint8_t> expected_result = {0x78, 0x56, 0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian32bits) {
  std::vector<uint8_t> buffer(4, 0);
  int write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345678, 32, /*big_endian=*/true, buffer.data(),
                             write_position),
              IsOk());
  EXPECT_EQ(write_position, 4);
  std::vector<uint8_t> expected_result = {0x12, 0x34, 0x56, 0x78};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, LittleEndian24Bits) {
  std::vector<uint8_t> buffer(3, 0);
  int write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345600, 24, /*big_endian=*/false,
                             buffer.data(), write_position),
              IsOk());
  EXPECT_EQ(write_position, 3);
  std::vector<uint8_t> expected_result = {0x56, 0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian24Bits) {
  std::vector<uint8_t> buffer(3, 0);
  int write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345600, 24, /*big_endian=*/true, buffer.data(),
                             write_position),
              IsOk());
  EXPECT_EQ(write_position, 3);
  std::vector<uint8_t> expected_result = {0x12, 0x34, 0x56};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, LittleEndian16Bits) {
  std::vector<uint8_t> buffer(2, 0);
  int write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12340000, 16, /*big_endian=*/false,
                             buffer.data(), write_position),
              IsOk());
  EXPECT_EQ(write_position, 2);
  std::vector<uint8_t> expected_result = {0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian16Bits) {
  std::vector<uint8_t> buffer(2, 0);
  int write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12340000, 16, /*big_endian=*/true, buffer.data(),
                             write_position),
              IsOk());
  EXPECT_EQ(write_position, 2);
  std::vector<uint8_t> expected_result = {0x12, 0x34};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, InvalidOver32Bits) {
  std::vector<uint8_t> buffer(5, 0);
  int write_position = 0;
  EXPECT_EQ(WritePcmSample(0x00000000, 40, /*big_endian=*/false, buffer.data(),
                           write_position)
                .code(),
            absl::StatusCode::kInvalidArgument);
}

const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
    kIdentityTransform = [](int32_t input, int32_t& output) {
      output = input;
      return absl::OkStatus();
    };

TEST(ConvertInterleavedToTimeChannel, FailsIfSamplesIsNotAMultipleOfChannels) {
  constexpr std::array<int32_t, 4> kFourTestValues = {1, 2, 3, 4};
  constexpr size_t kNumChannels = 3;
  std::vector<std::vector<int32_t>> undefined_result(
      1, std::vector<int32_t>(kNumChannels));
  size_t undefined_num_ticks;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(
                  absl::MakeConstSpan(kFourTestValues), kNumChannels,
                  kIdentityTransform, undefined_result, undefined_num_ticks),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertInterleavedToTimeChannel, FailsIfTooFewTicksInResult) {
  constexpr std::array<int32_t, 4> kFourTestValues = {1, 2, 3, 4};
  constexpr size_t kNumChannels = 2;
  const size_t input_num_ticks = kFourTestValues.size() / kNumChannels;

  // The result has one fewer ticks than the input, which will be rejected.
  std::vector<std::vector<int32_t>> undefined_result(
      input_num_ticks - 1, std::vector<int32_t>(kNumChannels));
  size_t undefined_num_ticks;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(
                  absl::MakeConstSpan(kFourTestValues), kNumChannels,
                  kIdentityTransform, undefined_result, undefined_num_ticks),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertInterleavedToTimeChannel, FailsIfDifferentChannelNumbersInResult) {
  constexpr std::array<int32_t, 4> kFourTestValues = {1, 2, 3, 4};
  constexpr size_t kNumChannels = 2;
  const size_t input_num_ticks = kFourTestValues.size() / kNumChannels;

  // The result has a different number of channels than the input, which will be
  // rejected.
  std::vector<std::vector<int32_t>> undefined_result(
      input_num_ticks, std::vector<int32_t>(kNumChannels + 1));
  size_t undefined_num_ticks;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(
                  absl::MakeConstSpan(kFourTestValues), kNumChannels,
                  kIdentityTransform, undefined_result, undefined_num_ticks),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertInterleavedToTimeChannel, PropagatesError) {
  const absl::Status kError = absl::InternalError("Test error");
  const size_t kNumChannels = 2;
  constexpr std::array<int32_t, 4> kSamples{1, 2, 3, 4};
  const size_t kNumTicks = kSamples.size() / kNumChannels;
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kAlwaysErrorTransform =
          [kError](int32_t input, int32_t& output) { return kError; };
  std::vector<std::vector<int32_t>> undefined_result(
      kNumTicks, std::vector<int32_t>(kNumChannels));
  size_t undefined_num_ticks;
  EXPECT_EQ(ConvertInterleavedToTimeChannel(
                absl::MakeConstSpan(kSamples), kNumChannels,
                kAlwaysErrorTransform, undefined_result, undefined_num_ticks),
            kError);
}

TEST(ConvertInterleavedToTimeChannel, SucceedsOnEmptySamples) {
  constexpr std::array<int32_t, 0> kEmptySamples{};
  constexpr size_t kNumChannels = 2;
  std::vector<std::vector<int32_t>> result;
  size_t num_ticks = 0;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(
                  absl::MakeConstSpan(kEmptySamples), kNumChannels,
                  kIdentityTransform, result, num_ticks),
              IsOk());
  EXPECT_EQ(num_ticks, 0);
}

TEST(ConvertInterleavedToTimeChannel, DoesNotAlterOutputVector) {
  constexpr size_t kNumChannels = 2;
  constexpr std::array<int32_t, 0> kEmptySamples{};
  std::vector<std::vector<int32_t>> result = {{1, 2}, {3, 4}};
  const auto copy_of_result = result;
  size_t num_ticks = 0;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(
                  absl::MakeConstSpan(kEmptySamples), kNumChannels,
                  kIdentityTransform, result, num_ticks),
              IsOk());

  // Result is not changed but the valid range (`num_ticks`) is zero, meaning
  // none of the result should be used.
  EXPECT_EQ(copy_of_result, result);
  EXPECT_EQ(num_ticks, 0);
}

TEST(ConvertInterleavedToTimeChannel, InterleavesResults) {
  constexpr size_t kNumChannels = 3;
  constexpr std::array<int32_t, 6> kTwoTicksOfThreeChannels{1, 2, 3, 4, 5, 6};
  const std::vector<std::vector<int32_t>> kExpectedTwoTicksForThreeChannels = {
      {1, 2, 3}, {4, 5, 6}};
  std::vector<std::vector<int32_t>> result(2,
                                           std::vector<int32_t>(kNumChannels));
  size_t num_ticks = 0;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(
                  absl::MakeConstSpan(kTwoTicksOfThreeChannels), kNumChannels,
                  kIdentityTransform, result, num_ticks),
              IsOk());
  EXPECT_EQ(result, kExpectedTwoTicksForThreeChannels);
  EXPECT_EQ(num_ticks, 2);
}

TEST(ConvertInterleavedToTimeChannel, AppliesTransform) {
  const size_t kNumChannels = 2;
  constexpr std::array<int32_t, 4> kSamples = {1, 2, 3, 4};
  const std::vector<std::vector<int32_t>> kExpectedResult = {{2, 4}, {6, 8}};
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kDoublingTransform = [](int32_t input, int32_t& output) {
        output = input * 2;
        return absl::OkStatus();
      };
  std::vector<std::vector<int32_t>> result(2,
                                           std::vector<int32_t>(kNumChannels));
  size_t num_ticks = 0;
  EXPECT_THAT(ConvertInterleavedToTimeChannel(absl::MakeConstSpan(kSamples),
                                              kNumChannels, kDoublingTransform,
                                              result, num_ticks),
              IsOk());
  EXPECT_EQ(result, kExpectedResult);
  EXPECT_EQ(num_ticks, 2);
}

TEST(ConvertTimeChannelToInterleaved,
     FailsIfSamplesHaveAnUnevenNumberOfChannels) {
  std::vector<std::vector<int32_t>> input = {{1, 2}, {3, 4, 5}};
  std::vector<int32_t> undefined_result;

  EXPECT_THAT(
      ConvertTimeChannelToInterleaved(absl::MakeConstSpan(input),
                                      kIdentityTransform, undefined_result),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertTimeChannelToInterleaved, PropagatesError) {
  const absl::Status kError = absl::InternalError("Test error");
  const std::vector<std::vector<int32_t>> kInput = {{1, 2, 3}, {4, 5, 6}};
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kAlwaysErrorTransform =
          [kError](int32_t /*input*/, int32_t& /*output*/) { return kError; };
  std::vector<int32_t> undefined_result;

  EXPECT_EQ(
      ConvertTimeChannelToInterleaved(absl::MakeConstSpan(kInput),
                                      kAlwaysErrorTransform, undefined_result),
      kError);
}

TEST(ConvertTimeChannelToInterleaved, SucceedsOnEmptyInput) {
  const std::vector<std::vector<int32_t>> kEmptyInput;
  std::vector<int32_t> result;

  EXPECT_THAT(ConvertTimeChannelToInterleaved(absl::MakeConstSpan(kEmptyInput),
                                              kIdentityTransform, result),
              IsOk());
  EXPECT_TRUE(result.empty());
}

TEST(ConvertTimeChannelToInterleaved, ClearsOutputVector) {
  const std::vector<std::vector<int32_t>> kInput = {{1}};
  std::vector<int32_t> result = {1, 2, 3};
  constexpr std::array<int32_t, 1> kExpectedResult{1};

  EXPECT_THAT(ConvertTimeChannelToInterleaved(absl::MakeConstSpan(kInput),
                                              kIdentityTransform, result),
              IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(ConvertTimeChannelToInterleaved, InterleavesResult) {
  const std::vector<std::vector<int32_t>> kInput = {{1, 2, 3}, {4, 5, 6}};
  std::vector<int32_t> result;
  constexpr std::array<int32_t, 6> kExpectedResult{1, 2, 3, 4, 5, 6};

  EXPECT_THAT(ConvertTimeChannelToInterleaved(absl::MakeConstSpan(kInput),
                                              kIdentityTransform, result),
              IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(ConvertTimeChannelToInterleaved, AppliesTransform) {
  const std::vector<std::vector<int32_t>> kInput = {{1, 2, 3}, {4, 5, 6}};
  std::vector<int32_t> result;
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kDoublingTransform = [](int32_t input, int32_t& output) {
        output = input * 2;
        return absl::OkStatus();
      };
  constexpr std::array<int32_t, 6> kExpectedResult{2, 4, 6, 8, 10, 12};

  EXPECT_THAT(ConvertTimeChannelToInterleaved(absl::MakeConstSpan(kInput),
                                              kDoublingTransform, result),
              IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
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

  ASSERT_THAT(wb.WriteUint8Vector(bytes), IsOk());
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

}  // namespace
}  // namespace iamf_tools
