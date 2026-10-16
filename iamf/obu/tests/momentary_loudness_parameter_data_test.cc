/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/momentary_loudness_parameter_data.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

TEST(Create, SucceedsWithValidValues) {
  auto data = MomentaryLoudnessParameterData::Create(
      /*momentary_loudness=*/23, /*reserved=*/2);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->momentary_loudness(), 23);
  EXPECT_EQ(data->reserved(), 2);
}

TEST(Create, SucceedsWithBoundaryValues) {
  auto min_data = MomentaryLoudnessParameterData::Create(
      /*momentary_loudness=*/0, /*reserved=*/0);
  ASSERT_THAT(min_data, IsOk());
  EXPECT_EQ(min_data->momentary_loudness(), 0);
  EXPECT_EQ(min_data->reserved(), 0);

  auto max_data = MomentaryLoudnessParameterData::Create(
      /*momentary_loudness=*/63, /*reserved=*/3);
  ASSERT_THAT(max_data, IsOk());
  EXPECT_EQ(max_data->momentary_loudness(), 63);
  EXPECT_EQ(max_data->reserved(), 3);
}

TEST(Create, ReturnsErrorWhenMomentaryLoudnessOutOfRange) {
  EXPECT_THAT(MomentaryLoudnessParameterData::Create(
                  /*momentary_loudness=*/64, /*reserved=*/0),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenReservedOutOfRange) {
  EXPECT_THAT(MomentaryLoudnessParameterData::Create(
                  /*momentary_loudness=*/0, /*reserved=*/4),
              Not(IsOk()));
}

TEST(CreateFromBuffer, ReadsMomentaryLoudnessAndReserved) {
  // momentary_loudness (6 bits) = 42 (0b101010)
  // reserved (2 bits)           = 3  (0b11)
  const std::vector<uint8_t> source = {0b101010'11};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto data = MomentaryLoudnessParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ(data->momentary_loudness(), 42);
  EXPECT_EQ(data->reserved(), 3);
}

TEST(CreateFromBuffer, ReturnsErrorWhenBufferIsEmpty) {
  const std::vector<uint8_t> source = {};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(MomentaryLoudnessParameterData::CreateFromBuffer(*buffer),
              Not(IsOk()));
}

TEST(Write, WritesMomentaryLoudnessAndReserved) {
  auto data = MomentaryLoudnessParameterData::Create(
      /*momentary_loudness=*/42, /*reserved=*/3);
  ASSERT_THAT(data, IsOk());

  WriteBitBuffer wb(1);
  EXPECT_THAT(data->Write(wb), IsOk());
  ValidateWriteResults(wb, {0b101010'11});
}

TEST(ToString, FormatsMomentaryLoudnessAndReserved) {
  auto data = MomentaryLoudnessParameterData::Create(
      /*momentary_loudness=*/42, /*reserved=*/3);
  ASSERT_THAT(data, IsOk());

  EXPECT_EQ(absl::StrCat(*data),
            "    momentary_loudness= 42\n"
            "    reserved= 3");
}

}  // namespace
}  // namespace iamf_tools
