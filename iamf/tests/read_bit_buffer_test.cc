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
#include "iamf/read_bit_buffer.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

class ReadBitBufferTest : public ::testing::Test {
 public:
  std::vector<uint8_t> rb_data_;
  int64_t rb_capacity_;
  std::unique_ptr<ReadBitBuffer> CreateReadBitBuffer() {
    return std::make_unique<ReadBitBuffer>(rb_capacity_, &rb_data_);
  }
};

TEST_F(ReadBitBufferTest, ReadBitBufferConstructor) {
  rb_data_ = {};
  rb_capacity_ = 0;
  std::unique_ptr<ReadBitBuffer> rb_ = CreateReadBitBuffer();
  EXPECT_NE(rb_, nullptr);
}

}  // namespace
}  // namespace iamf_tools
