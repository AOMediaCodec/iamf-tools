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
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr int32_t kDefaultBufferSize = 64;
constexpr DecodedUleb128 kParameterId = 0;
constexpr DecodedUleb128 kParameterRate = 48000;
constexpr DecodedUleb128 kDuration = 64;

ParamDefinition::BaseArgs GetParamDefinitionMode1Args() {
  return MakeScheduleInParameterBlockBaseArgs(kParameterId,
                                              /*parameter_rate=*/1);
}

TEST(MixGainParamDefinition, ConstructorSetsDefaultMixGainToZero) {
  MixGainParamDefinition mix_gain_param_definition(ParamDefinition::BaseArgs{});

  EXPECT_EQ(mix_gain_param_definition.default_mix_gain_.GetQ7_8(), 0);
}

TEST(MixGainParamDefinition, CopyConstructible) {
  MixGainParamDefinition mix_gain_param_definition(
      GetParamDefinitionMode1Args());
  mix_gain_param_definition.default_mix_gain_ =
      QFormatOrFloatingPoint::MakeFromQ7_8(-16);

  const auto other = mix_gain_param_definition;

  EXPECT_EQ(mix_gain_param_definition, other);
}

TEST(MixGainParamDefinition, GetTypeHasCorrectValue) {
  MixGainParamDefinition mix_gain_param_definition(ParamDefinition::BaseArgs{});

  EXPECT_EQ(mix_gain_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionMixGain);
}

TEST(MixGainParamDefinitionValidateAndWrite, DefaultParamDefinitionMode1) {
  MixGainParamDefinition mix_gain_param_definition(
      GetParamDefinitionMode1Args());
  mix_gain_param_definition.default_mix_gain_ =
      QFormatOrFloatingPoint::MakeFromQ7_8(0);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(mix_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            1,
                            // Param Definition Mode (upper bit).
                            0x80,
                            // Default Mix Gain.
                            0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, WritesParameterId) {
  auto args = GetParamDefinitionMode1Args();
  args.parameter_id = 1;
  MixGainParamDefinition mix_gain_param_definition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(mix_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x01,
                            // Same as default.
                            1, 0x80, 0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, NonMinimalLeb) {
  auto args = GetParamDefinitionMode1Args();
  args.parameter_id = 1;
  args.parameter_rate = 5;
  MixGainParamDefinition mix_gain_param_definition(args);
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  ASSERT_NE(leb_generator, nullptr);
  WriteBitBuffer wb(kDefaultBufferSize, *leb_generator);

  EXPECT_THAT(mix_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x81, 0x00,
                            // Parameter Rate.
                            0x85, 0x00,
                            // Same as default.
                            0x80, 0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, WritesParameterRate) {
  auto args = GetParamDefinitionMode1Args();
  args.parameter_rate = 64;
  MixGainParamDefinition param_definition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00,
                            // Parameter Rate.
                            64,
                            // Same as default.
                            0x80, 0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite, WritesDefaultMixGain) {
  MixGainParamDefinition param_definition(GetParamDefinitionMode1Args());
  param_definition.default_mix_gain_ = QFormatOrFloatingPoint::MakeFromQ7_8(3);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 1, 0x80,
                            // Default Mix Gain.
                            0, 3});
}

TEST(MixGainParamDefinitionValidate, ParameterRateMustNotBeZero) {
  auto args = GetParamDefinitionMode1Args();
  args.parameter_rate = 0;
  MixGainParamDefinition param_definition(args);

  EXPECT_THAT(param_definition.Validate(), Not(IsOk()));
}

TEST(MixGainParamDefinitionValidateAndWrite,
     ParamDefinitionMode0WithConstantSubblockDurationNonZero) {
  MixGainParamDefinition param_definition(
      MakeOneSubblockParamDefinitionBaseArgs(kParameterId, /*parameter_rate=*/1,
                                             /*duration=*/3));
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            1,
                            // Param Definition Mode (upper bit).
                            0,
                            // Duration.
                            3,
                            // Constant Subblock Duration.
                            3,
                            // Default Mix Gain.
                            0, 0});
}

TEST(MixGainParamDefinitionValidateAndWrite,
     ParamDefinitionModeWithVariableSubblockDurations) {
  MixGainParamDefinition param_definition(
      MakeVariableSubblocksParamDefinitionBaseArgs(
          kParameterId, /*parameter_rate=*/1, {1, 2, 3, 4}));

  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            1,
                            // Parameter Definition Mode (upper bit).
                            0,
                            // Duration.
                            10,
                            // Constant Subblock Duration.
                            0,
                            // Num subblocks.
                            4,
                            // Subblock 0.
                            1,
                            // Subblock 1.
                            2,
                            // Subblock 2.
                            3,
                            // Subblock 3.
                            4,
                            // Default Mix Gain.
                            0, 0});
}

TEST(ReadMixGainParamDefinitionTest, DefaultMixGainMode1) {
  MixGainParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> source = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x80,
      // Default Mix Gain.
      0, 4};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
            ParamDefinition::kParameterDefinitionMixGain);
  EXPECT_EQ(param_definition.default_mix_gain_.GetQ7_8(), 4);
}

TEST(ReadMixGainParamDefinitionTest, DefaultMixGainWithSubblockArray) {
  MixGainParamDefinition param_definition(ParamDefinition::BaseArgs{});
  std::vector<uint8_t> source = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x00,
      // `duration` (64).
      0xc0, 0x00,
      // `constant_subblock_duration`.
      0x00,
      // `num_subblocks`
      0x02,
      // `subblock_durations`
      // `subblock_duration[0]`
      40,
      // `subblock_duration[1]`
      24,
      // Default Mix Gain.
      0, 3};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(*param_definition.GetType(),
            ParamDefinition::kParameterDefinitionMixGain);
  EXPECT_EQ(param_definition.default_mix_gain_.GetQ7_8(), 3);
}

}  // namespace
}  // namespace iamf_tools
