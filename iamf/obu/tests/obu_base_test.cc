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
#include "iamf/obu/obu_base.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

// An erroneous OBU with a constant payload with a length of 1 bit.
class ImaginaryObuNonIntegerBytes : public ObuBase {
 public:
  ImaginaryObuNonIntegerBytes() : ObuBase(kObuIaReserved24) {}
  ~ImaginaryObuNonIntegerBytes() override = default;
  void PrintObu() const override {}

 private:
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override {
    return wb.WriteUnsignedLiteral(0, 1);
  }

  absl::Status ValidateAndReadPayload(ReadBitBuffer& rb) override {
    return absl::OkStatus();
  }
};

TEST(ObuBaseTest, ObuSizeImpliesValidateAndWritePayloadMustWriteIntegerBytes) {
  const ImaginaryObuNonIntegerBytes obu;

  WriteBitBuffer wb(1024);
  EXPECT_EQ(obu.ValidateAndWriteObu(wb).code(),
            absl::StatusCode::kInvalidArgument);
}

// A simple OBU with a constant payload with a length of 1 byte.
class OneByteObu : public ObuBase {
 public:
  OneByteObu() : ObuBase(kObuIaReserved24) {}
  explicit OneByteObu(const ObuHeader& header)
      : ObuBase(header, kObuIaReserved24) {}

  ~OneByteObu() override = default;
  void PrintObu() const override {}

 private:
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override {
    return wb.WriteUnsignedLiteral(255, 8);
  }

  absl::Status ValidateAndReadPayload(ReadBitBuffer& rb) override {
    return absl::OkStatus();
  }
};

TEST(ObuBaseTest, OneByteObu) {
  const OneByteObu obu;

  WriteBitBuffer wb(1024);
  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());
  ValidateObuWriteResults(wb,
                          {kObuIaReserved24 << 3,
                           // `obu_size`.
                           1},
                          {255});
}

TEST(ObuBaseTest, OneByteObuExtensionHeader) {
  const OneByteObu obu({.obu_extension_flag = true,
                        .extension_header_size = 1,
                        .extension_header_bytes = {128}});

  WriteBitBuffer wb(1024);
  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());
  ValidateObuWriteResults(wb,
                          {kObuIaReserved24 << 3 | 1,
                           // `obu_size`.
                           3,
                           // `extension_header_size`.
                           1,
                           // `extension_header_bytes`.
                           128},
                          {255});
}

}  // namespace
}  // namespace iamf_tools
