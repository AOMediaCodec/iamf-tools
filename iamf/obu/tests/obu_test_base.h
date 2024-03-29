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
#ifndef TESTS_OBU_TEST_BASE_H_
#define TESTS_OBU_TEST_BASE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

class ObuTestBase {
 public:
  static const int kObuRedundantCopyBitMask = 4;
  static const int kObuTrimmingStatusFlagBitMask = 2;
  static const int kObuExtensionFlagBitMask = 1;

  ObuTestBase(std::vector<uint8_t> expected_header,
              std::vector<uint8_t> expected_payload)

      : header_(),
        expected_header_(expected_header),
        expected_payload_(expected_payload) {}

 protected:
  void InitAndTestWrite(bool only_validate_size = false) {
    Init();
    TestWrite(only_validate_size);
  }

  void TestWrite(bool only_validate_size) {
    ASSERT_NE(leb_generator_, nullptr);
    // Allocate space for the expected size of the OBU.
    ASSERT_NE(leb_generator_, nullptr);
    WriteBitBuffer wb(expected_header_.size() + expected_payload_.size(),
                      *leb_generator_);
    // Write the OBU.
    WriteObu(wb);

    if (expected_write_status_code_ == absl::StatusCode::kOk) {
      // Validate either the size or byte-by-byte results match expected
      // depending on the mode.
      if (only_validate_size) {
        EXPECT_EQ(wb.bit_buffer().size(),
                  expected_header_.size() + expected_payload_.size());
      } else {
        ValidateObuWriteResults(wb, expected_header_, expected_payload_);
      }
    }
  }

  virtual void Init() = 0;
  virtual void WriteObu(WriteBitBuffer& wb) = 0;

  // Override this in subclasses to destroy the specific OBU.
  virtual ~ObuTestBase() = 0;

  std::unique_ptr<LebGenerator> leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kMinimum);
  ObuHeader header_;
  absl::StatusCode expected_init_status_code_ = absl::StatusCode::kOk;
  absl::StatusCode expected_write_status_code_ = absl::StatusCode::kOk;
  // TODO(b/296044377): Find a way to initialize `expected_header_` in a less
  //                    verbose way. Many tests manually configure it when the
  //                    size of the OBU or flags in the `ObuHeader` vary.
  std::vector<uint8_t> expected_header_;
  std::vector<uint8_t> expected_payload_;
};

}  // namespace iamf_tools

#endif  // TESTS_OBU_TEST_BASE_H_
