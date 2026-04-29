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
#include "iamf/obu/ambisonics_config.h"

#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using ::testing::Not;

TEST(TestValidateAmbisonicsMono, MappingInAscendingOrder) {
  // Users may map the Ambisonics Channel Number to substreams in numerical
  // order. (e.g. A0 to the zeroth substream, A1 to the first substream, ...).
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3}};
  EXPECT_THAT(ambisonics_mono.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsMono, MappingInArbitraryOrder) {
  // Users may map the Ambisonics Channel Number to substreams in any order.
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/3, /*A1=*/1, /*A2=*/0, /*A3=*/2}};
  EXPECT_THAT(ambisonics_mono.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsMono, MixedOrderAmbisonics) {
  // User may choose to map the Ambisonics Channel Number (ACN) to
  // `255` to drop that ACN (e.g. to drop A0 and A3).
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 2,
      .channel_mapping = {/*A0=*/255, /*A1=*/1, /*A2=*/0, /*A3=*/255}};
  EXPECT_THAT(ambisonics_mono.Validate(2), IsOk());
}

TEST(TestValidateAmbisonicsMono,
     ManyAmbisonicsChannelNumbersMappedToOneSubstream) {
  // User may choose to map several Ambisonics Channel Numbers (ACNs) to
  // one substream (e.g. A0, A1, A2, A3 are all mapped to the zeroth substream).
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 1,
      .channel_mapping = {/*A0=*/0, /*A1=*/0, /*A2=*/0, /*A3=*/0}};
  EXPECT_THAT(ambisonics_mono.Validate(1), IsOk());
}

TEST(TestValidateAmbisonicsMono,
     InvalidWhenObuSubstreamCountDoesNotEqualSubstreamCount) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3}};
  const DecodedUleb128 kInconsistentObuSubstreamCount = 3;
  EXPECT_THAT(ambisonics_mono.Validate(kInconsistentObuSubstreamCount),
              Not(IsOk()));
}

TEST(TestValidateAmbisonicsMono,
     InvalidWhenChannelMappingIsLargerThanSubstreamCount) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 2,
      .channel_mapping = {/*A0=*/255 /*A1=*/, 1 /*A2=*/, 0 /*A3=*/}};
  EXPECT_THAT(ambisonics_mono.Validate(2), Not(IsOk()));
}

TEST(TestValidateAmbisonicsMono, InvalidOutputChannelCount) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 5,
      .substream_count = 5,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3, /*A4=*/4}};
  EXPECT_THAT(ambisonics_mono.Validate(2), Not(IsOk()));
}

TEST(TestValidateAmbisonicsMono, InvalidWhenSubstreamIndexIsTooLarge) {
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 4,
      .channel_mapping = {/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/4}};
  EXPECT_THAT(ambisonics_mono.Validate(4), Not(IsOk()));
}

TEST(TestValidateAmbisonicsMono,
     InvalidWhenNoAmbisonicsChannelNumberIsMappedToASubstream) {
  // The OBU claims two associated substreams. But substream 1 is in limbo and
  // has no meaning because there are no Ambisonics Channel Numbers mapped to
  // it.
  const auto& ambisonics_mono = AmbisonicsMonoConfig{
      .output_channel_count = 4,
      .substream_count = 2,
      .channel_mapping = {/*A0=*/0, /*A1=*/0, /*A2=*/0, /*A3=*/0}};
  EXPECT_THAT(ambisonics_mono.Validate(2), Not(IsOk()));
}

