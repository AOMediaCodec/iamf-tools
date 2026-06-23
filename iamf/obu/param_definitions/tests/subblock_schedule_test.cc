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
#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::Not;
using ::testing::NotNull;

using absl::MakeConstSpan;

constexpr std::nullopt_t kDoNotWriteParameterData = std::nullopt;
constexpr int kInitialBufferSize = 64;

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

TEST(CreateWithConstantSubblockDuration, FailsWhenNumSubblocksExceedsMaximum) {
  EXPECT_THAT(SubblockSchedule::CreateWithConstantSubblockDuration(
                  SubblockSchedule::kMaxNumSubblocks + 1, 1),
              Not(IsOk()));
}

TEST(CreateWithConstantSubblockDuration,
     SucceedsWhenNumSubblocksEqualsMaximum) {
  EXPECT_THAT(SubblockSchedule::CreateWithConstantSubblockDuration(
                  SubblockSchedule::kMaxNumSubblocks, 1),
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
  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(30));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(34));
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
  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(2), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(3), IsOkAndHolds(16));
}

TEST(SubblockSchedule, GettersWorkForConstantSubblockDurationWithRemainder) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 15);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 15);
  EXPECT_EQ(schedule->GetNumSubblocks(), 5);
  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(15));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(15));
  EXPECT_THAT(schedule->GetSubblockDuration(2), IsOkAndHolds(15));
  EXPECT_THAT(schedule->GetSubblockDuration(3), IsOkAndHolds(15));
  EXPECT_THAT(schedule->GetSubblockDuration(4), IsOkAndHolds(4));
}

TEST(SubblockSchedule, GettersWorkForVariableSubblockDuration) {
  auto schedule = SubblockSchedule::CreateWithVariableSubblockDuration({60, 4});
  ASSERT_THAT(schedule, IsOk());

  EXPECT_EQ(schedule->GetDuration(), 64);
  EXPECT_EQ(schedule->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(schedule->GetNumSubblocks(), 2);
  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(60));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(4));
}

TEST(Write, ConstantSubblockDuration) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 16);
  ASSERT_THAT(schedule, IsOk());
  WriteBitBuffer buffer(kInitialBufferSize);

  ASSERT_THAT(schedule->Write(kDoNotWriteParameterData, buffer), IsOk());

  // Verify by reading back.
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(buffer.bit_buffer()));
  auto read_schedule = SubblockSchedule::CreateFromBuffer(*read_buffer);
  ASSERT_THAT(read_schedule, IsOk());
  EXPECT_EQ(read_schedule->GetDuration(), 64);
  EXPECT_EQ(read_schedule->GetConstantSubblockDuration(), 16);
}

TEST(Write, VariableSubblockDuration) {
  auto schedule =
      SubblockSchedule::CreateWithVariableSubblockDuration({30, 34});
  ASSERT_THAT(schedule, IsOk());
  WriteBitBuffer buffer(kInitialBufferSize);

  ASSERT_THAT(schedule->Write(kDoNotWriteParameterData, buffer), IsOk());

  // Verify by reading back.
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(buffer.bit_buffer()));
  auto read_schedule = SubblockSchedule::CreateFromBuffer(*read_buffer);
  ASSERT_THAT(read_schedule, IsOk());
  EXPECT_EQ(read_schedule->GetDuration(), 64);
  EXPECT_EQ(read_schedule->GetConstantSubblockDuration(), 0);
  EXPECT_EQ(read_schedule->GetNumSubblocks(), 2);
  EXPECT_THAT(read_schedule->GetSubblockDuration(0), IsOkAndHolds(30));
  EXPECT_THAT(read_schedule->GetSubblockDuration(1), IsOkAndHolds(34));
}

class DummyParameterData : public ParameterData {
 public:
  static std::unique_ptr<ParameterData> Create() {
    return std::make_unique<DummyParameterData>();
  }

  absl::Status ReadAndValidate(ReadBitBuffer& rb) override {
    // Read one byte as dummy data.
    uint8_t val;
    return rb.ReadUnsignedLiteral(8, val);
  }
  absl::Status Write(WriteBitBuffer& wb) const override {
    return wb.WriteUnsignedLiteral(0, 8);
  }
  void Print() const override {}
};

TEST(Write, ConstantSubblockDurationWithParameterData) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(
      /*duration=*/64, /*constant_subblock_duration=*/32);
  ASSERT_THAT(schedule, IsOk());
  WriteBitBuffer buffer(kInitialBufferSize);
  // For duration 64 and constant subblock duration 32, there are 2 subblocks.
  const std::array<std::unique_ptr<ParameterData>, 2> parameter_data = {
      std::make_unique<DummyParameterData>(),
      std::make_unique<DummyParameterData>()};

  ASSERT_THAT(schedule->Write(MakeConstSpan(parameter_data), buffer), IsOk());

  // Verify by reading back.
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(buffer.bit_buffer()));
  auto result = SubblockSchedule::CreateFromBufferWithParameterData(
      DummyParameterData::Create, *read_buffer);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->schedule.GetDuration(), 64);
  EXPECT_EQ(result->schedule.GetConstantSubblockDuration(), 32);
  EXPECT_EQ(result->schedule.GetNumSubblocks(), 2);
  EXPECT_THAT(result->parameter_data, ElementsAre(NotNull(), NotNull()));
}

