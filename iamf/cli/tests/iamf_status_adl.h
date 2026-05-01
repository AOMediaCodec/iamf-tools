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
#ifndef CLI_TESTS_IAMF_STATUS_ADL_H_
#define CLI_TESTS_IAMF_STATUS_ADL_H_

#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace iamf_tools {
namespace api {

// Enable `IsOk` and `Not(IsOk)` for `IamfStatus` via ADL.
inline const IamfStatus& GetStatus(const IamfStatus& status) { return status; }

}  // namespace api
}  // namespace iamf_tools

#endif  // CLI_TESTS_IAMF_STATUS_ADL_H_
