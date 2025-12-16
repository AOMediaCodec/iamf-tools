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

#include <cstdint>
#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr int32_t kDefaultBufferSize = 64;
constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 64;

class MockParamDefinition : public ParamDefinition {
 public:
  MOCK_METHOD(absl::Status, ValidateAndWrite, (WriteBitBuffer & wb),
              (const, override));
  MOCK_METHOD(absl::Status, ReadAndValidate, (ReadBitBuffer & rb), (override));

  MOCK_METHOD(std::unique_ptr<ParameterData>, CreateParameterData, (),
              (const, override));
  MOCK_METHOD(void, Print, (), (const, override));
};

void PopulateParameterDefinitionMode1(ParamDefinition& param_definition) {
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = 1;
  param_definition.param_definition_mode_ = 1;
  param_definition.reserved_ = 0;
}

void PopulateParameterDefinitionMode0(ParamDefinition& param_definition) {
  param_definition.parameter_id_ = kParameterId;
  param_definition.parameter_rate_ = kParameterRate;
  param_definition.param_definition_mode_ = 0;
  param_definition.duration_ = kDuration;
  param_definition.constant_subblock_duration_ = kDuration;
  param_definition.reserved_ = 0;
}

void InitSubblockDurations(
    ParamDefinition& param_definition,
    absl::Span<const DecodedUleb128> subblock_durations) {
  param_definition.InitializeSubblockDurations(
      static_cast<DecodedUleb128>(subblock_durations.size()));
  for (int i = 0; i < subblock_durations.size(); ++i) {
    EXPECT_THAT(param_definition.SetSubblockDuration(i, subblock_durations[i]),
                IsOk());
  }
}

TEST(GetNumSubblocks, ReturnsZeroWhenSubblockDurationsAreImplicitMode1) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.InitializeSubblockDurations(0);

  EXPECT_EQ(param_definition.GetNumSubblocks(), 0);
}

TEST(GetNumSubblocks, ReturnsZeroWhenSubblockDurationsAreImplicitMode0) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 64;
  param_definition.InitializeSubblockDurations(1);

  // TODO(b/345799072): Reporting zero is strange here, the parameter definition
  //                    represents one subblock, because the duration is implied
  //                    by "constant_subblock_duration". Also,
  //                    `GetSubblockDuration` calls would index out of bounds.
  EXPECT_EQ(param_definition.GetNumSubblocks(), 0);
}

TEST(GetNumSubblocks,
     ReturnsNumSubblocksWhenSubblockDurationsAreExplicitMode0) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  const DecodedUleb128 kNumSubblocks = 2;
  param_definition.InitializeSubblockDurations(kNumSubblocks);

  EXPECT_EQ(param_definition.GetNumSubblocks(), kNumSubblocks);
}

TEST(Validate, ValidatesParamDefinitionMode1) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, InvalidWhenParameterRateIsZero) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode1(param_definition);
  param_definition.parameter_rate_ = 0;

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, ValidatesParamDefinitionMode0) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, InvalidWhenParameterDefinitionMode0DurationIsZero) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 0;

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, InvalidWhenConstantSubblockDurationIsGreaterThanDuration) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 65;

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, ValidWhenConstantSubblockDurationIsLessThanDuration) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  // It is OK for `constant_subblock_duration` to be less than `duration`. The
  // spec has rounding rules for the final subblock duration.
  param_definition.constant_subblock_duration_ = 63;

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, ValidForExplicitSubblockDurations) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  // Subblock durations sum to 64.
  InitSubblockDurations(param_definition, {60, 4});

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, InvalidWhenSubblockDurationsSumIsLessThanDuration) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  // Subblock durations sum to less than 64.
  InitSubblockDurations(param_definition, {60, 3});

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, InvalidWhenSubblockDurationsSumIsGreaterThanDuration) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  // Subblock durations sum to less than 64.
  InitSubblockDurations(param_definition, {60, 5});

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(Validate, InvalidWhenAnySubblockDurationIsZero) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  // Subblock durations sum to less than 64.
  InitSubblockDurations(param_definition, {64, 0});

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(GetType, ReturnsNulloptForDefaultConstructor) {
  MockParamDefinition param_definition;

  EXPECT_EQ(param_definition.GetType(), std::nullopt);
}

TEST(GetSubblockDuration, MatchesExplicitSetSubblockDurations) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  const DecodedUleb128 kNumSubblocks = 2;
  param_definition.InitializeSubblockDurations(kNumSubblocks);

  const DecodedUleb128 kSubblockDuration0 = 60;
  const DecodedUleb128 kSubblockDuration1 = 4;
  EXPECT_THAT(param_definition.SetSubblockDuration(0, kSubblockDuration0),
              IsOk());
  EXPECT_THAT(param_definition.SetSubblockDuration(1, kSubblockDuration1),
              IsOk());

  EXPECT_EQ(param_definition.GetSubblockDuration(0), kSubblockDuration0);
  EXPECT_EQ(param_definition.GetSubblockDuration(1), kSubblockDuration1);
}

TEST(SetSubblockDuration, InvalidWhenSubblockIndexIsTooLarge) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  const DecodedUleb128 kNumSubblocks = 2;
  param_definition.InitializeSubblockDurations(kNumSubblocks);

  // The indices are zero-based, configure an off-by-one error.
  EXPECT_THAT(param_definition.SetSubblockDuration(2, 0), Not(IsOk()));
}

TEST(SetSubblockDuration, InvalidWhenSubblockIndexIsNegative) {
  MockParamDefinition param_definition;
  PopulateParameterDefinitionMode0(param_definition);
  param_definition.duration_ = 64;
  param_definition.constant_subblock_duration_ = 0;
  const DecodedUleb128 kNumSubblocks = 2;
  param_definition.InitializeSubblockDurations(kNumSubblocks);

  EXPECT_THAT(param_definition.SetSubblockDuration(-1, 0), Not(IsOk()));
}
}  // namespace
}  // namespace iamf_tools
