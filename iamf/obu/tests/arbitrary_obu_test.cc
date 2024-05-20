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

#include "iamf/obu/arbitrary_obu.h"

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(ArbitraryObuConstructor, SetsObuType) {
  const ObuType kExpectedObuType = kObuIaReserved25;
  ArbitraryObu obu(kExpectedObuType, {}, {},
                   ArbitraryObu::kInsertionHookBeforeDescriptors);

  EXPECT_EQ(obu.header_.obu_type, kExpectedObuType);
}

class ArbitraryObuTest : public ObuTestBase, public testing::Test {
 public:
  ArbitraryObuTest()
      : ObuTestBase(
            /*expected_header=*/{kObuIaReserved24 << 3, 0},
            /*expected_payload=*/{}),
        obu_type_(kObuIaReserved24),
        payload_({}),
        insertion_hook_(ArbitraryObu::kInsertionHookBeforeDescriptors) {}

  ~ArbitraryObuTest() override = default;

 protected:
  void InitExpectOk() override {
    obu_ = std::make_unique<ArbitraryObu>(obu_type_, header_, payload_,
                                          insertion_hook_);
  }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  std::unique_ptr<ArbitraryObu> obu_;

  ObuType obu_type_;
  std::vector<uint8_t> payload_;
  ArbitraryObu::InsertionHook insertion_hook_;
};

TEST_F(ArbitraryObuTest, Default) { InitAndTestWrite(); }

TEST_F(ArbitraryObuTest, ObuType) {
  obu_type_ = kObuIaReserved25;
  expected_header_ = {kObuIaReserved25 << 3, 0};
  InitAndTestWrite();
}

TEST_F(ArbitraryObuTest, ObuRedundantCopy) {
  header_.obu_redundant_copy = true;
  expected_header_ = {kObuIaReserved24 << 3 | kObuRedundantCopyBitMask, 0};
  InitAndTestWrite();
}

TEST_F(ArbitraryObuTest, ObuTrimmingStatusFlag) {
  obu_type_ = kObuIaAudioFrame;
  header_.obu_trimming_status_flag = true;
  expected_header_ = {kObuIaAudioFrame << 3 | kObuTrimmingStatusFlagBitMask, 2,
                      0, 0};
  InitAndTestWrite();
}

TEST_F(ArbitraryObuTest, ObuExtensionFlag) {
  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};
  expected_header_ = {kObuIaReserved24 << 3 | kObuExtensionFlagBitMask,
                      6,
                      5,
                      'e',
                      'x',
                      't',
                      'r',
                      'a'};
  InitAndTestWrite();
}

TEST_F(ArbitraryObuTest, ObuPayload) {
  payload_ = {1, 2, 3, 4, 5};
  expected_header_ = {kObuIaReserved24 << 3, 5};
  expected_payload_ = {1, 2, 3, 4, 5};
  InitAndTestWrite();
}

TEST(WriteObusWithHook, NoObus) {
  WriteBitBuffer wb(1024);
  EXPECT_THAT(ArbitraryObu::WriteObusWithHook(
                  ArbitraryObu::kInsertionHookBeforeDescriptors, {}, wb),
              IsOk());
  ValidateWriteResults(wb, {});
}

TEST(WriteObusWithHook, MultipleObusWithDifferentHooks) {
  std::list<ArbitraryObu> arbitrary_obus;
  arbitrary_obus.emplace_back(
      ArbitraryObu(kObuIaReserved24, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookBeforeDescriptors));
  arbitrary_obus.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterDescriptors));
  arbitrary_obus.emplace_back(
      ArbitraryObu(kObuIaReserved26, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookBeforeDescriptors));

  // Check that the OBUs with ID 24 and 26 are written when using the
  // `ArbitraryObu::kInsertionHookBeforeDescriptors` hook.
  WriteBitBuffer wb(1024);
  EXPECT_THAT(
      ArbitraryObu::WriteObusWithHook(
          ArbitraryObu::kInsertionHookBeforeDescriptors, arbitrary_obus, wb),
      IsOk());
  ValidateWriteResults(wb,
                       {kObuIaReserved24 << 3, 0, kObuIaReserved26 << 3, 0});
  wb.Reset();

  // Check that only the OBU with ID 25 is written when using the
  // `ArbitraryObu::kInsertionHookAfterDescriptors` hook.
  EXPECT_THAT(
      ArbitraryObu::WriteObusWithHook(
          ArbitraryObu::kInsertionHookAfterDescriptors, arbitrary_obus, wb),
      IsOk());
  ValidateWriteResults(wb, {kObuIaReserved25 << 3, 0});
}

}  // namespace
}  // namespace iamf_tools
