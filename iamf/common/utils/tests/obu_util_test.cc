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

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(ReadFileToBytes, FailsIfFileDoesNotExist) {
  const std::filesystem::path file_path_does_not_exist(
      GetAndCleanupOutputFileName(".bin"));

  ASSERT_FALSE(std::filesystem::exists(file_path_does_not_exist));

  std::vector<uint8_t> bytes;
  EXPECT_FALSE(ReadFileToBytes(file_path_does_not_exist, bytes).ok());
}

void WriteVectorToFile(const std::filesystem::path filename,
                       const std::vector<uint8_t>& bytes) {
  std::filesystem::remove(filename);
  WriteBitBuffer wb(0);

  ASSERT_THAT(wb.WriteUint8Vector(bytes), IsOk());
  auto output_file = std::make_optional<std::fstream>(
      filename.string(), std::ios::binary | std::ios::out);
  ASSERT_THAT(wb.FlushAndWriteToFile(output_file), IsOk());
  output_file->close();
}

TEST(ReadFileToBytes, ReadsFileContents) {
  // Prepare a file to read back.
  const std::filesystem::path file_to_read(GetAndCleanupOutputFileName(".bin"));
  const std::vector<uint8_t> kExpectedBytes = {0x01, 0x02, 0x00, 0x03, 0x04};
  WriteVectorToFile(file_to_read, kExpectedBytes);

  std::vector<uint8_t> bytes;
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());

  EXPECT_EQ(bytes, kExpectedBytes);
}

TEST(ReadFileToBytes, AppendsFileContents) {
  // Prepare a file to read back.
  const std::filesystem::path file_to_read(GetAndCleanupOutputFileName(".bin"));
  const std::vector<uint8_t> kExpectedBytes = {0x01, 0x02, 0x00, 0x03, 0x04};
  WriteVectorToFile(file_to_read, kExpectedBytes);

  std::vector<uint8_t> bytes;
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());
  EXPECT_EQ(bytes.size(), kExpectedBytes.size());
  // The vector grows with each read.
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());
  EXPECT_EQ(bytes.size(), kExpectedBytes.size() * 2);
}

TEST(ReadFileToBytes, ReadsBinaryFileWithPlatformDependentControlCharacters) {
  // Prepare a file to read back.
  const std::filesystem::path file_to_read(GetAndCleanupOutputFileName(".bin"));
  const std::vector<uint8_t> kBinaryDataWithPlatformDependentControlCharacters =
      {'\n', '\r', '\n', '\r', '\x1a', '\r', '\n', '\n', ' ', '\n'};
  WriteVectorToFile(file_to_read,
                    kBinaryDataWithPlatformDependentControlCharacters);

  std::vector<uint8_t> bytes;
  EXPECT_THAT(ReadFileToBytes(file_to_read, bytes), IsOk());

  EXPECT_THAT(bytes, kBinaryDataWithPlatformDependentControlCharacters);
}

}  // namespace
}  // namespace iamf_tools
