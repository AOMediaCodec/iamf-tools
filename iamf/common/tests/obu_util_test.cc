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
#include "iamf/common/obu_util.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

TEST(AddUint32CheckOverflow, SmallInput) {
  uint32_t result;
  EXPECT_TRUE(AddUint32CheckOverflow(1, 1, result).ok());
  EXPECT_EQ(result, 2);
}

TEST(AddUint32CheckOverflow, MaxOutput) {
  uint32_t result;
  EXPECT_TRUE(AddUint32CheckOverflow(
                  1, std::numeric_limits<uint32_t>::max() - 1, result)
                  .ok());
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

struct Int32ToNormalizedFloatSymmetryTestCase {
  int32_t test_val;
  int32_t symmetric_val;
};

using Int32ToNormalizedFloatSymmetry =
    ::testing::TestWithParam<Int32ToNormalizedFloatSymmetryTestCase>;

TEST_P(Int32ToNormalizedFloatSymmetry, Int32ToNormalizedFloatSymmetry) {
  // `std::numeric_limits<int32_t>::min()` has no symmetric pair.
  ASSERT_NE(GetParam().test_val, std::numeric_limits<int32_t>::min());

  // All other values are symmetric with their negative.
  ASSERT_EQ(GetParam().symmetric_val, -GetParam().test_val);

  EXPECT_EQ(Int32ToNormalizedFloat(GetParam().test_val),
            -Int32ToNormalizedFloat(GetParam().symmetric_val));
}

INSTANTIATE_TEST_SUITE_P(
    OneAndNegativeOne, Int32ToNormalizedFloatSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatSymmetryTestCase>({{1, -1}}));

// There is one more negative than positive `int32_t`.
INSTANTIATE_TEST_SUITE_P(
    MaxAndMinPlusOne, Int32ToNormalizedFloatSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatSymmetryTestCase>(
        {{std::numeric_limits<int32_t>::max(),
          std::numeric_limits<int32_t>::min() + 1}}));

INSTANTIATE_TEST_SUITE_P(
    ArbitraryXAndNegativeX, Int32ToNormalizedFloatSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatSymmetryTestCase>({
        {5, -5},
        {99, -99},
        {9999, -9999},
        {999999, -999999},
        {77777777, -77777777},
    }));

INSTANTIATE_TEST_SUITE_P(
    NegativePowersOfTwoAndPositivePowersOfTwo, Int32ToNormalizedFloatSymmetry,
    testing::ValuesIn<Int32ToNormalizedFloatSymmetryTestCase>({
        {-4, 4},
        {-64, 64},
        {-128, 128},
        {-1024, 1024},
        {-67108864, 67108864},
        {-1073741824, 1073741824},
    }));

struct Int32ToNormalizedFloatTestCase {
  int32_t test_val;
  float expected_val;
};

using Int32ToNormalizedFloatTest =
    ::testing::TestWithParam<Int32ToNormalizedFloatTestCase>;

TEST_P(Int32ToNormalizedFloatTest, Int32ToNormalizedFloat) {
  EXPECT_FLOAT_EQ(Int32ToNormalizedFloat(GetParam().test_val),
                  GetParam().expected_val);
}

INSTANTIATE_TEST_SUITE_P(MaxGetsSquishedToOne, Int32ToNormalizedFloatTest,
                         testing::ValuesIn<Int32ToNormalizedFloatTestCase>(
                             {{std::numeric_limits<int32_t>::max(), 1}}));

INSTANTIATE_TEST_SUITE_P(
    Zero, Int32ToNormalizedFloatTest,
    testing::ValuesIn<Int32ToNormalizedFloatTestCase>({{0, 0.0}}));

INSTANTIATE_TEST_SUITE_P(PositivePowersOf2, Int32ToNormalizedFloatTest,
                         testing::ValuesIn<Int32ToNormalizedFloatTestCase>({
                             {1 << 30, std::pow(2.0f, -1.0f)},
                             {1 << 29, std::pow(2.0f, -2.0f)},
                             {1 << 27, std::pow(2.0f, -4.0f)},
                             {1 << 23, std::pow(2.0f, -8.0f)},
                             {1 << 15, std::pow(2.0f, -16.0f)},
                             {1 << 6, std::pow(2.0f, -25.0f)},
                             {1 << 1, std::pow(2.0f, -30.0f)},
                             {1 << 0, std::pow(2.0f, -31.0f)},
                         }));

INSTANTIATE_TEST_SUITE_P(MinMinusOneGetsSquishedToNegativeOne,
                         Int32ToNormalizedFloatTest,
                         testing::ValuesIn<Int32ToNormalizedFloatTestCase>(
                             {{std::numeric_limits<int32_t>::min() + 1, -1}}));

INSTANTIATE_TEST_SUITE_P(Min, Int32ToNormalizedFloatTest,
                         testing::ValuesIn<Int32ToNormalizedFloatTestCase>(
                             {{std::numeric_limits<int32_t>::min(), -1}}));

struct NormalizedFloatToInt32SymmetryTestCase {
  float test_val;
  float symmetric_val;
};

using NormalizedFloatToInt32SymmetryTest =
    ::testing::TestWithParam<NormalizedFloatToInt32SymmetryTestCase>;

TEST_P(NormalizedFloatToInt32SymmetryTest, NormalizedFloatToInt32SymmetryTest) {
  // +1.0 may have an irregular symmetric pair.
  ASSERT_NE(GetParam().test_val, -1.0f);

  // Most regular values are symmetric with their negative.
  ASSERT_EQ(GetParam().symmetric_val, -GetParam().test_val);

  int32_t test_val_result;
  EXPECT_TRUE(
      NormalizedFloatToInt32(GetParam().test_val, test_val_result).ok());
  int32_t symmetric_val_result;
  EXPECT_TRUE(
      NormalizedFloatToInt32(GetParam().symmetric_val, symmetric_val_result)
          .ok());
  EXPECT_EQ(test_val_result, -symmetric_val_result);
}

INSTANTIATE_TEST_SUITE_P(
    PositivePowersOfTwoAndNegativePowersOfTwo,
    NormalizedFloatToInt32SymmetryTest,
    testing::ValuesIn<NormalizedFloatToInt32SymmetryTestCase>({
        {std::pow(2.0f, -1.0f), -std::pow(2.0f, -1.0f)},
        {std::pow(2.0f, -2.0f), -std::pow(2.0f, -2.0f)},
        {std::pow(2.0f, -4.0f), -std::pow(2.0f, -4.0f)},
        {std::pow(2.0f, -8.0f), -std::pow(2.0f, -8.0f)},
        {std::pow(2.0f, -16.0f), -std::pow(2.0f, -16.0f)},
    }));

INSTANTIATE_TEST_SUITE_P(
    Arbitrary, NormalizedFloatToInt32SymmetryTest,
    testing::ValuesIn<NormalizedFloatToInt32SymmetryTestCase>({
        {0.01f, -0.01f},
        {0.12f, -0.12f},
        {0.34f, -0.34f},
        {0.99f, -0.99f},
    }));

struct NormalizedFloatToInt32TestCase {
  float test_val;
  int32_t expected_val;
};

using NormalizedFloatToInt32Test =
    ::testing::TestWithParam<NormalizedFloatToInt32TestCase>;

TEST_P(NormalizedFloatToInt32Test, NormalizedFloatToInt32Test) {
  int32_t result;
  EXPECT_TRUE(NormalizedFloatToInt32(GetParam().test_val, result).ok());
  EXPECT_EQ(result, GetParam().expected_val);
}

INSTANTIATE_TEST_SUITE_P(One, NormalizedFloatToInt32Test,
                         testing::ValuesIn<NormalizedFloatToInt32TestCase>(
                             {{1.0, std::numeric_limits<int32_t>::max()}}));

INSTANTIATE_TEST_SUITE_P(NegativeOne, NormalizedFloatToInt32Test,
                         testing::ValuesIn<NormalizedFloatToInt32TestCase>(
                             {{-1.0, std::numeric_limits<int32_t>::min()}}));

INSTANTIATE_TEST_SUITE_P(ClipsOverMax, NormalizedFloatToInt32Test,
                         testing::ValuesIn<NormalizedFloatToInt32TestCase>(
                             {{2.0, std::numeric_limits<int32_t>::max()}}));

INSTANTIATE_TEST_SUITE_P(ClipsUnderMin, NormalizedFloatToInt32Test,
                         testing::ValuesIn<NormalizedFloatToInt32TestCase>(
                             {{-2.0, std::numeric_limits<int32_t>::min()}}));

INSTANTIATE_TEST_SUITE_P(PositivePowersOf2, NormalizedFloatToInt32Test,
                         testing::ValuesIn<NormalizedFloatToInt32TestCase>(
                             {{std::pow(2.0f, -1.0f), 1 << 30},
                              {std::pow(2.0f, -2.0f), 1 << 29},
                              {std::pow(2.0f, -4.0f), 1 << 27},
                              {std::pow(2.0f, -8.0f), 1 << 23},
                              {std::pow(2.0f, -16.0f), 1 << 15},
                              {std::pow(2.0f, -25.0f), 1 << 6},
                              {std::pow(2.0f, -30.0f), 1 << 1},
                              {std::pow(2.0f, -31.0f), 1 << 0}}));

TEST(NormalizedFloatToInt32MalformedInfoput, InvalidNan) {
  int32_t unused_result;
  EXPECT_FALSE(NormalizedFloatToInt32(std::nanf(""), unused_result).ok());
}

TEST(NormalizedFloatToInt32MalformedInfoput, InvalidInfinity) {
  int32_t unused_result;
  EXPECT_FALSE(NormalizedFloatToInt32(std::numeric_limits<float>::infinity(),
                                      unused_result)
                   .ok());
}

struct Uint32ToUint8FormatTestCase {
  uint32_t test_val;
  uint8_t expected_val;
  absl::StatusCode expected_status_code;
};

using Uint32ToUint8Format =
    ::testing::TestWithParam<Uint32ToUint8FormatTestCase>;

TEST_P(Uint32ToUint8Format, TestUint32ToUint8) {
  const Uint32ToUint8FormatTestCase& test_case = GetParam();

  uint8_t result;
  EXPECT_EQ(Uint32ToUint8(test_case.test_val, result).code(),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(Valid, Uint32ToUint8Format,
                         testing::ValuesIn<Uint32ToUint8FormatTestCase>({
                             {0, 0, absl::StatusCode::kOk},
                             {255, 255, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Invalid, Uint32ToUint8Format,
                         testing::ValuesIn<Uint32ToUint8FormatTestCase>({
                             {256, 0, absl::StatusCode::kInvalidArgument},
                             {UINT32_MAX, 0,
                              absl::StatusCode::kInvalidArgument},
                         }));

struct Uint32ToUint16FormatTestCase {
  uint32_t test_val;
  uint16_t expected_val;
  absl::StatusCode expected_status_code;
};

using Uint32ToUint16Format =
    ::testing::TestWithParam<Uint32ToUint16FormatTestCase>;

TEST_P(Uint32ToUint16Format, TestUint32ToUint16) {
  const Uint32ToUint16FormatTestCase& test_case = GetParam();

  uint16_t result;
  EXPECT_EQ(Uint32ToUint16(test_case.test_val, result).code(),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(Valid, Uint32ToUint16Format,
                         testing::ValuesIn<Uint32ToUint16FormatTestCase>({
                             {0, 0, absl::StatusCode::kOk},
                             {65535, 65535, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Invalid, Uint32ToUint16Format,
                         testing::ValuesIn<Uint32ToUint16FormatTestCase>({
                             {65536, 0, absl::StatusCode::kInvalidArgument},
                             {UINT32_MAX, 0,
                              absl::StatusCode::kInvalidArgument},
                         }));

struct Int32ToInt16FormatTestCase {
  int32_t test_val;
  int16_t expected_val;
  absl::StatusCode expected_status_code;
};

using Int32ToInt16Format = ::testing::TestWithParam<Int32ToInt16FormatTestCase>;

TEST_P(Int32ToInt16Format, TestInt32ToInt16) {
  const Int32ToInt16FormatTestCase& test_case = GetParam();

  int16_t result;
  EXPECT_EQ(Int32ToInt16(test_case.test_val, result).code(),
            test_case.expected_status_code);
  if (test_case.expected_status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.expected_val);
  }
}

INSTANTIATE_TEST_SUITE_P(Valid, Int32ToInt16Format,
                         testing::ValuesIn<Int32ToInt16FormatTestCase>({
                             {-32768, -32768, absl::StatusCode::kOk},
                             {-1, -1, absl::StatusCode::kOk},
                             {0, 0, absl::StatusCode::kOk},
                             {32767, 32767, absl::StatusCode::kOk},
                         }));

INSTANTIATE_TEST_SUITE_P(Invalid, Int32ToInt16Format,
                         testing::ValuesIn<Int32ToInt16FormatTestCase>({
                             {INT32_MIN, 0, absl::StatusCode::kInvalidArgument},
                             {-32769, 0, absl::StatusCode::kInvalidArgument},
                             {32768, 0, absl::StatusCode::kInvalidArgument},
                             {INT32_MAX, 0, absl::StatusCode::kInvalidArgument},
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

TEST(WritePcmSample, LittleEndian32Bits) {
  std::vector<uint8_t> buffer(4, 0);
  int write_position = 0;
  EXPECT_TRUE(WritePcmSample(0x12345678, 32, /*big_endian=*/false,
                             buffer.data(), write_position)
                  .ok());
  EXPECT_EQ(write_position, 4);
  std::vector<uint8_t> expected_result = {0x78, 0x56, 0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian32bits) {
  std::vector<uint8_t> buffer(4, 0);
  int write_position = 0;
  EXPECT_TRUE(WritePcmSample(0x12345678, 32, /*big_endian=*/true, buffer.data(),
                             write_position)
                  .ok());
  EXPECT_EQ(write_position, 4);
  std::vector<uint8_t> expected_result = {0x12, 0x34, 0x56, 0x78};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, LittleEndian24Bits) {
  std::vector<uint8_t> buffer(3, 0);
  int write_position = 0;
  EXPECT_TRUE(WritePcmSample(0x12345600, 24, /*big_endian=*/false,
                             buffer.data(), write_position)
                  .ok());
  EXPECT_EQ(write_position, 3);
  std::vector<uint8_t> expected_result = {0x56, 0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian24Bits) {
  std::vector<uint8_t> buffer(3, 0);
  int write_position = 0;
  EXPECT_TRUE(WritePcmSample(0x12345600, 24, /*big_endian=*/true, buffer.data(),
                             write_position)
                  .ok());
  EXPECT_EQ(write_position, 3);
  std::vector<uint8_t> expected_result = {0x12, 0x34, 0x56};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, LittleEndian16Bits) {
  std::vector<uint8_t> buffer(2, 0);
  int write_position = 0;
  EXPECT_TRUE(WritePcmSample(0x12340000, 16, /*big_endian=*/false,
                             buffer.data(), write_position)
                  .ok());
  EXPECT_EQ(write_position, 2);
  std::vector<uint8_t> expected_result = {0x34, 0x12};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, BigEndian16Bits) {
  std::vector<uint8_t> buffer(2, 0);
  int write_position = 0;
  EXPECT_TRUE(WritePcmSample(0x12340000, 16, /*big_endian=*/true, buffer.data(),
                             write_position)
                  .ok());
  EXPECT_EQ(write_position, 2);
  std::vector<uint8_t> expected_result = {0x12, 0x34};
  EXPECT_EQ(buffer, expected_result);
}

TEST(WritePcmSample, InvalidOver32Bits) {
  std::vector<uint8_t> buffer(5, 0);
  int write_position = 0;
  EXPECT_EQ(WritePcmSample(0x00000000, 40, /*big_endian=*/false, buffer.data(),
                           write_position)
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ValidateEqual, OkIfArgsAreEqual) {
  const auto kLeftArg = 123;
  const auto kRightArg = 123;
  EXPECT_TRUE(ValidateEqual(kLeftArg, kRightArg, "").ok());
}

TEST(ValidateEqual, NotOkIfArgsAreNotEqual) {
  const auto kLeftArg = 123;
  const auto kUnequalRightArg = 223;
  EXPECT_FALSE(ValidateEqual(kLeftArg, kUnequalRightArg, "").ok());
}

TEST(ValidateNotEqual, OkIfArgsAreNotEqual) {
  const auto kLeftArg = 123;
  const auto kRightArg = 124;
  EXPECT_TRUE(ValidateNotEqual(kLeftArg, kRightArg, "").ok());
}

TEST(ValidateNotEqual, NotOkIfArgsAreEqual) {
  const auto kLeftArg = 123;
  const auto kEqualRightArg = 123;
  EXPECT_FALSE(ValidateNotEqual(kLeftArg, kEqualRightArg, "").ok());
}

TEST(ValidateUnique, OkIfArgsAreUnique) {
  const std::vector<int> kVectorWithUniqueValues = {1, 2, 3, 99};

  EXPECT_TRUE(ValidateUnique(kVectorWithUniqueValues.begin(),
                             kVectorWithUniqueValues.end(), "")
                  .ok());
}

TEST(ValidateUnique, NotOkIfArgsAreNotUnique) {
  const std::vector<int> kVectorWithDuplicateValues = {1, 2, 3, 99, 1};

  EXPECT_FALSE(ValidateUnique(kVectorWithDuplicateValues.begin(),
                              kVectorWithDuplicateValues.end(), "")
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
