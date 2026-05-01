/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/tests/portable/get_test_path.h"

#include <filesystem>
#include <string>

#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

TEST(GetRunfilesPath, ReturnsNonEmptyString) {
  EXPECT_FALSE(GetRunfilesPath("any_path").empty());
}

TEST(GetRunfilesFile, ReturnsNonEmptyString) {
  EXPECT_FALSE(GetRunfilesFile("any_path", "any_file").empty());
}

TEST(GetRunfilesPath, ReturnsExistingPathForKnownDirectory) {
  // We know this directory exists in runfiles for tests in this package.
  const std::string path = GetRunfilesPath("iamf/cli/testdata/");
  EXPECT_TRUE(std::filesystem::exists(path));
}

TEST(GetRunfilesFile, ReturnsExistingPathForKnownFile) {
  // We know this file exists in runfiles for tests in this package.
  const std::string path =
      GetRunfilesFile("iamf/cli/testdata/", "stereo_8_samples_48khz_s16le.wav");
  EXPECT_TRUE(std::filesystem::exists(path));
}

}  // namespace
}  // namespace iamf_tools
