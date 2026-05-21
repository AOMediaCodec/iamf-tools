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
#include "iamf/obu/param_definitions/param_definition_base.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAre;
using ::testing::Not;

using absl::MakeConstSpan;

constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 64;

ParamDefinition::BaseArgs GetParamDefinitionMode1Args() {
  return MakeScheduleInParameterBlockBaseArgs(kParameterId, kParameterRate);
}

ParamDefinition::BaseArgs GetParamDefinitionMode0Args() {
  return MakeOneSubblockParamDefinitionBaseArgs(kParameterId, kParameterRate,
                                                kDuration);
}

TEST(GetNumSubblocks, ReturnsZeroWhenSubblockDurationsAreImplicitMode1) {
  MockParamDefinition param_definition(GetParamDefinitionMode1Args());

  EXPECT_EQ(param_definition.GetNumSubblocks(), 0);
}

TEST(GetNumSubblocks, ReturnsZeroWhenSubblockDurationsAreImplicitMode0) {
  auto args = GetParamDefinitionMode0Args();
  args.constant_subblock_duration = kDuration;
  MockParamDefinition param_definition(args);

  // TODO(b/345799072): Reporting zero is strange here, the parameter definition
  //                    represents one subblock, because the duration is implied
  //                    by "constant_subblock_duration". Also,
  //                    `GetSubblockDuration` calls would index out of bounds.
  EXPECT_EQ(param_definition.GetNumSubblocks(), 0);
}

TEST(GetNumSubblocks,
     ReturnsNumSubblocksWhenSubblockDurationsAreExplicitMode0) {
  auto args = GetParamDefinitionMode0Args();
  args.constant_subblock_duration = 0;
  constexpr DecodedUleb128 kNumSubblocks = 2;
  args.num_subblocks = kNumSubblocks;
  MockParamDefinition param_definition(args);

  EXPECT_EQ(param_definition.GetNumSubblocks(), kNumSubblocks);
}

TEST(Validate, ValidatesParamDefinitionMode1) {
  MockParamDefinition param_definition(GetParamDefinitionMode1Args());

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, InvalidWhenParameterRateIsZero) {
  auto args = GetParamDefinitionMode1Args();
  args.parameter_rate = 0;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, ValidatesParamDefinitionMode0) {
  MockParamDefinition param_definition(GetParamDefinitionMode0Args());

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, InvalidWhenParameterDefinitionMode0DurationIsZero) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 0;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, InvalidWhenConstantSubblockDurationIsGreaterThanDuration) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  args.constant_subblock_duration = 65;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, ValidWhenConstantSubblockDurationIsLessThanDuration) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  // It is OK for `constant_subblock_duration` to be less than `duration`. The
  // spec has rounding rules for the final subblock duration.
  args.constant_subblock_duration = 63;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, ValidForExplicitSubblockDurations) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  args.constant_subblock_duration = 0;
  args.num_subblocks = 2;
  args.subblock_durations = {60, 4};
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, InvalidWhenSubblockDurationsSumIsLessThanDuration) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  args.constant_subblock_duration = 0;
  args.num_subblocks = 2;
  args.subblock_durations = {60, 3};
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, InvalidWhenSubblockDurationsSumIsGreaterThanDuration) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  args.constant_subblock_duration = 0;
  args.num_subblocks = 2;
  args.subblock_durations = {60, 5};
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, InvalidWhenAnySubblockDurationIsZero) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  args.constant_subblock_duration = 0;
  args.num_subblocks = 2;
  args.subblock_durations = {64, 0};
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(GetSubblockDuration, MatchesExplicitSetSubblockDurations) {
  auto args = GetParamDefinitionMode0Args();
  args.duration = 64;
  args.constant_subblock_duration = 0;
  args.num_subblocks = 2;
  constexpr DecodedUleb128 kSubblockDuration0 = 60;
  constexpr DecodedUleb128 kSubblockDuration1 = 4;
  args.subblock_durations = {kSubblockDuration0, kSubblockDuration1};
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.GetSubblockDurations(),
              ElementsAre(kSubblockDuration0, kSubblockDuration1));
}

TEST(ReadAndValidate, InvalidWhenNumSubblocksExceedsMaximum) {
  constexpr auto source = std::to_array<uint8_t>(
      {// Parameter ID.
       0x00,
       // Parameter Rate.
       1,
       // Param Definition Mode (upper bit), next 7 bits reserved.
       0x00,
       // `duration` (64).
       0xc0, 0x00,
       // `constant_subblock_duration`.
       0x00,
       // `num_subblocks` (exceeds maximum 192000).
       0x81, 0xf7, 0x0b});
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));
  MockParamDefinition param_definition;

  EXPECT_THAT(param_definition.ParamDefinition::ReadAndValidate(*buffer),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