TEST(TestValidateAmbisonicsProjection, FOAWithMainDiagonalMatrix) {
  // Typical users MAY create a matrix with non-zero values on the main
  // diagonal and zeroes in other entries. This results in one Ambisonics
  // Channel Number (ACN) represented per substream.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = {/*           ACN#: 0, 1, 2, 3 */
                          /* Substream 0: */ 1, 0, 0, 0,
                          /* Substream 1: */ 0, 1, 0, 0,
                          /* Substream 2: */ 0, 0, 1, 0,
                          /* Substream 3: */ 0, 0, 0, 1}};
  EXPECT_THAT(ambisonics_projection.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FOAWithArbitraryMatrix) {
  // Users MAY set arbitrary values anywhere in this matrix, but the size MUST
  // comply with the spec. This results in multiple Ambisonics Channel Numbers
  // (ACNs) per substream.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = {/*           ACN#: 0, 1, 2, 3 */
                          /* Substream 0: */ 1, 2, 3, 4,
                          /* Substream 1: */ 2, 3, 4, 5,
                          /* Substream 2: */ 3, 4, 5, 6,
                          /* Substream 3: */ 4, 5, 6, 7}};
  EXPECT_THAT(ambisonics_projection.Validate(4), IsOk());
}

TEST(TestValidateAmbisonicsProjection, ZerothOrderAmbisonics) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 1,
      .substream_count = 1,
      .coupled_substream_count = 0,
      .demixing_matrix = {
          /*                                             ACN#: 0, */
          /* Substream 0: */ std::numeric_limits<int16_t>::max()}};
  EXPECT_THAT(ambisonics_projection.Validate(1), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FOAWithOnlyA2) {
  // Fewer substreams than `output_channel_count` are allowed.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 1,
      .coupled_substream_count = 0,
      .demixing_matrix = {/*           ACN#: 0, 1, 2, 3 */
                          /* Substream 0: */ 0, 0, 1, 0}};
  EXPECT_THAT(ambisonics_projection.Validate(1), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FOAOneCoupledStream) {
  // The first `coupled_substream_count` substreams are coupled. Each pair in
  // the coupling has a column in the bitstream (written as a row in this
  // test). The remaining streams are decoupled.
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 3,
      .coupled_substream_count = 1,
      .demixing_matrix = {/*             ACN#: 0, 1, 2, 3 */
                          /* Substream 0_a: */ 1, 0, 0, 0,
                          /* Substream 0_b: */ 0, 1, 0, 0,
                          /* Substream   1: */ 0, 0, 1, 0,
                          /* Substream   2: */ 0, 0, 0, 1}};
  EXPECT_THAT(ambisonics_projection.Validate(3), IsOk());
}

TEST(TestValidateAmbisonicsProjection, FourteenthOrderAmbisonicsIsSupported) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 225,
      .substream_count = 225,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(225 * 225, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(225), IsOk());
}

TEST(TestValidateAmbisonicsProjection,
     FourteenthOrderAmbisonicsWithCoupledSubstreamsIsSupported) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 225,
      .substream_count = 113,
      .coupled_substream_count = 112,
      .demixing_matrix = std::vector<int16_t>((113 + 112) * 225, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(113), IsOk());
}

TEST(TestValidateAmbisonicsProjection, InvalidOutputChannelCountMaxValue) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 255,
      .substream_count = 255,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(255 * 255, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(255), Not(IsOk()));
}

TEST(TestValidateAmbisonicsProjection, InvalidOutputChannelCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 3,
      .substream_count = 3,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(3 * 3, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(3), Not(IsOk()));
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenSubstreamCountIsGreaterThanOutputChannelCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 5,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(4 * 5, 1)};
  EXPECT_THAT(ambisonics_projection.Validate(5), Not(IsOk()));
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenObuSubstreamCountDoesNotEqualSubstreamCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 4,
      .coupled_substream_count = 0,
      .demixing_matrix = std::vector<int16_t>(4 * 4, 1)};
  const DecodedUleb128 kInconsistentObuSubstreamCount = 3;

  EXPECT_THAT(ambisonics_projection.Validate(kInconsistentObuSubstreamCount),
              Not(IsOk()));
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenCoupledSubstreamCountIsGreaterThanSubstreamCount) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 1,
      .coupled_substream_count = 3,
      .demixing_matrix = std::vector<int16_t>((1 + 3) * 4, 1)};

  EXPECT_THAT(ambisonics_projection.Validate(1), Not(IsOk()));
}

