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
#include "iamf/obu/momentary_loudness.h"

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
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Not;

MomentaryLoudnessParamDefinition CreateValidParamDefinition() {
  return MomentaryLoudnessParamDefinition(
      MakeOneSubblockParamDefinitionBaseArgs(
          /*parameter_id=*/127, /*parameter_rate=*/48000, /*duration=*/1024));
}

TEST(Create, PopulatesFieldsFromValidInputs) {
  const MomentaryLoudnessParamDefinition param_def =
      CreateValidParamDefinition();
  const std::vector<DecodedUleb128> kCounts = {10, 20, 30, 40};

  const auto momentary_loudness = MomentaryLoudness::Create(
      param_def, /*bin_width_minus_one=*/2, /*first_bin_center=*/32,
      /*reserved_momentary_loudness=*/0, kCounts);

  ASSERT_THAT(momentary_loudness, IsOk());
  EXPECT_EQ(momentary_loudness->momentary_loudness_param_def(), param_def);
  EXPECT_EQ(momentary_loudness->num_bin_pairs_minus_one(), 1);
  EXPECT_EQ(momentary_loudness->bin_width_minus_one(), 2);
  EXPECT_EQ(momentary_loudness->first_bin_center(), 32);
  EXPECT_EQ(momentary_loudness->reserved_momentary_loudness(), 0);
  EXPECT_THAT(momentary_loudness->count(), ElementsAre(10, 20, 30, 40));
}

TEST(Create, SucceedsWithMinBoundaryValues) {
  const MomentaryLoudnessParamDefinition param_def =
      CreateValidParamDefinition();
  const std::vector<DecodedUleb128> kMinCounts = {0, 0};

  const auto momentary_loudness = MomentaryLoudness::Create(
      param_def, /*bin_width_minus_one=*/0, /*first_bin_center=*/0,
      /*reserved_momentary_loudness=*/0, kMinCounts);

  ASSERT_THAT(momentary_loudness, IsOk());
  EXPECT_EQ(momentary_loudness->num_bin_pairs_minus_one(), 0);
  EXPECT_EQ(momentary_loudness->bin_width_minus_one(), 0);
  EXPECT_EQ(momentary_loudness->first_bin_center(), 0);
  EXPECT_EQ(momentary_loudness->reserved_momentary_loudness(), 0);
  EXPECT_THAT(momentary_loudness->count(), ElementsAre(0, 0));
}

TEST(Create, SucceedsWithMaxBoundaryValues) {
  const MomentaryLoudnessParamDefinition param_def =
      CreateValidParamDefinition();
  const std::vector<DecodedUleb128> kMaxCounts(16, 100);

  const auto momentary_loudness = MomentaryLoudness::Create(
      param_def, /*bin_width_minus_one=*/63, /*first_bin_center=*/63,
      /*reserved_momentary_loudness=*/1, kMaxCounts);

  ASSERT_THAT(momentary_loudness, IsOk());
  EXPECT_EQ(momentary_loudness->num_bin_pairs_minus_one(), 7);
  EXPECT_EQ(momentary_loudness->bin_width_minus_one(), 63);
  EXPECT_EQ(momentary_loudness->first_bin_center(), 63);
  EXPECT_EQ(momentary_loudness->reserved_momentary_loudness(), 1);
  EXPECT_THAT(momentary_loudness->count(), ElementsAreArray(kMaxCounts));
}

