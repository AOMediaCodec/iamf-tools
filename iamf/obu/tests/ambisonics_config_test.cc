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

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using ::testing::ElementsAreArray;
using ::testing::Not;
using ::testing::Optional;

TEST(AmbisonicsMonoConfigCreate, MappingInAscendingOrder) {
  // Users may map the Ambisonics Channel Number to substreams in numerical
  // order. (e.g. A0 to the zeroth substream, A1 to the first substream, ...).
  EXPECT_THAT(AmbisonicsMonoConfig::Create(
                  /*substream_count=*/4,
                  /*channel_mapping=*/{/*A0=*/0, /*A1=*/1, /*A2=*/2, /*A3=*/3}),
              IsOk());
}

TEST(AmbisonicsMonoConfigCreate, MappingInArbitraryOrder) {
  // Users may map the Ambisonics Channel Number to substreams in any order.
  EXPECT_THAT(AmbisonicsMonoConfig::Create(
                  /*substream_count=*/4,
                  /*channel_mapping=*/{/*A0=*/3, /*A1=*/1, /*A2=*/0, /*A3=*/2}),
              IsOk());
}

TEST(AmbisonicsMonoConfigCreate, MixedOrderAmbisonics) {
  // User may choose to map the Ambisonics Channel Number (ACN) to
  // `255` to drop that ACN (e.g. to drop A0 and A3).
  EXPECT_THAT(
      AmbisonicsMonoConfig::Create(
          /*substream_count=*/2,
          /*channel_mapping=*/{/*A0=*/255, /*A1=*/1, /*A2=*/0, /*A3=*/255}),
      IsOk());
}

TEST(AmbisonicsMonoConfigCreate,
     ManyAmbisonicsChannelNumbersMappedToOneSubstream) {
  // User may choose to map several Ambisonics Channel Numbers (ACNs) to
  // one substream (e.g. A0, A1, A2, A3 are all mapped to the zeroth substream).
  EXPECT_THAT(AmbisonicsMonoConfig::Create(
                  /*substream_count=*/1,
                  /*channel_mapping=*/{/*A0=*/0, /*A1=*/0, /*A2=*/0, /*A3=*/0}),
              IsOk());
}

TEST(AmbisonicsMonoConfigCreate,
     InvalidWhenChannelMappingIsLargerThanSubstreamCount) {
  // Wait, old test says InvalidWhenChannelMappingIsLargerThanSubstreamCount
  // But actually mapping size is output channel count, which must be >=
  // substream_count. Let's reflect actual validation: substream_count >
  // output_channel_count is invalid.
  EXPECT_THAT(
      AmbisonicsMonoConfig::Create(
          /*substream_count=*/5, /*channel_mapping=*/{/*A0=*/255, 1, 0, 255}),
      Not(IsOk()));
}

TEST(AmbisonicsMonoConfigCreate, InvalidWhenOutputChannelCount) {
  // output channel count 5 is not valid (must be (1+n)^2).
  EXPECT_THAT(AmbisonicsMonoConfig::Create(
                  /*substream_count=*/5,
                  /*channel_mapping=*/{0, 1, 2, 3, 4}),
              Not(IsOk()));
}

TEST(AmbisonicsMonoConfigCreate, InvalidWhenSubstreamIndexIsTooLarge) {
  EXPECT_THAT(AmbisonicsMonoConfig::Create(/*substream_count=*/4,
                                           /*channel_mapping=*/{0, 1, 2, 4}),
              Not(IsOk()));
}

TEST(AmbisonicsMonoConfigCreate,
     InvalidWhenNoAmbisonicsChannelNumberIsMappedToASubstream) {
  // The OBU claims two associated substreams. But substream 1 is in limbo and
  // has no meaning because there are no Ambisonics Channel Numbers mapped to
  // it.
  EXPECT_THAT(AmbisonicsMonoConfig::Create(
                  /*substream_count=*/2, /*channel_mapping=*/{0, 0, 0, 0}),
              Not(IsOk()));
}

