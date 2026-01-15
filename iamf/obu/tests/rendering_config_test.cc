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
#include "iamf/obu/rendering_config.h"

#include <array>
#include <cstdint>
#include <variant>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/cart16_param_definition.h"
#include "iamf/obu/param_definitions/cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart16_param_definition.h"
#include "iamf/obu/param_definitions/dual_cart8_param_definition.h"
#include "iamf/obu/param_definitions/dual_polar_param_definition.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/polar_param_definition.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using testing::ElementsAreArray;
using ::testing::Not;

using absl::MakeConstSpan;

using enum RenderingConfig::HeadphonesRenderingMode;

TEST(ValidateAndWrite, WritesStereoRenderingConfig) {
  RenderingConfig rendering_config{.headphones_rendering_mode =
                                       kHeadphonesRenderingModeStereo};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `headphones_rendering_mode` (2), reserved (6).
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_size`.
      0,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesBinauralWorldLockedRenderingConfig) {
  RenderingConfig rendering_config{
      .headphones_rendering_mode = kHeadphonesRenderingModeBinauralWorldLocked};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `headphones_rendering_mode` (2), reserved (6).
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      // `rendering_config_extension_size`.
      0,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesBinauralHeadLockedRenderingConfig) {
  RenderingConfig rendering_config{
      .headphones_rendering_mode = kHeadphonesRenderingModeBinauralHeadLocked};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `headphones_rendering_mode` (2), reserved (6).
      RenderingConfig::kHeadphonesRenderingModeBinauralHeadLocked << 6,
      // `rendering_config_extension_size`.
      0,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesReservedRenderingConfig) {
  RenderingConfig rendering_config{.headphones_rendering_mode =
                                       kHeadphonesRenderingModeReserved3};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `headphones_rendering_mode` (2), reserved (6).
      RenderingConfig::kHeadphonesRenderingModeReserved3 << 6,
      // `rendering_config_extension_size`.
      0,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, CreateFailsWithOverflowReservedField) {
  RenderingConfig rendering_config{
      .headphones_rendering_mode = kHeadphonesRenderingModeBinauralWorldLocked,
      .reserved = 64};

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), Not(IsOk()));
};

TEST(ValidateAndWrite, WritesRenderingConfigExtension) {
  RenderingConfig rendering_config{
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_extension_bytes = {'e', 'x'}};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>(
      {RenderingConfig::kHeadphonesRenderingModeStereo << 6,
       // `rendering_config_extension_size`.
       3,
       // `num_parameters`.
       0,
       // `rendering_config_extension_bytes`.
       'e', 'x'});

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesRenderingConfigPolarParamDefinition) {
  PolarParamDefinition polar_param_definition_1;
  polar_param_definition_1.parameter_id_ = 1;
  polar_param_definition_1.parameter_rate_ = 1;
  polar_param_definition_1.param_definition_mode_ = false;
  polar_param_definition_1.duration_ = 10;
  polar_param_definition_1.constant_subblock_duration_ = 10;
  polar_param_definition_1.default_azimuth_ = 2;
  polar_param_definition_1.default_elevation_ = 3;
  polar_param_definition_1.default_distance_ = 4;

  PolarParamDefinition polar_param_definition_2;
  polar_param_definition_2.parameter_id_ = 2;
  polar_param_definition_2.parameter_rate_ = 1;
  polar_param_definition_2.param_definition_mode_ = false;
  polar_param_definition_2.duration_ = 10;
  polar_param_definition_2.constant_subblock_duration_ = 10;
  polar_param_definition_2.default_azimuth_ = 181;
  polar_param_definition_2.default_elevation_ = 3;
  polar_param_definition_2.default_distance_ = 4;

  RenderingConfig rendering_config = {
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_param_definitions = {
          RenderingConfigParamDefinition::Create(polar_param_definition_1, {}),
          RenderingConfigParamDefinition::Create(polar_param_definition_2,
                                                 {})}};

  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_bytes size`.
      19,
      // `num_parameters`.
      2,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionPolar,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_azimuth = 2 (9 bits)
      // default_elevation = 3 (8 bits)
      // default_distance = 4 (7 bits)
      // 00000001 00000001 10000100
      0x01,
      0x01,
      0x84,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionPolar,
      // `param_definition`.
      2,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_azimuth = 180 (9 bits)
      // default_elevation = 3 (8 bits)
      // default_distance = 4 (7 bits)
      0b0101'1010,
      0b0000'0001,
      0b1000'0100,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesRenderingConfigCart8ParamDefinition) {
  Cart8ParamDefinition cart8_param_definition;
  cart8_param_definition.parameter_id_ = 1;
  cart8_param_definition.parameter_rate_ = 1;
  cart8_param_definition.param_definition_mode_ = false;
  cart8_param_definition.duration_ = 10;
  cart8_param_definition.constant_subblock_duration_ = 10;
  cart8_param_definition.default_x_ = 1;
  cart8_param_definition.default_y_ = 2;
  cart8_param_definition.default_z_ = 3;

  RenderingConfig rendering_config = {
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_param_definitions = {
          RenderingConfigParamDefinition::Create(cart8_param_definition, {})}};

  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_bytes size`.
      10,
      // `num_parameters`.
      1,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionCart8,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_x = 1
      // default_y = 2
      // default_z = 3
      0x01,
      0x02,
      0x03,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesRenderingConfigCart16ParamDefinition) {
  Cart16ParamDefinition cart16_param_definition;
  cart16_param_definition.parameter_id_ = 1;
  cart16_param_definition.parameter_rate_ = 1;
  cart16_param_definition.param_definition_mode_ = false;
  cart16_param_definition.duration_ = 10;
  cart16_param_definition.constant_subblock_duration_ = 10;
  cart16_param_definition.default_x_ = 1;
  cart16_param_definition.default_y_ = 2;
  cart16_param_definition.default_z_ = 3;

  RenderingConfig rendering_config = {
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_param_definitions = {
          RenderingConfigParamDefinition::Create(cart16_param_definition, {})}};

  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_bytes size`.
      13,
      // `num_parameters`.
      1,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionCart16,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_x = 1 (16 bits)
      // default_y = 2 (16 bits)
      // default_z = 3 (16 bits)
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesRenderingConfigDualPolarParamDefinition) {
  DualPolarParamDefinition dual_polar_param_definition;
  dual_polar_param_definition.parameter_id_ = 1;
  dual_polar_param_definition.parameter_rate_ = 1;
  dual_polar_param_definition.param_definition_mode_ = false;
  dual_polar_param_definition.duration_ = 10;
  dual_polar_param_definition.constant_subblock_duration_ = 10;
  dual_polar_param_definition.default_first_azimuth_ = 2;
  dual_polar_param_definition.default_first_elevation_ = 3;
  dual_polar_param_definition.default_first_distance_ = 4;
  dual_polar_param_definition.default_second_azimuth_ = 180;
  dual_polar_param_definition.default_second_elevation_ = 3;
  dual_polar_param_definition.default_second_distance_ = 4;

  RenderingConfig rendering_config = {
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_param_definitions = {
          RenderingConfigParamDefinition::Create(dual_polar_param_definition,
                                                 {})}};

  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_bytes size`.
      13,
      // `num_parameters`.
      1,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionDualPolar,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_azimuth = 2 (9 bits)
      // default_first_elevation = 3 (8 bits)
      // default_first_distance = 4 (7 bits)
      // 00000001 00000001 10000100
      0x01,
      0x01,
      0x84,
      // default_second_azimuth = 180 (9 bits)
      // default_second_elevation = 3 (8 bits)
      // default_second_distance = 4 (7 bits)
      0b0101'1010,
      0b0000'0001,
      0b1000'0100,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesRenderingConfigDualCart8ParamDefinition) {
  DualCart8ParamDefinition dual_cart8_param_definition;
  dual_cart8_param_definition.parameter_id_ = 1;
  dual_cart8_param_definition.parameter_rate_ = 1;
  dual_cart8_param_definition.param_definition_mode_ = false;
  dual_cart8_param_definition.duration_ = 10;
  dual_cart8_param_definition.constant_subblock_duration_ = 10;
  dual_cart8_param_definition.default_first_x_ = 1;
  dual_cart8_param_definition.default_first_y_ = 2;
  dual_cart8_param_definition.default_first_z_ = 3;
  dual_cart8_param_definition.default_second_x_ = 4;
  dual_cart8_param_definition.default_second_y_ = 5;
  dual_cart8_param_definition.default_second_z_ = 6;

  RenderingConfig rendering_config = {
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_param_definitions = {
          RenderingConfigParamDefinition::Create(dual_cart8_param_definition,
                                                 {})}};

  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_bytes size`.
      13,
      // `num_parameters`.
      1,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionDualCart8,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_x = 1
      // default_first_y = 2
      // default_first_z = 3
      // default_second_x = 4
      // default_second_y = 5
      // default_second_z = 6
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesRenderingConfigDualCart16ParamDefinition) {
  DualCart16ParamDefinition dual_cart16_param_definition;
  dual_cart16_param_definition.parameter_id_ = 1;
  dual_cart16_param_definition.parameter_rate_ = 1;
  dual_cart16_param_definition.param_definition_mode_ = false;
  dual_cart16_param_definition.duration_ = 10;
  dual_cart16_param_definition.constant_subblock_duration_ = 10;
  dual_cart16_param_definition.default_first_x_ = 1;
  dual_cart16_param_definition.default_first_y_ = 2;
  dual_cart16_param_definition.default_first_z_ = 3;
  dual_cart16_param_definition.default_second_x_ = 4;
  dual_cart16_param_definition.default_second_y_ = 5;
  dual_cart16_param_definition.default_second_z_ = 6;

  RenderingConfig rendering_config = {
      .headphones_rendering_mode = kHeadphonesRenderingModeStereo,
      .rendering_config_param_definitions = {
          RenderingConfigParamDefinition::Create(dual_cart16_param_definition,
                                                 {})}};

  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeStereo << 6,
      // `rendering_config_extension_bytes size`.
      19,
      // `num_parameters`.
      1,
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionDualCart16,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_x = 1 (16 bits)
      // default_first_y = 2 (16 bits)
      // default_first_z = 3 (16 bits)
      // default_second_x = 4 (16 bits)
      // default_second_y = 5 (16 bits)
      // default_second_z = 6 (16 bits)
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
      0x00,
      0x04,
      0x00,
      0x05,
      0x00,
      0x06,
  });

  WriteBitBuffer wb(0);
  EXPECT_THAT(rendering_config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(RenderingConfigCreateFromBuffer, NoExtensionBytes) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/0,
  });

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  EXPECT_TRUE(rendering_config->rendering_config_param_definitions.empty());
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer, PolarParamDefinitionRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/10,
      // num_params
      1,
      // Start RenderingConfigParamDefinition.
      // `param_definition_type`.
      ParamDefinition::ParameterDefinitionType::kParameterDefinitionPolar,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_azimuth = 2 (9 bits)
      // default_elevation = 3 (8 bits)
      // default_distance = 4 (7 bits)
      // 00000001 00000001 10000100
      0x01,
      0x01,
      0x84,
  });

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  ASSERT_EQ(rendering_config->rendering_config_param_definitions.size(), 1);
  auto* first_param_definition = std::get_if<PolarParamDefinition>(
      &rendering_config->rendering_config_param_definitions[0]
           .param_definition);
  ASSERT_NE(first_param_definition, nullptr);
  EXPECT_EQ(first_param_definition->parameter_id_, 1);
  EXPECT_EQ(first_param_definition->parameter_rate_, 1);
  EXPECT_EQ(first_param_definition->param_definition_mode_, false);
  EXPECT_EQ(first_param_definition->duration_, 10);
  EXPECT_EQ(first_param_definition->constant_subblock_duration_, 10);
  EXPECT_EQ(first_param_definition->default_azimuth_, 2);
  EXPECT_EQ(first_param_definition->default_elevation_, 3);
  EXPECT_EQ(first_param_definition->default_distance_, 4);
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer, Cart8ParamDefinitionRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/10,
      // num_params
      1,
      // Start RenderingConfigParamDefinition.
      // `param_definition_type`.
      ParamDefinition::ParameterDefinitionType::kParameterDefinitionCart8,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_x = 1
      // default_y = 2
      // default_z = 3
      0x01,
      0x02,
      0x03,
  });

  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  ASSERT_EQ(rendering_config->rendering_config_param_definitions.size(), 1);
  auto* first_param_definition = std::get_if<Cart8ParamDefinition>(
      &rendering_config->rendering_config_param_definitions[0]
           .param_definition);
  ASSERT_NE(first_param_definition, nullptr);
  EXPECT_EQ(first_param_definition->parameter_id_, 1);
  EXPECT_EQ(first_param_definition->parameter_rate_, 1);
  EXPECT_EQ(first_param_definition->param_definition_mode_, false);
  EXPECT_EQ(first_param_definition->duration_, 10);
  EXPECT_EQ(first_param_definition->constant_subblock_duration_, 10);
  EXPECT_EQ(first_param_definition->default_x_, 1);
  EXPECT_EQ(first_param_definition->default_y_, 2);
  EXPECT_EQ(first_param_definition->default_z_, 3);
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer, Cart16ParamDefinitionRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/13,
      // num_params
      1,
      // Start RenderingConfigParamDefinition.
      // `param_definition_type`.
      ParamDefinition::ParameterDefinitionType::kParameterDefinitionCart16,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_x = 1
      // default_y = 2
      // default_z = 3
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
  });
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  ASSERT_EQ(rendering_config->rendering_config_param_definitions.size(), 1);
  auto* first_param_definition = std::get_if<Cart16ParamDefinition>(
      &rendering_config->rendering_config_param_definitions[0]
           .param_definition);
  ASSERT_NE(first_param_definition, nullptr);
  EXPECT_EQ(first_param_definition->parameter_id_, 1);
  EXPECT_EQ(first_param_definition->parameter_rate_, 1);
  EXPECT_EQ(first_param_definition->param_definition_mode_, false);
  EXPECT_EQ(first_param_definition->duration_, 10);
  EXPECT_EQ(first_param_definition->constant_subblock_duration_, 10);
  EXPECT_EQ(first_param_definition->default_x_, 1);
  EXPECT_EQ(first_param_definition->default_y_, 2);
  EXPECT_EQ(first_param_definition->default_z_, 3);
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer, DualPolarParamDefinitionRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/13,
      // num_params
      1,
      // Start RenderingConfigParamDefinition.
      // `param_definition_type`.
      ParamDefinition::ParameterDefinitionType::kParameterDefinitionDualPolar,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_azimuth = 2 (9 bits)
      // default_first_elevation = 3 (8 bits)
      // default_first_distance = 4 (7 bits)
      // 00000001 00000001 10000100
      0x01,
      0x01,
      0x84,
      // default_second_azimuth = 180 (9 bits)
      // default_second_elevation = 3 (8 bits)
      // default_second_distance = 4 (7 bits)
      0b0101'1010,
      0b0000'0001,
      0b1000'0100,
  });
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  ASSERT_EQ(rendering_config->rendering_config_param_definitions.size(), 1);
  auto* first_param_definition = std::get_if<DualPolarParamDefinition>(
      &rendering_config->rendering_config_param_definitions[0]
           .param_definition);
  ASSERT_NE(first_param_definition, nullptr);
  EXPECT_EQ(first_param_definition->parameter_id_, 1);
  EXPECT_EQ(first_param_definition->parameter_rate_, 1);
  EXPECT_EQ(first_param_definition->param_definition_mode_, false);
  EXPECT_EQ(first_param_definition->duration_, 10);
  EXPECT_EQ(first_param_definition->constant_subblock_duration_, 10);
  EXPECT_EQ(first_param_definition->default_first_azimuth_, 2);
  EXPECT_EQ(first_param_definition->default_first_elevation_, 3);
  EXPECT_EQ(first_param_definition->default_first_distance_, 4);
  EXPECT_EQ(first_param_definition->default_second_azimuth_, 180);
  EXPECT_EQ(first_param_definition->default_second_elevation_, 3);
  EXPECT_EQ(first_param_definition->default_second_distance_, 4);
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer, DualCart8ParamDefinitionRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/13,
      // num_params
      1,
      // Start RenderingConfigParamDefinition.
      // `param_definition_type`.
      ParamDefinition::ParameterDefinitionType::kParameterDefinitionDualCart8,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_x = 1
      // default_first_y = 2
      // default_first_z = 3
      // default_second_x = 4
      // default_second_y = 5
      // default_second_z = 6
      0x01,
      0x02,
      0x03,
      0x04,
      0x05,
      0x06,
  });
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  ASSERT_EQ(rendering_config->rendering_config_param_definitions.size(), 1);
  auto* first_param_definition = std::get_if<DualCart8ParamDefinition>(
      &rendering_config->rendering_config_param_definitions[0]
           .param_definition);
  ASSERT_NE(first_param_definition, nullptr);
  EXPECT_EQ(first_param_definition->parameter_id_, 1);
  EXPECT_EQ(first_param_definition->parameter_rate_, 1);
  EXPECT_EQ(first_param_definition->param_definition_mode_, false);
  EXPECT_EQ(first_param_definition->duration_, 10);
  EXPECT_EQ(first_param_definition->constant_subblock_duration_, 10);
  EXPECT_EQ(first_param_definition->default_first_x_, 1);
  EXPECT_EQ(first_param_definition->default_first_y_, 2);
  EXPECT_EQ(first_param_definition->default_first_z_, 3);
  EXPECT_EQ(first_param_definition->default_second_x_, 4);
  EXPECT_EQ(first_param_definition->default_second_y_, 5);
  EXPECT_EQ(first_param_definition->default_second_z_, 6);
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer,
     DualCart16ParamDefinitionRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>({
      RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
      /*rendering_config_extension_size=*/19,
      // num_params
      1,
      // Start RenderingConfigParamDefinition.
      // `param_definition_type`.
      ParamDefinition::ParameterDefinitionType::kParameterDefinitionDualCart16,
      // `param_definition`.
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
      // default_first_x = 1 (16 bits)
      // default_first_y = 2 (16 bits)
      // default_first_z = 3 (16 bits)
      // default_second_x = 4 (16 bits)
      // default_second_y = 5 (16 bits)
      // default_second_z = 6 (16 bits)
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
      0x00,
      0x04,
      0x00,
      0x05,
      0x00,
      0x06,
  });
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  ASSERT_EQ(rendering_config->rendering_config_param_definitions.size(), 1);
  auto* first_param_definition = std::get_if<DualCart16ParamDefinition>(
      &rendering_config->rendering_config_param_definitions[0]
           .param_definition);
  ASSERT_NE(first_param_definition, nullptr);
  EXPECT_EQ(first_param_definition->parameter_id_, 1);
  EXPECT_EQ(first_param_definition->parameter_rate_, 1);
  EXPECT_EQ(first_param_definition->param_definition_mode_, false);
  EXPECT_EQ(first_param_definition->duration_, 10);
  EXPECT_EQ(first_param_definition->constant_subblock_duration_, 10);
  EXPECT_EQ(first_param_definition->default_first_x_, 1);
  EXPECT_EQ(first_param_definition->default_first_y_, 2);
  EXPECT_EQ(first_param_definition->default_first_z_, 3);
  EXPECT_EQ(first_param_definition->default_second_x_, 4);
  EXPECT_EQ(first_param_definition->default_second_y_, 5);
  EXPECT_EQ(first_param_definition->default_second_z_, 6);
  EXPECT_TRUE(rendering_config->rendering_config_extension_bytes.empty());
}

