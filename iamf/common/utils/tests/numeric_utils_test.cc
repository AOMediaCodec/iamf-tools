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
#include "iamf/common/utils/numeric_utils.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAreArray;
using ::testing::HasSubstr;

using ::absl::MakeConstSpan;
using ::absl::MakeSpan;

constexpr absl::string_view kOmitContext = "";
constexpr absl::string_view kCustomUserContext = "Custom User Context";

TEST(AddUint32CheckOverflow, SmallInput) {
  uint32_t result;
  EXPECT_THAT(AddUint32CheckOverflow(1, 1, result), IsOk());
  EXPECT_EQ(result, 2);
}

TEST(AddUint32CheckOverflow, MaxOutput) {
  uint32_t result;
  EXPECT_THAT(AddUint32CheckOverflow(
                  1, std::numeric_limits<uint32_t>::max() - 1, result),
              IsOk());
  EXPECT_EQ(result, std::numeric_limits<uint32_t>::max());
}

TEST(AddUint32CheckOverflow, Overflow) {
  uint32_t result;
  EXPECT_EQ(
      AddUint32CheckOverflow(1, std::numeric_limits<uint32_t>::max(), result)
          .code(),
      absl::StatusCode::kInvalidArgument);
}

struct FloatToQ78FormatTestCase {
  float test_val;
  int16_t expected_val;
  absl::StatusCode expected_status_code;
};

using FloatToQ78Format = ::testing::TestWithParam<FloatToQ78FormatTestCase>;