TEST(AmbisonicsProjectionConfigCreate, FOAWithMainDiagonalMatrix) {
  // Typical users MAY create a matrix with non-zero values on the main
  // diagonal and zeroes in other entries. This results in one Ambisonics
  // Channel Number (ACN) represented per substream.
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/4,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/
                  {/*           ACN#: 0, 1, 2, 3 */
                   /* Substream 0: */ 1, 0, 0, 0,
                   /* Substream 1: */ 0, 1, 0, 0,
                   /* Substream 2: */ 0, 0, 1, 0,
                   /* Substream 3: */ 0, 0, 0, 1}),
              IsOk());
}

TEST(AmbisonicsProjectionConfigCreate, FOAWithArbitraryMatrix) {
  // Users MAY set arbitrary values anywhere in this matrix, but the size MUST
  // comply with the spec. This results in multiple Ambisonics Channel Numbers
  // (ACNs) per substream.
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/4,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/
                  {/*           ACN#: 0, 1, 2, 3 */
                   /* Substream 0: */ 1, 2, 3, 4,
                   /* Substream 1: */ 2, 3, 4, 5,
                   /* Substream 2: */ 3, 4, 5, 6,
                   /* Substream 3: */ 4, 5, 6, 7}),
              IsOk());
}

TEST(AmbisonicsProjectionConfigCreate, ZerothOrderAmbisonics) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/1,
                  /*substream_count=*/1,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/
                  {/*                                             ACN#: 0, */
                   /* Substream 0: */ std::numeric_limits<int16_t>::max()}),
              IsOk());
}

TEST(AmbisonicsProjectionConfigCreate, FOAWithOnlyA2) {
  // Fewer substreams than `output_channel_count` are allowed.
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/1,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/
                  {/*           ACN#: 0, 1, 2, 3 */
                   /* Substream 0: */ 0, 0, 1, 0}),
              IsOk());
}

TEST(AmbisonicsProjectionConfigCreate, FOAOneCoupledStream) {
  // The first `coupled_substream_count` substreams are coupled. Each pair in
  // the coupling has a column in the bitstream (written as a row in this
  // test). The remaining streams are decoupled.
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/3,
                  /*coupled_substream_count=*/1,
                  /*demixing_matrix=*/
                  {/*             ACN#: 0, 1, 2, 3 */
                   /* Substream 0_a: */ 1, 0, 0, 0,
                   /* Substream 0_b: */ 0, 1, 0, 0,
                   /* Substream   1: */ 0, 0, 1, 0,
                   /* Substream   2: */ 0, 0, 0, 1}),
              IsOk());
}

TEST(AmbisonicsProjectionConfigCreate, FourteenthOrderAmbisonicsIsSupported) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/225,
                  /*substream_count=*/225,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/std::vector<int16_t>(225 * 225, 1)),
              IsOk());
}

TEST(AmbisonicsProjectionConfigCreate,
     FourteenthOrderAmbisonicsWithCoupledSubstreamsIsSupported) {
  EXPECT_THAT(
      AmbisonicsProjectionConfig::Create(
          /*output_channel_count=*/225,
          /*substream_count=*/113,
          /*coupled_substream_count=*/112,
          /*demixing_matrix=*/std::vector<int16_t>((113 + 112) * 225, 1)),
      IsOk());
}

TEST(AmbisonicsProjectionConfigCreate, InvalidOutputChannelCountMaxValue) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/255,
                  /*substream_count=*/255,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/std::vector<int16_t>(255 * 255, 1)),
              Not(IsOk()));
}

TEST(AmbisonicsProjectionConfigCreate, InvalidOutputChannelCount) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/3,
                  /*substream_count=*/3,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/std::vector<int16_t>(3 * 3, 1)),
              Not(IsOk()));
}

TEST(AmbisonicsProjectionConfigCreate,
     InvalidWhenSubstreamCountIsGreaterThanOutputChannelCount) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/5,
                  /*coupled_substream_count=*/0,
                  /*demixing_matrix=*/std::vector<int16_t>(4 * 5, 1)),
              Not(IsOk()));
}

