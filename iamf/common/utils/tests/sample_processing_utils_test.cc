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
#include "iamf/common/utils/sample_processing_utils.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::testing::ElementsAreArray;

TEST(WritePcmSample, LittleEndian32Bits) {
  std::vector<uint8_t> buffer(4, 0);
  size_t write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345678, 32, /*big_endian=*/false,
                             buffer.data(), write_position),
              IsOk());
  EXPECT_EQ(write_position, 4);
  std::vector<uint8_t> expected_result = {0x78, 0x56, 0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian32bits) {
  std::vector<uint8_t> buffer(4, 0);
  size_t write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345678, 32, /*big_endian=*/true, buffer.data(),
                             write_position),
              IsOk());
  EXPECT_EQ(write_position, 4);
  std::vector<uint8_t> expected_result = {0x12, 0x34, 0x56, 0x78};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, LittleEndian24Bits) {
  std::vector<uint8_t> buffer(3, 0);
  size_t write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345600, 24, /*big_endian=*/false,
                             buffer.data(), write_position),
              IsOk());
  EXPECT_EQ(write_position, 3);
  std::vector<uint8_t> expected_result = {0x56, 0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian24Bits) {
  std::vector<uint8_t> buffer(3, 0);
  size_t write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12345600, 24, /*big_endian=*/true, buffer.data(),
                             write_position),
              IsOk());
  EXPECT_EQ(write_position, 3);
  std::vector<uint8_t> expected_result = {0x12, 0x34, 0x56};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, LittleEndian16Bits) {
  std::vector<uint8_t> buffer(2, 0);
  size_t write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12340000, 16, /*big_endian=*/false,
                             buffer.data(), write_position),
              IsOk());
  EXPECT_EQ(write_position, 2);
  std::vector<uint8_t> expected_result = {0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian16Bits) {
  std::vector<uint8_t> buffer(2, 0);
  size_t write_position = 0;
  EXPECT_THAT(WritePcmSample(0x12340000, 16, /*big_endian=*/true, buffer.data(),
                             write_position),
              IsOk());
  EXPECT_EQ(write_position, 2);
  std::vector<uint8_t> expected_result = {0x12, 0x34};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, InvalidOver32Bits) {
  std::vector<uint8_t> buffer(5, 0);
  size_t write_position = 0;
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

TEST(ConvertInterleavedToChannelTime, FailsIfSamplesIsNotAMultipleOfChannels) {
  constexpr std::array<int32_t, 4> kFourTestValues = {1, 2, 3, 4};
  constexpr size_t kNumChannels = 3;
  std::vector<std::vector<int32_t>> undefined_result(kNumChannels);
  EXPECT_THAT(ConvertInterleavedToChannelTime(
                  absl::MakeConstSpan(kFourTestValues), kNumChannels,
                  undefined_result, kIdentityTransform),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertInterleavedToChannelTime, PropagatesError) {
  const absl::Status kError = absl::InternalError("Test error");
  const size_t kNumChannels = 2;
  constexpr std::array<int32_t, 4> kSamples{1, 2, 3, 4};
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kAlwaysErrorTransform =
          [kError](int32_t input, int32_t& output) { return kError; };
  std::vector<std::vector<int32_t>> undefined_result(kNumChannels);
  EXPECT_EQ(ConvertInterleavedToChannelTime(absl::MakeConstSpan(kSamples),
                                            kNumChannels, undefined_result,
                                            kAlwaysErrorTransform),
            kError);
}

TEST(ConvertInterleavedToChannelTime, SucceedsOnEmptySamples) {
  constexpr std::array<int32_t, 0> kEmptySamples{};
  constexpr size_t kNumChannels = 2;
  std::vector<std::vector<int32_t>> result(kNumChannels);
  EXPECT_THAT(
      ConvertInterleavedToChannelTime(absl::MakeConstSpan(kEmptySamples),
                                      kNumChannels, result, kIdentityTransform),
      IsOk());
  for (const auto& channel : result) {
    EXPECT_TRUE(channel.empty());
  }
}

TEST(ConvertInterleavedToChannelTime, InterleavesResults) {
  constexpr size_t kNumChannels = 3;
  constexpr std::array<int32_t, 6> kTwoTicksOfThreeChannels{1, 2, 3, 4, 5, 6};
  const std::vector<std::vector<int32_t>> kExpectedThreeChannelsOfTwoTicks = {
      {1, 4}, {2, 5}, {3, 6}};
  std::vector<std::vector<int32_t>> result(kNumChannels);
  EXPECT_THAT(ConvertInterleavedToChannelTime(
                  absl::MakeConstSpan(kTwoTicksOfThreeChannels), kNumChannels,
                  result, kIdentityTransform),
              IsOk());
  EXPECT_EQ(result, kExpectedThreeChannelsOfTwoTicks);
}

TEST(ConvertInterleavedToChannelTime, DefaultToIdentityTransform) {
  constexpr size_t kNumChannels = 3;
  constexpr std::array<int32_t, 6> kTwoTicksOfThreeChannels{1, 2, 3, 4, 5, 6};
  const std::vector<std::vector<int32_t>> kExpectedThreeChannelsOfTwoTicks = {
      {1, 4}, {2, 5}, {3, 6}};
  std::vector<std::vector<int32_t>> result(kNumChannels);

  // Skip the transform argument.
  EXPECT_THAT(
      ConvertInterleavedToChannelTime(
          absl::MakeConstSpan(kTwoTicksOfThreeChannels), kNumChannels, result),
      IsOk());
  EXPECT_EQ(result, kExpectedThreeChannelsOfTwoTicks);
}

TEST(ConvertInterleavedToChannelTime, AppliesTransform) {
  const size_t kNumChannels = 2;
  constexpr std::array<int32_t, 4> kSamples = {1, 2, 3, 4};
  const std::vector<std::vector<int32_t>> kExpectedResult = {{2, 6}, {4, 8}};
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kDoublingTransform = [](int32_t input, int32_t& output) {
        output = input * 2;
        return absl::OkStatus();
      };
  std::vector<std::vector<int32_t>> result(kNumChannels);
  EXPECT_THAT(
      ConvertInterleavedToChannelTime(absl::MakeConstSpan(kSamples),
                                      kNumChannels, result, kDoublingTransform),
      IsOk());
  EXPECT_EQ(result, kExpectedResult);
}

TEST(ConvertChannelTimeToInterleaved, FailsIfSamplesHaveAnUnevenNumberOfTicks) {
  std::vector<std::vector<int32_t>> input = {{1, 2}, {3, 4, 5}};
  std::vector<int32_t> undefined_result;

  EXPECT_THAT(
      ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(input),
                                      undefined_result, kIdentityTransform),
      StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ConvertChannelTimeToInterleaved, PropagatesError) {
  const absl::Status kError = absl::InternalError("Test error");
  const std::vector<std::vector<int32_t>> kInput = {{1, 2, 3}, {4, 5, 6}};
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kAlwaysErrorTransform =
          [kError](int32_t /*input*/, int32_t& /*output*/) { return kError; };
  std::vector<int32_t> undefined_result;

  EXPECT_EQ(
      ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(kInput),
                                      undefined_result, kAlwaysErrorTransform),
      kError);
}

TEST(ConvertChannelTimeToInterleaved, SucceedsOnEmptyInput) {
  const std::vector<std::vector<int32_t>> kEmptyInput;
  std::vector<int32_t> result;

  EXPECT_THAT(ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(kEmptyInput),
                                              result, kIdentityTransform),
              IsOk());
  EXPECT_TRUE(result.empty());
}