TEST(Write, VariableSubblockDurationWithParameterData) {
  auto schedule = SubblockSchedule::CreateWithVariableSubblockDuration(
      /*subblock_durations=*/{30, 34});
  ASSERT_THAT(schedule, IsOk());
  WriteBitBuffer buffer(kInitialBufferSize);
  // 2 subblocks.
  const std::array<std::unique_ptr<ParameterData>, 2> parameter_data = {
      std::make_unique<DummyParameterData>(),
      std::make_unique<DummyParameterData>()};

  ASSERT_THAT(schedule->Write(MakeConstSpan(parameter_data), buffer), IsOk());

  // Verify by reading back.
  auto read_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      MakeConstSpan(buffer.bit_buffer()));
  auto result = SubblockSchedule::CreateFromBufferWithParameterData(
      DummyParameterData::Create, *read_buffer);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->schedule.GetDuration(), 64);
  EXPECT_EQ(result->schedule.GetConstantSubblockDuration(), 0);
  EXPECT_EQ(result->schedule.GetNumSubblocks(), 2);
  EXPECT_THAT(result->schedule.GetSubblockDuration(0), IsOkAndHolds(30));
  EXPECT_THAT(result->schedule.GetSubblockDuration(1), IsOkAndHolds(34));
  EXPECT_THAT(result->parameter_data, ElementsAre(NotNull(), NotNull()));
}

TEST(Write, FailsWithMismatchedParameterDataSize) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(
      /*duration=*/64, /*constant_subblock_duration=*/32);
  ASSERT_THAT(schedule, IsOk());
  WriteBitBuffer buffer(kInitialBufferSize);
  // For duration 64 and constant subblock duration 32, there are 2 subblocks.
  // We erroneously provide 1 parameter data.
  const std::array<std::unique_ptr<ParameterData>, 1> too_few_parameter_data = {
      std::make_unique<DummyParameterData>()};

  EXPECT_THAT(schedule->Write(MakeConstSpan(too_few_parameter_data), buffer),
              Not(IsOk()));
}

TEST(GetSubblockDuration, InvalidNegativeIndex) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 16);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_THAT(schedule->GetSubblockDuration(-1), Not(IsOk()));
}

TEST(GetSubblockDuration, InvalidIndexAboveGetNumSubblocks) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 16);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_THAT(schedule->GetSubblockDuration(4), Not(IsOk()));
}

TEST(GetSubblockDuration, ConstantDuration) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(64, 16);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(2), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(3), IsOkAndHolds(16));
  EXPECT_THAT(schedule->GetSubblockDuration(4), Not(IsOk()));
}

TEST(GetSubblockDuration, ConstantDurationWithRemainder) {
  auto schedule = SubblockSchedule::CreateWithConstantSubblockDuration(32, 15);
  ASSERT_THAT(schedule, IsOk());

  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(15));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(15));
  EXPECT_THAT(schedule->GetSubblockDuration(2), IsOkAndHolds(2));
  EXPECT_THAT(schedule->GetSubblockDuration(3), Not(IsOk()));
}

TEST(GetSubblockDuration, VariableDuration) {
  auto schedule =
      SubblockSchedule::CreateWithVariableSubblockDuration({30, 34});
  ASSERT_THAT(schedule, IsOk());

  EXPECT_THAT(schedule->GetSubblockDuration(0), IsOkAndHolds(30));
  EXPECT_THAT(schedule->GetSubblockDuration(1), IsOkAndHolds(34));
  EXPECT_THAT(schedule->GetSubblockDuration(2), Not(IsOk()));
}

TEST(CreateFromBufferWithParameterData, ConstantSubblockDuration) {
  constexpr auto source = std::to_array<uint8_t>({
      0x40,  // duration (64)
      0x20,  // constant_subblock_duration (32)
      // 64 / 32 = 2 subblocks.
      // Each subblock will read 1 byte of dummy data.
      0xaa,  // dummy data for subblock 0
      0xbb   // dummy data for subblock 1
  });
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));

  auto dummy_data_factory = []() {
    return std::make_unique<DummyParameterData>();
  };
  auto result = SubblockSchedule::CreateFromBufferWithParameterData(
      dummy_data_factory, *buffer);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->schedule.GetDuration(), 64);
  EXPECT_EQ(result->schedule.GetConstantSubblockDuration(), 32);
  EXPECT_EQ(result->schedule.GetNumSubblocks(), 2);
  EXPECT_THAT(result->schedule.GetSubblockDuration(0), IsOkAndHolds(32));
  EXPECT_THAT(result->schedule.GetSubblockDuration(1), IsOkAndHolds(32));
  EXPECT_THAT(result->parameter_data, ElementsAre(NotNull(), NotNull()));
}

TEST(CreateFromBufferWithParameterData, VariableSubblockDuration) {
  constexpr auto source = std::to_array<uint8_t>({
      0x40,  // duration (64)
      0x00,  // constant_subblock_duration (0)
      0x02,  // num_subblocks (2)
      30,    // subblock_duration (30)
      0xaa,  // dummy data for subblock 0 (1 byte)
      34,    // subblock_duration (34)
      0xbb   // dummy data for subblock 1 (1 byte)
  });
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(MakeConstSpan(source));

  auto dummy_data_factory = []() {
    return std::make_unique<DummyParameterData>();
  };
  auto result = SubblockSchedule::CreateFromBufferWithParameterData(
      dummy_data_factory, *buffer);

  ASSERT_THAT(result, IsOk());
  EXPECT_EQ(result->schedule.GetDuration(), 64);
  EXPECT_EQ(result->schedule.GetConstantSubblockDuration(), 0);
  EXPECT_EQ(result->schedule.GetNumSubblocks(), 2);
  EXPECT_THAT(result->schedule.GetSubblockDuration(0), IsOkAndHolds(30));
  EXPECT_THAT(result->schedule.GetSubblockDuration(1), IsOkAndHolds(34));
  EXPECT_THAT(result->parameter_data, ElementsAre(NotNull(), NotNull()));
}

}  // namespace
}  // namespace iamf_tools
