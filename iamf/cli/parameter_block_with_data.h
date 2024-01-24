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

#ifndef CLI_PARAMETER_BLOCK_WITH_DATA_H_
#define CLI_PARAMETER_BLOCK_WITH_DATA_H_

#include <cstdint>
#include <memory>

#include "iamf/parameter_block.h"

namespace iamf_tools {

struct ParameterBlockWithData {
  std::unique_ptr<ParameterBlockObu> obu;
  int32_t start_timestamp = 0;
  int32_t end_timestamp = 0;
};

}  // namespace iamf_tools

#endif  // CLI_PARAMETER_BLOCK_WITH_DATA_H_
