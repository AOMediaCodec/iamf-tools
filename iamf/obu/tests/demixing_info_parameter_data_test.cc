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
#include "iamf/obu/demixing_info_parameter_data.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using enum DemixingInfoParameterData::DMixPMode;
using enum DemixingInfoParameterData::WIdxUpdateRule;

const int kDMixPModeBitShift = 5;
const int kDefaultWBitShift = 4;

TEST(DMixPModeToDownMixingParams, DMixPMode1) {
  DownMixingParams output_down_mix_args;
  EXPECT_THAT(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  kNormal, output_down_mix_args),
              IsOk());

  // When `previous_w_idx = 6` and `w_idx_update_rule = kNormal`, the current
  // `w_idx` will be `previous_w_idx + w_idx_offset = 6 - 1 = 5`, and the
  // corresponding `w` will be 0.25.
  EXPECT_EQ(output_down_mix_args.alpha, 1);
  EXPECT_EQ(output_down_mix_args.beta, 1);
  EXPECT_EQ(output_down_mix_args.gamma, .707);
  EXPECT_EQ(output_down_mix_args.delta, .707);
  EXPECT_EQ(output_down_mix_args.w_idx_offset, -1);
  EXPECT_EQ(output_down_mix_args.w_idx_used, 5);  // Current `w_idx` used.
  EXPECT_EQ(output_down_mix_args.w, .25);
  EXPECT_EQ(output_down_mix_args.in_bitstream, true);
}

TEST(DMixPModeToDownMixingParams, FirstFrameWAlwaysEqualTo0) {
  DownMixingParams output_down_mix_args;
  EXPECT_THAT(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  kFirstFrame, output_down_mix_args),
              IsOk());

  // When `w_idx_update_rule = kFirstFrame`, the `w_idx` is forced to be 0,
  // and the corresponding `w` will be 0 too (instead of 0.25 normally).
  EXPECT_EQ(output_down_mix_args.alpha, 1);
  EXPECT_EQ(output_down_mix_args.beta, 1);
  EXPECT_EQ(output_down_mix_args.gamma, .707);
  EXPECT_EQ(output_down_mix_args.delta, .707);
  EXPECT_EQ(output_down_mix_args.w_idx_offset, -1);
  EXPECT_EQ(output_down_mix_args.w_idx_used, 0);  // `w_idx` forced to be 0.
  EXPECT_EQ(output_down_mix_args.w, 0.0);
  EXPECT_EQ(output_down_mix_args.in_bitstream, true);
}

TEST(DMixPModeToDownMixingParams, DefaultWDirectlyUsed) {
  DownMixingParams output_down_mix_args;
  EXPECT_THAT(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  kDefault, output_down_mix_args),
              IsOk());

  // When `w_idx_update_rule = kDefault`, the `w_idx` is directly equal to
  // the `previous_w_idx` passed in, and the corresponding `w` will be
  // 0.3962 (instead of 0.25 normally).
  EXPECT_EQ(output_down_mix_args.alpha, 1);
  EXPECT_EQ(output_down_mix_args.beta, 1);
  EXPECT_EQ(output_down_mix_args.gamma, .707);
  EXPECT_EQ(output_down_mix_args.delta, .707);
  EXPECT_EQ(output_down_mix_args.w_idx_offset, -1);
  EXPECT_EQ(output_down_mix_args.w_idx_used, 6);  // Equal to `previous_w_idx`.
  EXPECT_EQ(output_down_mix_args.w, 0.3962);
  EXPECT_EQ(output_down_mix_args.in_bitstream, true);
}

TEST(DMixPModeToDownMixingParams, InvalidDMixPModeReserved) {
  DownMixingParams output_down_mix_args;
  EXPECT_FALSE(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                   kDMixPModeReserved1, 5,
                   /*w_idx_update_rule=*/
                   kNormal, output_down_mix_args)
                   .ok());
}

TEST(DMixPModeToDownMixingParams, InvalidWOffsetOver10) {
  DownMixingParams output_down_mix_args;
  EXPECT_FALSE(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                   kDMixPModeReserved1, 11,
                   /*w_idx_update_rule=*/
                   kNormal, output_down_mix_args)
                   .ok());
}