TEST(Create, ReturnsErrorWhenParamDefinitionIsInvalid) {
  const MomentaryLoudnessParamDefinition invalid_param_def(
      ParamDefinition::BaseArgs{.parameter_id = 1, .parameter_rate = 0});

  EXPECT_THAT(MomentaryLoudness::Create(
                  invalid_param_def, /*bin_width_minus_one=*/0,
                  /*first_bin_center=*/0, /*reserved_momentary_loudness=*/0,
                  /*count=*/{1, 2}),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenBinWidthMinusOneExceedsMax) {
  EXPECT_THAT(MomentaryLoudness::Create(
                  CreateValidParamDefinition(), /*bin_width_minus_one=*/64,
                  /*first_bin_center=*/0, /*reserved_momentary_loudness=*/0,
                  /*count=*/{1, 2}),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenFirstBinCenterExceedsMax) {
  EXPECT_THAT(MomentaryLoudness::Create(
                  CreateValidParamDefinition(), /*bin_width_minus_one=*/0,
                  /*first_bin_center=*/64, /*reserved_momentary_loudness=*/0,
                  /*count=*/{1, 2}),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenReservedMomentaryLoudnessExceedsMax) {
  EXPECT_THAT(MomentaryLoudness::Create(
                  CreateValidParamDefinition(), /*bin_width_minus_one=*/0,
                  /*first_bin_center=*/0, /*reserved_momentary_loudness=*/2,
                  /*count=*/{1, 2}),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenCountIsEmpty) {
  EXPECT_THAT(MomentaryLoudness::Create(
                  CreateValidParamDefinition(), /*bin_width_minus_one=*/0,
                  /*first_bin_center=*/0, /*reserved_momentary_loudness=*/0,
                  /*count=*/{}),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenCountSizeIsOdd) {
  EXPECT_THAT(MomentaryLoudness::Create(
                  CreateValidParamDefinition(), /*bin_width_minus_one=*/0,
                  /*first_bin_center=*/0, /*reserved_momentary_loudness=*/0,
                  /*count=*/{1, 2, 3}),
              Not(IsOk()));
}

TEST(Create, ReturnsErrorWhenCountSizeExceedsMax) {
  const std::vector<DecodedUleb128> kTooManyCounts(18, 1);

  EXPECT_THAT(MomentaryLoudness::Create(
                  CreateValidParamDefinition(), /*bin_width_minus_one=*/0,
                  /*first_bin_center=*/0, /*reserved_momentary_loudness=*/0,
                  kTooManyCounts),
              Not(IsOk()));
}

TEST(CreateFromBuffer, DeserializesSingleBinPair) {
  const std::vector<uint8_t> source = {
      // `momentary_loudness_param_def`:
      0x7f,              // `parameter_id` = 127
      0x80, 0xf7, 0x02,  // `parameter_rate` = 48000
      0x00,              // `param_definition_mode` (1) = 0, `reserved` (7) = 0
      0x80, 0x08,        // `duration` = 1024
      0x80, 0x08,        // `constant_subblock_duration` = 1024
      // `num_bin_pairs_minus_one` (3) = 0, `bin_width_minus_one` (6) = 63,
      // `first_bin_center` (6) = 32, `reserved_momentary_loudness` (1) = 0:
      0b000'11111, 0b1'100000'0,
      // `count` (2 elements):
      24, 5};
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  const auto momentary_loudness = MomentaryLoudness::CreateFromBuffer(*rb);

  ASSERT_THAT(momentary_loudness, IsOk());
  EXPECT_EQ(momentary_loudness->momentary_loudness_param_def(),
            CreateValidParamDefinition());
  EXPECT_EQ(momentary_loudness->num_bin_pairs_minus_one(), 0);
  EXPECT_EQ(momentary_loudness->bin_width_minus_one(), 63);
  EXPECT_EQ(momentary_loudness->first_bin_center(), 32);
  EXPECT_EQ(momentary_loudness->reserved_momentary_loudness(), 0);
  EXPECT_THAT(momentary_loudness->count(), ElementsAre(24, 5));
}

TEST(CreateFromBuffer, DeserializesMaxBinPairs) {
  const std::vector<uint8_t> source = {
      // `momentary_loudness_param_def`:
      1,   // `parameter_id`
      1,   // `parameter_rate`
      0,   // `param_definition_mode` (1) = 0, `reserved` (7) = 0
      10,  // `duration`
      10,  // `constant_subblock_duration`
      // `num_bin_pairs_minus_one` (3) = 7, `bin_width_minus_one` (6) = 1,
      // `first_bin_center` (6) = 2, `reserved_momentary_loudness` (1) = 1:
      0b111'00000, 0b1'000010'1,
      // `count` (16 elements):
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  const auto momentary_loudness = MomentaryLoudness::CreateFromBuffer(*rb);

  ASSERT_THAT(momentary_loudness, IsOk());
  EXPECT_EQ(momentary_loudness->num_bin_pairs_minus_one(), 7);
  EXPECT_EQ(momentary_loudness->bin_width_minus_one(), 1);
  EXPECT_EQ(momentary_loudness->first_bin_center(), 2);
  EXPECT_EQ(momentary_loudness->reserved_momentary_loudness(), 1);
  EXPECT_THAT(
      momentary_loudness->count(),
      ElementsAre(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
}

TEST(CreateFromBuffer, ReturnsErrorWhenParamDefinitionIsInvalid) {
  const std::vector<uint8_t> source = {
      // `momentary_loudness_param_def` with invalid `parameter_rate` = 0:
      1,   // `parameter_id`
      0,   // `parameter_rate` = 0 (invalid)
      0,   // `param_definition_mode` (1) = 0, `reserved` (7) = 0
      10,  // `duration`
      10,  // `constant_subblock_duration`
      0x00, 0x00, 1, 2};
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(MomentaryLoudness::CreateFromBuffer(*rb), Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorWhenBufferIsTruncatedBeforeBitfields) {
  const std::vector<uint8_t> source = {
      1,   // `parameter_id`
      1,   // `parameter_rate`
      0,   // `param_definition_mode` (1) = 0, `reserved` (7) = 0
      10,  // `duration`
      10,  // `constant_subblock_duration`
      0x00};
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(MomentaryLoudness::CreateFromBuffer(*rb), Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorWhenBufferHasFewerCountsThanIndicated) {
  const std::vector<uint8_t> source = {
      1,   // `parameter_id`
      1,   // `parameter_rate`
      0,   // `param_definition_mode` (1) = 0, `reserved` (7) = 0
      10,  // `duration`
      10,  // `constant_subblock_duration`
      // `num_bin_pairs_minus_one` (3) = 1 -> expects 4 counts, only 3 follow:
      0b001'00000, 0b0'000000'0, 1, 2, 3};
  auto rb =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(MomentaryLoudness::CreateFromBuffer(*rb), Not(IsOk()));
}

TEST(Write, SerializesSingleBinPair) {
  const auto momentary_loudness = MomentaryLoudness::Create(
      CreateValidParamDefinition(), /*bin_width_minus_one=*/63,
      /*first_bin_center=*/32, /*reserved_momentary_loudness=*/0,
      /*count=*/{24, 5});
  ASSERT_THAT(momentary_loudness, IsOk());
  WriteBitBuffer wb(16);

  EXPECT_THAT(momentary_loudness->Write(wb), IsOk());

  ValidateWriteResults(
      wb, {
              // `momentary_loudness_param_def`:
              0x7f,  // `parameter_id` = 127
              0x80,
              0xf7,
              0x02,  // `parameter_rate` = 48000
              0x00,  // `param_definition_mode` (1) = 0, `reserved` (7) = 0
              0x80,
              0x08,  // `duration` = 1024
              0x80,
              0x08,  // `constant_subblock_duration` = 1024
                     // `num_bin_pairs_minus_one` (3) = 0, `bin_width_minus_one`
                     // (6) = 63, `first_bin_center` (6) = 32,
                     // `reserved_momentary_loudness` (1) = 0:
              0b000'11111,
              0b1'100000'0,
              // `count`:
              24,
              5,
          });
}

TEST(Write, RoundTripsWithCreateFromBuffer) {
  const MomentaryLoudnessParamDefinition param_def(
      MakeOneSubblockParamDefinitionBaseArgs(
          /*parameter_id=*/5, /*parameter_rate=*/48000, /*duration=*/960));
  const auto original = MomentaryLoudness::Create(
      param_def, /*bin_width_minus_one=*/3, /*first_bin_center=*/28,
      /*reserved_momentary_loudness=*/1, /*count=*/{100, 200, 300, 400});
  ASSERT_THAT(original, IsOk());
  WriteBitBuffer wb(32);
  ASSERT_THAT(original->Write(wb), IsOk());
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(wb.bit_buffer()));

  const auto round_tripped = MomentaryLoudness::CreateFromBuffer(*rb);

  ASSERT_THAT(round_tripped, IsOk());
  EXPECT_EQ(*round_tripped, *original);
}

TEST(ToString, FormatsFieldsAndSupportsAbslStringify) {
  const MomentaryLoudnessParamDefinition param_def(
      MakeOneSubblockParamDefinitionBaseArgs(
          /*parameter_id=*/1, /*parameter_rate=*/1, /*duration=*/10));
  const auto momentary_loudness = MomentaryLoudness::Create(
      param_def, /*bin_width_minus_one=*/2, /*first_bin_center=*/32,
      /*reserved_momentary_loudness=*/1, /*count=*/{10, 20});
  ASSERT_THAT(momentary_loudness, IsOk());

  const std::string result = momentary_loudness->ToString();

  EXPECT_EQ(absl::StrCat(*momentary_loudness), result);
  EXPECT_EQ(result,
            "MomentaryLoudnessParamDefinition:\n"
            "  parameter_type= 9\n"
            "  parameter_id= 1\n"
            "  parameter_rate= 1\n"
            "  param_definition_mode= 0\n"
            "  reserved= 0\n"
            "  duration= 10\n"
            "  constant_subblock_duration= 10\n"
            "  num_subblocks= 1\n"
            "num_bin_pairs_minus_one= 0\n"
            "bin_width_minus_one= 2\n"
            "first_bin_center= 32\n"
            "reserved_momentary_loudness= 1\n"
            "count[0]= 10\n"
            "count[1]= 20");
}

}  // namespace
}  // namespace iamf_tools