TEST(RenderingConfigCreateFromBuffer, ExtensionBytesRenderingConfig) {
  constexpr auto kSource = std::to_array<uint8_t>(
      {RenderingConfig::kHeadphonesRenderingModeBinauralWorldLocked << 6,
       /*rendering_config_extension_size=*/15, 'e', 'x', 't', 'e', 'n', 's',
       'i', 'o', 'n', 's', 'b', 'y', 't', 'e', 's'});
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(kSource));
  auto rendering_config = RenderingConfig::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config, IsOk());

  EXPECT_EQ(rendering_config->headphones_rendering_mode,
            kHeadphonesRenderingModeBinauralWorldLocked);
  EXPECT_EQ(rendering_config->reserved, 0);
  EXPECT_TRUE(rendering_config->rendering_config_param_definitions.empty());
  EXPECT_EQ(rendering_config->rendering_config_extension_bytes,
            std::vector<uint8_t>({'e', 'x', 't', 'e', 'n', 's', 'i', 'o', 'n',
                                  's', 'b', 'y', 't', 'e', 's'}));
}

TEST(RenderingConfigParamDefinitionCreate, SucceedsWithPolarParamDefinition) {
  const auto kParamDefinitionBytes = std::vector<uint8_t>({1, 2, 3, 4, 5, 123});
  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::Create(PolarParamDefinition(),
                                             kParamDefinitionBytes);

  EXPECT_EQ(rendering_config_param_definition.param_definition_type,
            ParamDefinition::kParameterDefinitionPolar);
  EXPECT_TRUE(std::holds_alternative<PolarParamDefinition>(
      rendering_config_param_definition.param_definition));
  EXPECT_THAT(rendering_config_param_definition.param_definition_bytes,
              ElementsAreArray(kParamDefinitionBytes));
}

