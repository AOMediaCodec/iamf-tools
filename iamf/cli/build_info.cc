/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
namespace iamf_tools {

// Bazel linkstamp values are provided as a macro. We need to extract it to a
// c-style string.
#define IAMF_STRINGIFY(x) #x
#define IAMF_EXTRACT_AND_STRINGIFY(x) IAMF_STRINGIFY(x)

char const* GetBuildVersion() {
#ifdef BUILD_CHANGELIST
  static const char kBuildVersion[] =
      IAMF_EXTRACT_AND_STRINGIFY(BUILD_CHANGELIST);
#else
  static const char kBuildVersion[] = "unknown";
#endif
  return kBuildVersion;
}

#undef IAMF_EXTRACT_AND_STRINGIFY
#undef IAMF_STRINGIFY

}  // namespace iamf_tools