TEST(WriteDemixingInfoParameterData, WriteDMixPMode1) {
  constexpr auto kExpectedDMixPMode = kDMixPMode1;
  DemixingInfoParameterData data(kExpectedDMixPMode, 0);
  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(wb, {kExpectedDMixPMode << kDMixPModeBitShift});
}

TEST(WriteDemixingInfoParameterData, WriteDMixPMode3) {
  constexpr auto kExpectedDMixPMode = kDMixPMode3;
  DemixingInfoParameterData data(kExpectedDMixPMode, 0);
  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(wb, {kExpectedDMixPMode << kDMixPModeBitShift});
}

TEST(WriteDemixingInfoParameterData, WriteReservedMax) {
  constexpr auto kExpectedDMixPMode = kDMixPMode1;
  // The IAMF spec reserved a 5-bit value.
  const uint32_t kReservedMax = 31;
  DemixingInfoParameterData data(kExpectedDMixPMode, kReservedMax);
  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(
      wb, {kExpectedDMixPMode << kDMixPModeBitShift | kReservedMax});
}

TEST(WriteDemixingInfoParameterData, IllegalWriteDMixPModeReserved) {
  constexpr auto kReservedDMixPMode = kDMixPModeReserved1;
  DemixingInfoParameterData data(kReservedDMixPMode, 0);
  WriteBitBuffer undetermined_wb(1);
  EXPECT_FALSE(data.Write(undetermined_wb).ok());
}

TEST(WriteDefaultDemixingInfoParameterData, Writes) {
  constexpr auto kExpectedDMixPMode = kDMixPMode1;
  constexpr uint8_t kExpectedReserved = 31;
  constexpr uint8_t kExpectedDefaultW = 5;
  constexpr uint8_t kExpectedReservedDefault = 15;
  DefaultDemixingInfoParameterData data(kExpectedDMixPMode, kExpectedReserved,
                                        kExpectedDefaultW,
                                        kExpectedReservedDefault);
  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(
      wb, {kExpectedDMixPMode << kDMixPModeBitShift | kExpectedReserved,
           kExpectedDefaultW << kDefaultWBitShift | kExpectedReservedDefault});
}

TEST(ReadDemixingInfoParameterData, ReadDMixPMode1) {
  std::vector<uint8_t> source_data = {kDMixPMode1 << kDMixPModeBitShift};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source_data));
  DemixingInfoParameterData data;
  EXPECT_THAT(data.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, kDMixPMode1);
  EXPECT_EQ(data.reserved, 0);
}

TEST(ReadDemixingInfoParameterData, ReadDMixPMode3) {
  std::vector<uint8_t> source_data = {kDMixPMode3 << kDMixPModeBitShift};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source_data));
  DemixingInfoParameterData data;
  EXPECT_THAT(data.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, kDMixPMode3);
  EXPECT_EQ(data.reserved, 0);
}

TEST(ReadDemixingInfoParameterData, ReadReservedMax) {
  const uint32_t kReservedMax = 31;
  std::vector<uint8_t> source_data = {kDMixPMode1 << kDMixPModeBitShift |
                                      kReservedMax};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source_data));
  DemixingInfoParameterData data;
  EXPECT_THAT(data.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, kDMixPMode1);
  EXPECT_EQ(data.reserved, 31);
}

TEST(ReadsDefaultDemixingInfoParameterData, Reads) {
  constexpr auto kExpectedDMixPMode = kDMixPMode1_n;
  constexpr auto kExpectedReserved = 25;
  constexpr auto kExpectedDefaultW = 9;
  constexpr auto kExpectedReservedDefault = 12;
  std::vector<uint8_t> source_data = {
      kExpectedDMixPMode << kDMixPModeBitShift | kExpectedReserved,
      kExpectedDefaultW << kDefaultWBitShift | kExpectedReservedDefault};
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source_data));
  DefaultDemixingInfoParameterData data;
  EXPECT_THAT(data.ReadAndValidate(*rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, kExpectedDMixPMode);
  EXPECT_EQ(data.reserved, kExpectedReserved);
  EXPECT_EQ(data.default_w, kExpectedDefaultW);
  EXPECT_EQ(data.reserved_for_future_use, kExpectedReservedDefault);
}

}  // namespace
}  // namespace iamf_tools
