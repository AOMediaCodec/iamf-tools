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
#include "iamf/common/build_info.h"

#include <string>

#include "absl/base/no_destructor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace iamf_tools {

#ifdef IAMF_TOOLS_BUILD_VERSION
const char* GetStampedBuildVersion() { return IAMF_TOOLS_BUILD_VERSION; }
#else
const char* GetStampedBuildVersion();
#endif

namespace {

// `git archive` fills these in when preparing release archives.
constexpr absl::string_view kArchiveDescribe =
    R"($Format:%(describe:tags=true)$)";
constexpr absl::string_view kArchiveCommit = R"($Format:%h$)";

bool IsSubstitutedArchiveValue(absl::string_view value) {
  // "$" escape replacement by being split into two strings.
  return !value.empty() && !absl::StartsWith(value,
                                             "$"
                                             "Format:");
}

}  // namespace

std::string ResolveBuildVersion(absl::string_view stamped_version,
                                absl::string_view archive_describe,
                                absl::string_view archive_commit) {
  if (!stamped_version.empty()) {
    return std::string(stamped_version);
  }
  const bool has_describe = IsSubstitutedArchiveValue(archive_describe);
  const bool has_commit = IsSubstitutedArchiveValue(archive_commit);
  if (has_describe && has_commit) {
    if (absl::EndsWith(archive_describe, absl::StrCat("-g", archive_commit))) {
      return std::string(archive_describe);
    }
    return absl::StrCat(archive_describe, "-0-g", archive_commit);
  }
  if (has_describe) {
    return std::string(archive_describe);
  }
  if (has_commit) {
    return std::string(archive_commit);
  }
  return "unknown";
}

absl::string_view GetBuildVersion() {
  static const absl::NoDestructor<std::string> kBuildVersion(
      ResolveBuildVersion(GetStampedBuildVersion(), kArchiveDescribe,
                          kArchiveCommit));
  return *kBuildVersion;
}

}  // namespace iamf_tools