TEST(RenderingConfigParamDefinitionCreate, SucceedsWithCart8ParamDefinition) {
  const auto kParamDefinitionBytes = std::vector<uint8_t>({1, 2, 3, 4, 5, 123});
  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::Create(Cart8ParamDefinition(),
                                             kParamDefinitionBytes);

  EXPECT_EQ(rendering_config_param_definition.param_definition_type,
            ParamDefinition::kParameterDefinitionCart8);
  EXPECT_TRUE(std::holds_alternative<Cart8ParamDefinition>(
      rendering_config_param_definition.param_definition));
  EXPECT_THAT(rendering_config_param_definition.param_definition_bytes,
              ElementsAreArray(kParamDefinitionBytes));
}

TEST(RenderingConfigParamDefinitionCreate, SucceedsWithCart16ParamDefinition) {
  const auto kParamDefinitionBytes = std::vector<uint8_t>({1, 2, 3, 4, 5, 123});
  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::Create(Cart16ParamDefinition(),
                                             kParamDefinitionBytes);

  EXPECT_EQ(rendering_config_param_definition.param_definition_type,
            ParamDefinition::kParameterDefinitionCart16);
  EXPECT_TRUE(std::holds_alternative<Cart16ParamDefinition>(
      rendering_config_param_definition.param_definition));
  EXPECT_THAT(rendering_config_param_definition.param_definition_bytes,
              ElementsAreArray(kParamDefinitionBytes));
}

