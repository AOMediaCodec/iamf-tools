/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/common/leb_generator.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

class LebGeneratorTest : public testing::Test {
 public:
  LebGeneratorTest() = default;

  void TestUleb128ToUint8Vector(
      DecodedUleb128 input, std::vector<uint8_t> expected_result,
      absl::StatusCode expected_status_code = absl::StatusCode::kOk) {
    EXPECT_NE(leb_generator_, nullptr);

    std::vector<uint8_t> output_buffer;
    EXPECT_EQ(leb_generator_->Uleb128ToUint8Vector(input, output_buffer).code(),
              expected_status_code);
    if (expected_status_code == absl::StatusCode::kOk) {
      EXPECT_EQ(output_buffer, expected_result);
    }
  }

  void TestSleb128ToUint8Vector(
      DecodedSleb128 input, std::vector<uint8_t> expected_result,
      absl::StatusCode expected_status_code = absl::StatusCode::kOk) {
    EXPECT_NE(leb_generator_, nullptr);

    std::vector<uint8_t> output_buffer;
    EXPECT_EQ(leb_generator_->Sleb128ToUint8Vector(input, output_buffer).code(),
              expected_status_code);
    if (expected_status_code == absl::StatusCode::kOk) {
      EXPECT_EQ(output_buffer, expected_result);
    }
  }

 protected:
  std::unique_ptr<LebGenerator> leb_generator_ = LebGenerator::Create();
};

TEST_F(LebGeneratorTest, MinimalUlebZero) {
  TestUleb128ToUint8Vector(/*input=*/0, {0});
}

TEST_F(LebGeneratorTest, MinimalUlebMaxValueOneByte) {
  TestUleb128ToUint8Vector(/*input=*/127, {127});
}

TEST_F(LebGeneratorTest, MinimalUlebMinValueTwoBytes) {
  TestUleb128ToUint8Vector(/*input=*/128, {0x80, 0x01});
}

TEST_F(LebGeneratorTest, MinimalUlebMaxValueFourBytes) {
  TestUleb128ToUint8Vector(/*input=*/(1 << 28) - 1, {0xff, 0xff, 0xff, 0x7f});
}

TEST_F(LebGeneratorTest, MinimalUlebMinValueFiveBytes) {
  TestUleb128ToUint8Vector(/*input=*/(1 << 28), {0x80, 0x80, 0x80, 0x80, 0x01});
}

TEST_F(LebGeneratorTest, MinimalUlebMaxInputValue) {
  TestUleb128ToUint8Vector(/*input=*/std::numeric_limits<DecodedUleb128>::max(),
                           {0xff, 0xff, 0xff, 0xff, 0x0f});
}

TEST_F(LebGeneratorTest, UlebFixedSizeOne) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);

  TestUleb128ToUint8Vector(/*input=*/0, {0});
}

TEST_F(LebGeneratorTest, UlebFixedSizeFive) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);

  TestUleb128ToUint8Vector(/*input=*/0, {0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST_F(LebGeneratorTest, UlebFixedSizeEight) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);

  TestUleb128ToUint8Vector(/*input=*/0,
                           {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST_F(LebGeneratorTest, IllegalUlebFixedSizeOneTooSmall) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);

  TestUleb128ToUint8Vector(/*input=*/128, {},
                           absl::StatusCode::kInvalidArgument);
}

TEST_F(LebGeneratorTest, MinimalSlebZero) {
  TestSleb128ToUint8Vector(/*input=*/0, {0});
}

TEST_F(LebGeneratorTest, MinimalSlebMinPositive) {
  TestSleb128ToUint8Vector(/*input=*/1, {0x01});
}

TEST_F(LebGeneratorTest, MinimalSlebMaxPositiveValueOneByte) {
  TestSleb128ToUint8Vector(/*input=*/63, {63});
}

TEST_F(LebGeneratorTest, MinimalSlebMinPositiveValueTwoBytes) {
  TestSleb128ToUint8Vector(/*input=*/64, {0xc0, 0x00});
}

TEST_F(LebGeneratorTest, MinimalSlebMaxPositiveValueFourBytes) {
  TestSleb128ToUint8Vector(/*input=*/(1 << 27) - 1, {0xff, 0xff, 0xff, 0x3f});
}

TEST_F(LebGeneratorTest, MinimalSlebMinPositiveValueFiveBytes) {
  TestSleb128ToUint8Vector(/*input=*/(1 << 27), {0x80, 0x80, 0x80, 0xc0, 0x00});
}

TEST_F(LebGeneratorTest, MinimalSlebMaxPositiveInputValue) {
  TestSleb128ToUint8Vector(/*input=*/std::numeric_limits<DecodedSleb128>::max(),
                           {0xff, 0xff, 0xff, 0xff, 0x07});
}

TEST_F(LebGeneratorTest, MinimalSlebMinNegativeInputValue) {
  TestSleb128ToUint8Vector(/*input=*/std::numeric_limits<DecodedSleb128>::min(),
                           {0x80, 0x80, 0x80, 0x80, 0x78});
}

TEST_F(LebGeneratorTest, MinimalSlebMinNegativeValueFourBytes) {
  TestSleb128ToUint8Vector(/*input=*/-(1 << 27), {0x80, 0x80, 0x80, 0x40});
}

TEST_F(LebGeneratorTest, MinimalSlebMaxNegativeValueFiveBytes) {
  TestSleb128ToUint8Vector(/*input=*/(-(1 << 27)) - 1,
                           {0xff, 0xff, 0xff, 0xbf, 0x7f});
}

TEST_F(LebGeneratorTest, MinimalSlebMaxNegativeInputValue) {
  TestSleb128ToUint8Vector(/*input=*/-1, {0x7f});
}

TEST_F(LebGeneratorTest, SlebFixedSizeOne) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);

  TestSleb128ToUint8Vector(/*input=*/0, {0});
}

