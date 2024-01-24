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
#include "iamf/demixing_info_param_data.h"

#include <cstdint>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/tests/test_utils.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {
namespace {

TEST(DMixPModeToDownMixingParams, DMixPMode1) {
  DownMixingParams output_down_mix_args;
  EXPECT_TRUE(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  DemixingInfoParameterData::kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  DemixingInfoParameterData::kNormal, output_down_mix_args)
                  .ok());

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
  EXPECT_TRUE(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  DemixingInfoParameterData::kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  DemixingInfoParameterData::kFirstFrame, output_down_mix_args)
                  .ok());

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
  EXPECT_TRUE(DemixingInfoParameterData::DMixPModeToDownMixingParams(
                  DemixingInfoParameterData::kDMixPMode1,
                  /*previous_w_idx=*/6,
                  /*w_idx_update_rule=*/
                  DemixingInfoParameterData::kDefault, output_down_mix_args)
                  .ok());

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

const int kDMixPModeBitShift = 5;

TEST(WriteDemixingInfoParameterData, WriteDMixPMode1) {
  DemixingInfoParameterData data;
  data.dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  data.reserved = 0;

  WriteBitBuffer wb(1);
  EXPECT_TRUE(data.Write(wb).ok());
  ValidateWriteResults(
      wb, {DemixingInfoParameterData::kDMixPMode1 << kDMixPModeBitShift});
}

TEST(WriteDemixingInfoParameterData, WriteDMixPMode3) {
  DemixingInfoParameterData data;
  data.dmixp_mode = DemixingInfoParameterData::kDMixPMode3;
  data.reserved = 0;

  WriteBitBuffer wb(1);
  EXPECT_TRUE(data.Write(wb).ok());
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
  EXPECT_TRUE(data.Write(wb).ok());
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

}  // namespace
}  // namespace iamf_tools
