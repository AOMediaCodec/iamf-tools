/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_RENDERER_PRECOMPUTED_GAINS_H_
#define CLI_RENDERER_PRECOMPUTED_GAINS_H_

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"

namespace iamf_tools {

typedef absl::flat_hash_map<
    std::string,
    absl::flat_hash_map<std::string, std::vector<std::vector<double>>>>
    PrecomputedGains;

/*!\brief Fills precomputed gains matrices.
 *
 * \return Map from input layout name to output layout name to precomputed
 *         gains.
 */
PrecomputedGains InitPrecomputedGains();

}  // namespace iamf_tools

#endif  // CLI_RENDERER_PRECOMPUTED_GAINS_H_
