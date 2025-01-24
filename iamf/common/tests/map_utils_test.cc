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
#include "iamf/common/map_utils.h"

#include <array>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

constexpr absl::string_view kOmitContext = "";
constexpr absl::string_view kCustomUserContext = "Custom User Context";

TEST(CopyFromMap, ReturnsOkWhenLookupSucceeds) {
  const absl::flat_hash_map<int, bool> kIntegerToIsPrime = {
      {1, false}, {2, true}, {3, true}, {4, false}};

  bool result;
  EXPECT_THAT(CopyFromMap(kIntegerToIsPrime, 3, kOmitContext, result), IsOk());

  EXPECT_TRUE(result);
}

TEST(CopyFromMap, ReturnsStatusNotFoundWhenLookupFails) {
  const absl::flat_hash_map<int, bool> kIntegerToIsPrime = {
      {1, false}, {2, true}, {3, true}, {4, false}};

  bool undefined_result;
  EXPECT_THAT(
      CopyFromMap(kIntegerToIsPrime, -1, kOmitContext, undefined_result),
      StatusIs(absl::StatusCode::kNotFound));
}

TEST(CopyFromMap, MessageContainsEmptyWhenMapIsEmpty) {
  const absl::flat_hash_map<int, bool> kEmptyMap = {};

  bool undefined_result;
  EXPECT_THAT(
      CopyFromMap(kEmptyMap, 3, kOmitContext, undefined_result).message(),
      HasSubstr("empty"));
}

TEST(CopyFromMap, MessageContainsContextOnError) {
  const absl::flat_hash_map<int, bool> kEmptyMap = {};

  bool undefined_result;
  EXPECT_THAT(
      CopyFromMap(kEmptyMap, 3, kCustomUserContext, undefined_result).message(),
      HasSubstr(kCustomUserContext));
}

TEST(LookupInMapStatusOr, OkIfLookupSucceeds) {
  const absl::flat_hash_map<int, bool> kIntegerToIsPrime = {
      {1, false}, {2, true}, {3, true}, {4, false}};

  EXPECT_THAT(LookupInMap(kIntegerToIsPrime, 3, kOmitContext),
              IsOkAndHolds(true));
}

TEST(LookupInMapStatusOr, ReturnsStatusNotFoundWhenLookupFails) {
  const absl::flat_hash_map<int, bool> kIntegerToIsPrime = {
      {1, false}, {2, true}, {3, true}, {4, false}};

  EXPECT_THAT(LookupInMap(kIntegerToIsPrime, -1, kOmitContext),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(LookupInMapStatusOr, MessageContainsContextOnError) {
  const absl::flat_hash_map<int, bool> kEmptyMap = {};

  EXPECT_THAT(LookupInMap(kEmptyMap, 3, kCustomUserContext).status().message(),
              HasSubstr(kCustomUserContext));
}

TEST(LookupInMapStatusOr, MessageContainsEmptyWhenMapIsEmpty) {
  const absl::flat_hash_map<int, bool> kEmptyMap = {};

  EXPECT_THAT(LookupInMap(kEmptyMap, 3, kOmitContext).status().message(),
              HasSubstr("empty"));
}

TEST(BuildStaticMapFromPairs, SucceedsOnEmptyContainer) {
  constexpr std::array<std::pair<int, float>, 0> kPairs{};
  static const auto kMap = BuildStaticMapFromPairs(kPairs);

  EXPECT_TRUE(kMap->empty());
}

TEST(BuildStaticMapFromPairs, BuildsMap) {
  constexpr std::array<std::pair<int, float>, 3> kPairs{
      {{1, 2.0f}, {3, 6.0f}, {5, 10.f}}};
  const absl::flat_hash_map<int, float> kExpectedMap = {
      {1, 2.0f}, {3, 6.0f}, {5, 10.f}};

  static const auto kMap = BuildStaticMapFromPairs(kPairs);

  EXPECT_EQ(*kMap, kExpectedMap);
}

TEST(BuildStaticMapFromPairs, BuildsMapWithDuplicateValues) {
  constexpr float kDuplicateValue = 2.0;
  constexpr std::array<std::pair<int, float>, 3> kPairsWithDuplicateSecond{
      {{1, kDuplicateValue}, {3, kDuplicateValue}, {5, 10.f}}};
  const absl::flat_hash_map<int, float> kExpectedMap = {
      {1, kDuplicateValue}, {3, kDuplicateValue}, {5, 10.f}};

  static const auto kMap = BuildStaticMapFromPairs(kPairsWithDuplicateSecond);

  EXPECT_EQ(*kMap, kExpectedMap);
}

TEST(BuildStaticMapFromPairs, ReturnsEmptyMapOnDuplicateKey) {
  constexpr int kDuplicateKey = 1;
  constexpr std::array<std::pair<int, float>, 3> kPairsWithDuplicateFirst{
      {{kDuplicateKey, 2.0f}, {kDuplicateKey, 6.0f}, {5, 10.f}}};

  static const auto kMap = BuildStaticMapFromPairs(kPairsWithDuplicateFirst);

  EXPECT_TRUE(kMap->empty());
}

TEST(BuildStaticMapFromInvertedPairs, SucceedsOnEmptyContainer) {
  constexpr std::array<std::pair<int, float>, 0> kEmptyPairs{};
  static const auto kMap = BuildStaticMapFromInvertedPairs(kEmptyPairs);

  EXPECT_TRUE(kMap->empty());
}

TEST(BuildStaticMapFromInvertedPairs, BuildsInvertedMap) {
  constexpr std::array<std::pair<int, float>, 3> kPairs{
      {{1, 2.0f}, {3, 6.0f}, {5, 10.f}}};
  const absl::flat_hash_map<float, int> kExpectedInvertedMap = {
      {2.0f, 1}, {6.0f, 3}, {10.f, 5}};

  static const auto kMap = BuildStaticMapFromInvertedPairs(kPairs);

  EXPECT_EQ(*kMap, kExpectedInvertedMap);
}

TEST(BuildStaticMapFromInvertedPairs, BuildsInvertedMapWithDuplicateValues) {
  constexpr int kDuplicateValue = 1;
  constexpr std::array<std::pair<int, float>, 3> kPairsWithDuplicateFirst{
      {{kDuplicateValue, 2.0f}, {kDuplicateValue, 6.0f}, {5, 10.f}}};
  const absl::flat_hash_map<float, int> kExpectedInvertedMap = {
      {2.0f, kDuplicateValue}, {6.0f, kDuplicateValue}, {10.f, 5}};

  static const auto kMap =
      BuildStaticMapFromInvertedPairs(kPairsWithDuplicateFirst);

  EXPECT_EQ(*kMap, kExpectedInvertedMap);
}

TEST(BuildStaticMapFromInvertedPairs, ReturnsEmptyMapOnDuplicateKey) {
  constexpr int kDuplicateKey = 1.0f;
  constexpr std::array<std::pair<int, float>, 3> kPairsWithDuplicateSecond{
      {{1, kDuplicateKey}, {3, kDuplicateKey}, {5, 10.f}}};

  static const auto kMap =
      BuildStaticMapFromInvertedPairs(kPairsWithDuplicateSecond);

  EXPECT_TRUE(kMap->empty());
}

}  // namespace
}  // namespace iamf_tools
