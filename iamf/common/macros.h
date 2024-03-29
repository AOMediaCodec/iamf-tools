/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_MACROS_H_
#define COMMON_MACROS_H_

namespace iamf_tools {

// For propagating errors when calling a function. Beware that defining
// `NO_CHECK_ERROR` is not thoroughly tested and may result in unexpected
// behavior.
#ifdef NO_CHECK_ERROR
#define RETURN_IF_NOT_OK(...)    \
  do {                           \
    (__VA_ARGS__).IgnoreError(); \
  } while (0)
#else
#define RETURN_IF_NOT_OK(...)             \
  do {                                    \
    absl::Status _status = (__VA_ARGS__); \
    if (!_status.ok()) return _status;    \
  } while (0)
#endif

}  // namespace iamf_tools

#endif  // COMMON_MACROS_H_