TEST_F(LebGeneratorTest, SlebFixedSizeFive) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);

  TestSleb128ToUint8Vector(/*input=*/0, {0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST_F(LebGeneratorTest, SlebFixedSizeEight) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);

  TestSleb128ToUint8Vector(/*input=*/0,
                           {0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00});
}

TEST_F(LebGeneratorTest, SlebFixedSizeOneTooSmall) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 1);

  TestSleb128ToUint8Vector(/*input=*/64, {},
                           absl::StatusCode::kInvalidArgument);
}

struct Uleb128MinSizeTestCase {
  DecodedUleb128 decoded_uleb128;
  int8_t expected_size;
};

class LebGeneratorTestForUleb128MinSize
    : public testing::TestWithParam<Uleb128MinSizeTestCase> {};

TEST_P(LebGeneratorTestForUleb128MinSize, Uleb128ToUint8VectorOutputSize) {
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(LebGenerator::Create()->Uleb128ToUint8Vector(
                  GetParam().decoded_uleb128, output_buffer),
              IsOk());
  EXPECT_EQ(output_buffer.size(), GetParam().expected_size);
}

INSTANTIATE_TEST_SUITE_P(Zero, LebGeneratorTestForUleb128MinSize,
                         testing::ValuesIn<Uleb128MinSizeTestCase>({{0, 1}}));

INSTANTIATE_TEST_SUITE_P(Max, LebGeneratorTestForUleb128MinSize,
                         testing::ValuesIn<Uleb128MinSizeTestCase>(
                             {{std::numeric_limits<DecodedUleb128>::max(),
                               5}}));

INSTANTIATE_TEST_SUITE_P(EdgeCases, LebGeneratorTestForUleb128MinSize,
                         testing::ValuesIn<Uleb128MinSizeTestCase>({
                             {(1l << 7) - 1, 1},
                             {(1l << 7), 2},
                             {(1l << 14) - 1, 2},
                             {(1l << 14), 3},
                             {(1l << 21) - 1, 3},
                             {(1l << 21), 4},
                             {(1l << 28) - 1, 4},
                             {(1l << 28), 5},
                         }));

struct Sleb128MinSizeTestCase {
  DecodedSleb128 decoded_sleb128;
  int8_t expected_size;
};

class LebGeneratorTestForSleb128MinSize
    : public testing::TestWithParam<Sleb128MinSizeTestCase> {};

TEST_P(LebGeneratorTestForSleb128MinSize, Sleb128ToUint8VectorOutputSize) {
  std::vector<uint8_t> output_buffer;
  EXPECT_THAT(LebGenerator::Create()->Sleb128ToUint8Vector(
                  GetParam().decoded_sleb128, output_buffer),
              IsOk());
  EXPECT_EQ(output_buffer.size(), GetParam().expected_size);
}

INSTANTIATE_TEST_SUITE_P(Zero, LebGeneratorTestForSleb128MinSize,
                         testing::ValuesIn<Sleb128MinSizeTestCase>({{0, 1}}));

INSTANTIATE_TEST_SUITE_P(Max, LebGeneratorTestForSleb128MinSize,
                         testing::ValuesIn<Sleb128MinSizeTestCase>({
                             {std::numeric_limits<DecodedSleb128>::max(), 5},
                         }));

INSTANTIATE_TEST_SUITE_P(Min, LebGeneratorTestForSleb128MinSize,
                         testing::ValuesIn<Sleb128MinSizeTestCase>({
                             {std::numeric_limits<DecodedSleb128>::min(), 5},
                         }));

INSTANTIATE_TEST_SUITE_P(PositiveEdgeCases, LebGeneratorTestForSleb128MinSize,
                         testing::ValuesIn<Sleb128MinSizeTestCase>({
                             {(1 << 6) - 1, 1},
                             {(1 << 6), 2},
                             {(1 << 13) - 1, 2},
                             {(1 << 13), 3},
                             {(1 << 20) - 1, 3},
                             {(1 << 20), 4},
                             {(1 << 27) - 1, 4},
                             {(1 << 27), 5},
                         }));

INSTANTIATE_TEST_SUITE_P(NegativeEdgeCases, LebGeneratorTestForSleb128MinSize,
                         testing::ValuesIn<Sleb128MinSizeTestCase>({
                             {-(1 << 6), 1},
                             {-(1 << 6) - 1, 2},
                             {-(1 << 13), 2},
                             {-(1 << 13) - 1, 3},
                             {-(1 << 20), 3},
                             {-(1 << 20) - 1, 4},
                             {-(1 << 27), 4},
                             {-(1 << 27) - 1, 5},
                         }));

}  // namespace
}  // namespace iamf_tools
