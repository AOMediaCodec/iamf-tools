#include "iamf/common/utils/validation_utils.h"

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
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::HasSubstr;
using ::testing::Not;

constexpr absl::string_view kOmitContext = "";
constexpr absl::string_view kCustomUserContext = "Custom User Context";
constexpr std::array<int, 4> kFourTestValues = {1, 2, 3, 4};

TEST(ValidateContainerSizeEqual, OkIfArgsAreEqual) {
  constexpr uint8_t kReportedSizeFour = 4;

  EXPECT_THAT(ValidateContainerSizeEqual(kOmitContext, kFourTestValues,
                                         kReportedSizeFour),
              IsOk());
}

TEST(ValidateContainerSizeEqual, NotOkIfArgsAreNotEquals) {
  constexpr uint8_t kInaccurateSizeFive = 5;

  EXPECT_FALSE(ValidateContainerSizeEqual(kOmitContext, kFourTestValues,
                                          kInaccurateSizeFive)
                   .ok());
}

TEST(ValidateContainerSizeEqual, MessageContainsContextOnError) {
  constexpr uint8_t kInaccurateSizeFive = 5;

  EXPECT_THAT(ValidateContainerSizeEqual(kCustomUserContext, kFourTestValues,
                                         kInaccurateSizeFive)
                  .message(),
              HasSubstr(kCustomUserContext));
}

TEST(ValidateEqual, OkIfArgsAreEqual) {
  const auto kLeftArg = 123;
  const auto kRightArg = 123;
  EXPECT_THAT(ValidateEqual(kLeftArg, kRightArg, kOmitContext), IsOk());
}

TEST(ValidateEqual, NotOkIfArgsAreNotEqual) {
  const auto kLeftArg = 123;
  const auto kUnequalRightArg = 223;
  EXPECT_FALSE(ValidateEqual(kLeftArg, kUnequalRightArg, kOmitContext).ok());
}

TEST(ValidateNotEqual, OkIfArgsAreNotEqual) {
  const auto kLeftArg = 123;
  const auto kRightArg = 124;
  EXPECT_THAT(ValidateNotEqual(kLeftArg, kRightArg, kOmitContext), IsOk());
}

TEST(ValidateNotEqual, NotOkIfArgsAreEqual) {
  const auto kLeftArg = 123;
  const auto kEqualRightArg = 123;
  EXPECT_FALSE(ValidateNotEqual(kLeftArg, kEqualRightArg, kOmitContext).ok());
}

TEST(ValidateHasValue, OkIfArgHasValue) {
  constexpr std::optional<int> kArg = 123;
  EXPECT_THAT(ValidateHasValue(kArg, kOmitContext), IsOk());
}

TEST(ValidateHasValue, NotOkIfArgDoesNotHaveValue) {
  constexpr std::optional<int> kArg = std::nullopt;
  EXPECT_FALSE(ValidateHasValue(kArg, kOmitContext).ok());
}

TEST(ValidateUnique, OkIfArgsAreUnique) {
  const std::vector<int> kVectorWithUniqueValues = {1, 2, 3, 99};

  EXPECT_THAT(ValidateUnique(kVectorWithUniqueValues.begin(),
                             kVectorWithUniqueValues.end(), kOmitContext),
              IsOk());
}

TEST(ValidateUnique, NotOkIfArgsAreNotUnique) {
  const std::vector<int> kVectorWithDuplicateValues = {1, 2, 3, 99, 1};

  EXPECT_FALSE(ValidateUnique(kVectorWithDuplicateValues.begin(),
                              kVectorWithDuplicateValues.end(), kOmitContext)
                   .ok());
}

TEST(ValidateInRange, OkIfValueInRange) {
  EXPECT_THAT(ValidateInRange(0, {-1, 1}, kOmitContext), IsOk());
  EXPECT_THAT(ValidateInRange(-1, {-1, 1}, kOmitContext), IsOk());
  EXPECT_THAT(ValidateInRange(1, {-1, 1}, kOmitContext), IsOk());
  EXPECT_THAT(ValidateInRange(1.1f, {1.0f, 1.2f}, kOmitContext), IsOk());
  EXPECT_THAT(
      ValidateInRange(uint8_t{254}, {uint8_t{253}, uint8_t{255}}, kOmitContext),
      IsOk());
  EXPECT_THAT(
      ValidateInRange(int64_t{-0xFFFFFE},
                      {int64_t{-0xFFFFFF}, int64_t{-0xFFFF}}, kOmitContext),
      IsOk());
}

TEST(ValidateInRange, InvalidIfValueOutOfRange) {
  EXPECT_THAT(ValidateInRange(2, {0, 1}, kOmitContext), Not(IsOk()));
  EXPECT_THAT(ValidateInRange(-1, {0, 1}, kOmitContext), Not(IsOk()));
  EXPECT_THAT(ValidateInRange(1.11f, {1.0f, 1.1f}, kOmitContext), Not(IsOk()));
  EXPECT_THAT(
      ValidateInRange(uint8_t{255}, {uint8_t{253}, uint8_t{254}}, kOmitContext),
      Not(IsOk()));
  EXPECT_THAT(
      ValidateInRange(int64_t{-0xFFFE}, {int64_t{-0xFFFFFF}, int64_t{-0xFFFF}},
                      kOmitContext),
      Not(IsOk()));
}

TEST(ValidateComparison, OkIfValidComparison) {
  EXPECT_THAT(Validate(1, std::less{}, 2, kOmitContext), IsOk());
  EXPECT_THAT(Validate(2.0f, std::greater{}, 1.0f, kOmitContext), IsOk());
  EXPECT_THAT(Validate(2, std::greater_equal{}, 2, kOmitContext), IsOk());
  EXPECT_THAT(Validate(1, std::equal_to{}, 1, kOmitContext), IsOk());
  EXPECT_THAT(Validate(2, std::not_equal_to{}, 1, kOmitContext), IsOk());
}

TEST(ValidateComparison, InvalidIfInvalidComparison) {
  EXPECT_THAT(Validate(2, std::less{}, 1, kOmitContext), Not(IsOk()));
  EXPECT_THAT(Validate(1.0f, std::greater{}, 2.0f, kOmitContext), Not(IsOk()));
  EXPECT_THAT(Validate(2, std::equal_to{}, 1, kOmitContext), Not(IsOk()));
  EXPECT_THAT(Validate(1, std::not_equal_to{}, 1, kOmitContext), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
