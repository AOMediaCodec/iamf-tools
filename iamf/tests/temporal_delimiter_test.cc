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
#include "iamf/temporal_delimiter.h"

#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"
#include "iamf/tests/obu_test_base.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {
namespace {

class TemporalDelimiterTestBase : public ObuTestBase {
 public:
  TemporalDelimiterTestBase()
      : ObuTestBase(
            /*expected_header=*/{kObuIaTemporalDelimiter << 3, 0},
            /*expected_payload=*/{}),
        obu_() {}

  ~TemporalDelimiterTestBase() override {}

 protected:
  void Init() override {
    obu_ = std::make_unique<TemporalDelimiterObu>(header_);
  }

  void WriteObu(WriteBitBuffer& wb) override {
    EXPECT_EQ(obu_->ValidateAndWriteObu(wb).code(),
              expected_write_status_code_);
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

TEST_F(TemporalDelimiterTest, IllegalRedundantCopy) {
  header_.obu_redundant_copy = true;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

TEST_F(TemporalDelimiterTest, IllegalTrimmingStatus) {
  header_.obu_trimming_status_flag = true;
  expected_write_status_code_ = absl::StatusCode::kInvalidArgument;
  InitAndTestWrite();
}

}  // namespace
}  // namespace iamf_tools
