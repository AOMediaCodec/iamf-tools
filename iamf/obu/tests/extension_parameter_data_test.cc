/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/obu/extension_parameter_data.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;
using ::testing::Not;

// TODO(b/549854166): Remove these tests once migration to `CreateFromBuffer` is
//                     complete.
TEST(ExtensionParameterDataReadTest, NineBytes) {
  std::vector<uint8_t> source_data = {// `parameter_data_size`.
                                      9,
                                      // `parameter_data_bytes`.
                                      'a', 'r', 'b', 'i', 't', 'r', 'a', 'r',
                                      'y'};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  ExtensionParameterData extension_parameter_data;
  EXPECT_THAT(extension_parameter_data.ReadAndValidate(*buffer), IsOk());
  EXPECT_EQ(extension_parameter_data.parameter_data_bytes.size(), 9);
  const std::vector<uint8_t> expected_parameter_data_bytes = {
      'a', 'r', 'b', 'i', 't', 'r', 'a', 'r', 'y'};
  EXPECT_EQ(extension_parameter_data.parameter_data_bytes,
            expected_parameter_data_bytes);
}

TEST(ExtensionParameterDataReadTest, FailsWithParameterDataSizeTooLarge) {
  WriteBitBuffer wb(8);
  EXPECT_THAT(wb.WriteUleb128(kEntireObuSizeMaxTwoMegabytes + 1), IsOk());
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(wb.bit_buffer()));
  ExtensionParameterData extension_parameter_data;

  EXPECT_THAT(extension_parameter_data.ReadAndValidate(*rb), Not(IsOk()));
}

TEST(ExtensionParameterDataCreateTest, NineBytes) {
  std::vector<uint8_t> source_data = {// `parameter_data_size`.
                                      9,
                                      // `parameter_data_bytes`.
                                      'a', 'r', 'b', 'i', 't', 'r', 'a', 'r',
                                      'y'};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  auto data = ExtensionParameterData::CreateFromBuffer(*buffer);

  ASSERT_THAT(data, IsOk());
  EXPECT_EQ((*data)->parameter_data_bytes.size(), 9);
  const std::vector<uint8_t> expected_parameter_data_bytes = {
      'a', 'r', 'b', 'i', 't', 'r', 'a', 'r', 'y'};
  EXPECT_EQ((*data)->parameter_data_bytes, expected_parameter_data_bytes);
}

TEST(ExtensionParameterDataCreateTest, FailsWithParameterDataSizeTooLarge) {
  WriteBitBuffer wb(8);
  EXPECT_THAT(wb.WriteUleb128(kEntireObuSizeMaxTwoMegabytes + 1), IsOk());
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(wb.bit_buffer()));

  auto data = ExtensionParameterData::CreateFromBuffer(*rb);

  EXPECT_THAT(data, Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