TEST(RenderingConfigParamDefinitionCreate,
     SucceedsWithDualPolarParamDefinition) {
  const auto kParamDefinitionBytes = std::vector<uint8_t>({1, 2, 3, 4, 5, 123});
  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::Create(DualPolarParamDefinition(),
                                             kParamDefinitionBytes);

  EXPECT_EQ(rendering_config_param_definition.param_definition_type,
            ParamDefinition::kParameterDefinitionDualPolar);
  EXPECT_TRUE(std::holds_alternative<DualPolarParamDefinition>(
      rendering_config_param_definition.param_definition));
  EXPECT_THAT(rendering_config_param_definition.param_definition_bytes,
              ElementsAreArray(kParamDefinitionBytes));
}

TEST(RenderingConfigParamDefinitionCreate,
     SucceedsWithDualCart8ParamDefinition) {
  const auto kParamDefinitionBytes = std::vector<uint8_t>({1, 2, 3, 4, 5, 123});
  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::Create(DualCart8ParamDefinition(),
                                             kParamDefinitionBytes);

  EXPECT_EQ(rendering_config_param_definition.param_definition_type,
            ParamDefinition::kParameterDefinitionDualCart8);
  EXPECT_TRUE(std::holds_alternative<DualCart8ParamDefinition>(
      rendering_config_param_definition.param_definition));
  EXPECT_THAT(rendering_config_param_definition.param_definition_bytes,
              ElementsAreArray(kParamDefinitionBytes));
}

