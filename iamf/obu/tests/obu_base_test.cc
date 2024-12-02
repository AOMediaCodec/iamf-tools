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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/macros.h"
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

  static absl::StatusOr<ImaginaryObuNonIntegerBytes> CreateFromBuffer(
      int64_t payload_size, ReadBitBuffer& rb) {
    ImaginaryObuNonIntegerBytes obu;
    RETURN_IF_NOT_OK(obu.ReadAndValidatePayload(payload_size, rb));
    return obu;
  }

 private:
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override {
    return wb.WriteUnsignedLiteral(0, 1);
  }

  absl::Status ReadAndValidatePayloadDerived(int64_t /*payload_size*/,
                                             ReadBitBuffer& rb) override {
    uint8_t payload_to_read;
    return rb.ReadUnsignedLiteral(1, payload_to_read);
  }
};

TEST(ObuBaseTest, ObuSizeImpliesValidateAndWritePayloadMustWriteIntegerBytes) {
  const ImaginaryObuNonIntegerBytes obu;

  WriteBitBuffer wb(1024);
  EXPECT_EQ(obu.ValidateAndWriteObu(wb).code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(ObuBaseTest, InvalidWhenValidatePayloadDerivedDoesNotReadIntegerBytes) {
  const ImaginaryObuNonIntegerBytes obu;

  std::vector<uint8_t> source_data = {kObuIaReserved24 << 3, 1, 0x80};
  ReadBitBuffer rb(1024, &source_data);

  EXPECT_FALSE(ImaginaryObuNonIntegerBytes::CreateFromBuffer(1, rb).ok());
}

// A simple OBU with a constant payload with a length of 1 byte.
class OneByteObu : public ObuBase {
 public:
  OneByteObu() : ObuBase(kObuIaReserved24) {}
  explicit OneByteObu(const ObuHeader& header)
      : ObuBase(header, kObuIaReserved24) {}

  static absl::StatusOr<OneByteObu> CreateFromBuffer(const ObuHeader& header,
                                                     int64_t payload_size,
                                                     ReadBitBuffer& rb) {
    OneByteObu obu(header);
    RETURN_IF_NOT_OK(obu.ReadAndValidatePayload(payload_size, rb));
    return obu;
  }

  ~OneByteObu() override = default;
  void PrintObu() const override {}

 private:
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override {
    return wb.WriteUnsignedLiteral(255, 8);
  }

  absl::Status ReadAndValidatePayloadDerived(int64_t /*payload_size*/,
                                             ReadBitBuffer& rb) override {
    uint8_t payload_to_read;
    return rb.ReadUnsignedLiteral(8, payload_to_read);
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

TEST(ObuBaseTest, WritesObuFooterAndConsistentObuSize) {
  constexpr uint8_t kExpectedObuSizeWithFooter = 7;
  OneByteObu obu;
  obu.footer_ = {'f', 'o', 'o', 't', 'e', 'r'};

  WriteBitBuffer wb(1024);
  EXPECT_THAT(obu.ValidateAndWriteObu(wb), IsOk());

  ValidateObuWriteResults(wb,
                          {kObuIaReserved24 << 3,
                           // `obu_size`.
                           kExpectedObuSizeWithFooter},
                          {255, 'f', 'o', 'o', 't', 'e', 'r'});
}

TEST(ObuBaseTest, ReadWithConsistentSize) {
  std::vector<uint8_t> source_data = {kObuIaReserved24 << 3, 1, 255};
  ReadBitBuffer rb(1024, &source_data);

  auto obu = OneByteObu::CreateFromBuffer(ObuHeader(), 1, rb);
  EXPECT_THAT(obu, IsOk());
}

TEST(ObuBaseTest, ReadDoesNotOverflowWhenBufferIsLarge) {
  // Create a large buffer, and put an OBU at the end.
  const std::vector<uint8_t> kObu = {kObuIaReserved24 << 3, 1, 255};
  constexpr size_t kJunkDataSize = 1 << 29;
  std::vector<uint8_t> source_data(kJunkDataSize + kObu.size(), 0);
  source_data.insert(source_data.end() - kObu.size(), kObu.begin(), kObu.end());
  ReadBitBuffer rb(1024, &source_data);
  // Advance the buffer to just before the OBU of interest.
  std::vector<uint8_t> junk_data(kJunkDataSize);
  ASSERT_THAT(rb.ReadUint8Span(absl::MakeSpan(junk_data)), IsOk());

  EXPECT_THAT(OneByteObu::CreateFromBuffer(ObuHeader(), 1, rb), IsOk());
}

TEST(ObuBaseTest, ReadFailsWhenSizeIsTooSmall) {
  const int64_t kSizeTooSmall = 0;
  std::vector<uint8_t> source_data = {kObuIaReserved24 << 3, 1, 255};
  ReadBitBuffer rb(1024, &source_data);

  EXPECT_FALSE(
      OneByteObu::CreateFromBuffer(ObuHeader(), kSizeTooSmall, rb).ok());
}

TEST(ObuBaseTest, ReadsFooterWhenObuSizeIsTooLarge) {
  const int64_t kSizeWithExtraData = 4;
  const std::vector<uint8_t> kExtraData = {'e', 'x', 't'};

  std::vector<uint8_t> source_data = {255, 'e', 'x', 't'};
  ReadBitBuffer rb(1024, &source_data);

  const auto obu =
      OneByteObu::CreateFromBuffer(ObuHeader(), kSizeWithExtraData, rb);

  EXPECT_EQ(rb.Tell(), kSizeWithExtraData * 8);
  ASSERT_THAT(obu, IsOk());
  EXPECT_EQ(obu->footer_, kExtraData);
}

}  // namespace
}  // namespace iamf_tools
