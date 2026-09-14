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
#include <string>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/subblock_schedule.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
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

TEST(GetNumSubblocks, ReturnsImplicitNumSubblocksMode0) {
  auto args = GetParamDefinitionMode0Args();
  MockParamDefinition param_definition(args);

  EXPECT_EQ(param_definition.GetNumSubblocks(), 1);
}

TEST(GetNumSubblocks,
     ReturnsNumSubblocksWhenSubblockDurationsAreExplicitMode0) {
  auto args = GetParamDefinitionMode0Args();
  auto schedule = SubblockSchedule::CreateWithVariableSubblockDuration(
      {kDuration / 2, kDuration / 2});
  ASSERT_THAT(schedule, IsOk());
  args.schedule = *schedule;
  MockParamDefinition param_definition(args);

  EXPECT_EQ(param_definition.GetNumSubblocks(), 2);
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

TEST(Validate, ValidWhenConstantSubblockDurationIsLessThanDuration) {
  auto args = GetParamDefinitionMode0Args();
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 63);
  ASSERT_THAT(schedule, IsOk());
  args.schedule = *schedule;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(Validate, ValidForExplicitSubblockDurations) {
  auto args = GetParamDefinitionMode0Args();
  auto schedule = SubblockSchedule::CreateWithVariableSubblockDuration({60, 4});
  ASSERT_THAT(schedule, IsOk());
  args.schedule = *schedule;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), IsOk());
}

TEST(GetSubblockDuration, MatchesExplicitSetSubblockDurations) {
  auto args = GetParamDefinitionMode0Args();
  constexpr DecodedUleb128 kSubblockDuration0 = 60;
  constexpr DecodedUleb128 kSubblockDuration1 = 4;
  auto schedule = SubblockSchedule::CreateWithVariableSubblockDuration(
      {kSubblockDuration0, kSubblockDuration1});
  ASSERT_THAT(schedule, IsOk());
  args.schedule = *schedule;
  MockParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.GetSchedule()->GetSubblockDuration(0),
              IsOkAndHolds(kSubblockDuration0));
  EXPECT_THAT(param_definition.GetSchedule()->GetSubblockDuration(1),
              IsOkAndHolds(kSubblockDuration1));
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

/*!\brief Minimal concrete subclass of ParamDefinition for testing. */
class ConcreteParamDefinition : public ParamDefinition {
 public:
  /*!\brief Constructor.
   *
   * \param args Arguments for `ParamDefinition`.
   */
  explicit ConcreteParamDefinition(const ParamDefinition::BaseArgs& args)
      : ParamDefinition(kParameterDefinitionMixGain, args) {}

  /*!\brief Stub implementation of ValidateAndWrite.
   *
   * \param wb Buffer to write to.
   * \return Always returns OkStatus.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override {
    return absl::OkStatus();
  }

  /*!\brief Stub implementation of ReadAndValidate.
   *
   * \param rb Buffer to read from.
   * \return Always returns OkStatus.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override {
    return absl::OkStatus();
  }

  /*!\brief Stub implementation of CreateParameterDataFromBuffer.
   *
   * \param rb Buffer to read from.
   * \return Always returns nullptr.
   */
  absl::StatusOr<std::unique_ptr<ParameterData>> CreateParameterDataFromBuffer(
      ReadBitBuffer& rb) const override {
    return nullptr;
  }
};

TEST(AbslStringify, FormatsWithoutSchedule) {
  const ConcreteParamDefinition param_definition(GetParamDefinitionMode1Args());

  const std::string formatted = absl::StrCat(param_definition);

  EXPECT_EQ(formatted,
            "  parameter_type= 0\n"
            "  parameter_id= 0\n"
            "  parameter_rate= 48000\n"
            "  param_definition_mode= 1\n"
            "  reserved= 0");
}

TEST(AbslStringify, FormatsWithSchedule) {
  const ConcreteParamDefinition param_definition(GetParamDefinitionMode0Args());

  const std::string formatted = absl::StrCat(param_definition);

  EXPECT_EQ(formatted,
            "  parameter_type= 0\n"
            "  parameter_id= 0\n"
            "  parameter_rate= 48000\n"
            "  param_definition_mode= 0\n"
            "  reserved= 0\n"
            "  duration= 64\n"
            "  constant_subblock_duration= 64\n"
            "  num_subblocks= 1");
}

}  // namespace
}  // namespace iamf_tools