TEST(RenderingConfigParamDefinitionCreate,
     SucceedsWithDualCart16ParamDefinition) {
  const auto kParamDefinitionBytes = std::vector<uint8_t>({1, 2, 3, 4, 5, 123});
  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::Create(DualCart16ParamDefinition(),
                                             kParamDefinitionBytes);

  EXPECT_EQ(rendering_config_param_definition.param_definition_type,
            ParamDefinition::kParameterDefinitionDualCart16);
  EXPECT_TRUE(std::holds_alternative<DualCart16ParamDefinition>(
      rendering_config_param_definition.param_definition));
  EXPECT_THAT(rendering_config_param_definition.param_definition_bytes,
              ElementsAreArray(kParamDefinitionBytes));
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     FailsWithNonPositionParamDefinition) {
  std::vector<uint8_t> source = {// `param_definition_type`.
                                 ParamDefinition::kParameterDefinitionMixGain};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  EXPECT_THAT(RenderingConfigParamDefinition::CreateFromBuffer(*buffer),
              Not(IsOk()));
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     SucceedsWithPolarParamDefinition) {
  std::vector<uint8_t> source = {
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionPolar,
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
           // default_azimuth = 2 (9 bits)
           // default_elevation = 3 (8 bits)
           // default_distance = 4 (7 bits)
           // 00000001 00000001 10000100
      0x01,
      0x01,
      0x84,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config_param_definition, IsOk());

  EXPECT_EQ(rendering_config_param_definition->param_definition_type,
            ParamDefinition::kParameterDefinitionPolar);
  EXPECT_TRUE(std::holds_alternative<PolarParamDefinition>(
      rendering_config_param_definition->param_definition));
  const auto& param_definition = std::get<PolarParamDefinition>(
      rendering_config_param_definition->param_definition);
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_FALSE(param_definition.param_definition_mode_);
  EXPECT_EQ(param_definition.default_azimuth_, 2);
  EXPECT_EQ(param_definition.default_elevation_, 3);
  EXPECT_EQ(param_definition.default_distance_, 4);
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     SucceedsWithCart8ParamDefinition) {
  std::vector<uint8_t> source = {// `param_definition_type`.
                                 ParamDefinition::kParameterDefinitionCart8,
                                 1,   // parameter_id
                                 1,   // parameter_rate
                                 0,   // mode
                                 10,  // duration
                                 10,  // constant_subblock_duration
                                      // default_x = 1
                                      // default_y = 2
                                      // default_z = 3
                                 0x01, 0x02, 0x03};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config_param_definition, IsOk());

  EXPECT_EQ(rendering_config_param_definition->param_definition_type,
            ParamDefinition::kParameterDefinitionCart8);
  EXPECT_TRUE(std::holds_alternative<Cart8ParamDefinition>(
      rendering_config_param_definition->param_definition));
  const auto& param_definition = std::get<Cart8ParamDefinition>(
      rendering_config_param_definition->param_definition);
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_FALSE(param_definition.param_definition_mode_);
  EXPECT_EQ(param_definition.default_x_, 1);
  EXPECT_EQ(param_definition.default_y_, 2);
  EXPECT_EQ(param_definition.default_z_, 3);
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     SucceedsWithCart16ParamDefinition) {
  std::vector<uint8_t> source = {// `param_definition_type`.
                                 ParamDefinition::kParameterDefinitionCart16,
                                 1,   // parameter_id
                                 1,   // parameter_rate
                                 0,   // mode
                                 10,  // duration
                                 10,  // constant_subblock_duration
                                      // default_x = 1 (16 bits)
                                      // default_y = 2 (16 bits)
                                      // default_z = 3 (16 bits)
                                 0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config_param_definition, IsOk());

  EXPECT_EQ(rendering_config_param_definition->param_definition_type,
            ParamDefinition::kParameterDefinitionCart16);
  EXPECT_TRUE(std::holds_alternative<Cart16ParamDefinition>(
      rendering_config_param_definition->param_definition));
  const auto& param_definition = std::get<Cart16ParamDefinition>(
      rendering_config_param_definition->param_definition);
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_FALSE(param_definition.param_definition_mode_);
  EXPECT_EQ(param_definition.default_x_, 1);
  EXPECT_EQ(param_definition.default_y_, 2);
  EXPECT_EQ(param_definition.default_z_, 3);
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     SucceedsWithDualPolarParamDefinition) {
  std::vector<uint8_t> source = {// `param_definition_type`.
                                 ParamDefinition::kParameterDefinitionDualPolar,
                                 1,   // parameter_id
                                 1,   // parameter_rate
                                 0,   // mode
                                 10,  // duration
                                 10,  // constant_subblock_duration
                                      // default_azimuth = 2 (9 bits)
                                      // default_elevation = 3 (8 bits)
                                      // default_distance = 4 (7 bits)
                                      // 00000001 00000001 10000100
                                 0x01, 0x01, 0x84,
                                 // default_second_azimuth = 5 (9 bits)
                                 // default_second_elevation = 6 (8 bits)
                                 // default_second_distance = 7 (7 bits)
                                 0b0000'0010, 0b1000'0011, 0b0000'0111};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config_param_definition, IsOk());

  EXPECT_EQ(rendering_config_param_definition->param_definition_type,
            ParamDefinition::kParameterDefinitionDualPolar);
  EXPECT_TRUE(std::holds_alternative<DualPolarParamDefinition>(
      rendering_config_param_definition->param_definition));
  const auto& param_definition = std::get<DualPolarParamDefinition>(
      rendering_config_param_definition->param_definition);
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_FALSE(param_definition.param_definition_mode_);
  EXPECT_EQ(param_definition.default_first_azimuth_, 2);
  EXPECT_EQ(param_definition.default_first_elevation_, 3);
  EXPECT_EQ(param_definition.default_first_distance_, 4);
  EXPECT_EQ(param_definition.default_second_azimuth_, 5);
  EXPECT_EQ(param_definition.default_second_elevation_, 6);
  EXPECT_EQ(param_definition.default_second_distance_, 7);
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     SucceedsWithDualCart8ParamDefinition) {
  std::vector<uint8_t> source = {// `param_definition_type`.
                                 ParamDefinition::kParameterDefinitionDualCart8,
                                 1,   // parameter_id
                                 1,   // parameter_rate
                                 0,   // mode
                                 10,  // duration
                                 10,  // constant_subblock_duration
                                      // default_first_x = 1
                                      // default_first_y = 2
                                      // default_first_z = 3
                                      // default_second_x = 4
                                      // default_second_y = 5
                                      // default_second_z = 6
                                 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config_param_definition, IsOk());

  EXPECT_EQ(rendering_config_param_definition->param_definition_type,
            ParamDefinition::kParameterDefinitionDualCart8);
  EXPECT_TRUE(std::holds_alternative<DualCart8ParamDefinition>(
      rendering_config_param_definition->param_definition));
  const auto& param_definition = std::get<DualCart8ParamDefinition>(
      rendering_config_param_definition->param_definition);
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_FALSE(param_definition.param_definition_mode_);
  EXPECT_EQ(param_definition.default_first_x_, 1);
  EXPECT_EQ(param_definition.default_first_y_, 2);
  EXPECT_EQ(param_definition.default_first_z_, 3);
  EXPECT_EQ(param_definition.default_second_x_, 4);
  EXPECT_EQ(param_definition.default_second_y_, 5);
  EXPECT_EQ(param_definition.default_second_z_, 6);
}

TEST(RenderingConfigParamDefinitionCreateFromBufferTest,
     SucceedsWithDualCart16ParamDefinition) {
  std::vector<uint8_t> source = {
      // `param_definition_type`.
      ParamDefinition::kParameterDefinitionDualCart16,
      1,   // parameter_id
      1,   // parameter_rate
      0,   // mode
      10,  // duration
      10,  // constant_subblock_duration
           // default_first_x = 1 (16 bits)
           // default_first_y = 2 (16 bits)
           // default_first_z = 3 (16 bits)
           // default_second_x = 4 (16 bits)
           // default_second_y = 5 (16 bits)
           // default_second_z = 6 (16 bits)
      0x00,
      0x01,
      0x00,
      0x02,
      0x00,
      0x03,
      0x00,
      0x04,
      0x00,
      0x05,
      0x00,
      0x06,
  };
  auto buffer =
      MemoryBasedReadBitBuffer::CreateFromSpan(absl::MakeConstSpan(source));

  auto rendering_config_param_definition =
      RenderingConfigParamDefinition::CreateFromBuffer(*buffer);
  EXPECT_THAT(rendering_config_param_definition, IsOk());

  EXPECT_EQ(rendering_config_param_definition->param_definition_type,
            ParamDefinition::kParameterDefinitionDualCart16);
  EXPECT_TRUE(std::holds_alternative<DualCart16ParamDefinition>(
      rendering_config_param_definition->param_definition));
  const auto& param_definition = std::get<DualCart16ParamDefinition>(
      rendering_config_param_definition->param_definition);
  EXPECT_EQ(param_definition.parameter_id_, 1);
  EXPECT_EQ(param_definition.parameter_rate_, 1);
  EXPECT_FALSE(param_definition.param_definition_mode_);
  EXPECT_EQ(param_definition.default_first_x_, 1);
  EXPECT_EQ(param_definition.default_first_y_, 2);
  EXPECT_EQ(param_definition.default_first_z_, 3);
  EXPECT_EQ(param_definition.default_second_x_, 4);
  EXPECT_EQ(param_definition.default_second_y_, 5);
  EXPECT_EQ(param_definition.default_second_z_, 6);
}

}  // namespace
}  // namespace iamf_tools
