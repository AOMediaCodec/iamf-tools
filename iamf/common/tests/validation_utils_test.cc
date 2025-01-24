#include "iamf/common/validation_utils.h"

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

  EXPECT_FALSE(
      ValidateContainerSizeEqual("", kFourTestValues, kInaccurateSizeFive)
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

}  // namespace
}  // namespace iamf_tools
