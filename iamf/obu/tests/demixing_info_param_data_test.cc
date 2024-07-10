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
#include "iamf/obu/demixing_info_param_data.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

const int kDMixPModeBitShift = 5;
const int kDefaultWBitShift = 4;

TEST(DMixPModeToDownMixingParams, DMixPMode1) {
  DownMixingParams output_down_mix_args;
  EXPECT_THAT(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  DemixingInfoParameterData::kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  DemixingInfoParameterData::kNormal, output_down_mix_args),
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
                  DemixingInfoParameterData::kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  DemixingInfoParameterData::kFirstFrame, output_down_mix_args),
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
                  DemixingInfoParameterData::kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  DemixingInfoParameterData::kDefault, output_down_mix_args),
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
  EXPECT_EQ(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                DemixingInfoParameterData::kDMixPModeReserved1, 5,
                /*w_idx_update_rule=*/
                DemixingInfoParameterData::kNormal, output_down_mix_args)
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(DMixPModeToDownMixingParams, InvalidWOffsetOver10) {
  DownMixingParams output_down_mix_args;
  EXPECT_EQ(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                DemixingInfoParameterData::kDMixPModeReserved1, 11,
                /*w_idx_update_rule=*/
                DemixingInfoParameterData::kNormal, output_down_mix_args)
                .code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(WriteDemixingInfoParameterData, WriteDMixPMode1) {
  DemixingInfoParameterData data;
  data.dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  data.reserved = 0;

  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(
      wb, {DemixingInfoParameterData::kDMixPMode1 << kDMixPModeBitShift});
}

TEST(WriteDemixingInfoParameterData, WriteDMixPMode3) {
  DemixingInfoParameterData data;
  data.dmixp_mode = DemixingInfoParameterData::kDMixPMode3;
  data.reserved = 0;

  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(
      wb, {DemixingInfoParameterData::kDMixPMode3 << kDMixPModeBitShift});
}

TEST(WriteDemixingInfoParameterData, WriteReservedMax) {
  DemixingInfoParameterData data;
  data.dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  // The IAMF spec reserved a 5-bit value.
  const uint32_t kReservedMax = 31;
  data.reserved = kReservedMax;

  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());
  ValidateWriteResults(
      wb, {DemixingInfoParameterData::kDMixPMode1 << kDMixPModeBitShift |
           kReservedMax});
}

TEST(WriteDemixingInfoParameterData, IllegalWriteDMixPModeReserved) {
  DemixingInfoParameterData data;
  data.dmixp_mode = DemixingInfoParameterData::kDMixPModeReserved1;
  data.reserved = 0;

  WriteBitBuffer undetermined_wb(1);
  EXPECT_EQ(data.Write(undetermined_wb).code(),
            absl::StatusCode::kUnimplemented);
}

TEST(WriteDefaultDemixingInfoParameterData, Writes) {
  constexpr auto kExpectedDMixPMode = DemixingInfoParameterData::kDMixPMode1;
  constexpr auto kExpectedReserved = 31;
  constexpr auto kExpectedDefaultW = 5;
  constexpr auto kExpectedReservedDefault = 15;
  DefaultDemixingInfoParameterData data;
  data.dmixp_mode = kExpectedDMixPMode;
  data.reserved = kExpectedReserved;
  data.default_w = kExpectedDefaultW;
  data.reserved_default = kExpectedReservedDefault;

  WriteBitBuffer wb(1);
  EXPECT_THAT(data.Write(wb), IsOk());

  ValidateWriteResults(
      wb, {kExpectedDMixPMode << kDMixPModeBitShift | kExpectedReserved,
           kExpectedDefaultW << kDefaultWBitShift | kExpectedReservedDefault});
}

TEST(ReadDemixingInfoParameterData, ReadDMixPMode1) {
  std::vector<uint8_t> source_data = {DemixingInfoParameterData::kDMixPMode1
                                      << kDMixPModeBitShift};
  ReadBitBuffer rb(1024, &source_data);
  DemixingInfoParameterData data;
  EXPECT_THAT(data.Read(rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, DemixingInfoParameterData::kDMixPMode1);
  EXPECT_EQ(data.reserved, 0);
}

TEST(ReadDemixingInfoParameterData, ReadDMixPMode3) {
  std::vector<uint8_t> source_data = {DemixingInfoParameterData::kDMixPMode3
                                      << kDMixPModeBitShift};
  ReadBitBuffer rb(1024, &source_data);
  DemixingInfoParameterData data;
  EXPECT_THAT(data.Read(rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, DemixingInfoParameterData::kDMixPMode3);
  EXPECT_EQ(data.reserved, 0);
}

TEST(ReadDemixingInfoParameterData, ReadReservedMax) {
  const uint32_t kReservedMax = 31;
  std::vector<uint8_t> source_data = {DemixingInfoParameterData::kDMixPMode1
                                          << kDMixPModeBitShift |
                                      kReservedMax};
  ReadBitBuffer rb(1024, &source_data);
  DemixingInfoParameterData data;
  EXPECT_THAT(data.Read(rb), IsOk());
  EXPECT_EQ(data.dmixp_mode, DemixingInfoParameterData::kDMixPMode1);
  EXPECT_EQ(data.reserved, 31);
}

TEST(ReadsDefaultDemixingInfoParameterData, Reads) {
  constexpr auto kExpectedDMixPMode = DemixingInfoParameterData::kDMixPMode1_n;
  constexpr auto kExpectedReserved = 25;
  constexpr auto kExpectedDefaultW = 9;
  constexpr auto kExpectedReservedDefault = 12;
  std::vector<uint8_t> source_data = {
      kExpectedDMixPMode << kDMixPModeBitShift | kExpectedReserved,
      kExpectedDefaultW << kDefaultWBitShift | kExpectedReservedDefault};
  ReadBitBuffer rb(1024, &source_data);
  DefaultDemixingInfoParameterData data;

  EXPECT_THAT(data.Read(rb), IsOk());

  EXPECT_EQ(data.dmixp_mode, kExpectedDMixPMode);
  EXPECT_EQ(data.reserved, kExpectedReserved);
  EXPECT_EQ(data.default_w, kExpectedDefaultW);
  EXPECT_EQ(data.reserved_default, kExpectedReservedDefault);
}

}  // namespace
}  // namespace iamf_tools
