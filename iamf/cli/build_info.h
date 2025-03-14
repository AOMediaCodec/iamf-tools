/*
 * Copyright (c) 2025 Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef CLI_BUILD_INFO_H_
#define CLI_BUILD_INFO_H_

namespace iamf_tools {

/*!\brief Returns a string with the build version.
 *
 * \return Null-terminated string representing the build version, or "unknown"
 *         if the build version is not available.
 */
const char* const GetBuildVersion();

}  // namespace iamf_tools

#endif  // CLI_BUILD_INFO_H_
