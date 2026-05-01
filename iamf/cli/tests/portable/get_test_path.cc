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

// [internal] Placeholder for get runfiles header.
#include "absl/strings/string_view.h"

namespace iamf_tools {

std::string GetRunfilesPath(absl::string_view path) {
  return (std::filesystem::current_path() / path).string();
}

std::string GetRunfilesFile(absl::string_view path, std::string_view filename) {
  return ((std::filesystem::current_path() / path) / filename).string();
}

}  // namespace iamf_tools
