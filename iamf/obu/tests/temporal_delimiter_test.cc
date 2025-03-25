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
#include "iamf/obu/temporal_delimiter.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(TemporalDelimiterConstructor, SetsObuType) {
  TemporalDelimiterObu obu({});

  EXPECT_EQ(obu.header_.obu_type, kObuIaTemporalDelimiter);
}

class TemporalDelimiterTestBase : public ObuTestBase {
 public:
  TemporalDelimiterTestBase()
      : ObuTestBase(
            /*expected_header=*/{kObuIaTemporalDelimiter << 3, 0},
            /*expected_payload=*/{}),
        obu_() {}

  ~TemporalDelimiterTestBase() override = default;

 protected:
  void InitExpectOk() override {
    obu_ = std::make_unique<TemporalDelimiterObu>(header_);
  }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  std::unique_ptr<TemporalDelimiterObu> obu_;
};

class TemporalDelimiterTest : public TemporalDelimiterTestBase,
                              public testing::Test {};

TEST_F(TemporalDelimiterTest, Default) { InitAndTestWrite(); }

TEST_F(TemporalDelimiterTest, ExtensionHeader) {
  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaTemporalDelimiter << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      6,
                      // `extension_header_size`.
                      5,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};
  InitAndTestWrite();
}

TEST_F(TemporalDelimiterTest, NonMinimalLebGeneratorAffectsObuHeader) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);

  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaTemporalDelimiter << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      0x80 | 7, 0x00,
                      // `extension_header_size`.
                      0x80 | 5, 0x00,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};
  InitAndTestWrite();
}

TEST_F(TemporalDelimiterTest,
       ValidateAndWriteObuFailsWithIllegalRedundantCopy) {
  header_.obu_redundant_copy = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST_F(TemporalDelimiterTest,
       ValidateAndWriteObuFailsWithIllegalTrimmingStatus) {
  header_.obu_trimming_status_flag = true;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

TEST(CreateFromBuffer, SucceedsWithEmptyBuffer) {
  std::vector<uint8_t> source_data = {};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  EXPECT_THAT(TemporalDelimiterObu::CreateFromBuffer(
                  ObuHeader(), source_data.size(), *buffer),
              IsOk());
}

TEST(CreateFromBuffer, SetsObuType) {
  std::vector<uint8_t> source_data = {};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));

  absl::StatusOr<TemporalDelimiterObu> obu =
      TemporalDelimiterObu::CreateFromBuffer(ObuHeader(), source_data.size(),
                                             *buffer);
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->header_.obu_type, kObuIaTemporalDelimiter);
}

TEST(CreateFromBuffer, DoesNotConsumeBufferWhenObuPayloadSizeIsZero) {
  const int64_t kObuPayloadSize = 0;
  std::vector<uint8_t> source_data = {99};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  EXPECT_THAT(TemporalDelimiterObu::CreateFromBuffer(ObuHeader(),
                                                     kObuPayloadSize, *buffer),
              IsOk());
  uint8_t next_byte;
  EXPECT_THAT(buffer->ReadUnsignedLiteral(8, next_byte), IsOk());

  EXPECT_EQ(next_byte, 99);
}

}  // namespace
}  // namespace iamf_tools