TEST(TestValidateAmbisonicsProjection,
     InvalidWhenSubstreamCountPlusCoupledSubstreamCountIsTooLarge) {
  const AmbisonicsProjectionConfig ambisonics_projection = {
      .output_channel_count = 4,
      .substream_count = 3,
      .coupled_substream_count = 2,
      .demixing_matrix = std::vector<int16_t>((3 + 2) * 4, 1)};

  EXPECT_THAT(ambisonics_projection.Validate(3), Not(IsOk()));
}

TEST(TestGetNextValidCount, ReturnsNextHighestCount) {
  uint8_t next_valid_count;
  EXPECT_THAT(
      AmbisonicsConfig::GetNextValidOutputChannelCount(0, next_valid_count),
      IsOk());
  EXPECT_EQ(next_valid_count, 1);
}

TEST(TestGetNextValidCount, SupportsFirstOrderAmbisonics) {
  uint8_t next_valid_count;
  EXPECT_THAT(
      AmbisonicsConfig::GetNextValidOutputChannelCount(4, next_valid_count),
      IsOk());
  EXPECT_EQ(next_valid_count, 4);
}

TEST(TestGetNextValidCount, SupportsFourteenthOrderAmbisonics) {
  uint8_t next_valid_count;
  EXPECT_THAT(
      AmbisonicsConfig::GetNextValidOutputChannelCount(225, next_valid_count),
      IsOk());
  EXPECT_EQ(next_valid_count, 225);
}

TEST(TestGetNextValidCount, InvalidInputTooLarge) {
  uint8_t unused_next_valid_count;
  EXPECT_THAT(AmbisonicsConfig::GetNextValidOutputChannelCount(
                  226, unused_next_valid_count),
              Not(IsOk()));
}

TEST(AmbisonicsConfig, ValidateAndWriteMono) {
  AmbisonicsConfig config = {
      .ambisonics_mode = AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeMono,
      .ambisonics_config = AmbisonicsMonoConfig{.output_channel_count = 1,
                                                .substream_count = 1,
                                                .channel_mapping = {0}}};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(config.ValidateAndWrite(1, wb), IsOk());

  ValidateWriteResults(wb, {AmbisonicsConfig::kAmbisonicsModeMono, 1, 1, 0});
}

TEST(AmbisonicsConfig, ReadAndValidateMono) {
  const std::vector<uint8_t> kData = {AmbisonicsConfig::kAmbisonicsModeMono, 1,
                                      1, 0};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kData);
  AmbisonicsConfig config;

  EXPECT_THAT(config.ReadAndValidate(1, *rb), IsOk());

  EXPECT_EQ(config.ambisonics_mode,
            AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeMono);
  EXPECT_TRUE(
      std::holds_alternative<AmbisonicsMonoConfig>(config.ambisonics_config));
}

TEST(AmbisonicsConfig, ValidateAndWriteProjection) {
  AmbisonicsConfig config = {
      .ambisonics_mode =
          AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeProjection,
      .ambisonics_config =
          AmbisonicsProjectionConfig{.output_channel_count = 1,
                                     .substream_count = 1,
                                     .coupled_substream_count = 0,
                                     .demixing_matrix = {100}}};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(config.ValidateAndWrite(1, wb), IsOk());

  ValidateWriteResults(
      wb, {AmbisonicsConfig::kAmbisonicsModeProjection, 1, 1, 0, 0, 100});
}

TEST(AmbisonicsConfig, ReadAndValidateProjection) {
  const std::vector<uint8_t> kData = {
      AmbisonicsConfig::kAmbisonicsModeProjection, 1, 1, 0, 0, 100};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kData);
  AmbisonicsConfig config;

  EXPECT_THAT(config.ReadAndValidate(1, *rb), IsOk());

  EXPECT_EQ(config.ambisonics_mode,
            AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeProjection);
  EXPECT_TRUE(std::holds_alternative<AmbisonicsProjectionConfig>(
      config.ambisonics_config));
}

}  // namespace
}  // namespace iamf_tools
