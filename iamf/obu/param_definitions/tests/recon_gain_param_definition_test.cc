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
#include "iamf/obu/param_definitions/recon_gain_param_definition.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
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

ParamDefinition::BaseArgs GetDefaultReconGainBaseArgs() {
  return MakeOneSubblockParamDefinitionBaseArgs(
      kParameterId, /*parameter_rate=*/1, kDuration);
}

ReconGainParamDefinition CreateReconGainParamDefinition(
    const ParamDefinition::BaseArgs& args = GetDefaultReconGainBaseArgs(),
    uint32_t audio_element_id = 0) {
  return ReconGainParamDefinition(args, audio_element_id);
}

TEST(ReconGainParamDefinition, CopyConstructible) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();

  const auto other = recon_gain_param_definition;

  EXPECT_EQ(recon_gain_param_definition, other);
}

TEST(ReconGainParamDefinition, GetTypeHasCorrectValue) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();

  EXPECT_EQ(recon_gain_param_definition.GetType(),
            ParamDefinition::kParameterDefinitionReconGain);
}

TEST(ReconGainParamDefinitionValidateAndWrite,
     WritesCorrectlyWithDefaultValues) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            0x01,
                            // Parameter Definition Mode (upper bit).
                            0x00,
                            // Duration.
                            64,
                            // Constant Subblock Duration.
                            64});
}

TEST(ReconGainParamDefinitionValidateAndWrite, WritesParameterId) {
  auto args = GetDefaultReconGainBaseArgs();
  args.parameter_id = 1;
  auto recon_gain_param_definition = CreateReconGainParamDefinition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// Parameter ID.
                            0x01,
                            // Same as default.
                            0x01, 0x00, 64, 64});
}

TEST(ReconGainParamDefinitionValidateAndWrite, WritesParameterRate) {
  auto args = GetDefaultReconGainBaseArgs();
  args.parameter_id = 1;
  auto recon_gain_param_definition = CreateReconGainParamDefinition(args);
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x01,
                            // Parameter Rate.
                            0x01,
                            // Same as default.
                            0x00, 64, 64});
}

TEST(ReconGainParamDefinitionValidateAndWrite, WritesDuration) {
  auto recon_gain_param_definition =
      CreateReconGainParamDefinition(MakeOneSubblockParamDefinitionBaseArgs(
          kParameterId, /*parameter_rate=*/1, /*duration=*/32));
  WriteBitBuffer wb(kDefaultBufferSize);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {0x00, 0x01, 0x00,
                            // Duration.
                            32,
                            // Constant Subblock Duration.
                            32});
}

TEST(ReconGainParamDefinitionValidateAndWrite, AuxiliaryDataNotWritten) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition();
  WriteBitBuffer wb(kDefaultBufferSize);

  // Fill in some auxililary data.
  recon_gain_param_definition.aux_data_ = {
      {
          .recon_gain_is_present_flag = false,
          .channel_numbers_for_layer =
              {.surround = 2, .lfe = 0, .height = 0, .bottom = 0},
      },
      {
          .recon_gain_is_present_flag = true,
          .channel_numbers_for_layer =
              {.surround = 5, .lfe = 1, .height = 2, .bottom = 0},
      },
  };
  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  // Same as the bitstream in the `ReconGainParamDefinitionTest.Default` test
  // above, without the auxiliary data.
  ValidateWriteResults(wb, {// Parameter ID.
                            0x00,
                            // Parameter Rate.
                            0x01,
                            // Parameter Definition Mode (upper bit).
                            0x00,
                            // Duration.
                            64,
                            // Constant Subblock Duration.
                            64});
}