TEST(AmbisonicsProjectionConfigCreate,
     InvalidWhenCoupledSubstreamCountIsGreaterThanSubstreamCount) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/1,
                  /*coupled_substream_count=*/3,
                  /*demixing_matrix=*/std::vector<int16_t>((1 + 3) * 4, 1)),
              Not(IsOk()));
}

TEST(AmbisonicsProjectionConfigCreate,
     InvalidWhenSubstreamCountPlusCoupledSubstreamCountIsTooLarge) {
  EXPECT_THAT(AmbisonicsProjectionConfig::Create(
                  /*output_channel_count=*/4,
                  /*substream_count=*/3,
                  /*coupled_substream_count=*/2,
                  /*demixing_matrix=*/std::vector<int16_t>((3 + 2) * 4, 1)),
              Not(IsOk()));
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
  auto mono_config = AmbisonicsMonoConfig::Create(
      /*substream_count=*/1, /*channel_mapping=*/{0});
  ASSERT_THAT(mono_config, IsOk());
  AmbisonicsConfig config = {.ambisonics_config = *mono_config};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, {AmbisonicsConfig::kAmbisonicsModeMono, 1, 1, 0});
}

TEST(AmbisonicsConfig, ReadAndValidateMono) {
  const std::vector<uint8_t> kData = {AmbisonicsConfig::kAmbisonicsModeMono, 1,
                                      1, 0};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kData);
  AmbisonicsConfig config;

  EXPECT_THAT(config.ReadAndValidate(*rb), IsOk());

  EXPECT_EQ(config.GetAmbisonicsMode(),
            AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeMono);
  EXPECT_TRUE(
      std::holds_alternative<AmbisonicsMonoConfig>(config.ambisonics_config));
}

TEST(AmbisonicsConfig, ValidateAndWriteProjection) {
  auto projection_config = AmbisonicsProjectionConfig::Create(
      /*output_channel_count=*/1,
      /*substream_count=*/1,
      /*coupled_substream_count=*/0,
      /*demixing_matrix=*/{100});
  ASSERT_THAT(projection_config.status(), IsOk());
  AmbisonicsConfig config = {.ambisonics_config = *projection_config};
  WriteBitBuffer wb(1024);

  EXPECT_THAT(config.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(
      wb, {AmbisonicsConfig::kAmbisonicsModeProjection, 1, 1, 0, 0, 100});
}

TEST(AmbisonicsConfig, ReadAndValidateProjection) {
  const std::vector<uint8_t> kData = {
      AmbisonicsConfig::kAmbisonicsModeProjection, 1, 1, 0, 0, 100};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kData);
  AmbisonicsConfig config;

  EXPECT_THAT(config.ReadAndValidate(*rb), IsOk());

  EXPECT_EQ(config.GetAmbisonicsMode(),
            AmbisonicsConfig::AmbisonicsMode::kAmbisonicsModeProjection);
  EXPECT_TRUE(std::holds_alternative<AmbisonicsProjectionConfig>(
      config.ambisonics_config));
}

TEST(AmbisonicsConfig, GetDemixingMatrixReturnsNulloptForMonoConfig) {
  const auto mono_config = AmbisonicsMonoConfig::Create(
      /*substream_count=*/1, /*channel_mapping=*/{0});
  ASSERT_THAT(mono_config.status(), IsOk());
  AmbisonicsConfig config = {.ambisonics_config = *mono_config};

  EXPECT_FALSE(config.GetDemixingMatrix().has_value());
}

TEST(AmbisonicsConfig, GetDemixingMatrixReturnsDataForProjectionConfig) {
  const std::vector<int16_t> kExpectedDemixingMatrix{
      16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1};
  const auto projection_config = AmbisonicsProjectionConfig::Create(
      /*output_channel_count=*/4,
      /*substream_count=*/4,
      /*coupled_substream_count=*/0, kExpectedDemixingMatrix);
  ASSERT_THAT(projection_config.status(), IsOk());
  AmbisonicsConfig config = {.ambisonics_config = *projection_config};

  auto demixing_matrix = config.GetDemixingMatrix();

  EXPECT_THAT(demixing_matrix,
              Optional(ElementsAreArray(kExpectedDemixingMatrix)));
}

}  // namespace
}  // namespace iamf_tools
