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
#ifndef COMMON_BUILD_INFO_H_
#define COMMON_BUILD_INFO_H_

#include <string>

#include "absl/strings/string_view.h"

namespace iamf_tools {

/*!\brief Resolves build version from stamped macro or `git archive` strings.
 *
 * Normalizes `git archive` `export-subst` values to match `git describe --tags
 * --long --always` (e.g. expanding an exact tag `v4.0.0` with commit `4b6caa6`
 * to `v4.0.0-0-g4b6caa6`), and falls back to `"unknown"` when neither
 * build-time stamping nor `git archive` substitution occurred.
 *
 * \param stamped_version Build-time `IAMF_TOOLS_BUILD_VERSION` value.
 * \param archive_describe `git archive` `%(describe:tags=true)` string.
 * \param archive_commit `git archive` `%h` short commit hash string.
 * \return Resolved build version string.
 */
std::string ResolveBuildVersion(absl::string_view stamped_version,
                                absl::string_view archive_describe,
                                absl::string_view archive_commit);

/*!\brief Returns build version string.
 *
 * \return View of static build version string, or "unknown" if build version is
 *         not available.
 */
absl::string_view GetBuildVersion();

}  // namespace iamf_tools

#endif  // COMMON_BUILD_INFO_H_
