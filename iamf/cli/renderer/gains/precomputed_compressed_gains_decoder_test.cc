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

#include "iamf/cli/renderer/gains/precomputed_compressed_gains_decoder.h"

#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/renderer/gains/precomputed_compressed_gains.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::ElementsAre;
using ::testing::Not;

TEST(DecompressMatrixTest, DecompressDenseMatrix) {
  CompressedMatrix compressed;
  // Input layout "0+2+0" to output layout "0+2+0" (2 input channels, 2 output
  // channels = 4 elements).
  compressed.dense_data = {1.0f, 0.0f, 0.0f, 1.0f};

  auto decompressed = DecompressMatrix("0+2+0", "0+2+0", compressed);

  EXPECT_THAT(decompressed, IsOkAndHolds(ElementsAre(ElementsAre(1.0, 0.0),
                                                     ElementsAre(0.0, 1.0))));
}

TEST(DecompressMatrixTest, DecompressSparseMatrix) {
  CompressedMatrix compressed;
  // Input layout "0+2+0" to output layout "0+2+0" (2 input channels, 2 output
  // channels). Row 0 has non-zero at col 0 with val 1.0 (codebook index 9). Row
  // 1 has non-zero at col 1 with val 0.5 (codebook index 5).
  compressed.row_ptr = {0, 1, 2};
  compressed.col_indices = {0, 1};
  compressed.sparse_data = {6, 1};  // 1.0 and 0.5.

  auto decompressed = DecompressMatrix("0+2+0", "0+2+0", compressed);

  EXPECT_THAT(decompressed, IsOkAndHolds(ElementsAre(ElementsAre(1.0, 0.0),
                                                     ElementsAre(0.0, 0.5))));
}

TEST(DecompressMatrixTest, InvalidLayoutReturnsError) {
  CompressedMatrix compressed;
  compressed.dense_data = {1.0f};

  auto decompressed = DecompressMatrix("InvalidLayout", "0+2+0", compressed);

  EXPECT_THAT(decompressed, Not(IsOk()));
}

struct LayoutDimensions {
  std::string input_layout_name;
  std::string output_layout_name;
  int expected_rows;
  int expected_cols;
};

class DecompressMatrixDimensionsTest
    : public ::testing::TestWithParam<LayoutDimensions> {};

using ::testing::AllOf;
using ::testing::Each;
using ::testing::SizeIs;

TEST_P(DecompressMatrixDimensionsTest, VerifyLayoutDimensions) {
  const auto& layout_dimensions = GetParam();
  auto decompressed = DecompressMatrix(layout_dimensions.input_layout_name,
                                       layout_dimensions.output_layout_name,
                                       CompressedMatrix());

  ASSERT_THAT(decompressed, IsOk());
  EXPECT_THAT(*decompressed,
              AllOf(SizeIs(layout_dimensions.expected_rows),
                    Each(SizeIs(layout_dimensions.expected_cols))));
}

INSTANTIATE_TEST_SUITE_P(
    DecompressMatrixDimensionsTests, DecompressMatrixDimensionsTest,
    ::testing::Values(LayoutDimensions{"0+1+0", "0+2+0", 1, 2},
                      LayoutDimensions{"0+2+0", "0+5+0", 2, 6},
                      LayoutDimensions{"3.1.2", "0+2+0", 6, 2},
                      LayoutDimensions{"7.1.2", "0+2+0", 10, 2},
                      LayoutDimensions{"9.1.6", "0+2+0", 16, 2},
                      LayoutDimensions{"3+7+0", "0+2+0", 12, 2},
                      LayoutDimensions{"4+9+0", "0+2+0", 14, 2},
                      LayoutDimensions{"7.1.5.4", "0+2+0", 17, 2},
                      LayoutDimensions{"9+10+3", "0+2+0", 24, 2},
                      LayoutDimensions{"4+5+1", "0+2+0", 11, 2},
                      LayoutDimensions{"A0", "0+2+0", 1, 2},
                      LayoutDimensions{"A1", "0+2+0", 4, 2},
                      LayoutDimensions{"A2", "0+2+0", 9, 2},
                      LayoutDimensions{"A3", "0+2+0", 16, 2},
                      LayoutDimensions{"A4", "0+2+0", 25, 2}));

TEST(DecompressMatrixTest, DecompressDenseMatrixDifferentLayout) {
  CompressedMatrix compressed;
  // Input layout "0+1+0" to output layout "0+2+0" (1 input channel, 2 output
  // channels = 2 elements).
  compressed.dense_data = {1.0f, 0.5f};

  auto decompressed = DecompressMatrix("0+1+0", "0+2+0", compressed);

  EXPECT_THAT(decompressed, IsOkAndHolds(ElementsAre(ElementsAre(1.0, 0.5))));
}

TEST(DecompressMatrixTest, DecompressSparseMatrixDifferentLayout) {
  CompressedMatrix compressed;
  // Input layout "0+2+0" to output layout "0+5+0" (2 input channels, 6 output
  // channels).
  // Row 0 has non-zero at col 2 with val 1.0.
  // Row 1 has non-zero at col 4 with val 0.5.
  compressed.row_ptr = {0, 1, 2};
  compressed.col_indices = {2, 4};
  compressed.sparse_data = {6, 1};  // 1.0 and 0.5.

  auto decompressed = DecompressMatrix("0+2+0", "0+5+0", compressed);

  EXPECT_THAT(decompressed, IsOkAndHolds(ElementsAre(
                                ElementsAre(0.0, 0.0, 1.0, 0.0, 0.0, 0.0),
                                ElementsAre(0.0, 0.0, 0.0, 0.0, 0.5, 0.0))));
}

}  // namespace
}  // namespace iamf_tools