TEST(ReconGainParamDefinitionValidateAndWrite,
     NonMinimalLebGeneratorAffectsAllLeb128s) {
  auto leb_generator =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  ASSERT_NE(leb_generator, nullptr);
  auto recon_gain_param_definition =
      CreateReconGainParamDefinition(GetDefaultReconGainBaseArgs());
  WriteBitBuffer wb(kDefaultBufferSize, *leb_generator);

  EXPECT_THAT(recon_gain_param_definition.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {// `parameter_id`.
                            0x80, 0x00,
                            // `parameter_rate`.
                            0x81, 0x00,
                            // `param_definition_mode` (1), reserved (7).
                            0x00,
                            // `duration`.
                            0x80 | 64, 0x00,
                            // `constant_subblock_duration`.
                            0x80 | 64, 0x00});
}

TEST(ReconGainParamDefinitionValidate,
     InvalidWhenSubblockScheduleIsInParameterBlock) {
  auto recon_gain_param_definition = CreateReconGainParamDefinition(
      MakeScheduleInParameterBlockBaseArgs(kParameterId, kParameterRate));

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReconGainParamDefinitionValidate, InvalidWhenImpliedNumSubblocksIsNotOne) {
  auto base_args = MakeConstantSubblocksParamDefinitionBaseArgs(
      kParameterId, kParameterRate, kDuration, kDuration / 2);
  auto recon_gain_param_definition = CreateReconGainParamDefinition(base_args);

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReconGainParamDefinitionValidate,
     InvalidWhenDurationDoesNotEqualConstantSubblockDuration) {
  const DecodedUleb128 kMismatchingSubblockDuration = kDuration - 1;
  auto base_args = MakeConstantSubblocksParamDefinitionBaseArgs(
      kParameterId, kParameterRate, kDuration, kMismatchingSubblockDuration);
  auto recon_gain_param_definition = CreateReconGainParamDefinition(base_args);

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReconGainParamDefinitionValidate, InvalidWhenParamDefinitionModeIsOne) {
  auto args =
      MakeScheduleInParameterBlockBaseArgs(kParameterId, kParameterRate);
  auto recon_gain_param_definition = CreateReconGainParamDefinition(args);

  EXPECT_THAT(recon_gain_param_definition.Validate(), Not(IsOk()));
}

TEST(ReadReconGainParamDefinitionTest, ReadsCorrectlyWithDefaultValues) {
  std::vector<uint8_t> bitstream = {// Parameter ID.
                                    0x00,
                                    // Parameter Rate.
                                    0x01,
                                    // Parameter Definition Mode (upper bit).
                                    0x00,
                                    // Duration.
                                    64,
                                    // Constant Subblock Duration.
                                    64};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition =
      ReconGainParamDefinition(ParamDefinition::BaseArgs{}, 0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(param_definition.GetType(),
            ParamDefinition::kParameterDefinitionReconGain);
}

TEST(ReadReconGainParamDefinitionTest, ReadsMode1) {
  std::vector<uint8_t> bitstream = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x80};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition =
      ReconGainParamDefinition(ParamDefinition::BaseArgs{}, 0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
}

TEST(ReadReconGainParamDefinitionTest, Mode0NonZeroSubblockDuration) {
  std::vector<uint8_t> bitstream = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x00,
      // `duration`.
      0xc0, 0x00,
      // `constant_subblock_duration`.
      0xc0, 0x00};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition =
      ReconGainParamDefinition(ParamDefinition::BaseArgs{}, 0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
}

TEST(ReadReconGainParamDefinitionTest, Mode0SubblockArray) {
  std::vector<uint8_t> bitstream = {
      // Parameter ID.
      0x00,
      // Parameter Rate.
      1,
      // Param Definition Mode (upper bit), next 7 bits reserved.
      0x00,
      // `duration` (64).
      0xc0, 0x00,
      // `constant_subblock_duration` (64).
      0xc0, 0x00,
      // `num_subblocks`
      0x02,
      // `subblock_durations`
      // `subblock_duration[0]`
      64};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(bitstream));
  ReconGainParamDefinition param_definition =
      ReconGainParamDefinition(ParamDefinition::BaseArgs{}, 0);
  EXPECT_THAT(param_definition.ReadAndValidate(*buffer), IsOk());
}

}  // namespace
}  // namespace iamf_tools