TEST(ConvertChannelTimeToInterleaved, ClearsOutputVector) {
  const std::vector<std::vector<int32_t>> kInput = {{1}};
  std::vector<int32_t> result = {1, 2, 3};
  constexpr std::array<int32_t, 1> kExpectedResult{1};

  EXPECT_THAT(ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(kInput),
                                              result, kIdentityTransform),
              IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(ConvertChannelTimeToInterleaved, InterleavesResult) {
  const std::vector<std::vector<int32_t>> kInput = {{1, 4}, {2, 5}, {3, 6}};
  std::vector<int32_t> result;
  constexpr std::array<int32_t, 6> kExpectedResult{1, 2, 3, 4, 5, 6};

  EXPECT_THAT(ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(kInput),
                                              result, kIdentityTransform),
              IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(ConvertChannelTimeToInterleaved, DefaultToIdentityTransform) {
  const std::vector<std::vector<int32_t>> kInput = {{1, 4}, {2, 5}, {3, 6}};
  std::vector<int32_t> result;
  constexpr std::array<int32_t, 6> kExpectedResult{1, 2, 3, 4, 5, 6};

  // Skip the transform argument.
  EXPECT_THAT(
      ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(kInput), result),
      IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(ConvertChannelTimeToInterleaved, AppliesTransform) {
  const std::vector<std::vector<int32_t>> kInput = {{1, 4}, {2, 5}, {3, 6}};
  std::vector<int32_t> result;
  const absl::AnyInvocable<absl::Status(int32_t, int32_t&) const>
      kDoublingTransform = [](int32_t input, int32_t& output) {
        output = input * 2;
        return absl::OkStatus();
      };
  constexpr std::array<int32_t, 6> kExpectedResult{2, 4, 6, 8, 10, 12};

  EXPECT_THAT(ConvertChannelTimeToInterleaved(MakeSpanOfConstSpans(kInput),
                                              result, kDoublingTransform),
              IsOk());
  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

}  // namespace
}  // namespace iamf_tools