TEST_P(FloatToQ78Format, TestQ78Format) {
  const FloatToQ78FormatTestCase& test_case = GetParam();

  int16_t q7_8_value;
  EXPECT_EQ(FloatToQ7_8(test_case.test_val, q7_8_value).code(),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(q7_8_value, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(Positive, FloatToQ78Format,
                         testing::ValuesIn<FloatToQ78FormatTestCase>({
                             {0, 0, absl::StatusCode::kOk},
                             {0.00390625, 1, absl::StatusCode::kOk},
                             {0.390625, 100, absl::StatusCode::kOk},
                             {0.99609375, 255, absl::StatusCode::kOk},
                             {1, 256, absl::StatusCode::kOk},
                             {2, 512, absl::StatusCode::kOk},
                             {100, 25600, absl::StatusCode::kOk},
                             {127, 32512, absl::StatusCode::kOk},
                             {127.99609375, 32767, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(FlooredRounding, FloatToQ78Format,
                         testing::ValuesIn<FloatToQ78FormatTestCase>({
                             {.00390624, 0, absl::StatusCode::kOk},
                             {.00390626, 1, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Negative, FloatToQ78Format,
                         testing::ValuesIn<FloatToQ78FormatTestCase>({
                             {-0.00390625, -1, absl::StatusCode::kOk},
                             {-1.0, -256, absl::StatusCode::kOk},
                             {-1.00390625, -257, absl::StatusCode::kOk},
                             {-4.0, -1024, absl::StatusCode::kOk},
                             {-16.0, -4096, absl::StatusCode::kOk},
                             {-64.0, -16384, absl::StatusCode::kOk},
                             {-127.99609375, -32767, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Max, FloatToQ78Format,
                         testing::ValuesIn<FloatToQ78FormatTestCase>({
                             {127.99609375, 32767, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Min, FloatToQ78Format,
                         testing::ValuesIn<FloatToQ78FormatTestCase>({
                             {-128.0, -32768, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Invalid, FloatToQ78Format,
                         testing::ValuesIn<FloatToQ78FormatTestCase>({
                             {128.0, 0, absl::StatusCode::kUnknown},
                             {-128.00390625, 0, absl::StatusCode::kUnknown},
                             {nanf(""), 0, absl::StatusCode::kUnknown},
                         }));

struct Q78ToFloatFormatTestCase {
  int16_t test_val;
  float expected_val;
};

using Q78ToFloatFormat = ::testing::TestWithParam<Q78ToFloatFormatTestCase>;

TEST_P(Q78ToFloatFormat, TestQ78Format) {
  const Q78ToFloatFormatTestCase& test_case = GetParam();
  EXPECT_EQ(Q7_8ToFloat(test_case.test_val), test_case.expected_val);
}

INSTANTIATE_TEST_SUITE_P(Positive, Q78ToFloatFormat,
                         testing::ValuesIn<Q78ToFloatFormatTestCase>({
                             {0, 0},
                             {1, 0.00390625},
                             {100, 0.390625},
                             {255, 0.99609375},
                             {256, 1},
                             {512, 2},
                             {25600, 100},
                             {32512, 127},
                             {32767, 127.99609375},
                         }));

INSTANTIATE_TEST_SUITE_P(Negative, Q78ToFloatFormat,
                         testing::ValuesIn<Q78ToFloatFormatTestCase>({
                             {-1, -0.00390625},
                             {-256, -1.0},
                             {-257, -1.00390625},
                             {-1024, -4.0},
                             {-4096, -16.0},
                             {-16384, -64.0},
                             {-32767, -127.99609375},
                         }));

INSTANTIATE_TEST_SUITE_P(Max, Q78ToFloatFormat,
                         testing::ValuesIn<Q78ToFloatFormatTestCase>({
                             {32767, 127.99609375},
                         }));

INSTANTIATE_TEST_SUITE_P(Min, Q78ToFloatFormat,
                         testing::ValuesIn<Q78ToFloatFormatTestCase>({
                             {-32768, -128.0},
                         }));

struct FloatToQ08FormatTestCase {
  float test_val;
  uint8_t expected_val;
  absl::StatusCode expected_status_code;
};

using FloatToQ08Format = ::testing::TestWithParam<FloatToQ08FormatTestCase>;

TEST_P(FloatToQ08Format, TestQ08Format) {
  const FloatToQ08FormatTestCase& test_case = GetParam();

  uint8_t q0_8_value;
  EXPECT_EQ(FloatToQ0_8(test_case.test_val, q0_8_value).code(),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(q0_8_value, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(Basic, FloatToQ08Format,
                         testing::ValuesIn<FloatToQ08FormatTestCase>({
                             {0.00390625, 1, absl::StatusCode::kOk},
                             {0.390625, 100, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(FlooredRounding, FloatToQ08Format,
                         testing::ValuesIn<FloatToQ08FormatTestCase>({
                             {.00390624, 0, absl::StatusCode::kOk},
                             {.00390626, 1, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Max, FloatToQ08Format,
                         testing::ValuesIn<FloatToQ08FormatTestCase>({
                             {0.99609375, 255, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Min, FloatToQ08Format,
                         testing::ValuesIn<FloatToQ08FormatTestCase>({
                             {0, 0, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Invalid, FloatToQ08Format,
                         testing::ValuesIn<FloatToQ08FormatTestCase>({
                             {-0.00390625, 0, absl::StatusCode::kUnknown},
                             {1, 0, absl::StatusCode::kUnknown},
                             {std::nanf(""), 0, absl::StatusCode::kUnknown},
                         }));

struct Q08ToFloatFormatTestCase {
  uint8_t test_val;
  float expected_val;
};

using Q08ToFloatFormat = ::testing::TestWithParam<Q08ToFloatFormatTestCase>;

TEST_P(Q08ToFloatFormat, TestQ78Format) {
  const Q08ToFloatFormatTestCase& test_case = GetParam();
  EXPECT_EQ(Q0_8ToFloat(test_case.test_val), test_case.expected_val);
}

INSTANTIATE_TEST_SUITE_P(Basic, Q08ToFloatFormat,
                         testing::ValuesIn<Q08ToFloatFormatTestCase>({
                             {0, 0},
                             {1, 0.00390625},
                             {100, 0.390625},
                             {255, 0.99609375},
                         }));

struct Int32ToNormalizedFloatingPointSymmetryTestCase {
  int32_t test_val;
  int32_t symmetric_val;
};

using Int32ToNormalizedFloatingPointSymmetry =
    ::testing::TestWithParam<Int32ToNormalizedFloatingPointSymmetryTestCase>;

TEST_P(Int32ToNormalizedFloatingPointSymmetry, Int32ToNormalizedFloatSymmetry) {
  // `std::numeric_limits<int32_t>::min()` has no symmetric pair.
  ASSERT_NE(GetParam().test_val, std::numeric_limits<int32_t>::min());

  // All other values are symmetric with their negative.
  ASSERT_EQ(GetParam().symmetric_val, -GetParam().test_val);

  EXPECT_EQ(Int32ToNormalizedFloatingPoint<float>(GetParam().test_val),
            -Int32ToNormalizedFloatingPoint<float>(GetParam().symmetric_val));

  EXPECT_EQ(Int32ToNormalizedFloatingPoint<double>(GetParam().test_val),
            -Int32ToNormalizedFloatingPoint<double>(GetParam().symmetric_val));

  EXPECT_EQ(
      Int32ToNormalizedFloatingPoint<InternalSampleType>(GetParam().test_val),
      -Int32ToNormalizedFloatingPoint<InternalSampleType>(
          GetParam().symmetric_val));
}

INSTANTIATE_TEST_SUITE_P(
    OneAndNegativeOne, Int32ToNormalizedFloatingPointSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatingPointSymmetryTestCase>({{1,
                                                                        -1}}));

// There is one more negative than positive `int32_t`.
INSTANTIATE_TEST_SUITE_P(
    MaxAndMinPlusOne, Int32ToNormalizedFloatingPointSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatingPointSymmetryTestCase>(
        {{std::numeric_limits<int32_t>::max(),
          std::numeric_limits<int32_t>::min() + 1}}));

INSTANTIATE_TEST_SUITE_P(
    ArbitraryXAndNegativeX, Int32ToNormalizedFloatingPointSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatingPointSymmetryTestCase>({
        {5, -5},
        {99, -99},
        {9999, -9999},
        {999999, -999999},
        {77777777, -77777777},
    }));

INSTANTIATE_TEST_SUITE_P(
    NegativePowersOfTwoAndPositivePowersOfTwo,
    Int32ToNormalizedFloatingPointSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatingPointSymmetryTestCase>({
        {-4, 4},
        {-64, 64},
        {-128, 128},
        {-1024, 1024},
        {-67108864, 67108864},
        {-1073741824, 1073741824},
    }));

struct Int32ToNormalizedFloatingPointTestCase {
  int32_t input_val;
  float expected_val_as_float;
  double expected_val_as_double;
};

using Int32ToNormalizedFloatingPointTest =
    ::testing::TestWithParam<Int32ToNormalizedFloatingPointTestCase>;

TEST_P(Int32ToNormalizedFloatingPointTest, Int32ToNormalizedFloat) {
  EXPECT_FLOAT_EQ(Int32ToNormalizedFloatingPoint<float>(GetParam().input_val),
                  GetParam().expected_val_as_float);

  EXPECT_NEAR(Int32ToNormalizedFloatingPoint<double>(GetParam().input_val),
              GetParam().expected_val_as_double, .0000001);
}

INSTANTIATE_TEST_SUITE_P(
    MaxGetsSquishedToOne, Int32ToNormalizedFloatingPointTest,
    testing::ValuesIn<Int32ToNormalizedFloatingPointTestCase>(
        {{std::numeric_limits<int32_t>::max(), 1.0f, 1.0}}));

INSTANTIATE_TEST_SUITE_P(
    Zero, Int32ToNormalizedFloatingPointTest,
    testing::ValuesIn<Int32ToNormalizedFloatingPointTestCase>({{0, 0.0f,
                                                                0.0}}));

INSTANTIATE_TEST_SUITE_P(
    PositivePowersOf2, Int32ToNormalizedFloatingPointTest,
    testing::ValuesIn<Int32ToNormalizedFloatingPointTestCase>({
        {1 << 30, std::pow(2.0f, -1.0f), std::pow(2.0, -1.0)},
        {1 << 29, std::pow(2.0f, -2.0f), std::pow(2.0, -2.0)},
        {1 << 27, std::pow(2.0f, -4.0f), std::pow(2.0, -4.0)},
        {1 << 23, std::pow(2.0f, -8.0f), std::pow(2.0, -8.0)},
        {1 << 15, std::pow(2.0f, -16.0f), std::pow(2.0, -16.0)},
        {1 << 6, std::pow(2.0f, -25.0f), std::pow(2.0, -25.0)},
        {1 << 1, std::pow(2.0f, -30.0f), std::pow(2.0, -30.0)},
        {1 << 0, std::pow(2.0f, -31.0f), std::pow(2.0, -31.0)},
    }));

INSTANTIATE_TEST_SUITE_P(
    MinMinusOneGetsSquishedToNegativeOne, Int32ToNormalizedFloatingPointTest,
    testing::ValuesIn<Int32ToNormalizedFloatingPointTestCase>(
        {{std::numeric_limits<int32_t>::min() + 1, -1.0f, -1.0}}));

INSTANTIATE_TEST_SUITE_P(
    Min, Int32ToNormalizedFloatingPointTest,
    testing::ValuesIn<Int32ToNormalizedFloatingPointTestCase>(
        {{std::numeric_limits<int32_t>::min(), -1.0f, -1.0}}));

struct NormalizedFloatingPointToInt32SymmetryTestCase {
  float test_val;
  float symmetric_val;
};

using NormalizedFloatingPointToInt32SymmetryTest =
    ::testing::TestWithParam<NormalizedFloatingPointToInt32SymmetryTestCase>;

TEST_P(NormalizedFloatingPointToInt32SymmetryTest,
       NormalizedFloatingPointToInt32SymmetryTest) {
  // +1.0 may have an irregular symmetric pair.
  ASSERT_NE(GetParam().test_val, -1.0f);

  // Most regular values are symmetric with their negative.
  ASSERT_EQ(GetParam().symmetric_val, -GetParam().test_val);

  int32_t test_val_result;
  EXPECT_THAT(
      NormalizedFloatingPointToInt32(GetParam().test_val, test_val_result),
      IsOk());
  int32_t symmetric_val_result;
  EXPECT_THAT(NormalizedFloatingPointToInt32(GetParam().symmetric_val,
                                             symmetric_val_result),
              IsOk());
  EXPECT_EQ(test_val_result, -symmetric_val_result);
}

INSTANTIATE_TEST_SUITE_P(
    PositivePowersOfTwoAndNegativePowersOfTwo,
    NormalizedFloatingPointToInt32SymmetryTest,
    testing::ValuesIn<NormalizedFloatingPointToInt32SymmetryTestCase>({
        {std::pow(2.0f, -1.0f), -std::pow(2.0f, -1.0f)},
        {std::pow(2.0f, -2.0f), -std::pow(2.0f, -2.0f)},
        {std::pow(2.0f, -4.0f), -std::pow(2.0f, -4.0f)},
        {std::pow(2.0f, -8.0f), -std::pow(2.0f, -8.0f)},
        {std::pow(2.0f, -16.0f), -std::pow(2.0f, -16.0f)},
    }));

INSTANTIATE_TEST_SUITE_P(
    Arbitrary, NormalizedFloatingPointToInt32SymmetryTest,
    testing::ValuesIn<NormalizedFloatingPointToInt32SymmetryTestCase>({
        {0.01f, -0.01f},
        {0.12f, -0.12f},
        {0.34f, -0.34f},
        {0.99f, -0.99f},
    }));

struct NormalizedFloatingPointToInt32TestCase {
  float test_val;
  int32_t expected_val;
};

using NormalizedFloatingPointToInt32Test =
    ::testing::TestWithParam<NormalizedFloatingPointToInt32TestCase>;

TEST_P(NormalizedFloatingPointToInt32Test, NormalizedFloatingPointToInt32Test) {
  int32_t result;
  EXPECT_THAT(NormalizedFloatingPointToInt32(GetParam().test_val, result),
              IsOk());
  EXPECT_EQ(result, GetParam().expected_val);
}

INSTANTIATE_TEST_SUITE_P(
    One, NormalizedFloatingPointToInt32Test,
    testing::ValuesIn<NormalizedFloatingPointToInt32TestCase>(
        {{1.0, std::numeric_limits<int32_t>::max()}}));

INSTANTIATE_TEST_SUITE_P(
    NegativeOne, NormalizedFloatingPointToInt32Test,
    testing::ValuesIn<NormalizedFloatingPointToInt32TestCase>(
        {{-1.0, std::numeric_limits<int32_t>::min()}}));

INSTANTIATE_TEST_SUITE_P(
    ClipsOverMax, NormalizedFloatingPointToInt32Test,
    testing::ValuesIn<NormalizedFloatingPointToInt32TestCase>(
        {{2.0, std::numeric_limits<int32_t>::max()}}));

INSTANTIATE_TEST_SUITE_P(
    ClipsUnderMin, NormalizedFloatingPointToInt32Test,
    testing::ValuesIn<NormalizedFloatingPointToInt32TestCase>(
        {{-2.0, std::numeric_limits<int32_t>::min()}}));

INSTANTIATE_TEST_SUITE_P(
    PositivePowersOf2, NormalizedFloatingPointToInt32Test,
    testing::ValuesIn<NormalizedFloatingPointToInt32TestCase>(
        {{std::pow(2.0f, -1.0f), 1 << 30},
         {std::pow(2.0f, -2.0f), 1 << 29},
         {std::pow(2.0f, -4.0f), 1 << 27},
         {std::pow(2.0f, -8.0f), 1 << 23},
         {std::pow(2.0f, -16.0f), 1 << 15},
         {std::pow(2.0f, -25.0f), 1 << 6},
         {std::pow(2.0f, -30.0f), 1 << 1},
         {std::pow(2.0f, -31.0f), 1 << 0}}));

TEST(NormalizedFloatingPointToInt32MalformedOutput, InvalidFloatNan) {
  int32_t undefined_result;
  EXPECT_FALSE(
      NormalizedFloatingPointToInt32(std::nanf(""), undefined_result).ok());
}

TEST(NormalizedFloatingPointToInt32MalformedInfoput, InvalidDoubleNan) {
  int32_t undefined_result;
  EXPECT_FALSE(
      NormalizedFloatingPointToInt32(std::nan(""), undefined_result).ok());
}

TEST(NormalizedFloatingPointToInt32MalformedOutput, InvalidFloatInfinity) {
  int32_t undefined_result;
  EXPECT_FALSE(NormalizedFloatingPointToInt32(
                   std::numeric_limits<float>::infinity(), undefined_result)
                   .ok());
}

TEST(NormalizedFloatingPointToInt32MalformedOutput, InvalidDoubleInfinity) {
  int32_t undefined_result;
  EXPECT_FALSE(NormalizedFloatingPointToInt32(
                   std::numeric_limits<double>::infinity(), undefined_result)
                   .ok());
}

TEST(StaticCastIfInRange, SucceedsIfStaticCastSucceeds) {
  constexpr int8_t input = 1;
  int output;

  EXPECT_THAT((StaticCastIfInRange<int8_t, int>(kOmitContext, input, output)),
              IsOk());
  EXPECT_EQ(output, input);
}

TEST(StaticCastIfInRange, FailsIfStaticCastWouldFail) {
  constexpr int input = std::numeric_limits<int8_t>::max() + 1;
  int8_t output;

  EXPECT_FALSE(
      (StaticCastIfInRange<int, int8_t>(kOmitContext, input, output)).ok());
}

TEST(StaticCastIfInRange, MessageContainsContextOnError) {
  constexpr int input = std::numeric_limits<int8_t>::max() + 1;
  int8_t output;

  EXPECT_THAT(
      (StaticCastIfInRange<int, int8_t>(kCustomUserContext, input, output))
          .message(),
      HasSubstr(kCustomUserContext));
}

TEST(StaticCastIfInRange, SucceedsForExtremeCharValues) {
  // `char` can be either signed or unsigned, but this test ensures we don't
  // care about the signedness.
  constexpr char input = 0xff;
  uint8_t output;
  constexpr uint8_t kExpectedOutput = 0xff;

  EXPECT_THAT((StaticCastIfInRange<char, uint8_t>(kOmitContext, input, output)),
              IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

struct StaticCastIfInRangeUint32ToUint8TestCase {
  uint32_t test_val;
  uint8_t expected_val;
  absl::StatusCode expected_status_code;
};

using StaticCastIfInRangeUint32ToUint8Test =
    ::testing::TestWithParam<StaticCastIfInRangeUint32ToUint8TestCase>;

TEST_P(StaticCastIfInRangeUint32ToUint8Test, TestUint32ToUint8) {
  const StaticCastIfInRangeUint32ToUint8TestCase& test_case = GetParam();

  uint8_t result;
  EXPECT_EQ((StaticCastIfInRange<uint32_t, uint8_t>(kOmitContext,
                                                    test_case.test_val, result)
                 .code()),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Valid, StaticCastIfInRangeUint32ToUint8Test,
    testing::ValuesIn<StaticCastIfInRangeUint32ToUint8TestCase>({
        {0, 0, absl::StatusCode::kOk},
        {255, 255, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    Invalid, StaticCastIfInRangeUint32ToUint8Test,
    testing::ValuesIn<StaticCastIfInRangeUint32ToUint8TestCase>({
        {256, 0, absl::StatusCode::kInvalidArgument},
        {UINT32_MAX, 0, absl::StatusCode::kInvalidArgument},
    }));

struct StaticCastIfInRangeUint32ToUint16TestCase {
  uint32_t test_val;
  uint16_t expected_val;
  absl::StatusCode expected_status_code;
};

using StaticCastIfInRangeUint32ToUint16Test =
    ::testing::TestWithParam<StaticCastIfInRangeUint32ToUint16TestCase>;

TEST_P(StaticCastIfInRangeUint32ToUint16Test, TestUint32ToUint16) {
  const StaticCastIfInRangeUint32ToUint16TestCase& test_case = GetParam();

  uint16_t result;
  EXPECT_EQ((StaticCastIfInRange<uint32_t, uint16_t>(kOmitContext,
                                                     test_case.test_val, result)
                 .code()),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Valid, StaticCastIfInRangeUint32ToUint16Test,
    testing::ValuesIn<StaticCastIfInRangeUint32ToUint16TestCase>({
        {0, 0, absl::StatusCode::kOk},
        {65535, 65535, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    Invalid, StaticCastIfInRangeUint32ToUint16Test,
    testing::ValuesIn<StaticCastIfInRangeUint32ToUint16TestCase>({
        {65536, 0, absl::StatusCode::kInvalidArgument},
        {UINT32_MAX, 0, absl::StatusCode::kInvalidArgument},
    }));

struct StaticCastIfInRangeInt32ToInt16TestCase {
  int32_t test_val;
  int16_t expected_val;
  absl::StatusCode expected_status_code;
};

using StaticCastIfInRangeInt32ToInt16Test =
    ::testing::TestWithParam<StaticCastIfInRangeInt32ToInt16TestCase>;

TEST_P(StaticCastIfInRangeInt32ToInt16Test, TestInt32ToInt16) {
  const StaticCastIfInRangeInt32ToInt16TestCase& test_case = GetParam();

  int16_t result;
  EXPECT_EQ((StaticCastIfInRange<int32_t, int16_t>(kOmitContext,
                                                   test_case.test_val, result)
                 .code()),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(
    Valid, StaticCastIfInRangeInt32ToInt16Test,
    testing::ValuesIn<StaticCastIfInRangeInt32ToInt16TestCase>({
        {-32768, -32768, absl::StatusCode::kOk},
        {-1, -1, absl::StatusCode::kOk},
        {0, 0, absl::StatusCode::kOk},
        {32767, 32767, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    Invalid, StaticCastIfInRangeInt32ToInt16Test,
    testing::ValuesIn<StaticCastIfInRangeInt32ToInt16TestCase>({
        {INT32_MIN, 0, absl::StatusCode::kInvalidArgument},
        {-32769, 0, absl::StatusCode::kInvalidArgument},
        {32768, 0, absl::StatusCode::kInvalidArgument},
        {INT32_MAX, 0, absl::StatusCode::kInvalidArgument},
    }));

TEST(LittleEndianBytesToInt32Test, InvalidTooManyBytes) {
  int32_t unused_result = 0;
  absl::Status status =
      LittleEndianBytesToInt32({1, 2, 3, 4, 5}, unused_result);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(LittleEndianBytesToInt32Test, InvalidTooFewBytes) {
  int32_t result = 0;
  absl::Status status = LittleEndianBytesToInt32({}, result);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

struct LittleEndianBytesToInt32TestCase {
  std::vector<uint8_t> test_val;
  int32_t expected_val;
};

using LittleEndianBytesToInt32Format =
    ::testing::TestWithParam<LittleEndianBytesToInt32TestCase>;

TEST_P(LittleEndianBytesToInt32Format, TestLittleEndianBytesToInt32) {
  const LittleEndianBytesToInt32TestCase& test_case = GetParam();

  int32_t result;
  EXPECT_THAT(LittleEndianBytesToInt32(test_case.test_val, result), IsOk());
  EXPECT_EQ(result, test_case.expected_val);
}

INSTANTIATE_TEST_SUITE_P(OneByte, LittleEndianBytesToInt32Format,
                         ::testing::ValuesIn<LittleEndianBytesToInt32TestCase>({
                             {{0b00000000}, 0},
                             {{0x7f}, 0x7f000000},
                             {{0xff}, static_cast<int32_t>(0xff000000)},
                             {{0x80}, static_cast<int32_t>(0x80000000)},
                         }));

INSTANTIATE_TEST_SUITE_P(TwoBytes, LittleEndianBytesToInt32Format,
                         ::testing::ValuesIn<LittleEndianBytesToInt32TestCase>({
                             {{0x00, 0x00}, 0},
                             {{0x01, 0x02}, 0x02010000},
                             {{0xff, 0x7f}, static_cast<int32_t>(0x7fff0000)},
                             {{0xff, 0xff}, static_cast<int32_t>(0xffff0000)},
                             {{0x00, 0x80}, static_cast<int32_t>(0x80000000)},
                         }));

INSTANTIATE_TEST_SUITE_P(
    ThreeBytes, LittleEndianBytesToInt32Format,
    ::testing::ValuesIn<LittleEndianBytesToInt32TestCase>({
        {{0x00, 0x00, 0x00}, 0},
        {{0x01, 0x02, 0x03}, 0x03020100},
        {{0xff, 0xff, 0x7f}, static_cast<int32_t>(0x7fffff00)},
        {{0xff, 0xff, 0xff}, static_cast<int32_t>(0xffffff00)},
        {{0x00, 0x00, 0x80}, static_cast<int32_t>(0x80000000)},
    }));

INSTANTIATE_TEST_SUITE_P(
    FourBytes, LittleEndianBytesToInt32Format,
    ::testing::ValuesIn<LittleEndianBytesToInt32TestCase>({
        {{0x00, 0x00, 0x00, 0x00}, 0},
        {{0x01, 0x02, 0x03, 0x04}, 0x04030201},
        {{0xff, 0xff, 0xff, 0x7f}, static_cast<int32_t>(0x7fffffff)},
        {{0xff, 0xff, 0xff, 0xff}, static_cast<int32_t>(0xffffffff)},
        {{0x00, 0x00, 0x00, 0x80}, static_cast<int32_t>(0x80000000)},
    }));

TEST(BigEndianBytesToInt32Test, InvalidTooManyBytes) {
  int32_t unused_result = 0;
  absl::Status status = BigEndianBytesToInt32({1, 2, 3, 4, 5}, unused_result);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(BigEndianBytesToInt32Test, InvalidTooFewBytes) {
  int32_t result = 0;
  absl::Status status = BigEndianBytesToInt32({}, result);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

struct BigEndianBytesToInt32TestCase {
  std::vector<uint8_t> test_val;
  int32_t expected_val;
};

using BigEndianBytesToInt32Format =
    ::testing::TestWithParam<BigEndianBytesToInt32TestCase>;

TEST_P(BigEndianBytesToInt32Format, TestBigEndianBytesToInt32) {
  const BigEndianBytesToInt32TestCase& test_case = GetParam();

  int32_t result;
  EXPECT_THAT(BigEndianBytesToInt32(test_case.test_val, result), IsOk());
  EXPECT_EQ(result, test_case.expected_val);
}

INSTANTIATE_TEST_SUITE_P(OneByte, BigEndianBytesToInt32Format,
                         ::testing::ValuesIn<BigEndianBytesToInt32TestCase>({
                             {{0b00000000}, 0},
                             {{0x7f}, 0x7f000000},
                             {{0xff}, static_cast<int32_t>(0xff000000)},
                             {{0x80}, static_cast<int32_t>(0x80000000)},
                         }));

INSTANTIATE_TEST_SUITE_P(TwoBytes, BigEndianBytesToInt32Format,
                         ::testing::ValuesIn<BigEndianBytesToInt32TestCase>({
                             {{0x00, 0x00}, 0},
                             {{0x02, 0x01}, 0x02010000},
                             {{0x7f, 0xff}, 0x7fff0000},
                             {{0xff, 0xff}, static_cast<int32_t>(0xffff0000)},
                             {{0x80, 0x00}, static_cast<int32_t>(0x80000000)},
                         }));

INSTANTIATE_TEST_SUITE_P(
    ThreeBytes, BigEndianBytesToInt32Format,
    ::testing::ValuesIn<BigEndianBytesToInt32TestCase>({
        {{0x00, 0x00, 0x00}, 0},
        {{0x03, 0x02, 0x01}, 0x03020100},
        {{0x7f, 0xff, 0xff}, 0x7fffff00},
        {{0xff, 0xff, 0xff}, static_cast<int32_t>(0xffffff00)},
        {{0x80, 0x00, 0x00}, static_cast<int32_t>(0x80000000)},
    }));

INSTANTIATE_TEST_SUITE_P(
    FourBytes, BigEndianBytesToInt32Format,
    ::testing::ValuesIn<BigEndianBytesToInt32TestCase>({
        {{0x00, 0x00, 0x00, 0x00}, 0},
        {{0x04, 0x03, 0x02, 0x01}, 0x04030201},
        {{0x7f, 0xff, 0xff, 0xff}, 0x7fffffff},
        {{0xff, 0xff, 0xff, 0xff}, static_cast<int32_t>(0xffffffff)},
        {{0x80, 0x00, 0x00, 0x00}, static_cast<int32_t>(0x80000000)},
    }));

struct ClipDoubleToInt32TestCase {
  double test_val;
  int32_t expected_val;
  absl::StatusCode expected_status_code;
};

using ClipDoubleToInt32Test =
    ::testing::TestWithParam<ClipDoubleToInt32TestCase>;

TEST_P(ClipDoubleToInt32Test, TestClipDoubleToInt32) {
  const ClipDoubleToInt32TestCase& test_case = GetParam();

  int32_t result;
  EXPECT_EQ(ClipDoubleToInt32(test_case.test_val, result).code(),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ArgInBounds, ClipDoubleToInt32Test,
    testing::ValuesIn<ClipDoubleToInt32TestCase>({
        {-2147483648.0l, -2147483648, absl::StatusCode::kOk},
        {0.0l, 0, absl::StatusCode::kOk},
        {100.0l, 100, absl::StatusCode::kOk},
        {100.5l, 100, absl::StatusCode::kOk},
        {21474836467.0l, 2147483647, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    ArgOver, ClipDoubleToInt32Test,
    testing::ValuesIn<ClipDoubleToInt32TestCase>({
        {21474836467.0001l, 2147483647, absl::StatusCode::kOk},
        {21474836467.0l, 2147483647, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(
    ArgUnder, ClipDoubleToInt32Test,
    testing::ValuesIn<ClipDoubleToInt32TestCase>({
        {-2147483649.0l, -2147483648, absl::StatusCode::kOk},
        {-2147483648.001l, -2147483648, absl::StatusCode::kOk},
    }));

INSTANTIATE_TEST_SUITE_P(Invalid, ClipDoubleToInt32Test,
                         testing::ValuesIn<ClipDoubleToInt32TestCase>({
                             {std::nanf(""), 0,
                              absl::StatusCode::kInvalidArgument},
                         }));

TEST(StaticCastSpanIfInRange, SucceedsIfArgsAreEqualSize) {
  constexpr std::array<uint8_t, 4> kContainer = {1, 2, 3, 4};
  constexpr std::array<char, 4> kExpectedResult = {0x01, 0x02, 0x03, 0x04};

  std::vector<char> result(kContainer.size());
  EXPECT_THAT(
      StaticCastSpanIfInRange(kOmitContext, absl::MakeConstSpan(kContainer),
                              absl::MakeSpan(result)),
      IsOk());

  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

TEST(StaticCastSpanIfInRange, FailsIfArgsAreNotEqualSize) {
  constexpr size_t kMismatchedSize = 3;
  constexpr std::array<uint8_t, 4> kContainer = {1, 2, 3, 4};

  std::vector<char> result(kMismatchedSize);
  EXPECT_FALSE(StaticCastSpanIfInRange(kOmitContext,
                                       absl::MakeConstSpan(kContainer),
                                       absl::MakeSpan(result))
                   .ok());
}

TEST(StaticCastSpanIfInRange, FailsIfStaticCastWouldBeOutOfRange) {
  constexpr std::array<int16_t, 1> kContainerWithOutOfRangeValue = {256};

  std::vector<char> char_based_result(kContainerWithOutOfRangeValue.size());
  EXPECT_FALSE(StaticCastSpanIfInRange(
                   kOmitContext,
                   absl::MakeConstSpan(kContainerWithOutOfRangeValue),
                   absl::MakeSpan(char_based_result))
                   .ok());
}

TEST(StaticCastSpanIfInRange, MessageContainsContextOnError) {
  constexpr size_t kMismatchedSize = 3;
  constexpr std::array<uint8_t, 4> kContainer = {1, 2, 3, 4};
  const absl::string_view kFieldName = "user-specified field name";

  std::vector<char> result(kMismatchedSize);
  EXPECT_THAT(
      StaticCastSpanIfInRange(kFieldName, absl::MakeConstSpan(kContainer),
                              absl::MakeSpan(result))
          .message(),
      HasSubstr(kFieldName));
}

TEST(StaticCastSpanIfInRange, SucceedsForProtoBytesSpanToUint8Span) {
  iamf_tools_cli_proto::ObuHeaderMetadata obu_header;
  obu_header.set_extension_header_bytes({'\x01', '\x02', '\x7e', '\x7f'});
  constexpr std::array<uint8_t, 4> kExpectedResult = {0x01, 0x02, 0x7e, 0x7f};

  std::vector<uint8_t> result(obu_header.extension_header_bytes().size());
  EXPECT_THAT(
      StaticCastSpanIfInRange(
          kOmitContext, MakeConstSpan(obu_header.extension_header_bytes()),
          MakeSpan(result)),
      IsOk());

  EXPECT_THAT(result, ElementsAreArray(kExpectedResult));
}

}  // namespace
}  // namespace iamf_tools
