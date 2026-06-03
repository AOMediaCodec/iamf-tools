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
#include "iamf/obu/param_definitions/subblock_schedule.h"

#include <array>
#include <cstdint>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAre;
using ::testing::Not;

using absl::MakeConstSpan;

TEST(CreateWithConstantSubblockDuration, FailsWhenDurationIsZero) {
  EXPECT_THAT(SubblockSchedule::CreateWithConstantSubblockDuration(
                  /*duration=*/0, /*constant_subblock_duration=*/0),
              Not(IsOk()));
}

TEST(CreateWithConstantSubblockDuration,
     FailsWhenConstantSubblockDurationIsGreaterThanDuration) {
  EXPECT_THAT(SubblockSchedule::CreateWithConstantSubblockDuration(64, 65),
              Not(IsOk()));
}

TEST(CreateWithConstantSubblockDuration,
     SucceedsWhenConstantSubblockDurationIsLessThanDuration) {
  EXPECT_THAT(SubblockSchedule::CreateWithConstantSubblockDuration(64, 63),
              IsOk());
}

TEST(CreateWithVariableSubblockDuration, FailsWhenEmptyInput) {
  EXPECT_THAT(SubblockSchedule::CreateWithVariableSubblockDuration({}),
              Not(IsOk()));
}

TEST(CreateWithVariableSubblockDuration, FailsWhenTotalDurationIsZero) {
  EXPECT_THAT(SubblockSchedule::CreateWithVariableSubblockDuration({0}),
              Not(IsOk()));
}

TEST(CreateWithVariableSubblockDuration, FailsWhenAnyDurationIsZero) {
  EXPECT_THAT(SubblockSchedule::CreateWithVariableSubblockDuration({1, 0}),
              Not(IsOk()));
}

TEST(CreateWithVariableSubblockDuration, SucceedsForExplicitSubblockDurations) {
  EXPECT_THAT(SubblockSchedule::CreateWithVariableSubblockDuration({60, 4}),
              IsOk());
}

TEST(CreateWithVariableSubblockDuration, FailsWhenAnySubblockDurationIsZero) {
  EXPECT_THAT(SubblockSchedule::CreateWithVariableSubblockDuration({64, 0}),
              Not(IsOk()));
}

TEST(CreateFromBuffer, ConstantSubblockDuration) {
  constexpr auto source = std::to_array<uint8_t>({
      0x40,  // duration (64)
      0x10   // constant_subblock_duration (16)
  });
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));

  auto schedule = SubblockSchedule::CreateFromBuffer(*buffer);

  ASSERT_THAT(schedule, IsOk());
  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 16);
}

TEST(CreateFromBuffer, VariableSubblockDuration) {
  constexpr auto source = std::to_array<uint8_t>({
      0x40,  // duration (64)
      0x00,  // constant_subblock_duration (0)
      0x02,  // num_subblocks (2)
      30,    // subblock_duration (30)
      34     // subblock_duration (34)
  });
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));

  auto schedule = SubblockSchedule::CreateFromBuffer(*buffer);

  ASSERT_THAT(schedule, IsOk());
  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(schedule->GetNumSubblocks(), 2);
  EXPECT_THAT(schedule->GetSubblockDurations(), ElementsAre(30, 34));
}

TEST(CreateFromBuffer, InvalidWhenNumSubblocksExceedsMaximum) {
  constexpr auto source = std::to_array<uint8_t>({
      0xc0, 0x00,       // duration (64)
      0x00,             // constant_subblock_duration (0)
      0x81, 0xf7, 0x0b  // num_subblocks (exceeds maximum)
  });
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));

  EXPECT_THAT(SubblockSchedule::CreateFromBuffer(*buffer), Not(IsOk()));
}

TEST(SubblockSchedule, GettersWorkForConstantSubblockDuration) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 16);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 16);
  EXPECT_EQ(schedule->GetNumSubblocks(), 4);
}

TEST(SubblockSchedule, GettersWorkForConstantSubblockDurationWithRemainder) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 15);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 15);
  EXPECT_EQ(schedule->GetNumSubblocks(), 5);
}

TEST(SubblockSchedule, GettersWorkForVariableSubblockDuration) {
  auto schedule = SubblockSchedule::CreateWithVariableSubblockDuration({60, 4});
  ASSERT_THAT(schedule, IsOk());

  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(schedule->GetNumSubblocks(), 2);
  EXPECT_THAT(schedule->GetSubblockDurations(), ElementsAre(60, 4));
}

TEST(Write, ConstantSubblockDuration) {
  auto schedule_or =
      SubblockSchedule::CreateWithConstantSubblockDuration(64, 16);
  ASSERT_THAT(schedule_or, IsOk());
  const auto& schedule = *schedule_or;
  WriteBitBuffer buffer(64);

  ASSERT_THAT(schedule.Write(buffer), IsOk());

  // Verify by reading back.
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(buffer.bit_buffer()));
  auto read_schedule = SubblockSchedule::CreateFromBuffer(*read_buffer);
  ASSERT_THAT(read_schedule, IsOk());
  EXPECT_EQ(read_schedule->GetDuration(), 64);
  EXPECT_EQ(read_schedule->GetConstantSubblockDuration(), 16);
}

TEST(Write, VariableSubblockDuration) {
  auto schedule_or =
      SubblockSchedule::CreateWithVariableSubblockDuration({30, 34});
  ASSERT_THAT(schedule_or, IsOk());
  const auto& schedule = *schedule_or;
  WriteBitBuffer buffer(64);

  ASSERT_THAT(schedule.Write(buffer), IsOk());

  // Verify by reading back.
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(buffer.bit_buffer()));
  auto read_schedule = SubblockSchedule::CreateFromBuffer(*read_buffer);
  ASSERT_THAT(read_schedule, IsOk());
  EXPECT_EQ(read_schedule->GetDuration(), 64);
  EXPECT_EQ(read_schedule->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(read_schedule->GetNumSubblocks(), 2);
  EXPECT_THAT(read_schedule->GetSubblockDurations(), ElementsAre(30, 34));
}

}  // namespace
}  // namespace iamf_tools
