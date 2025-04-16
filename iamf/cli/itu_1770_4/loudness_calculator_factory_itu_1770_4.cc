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
#include "iamf/cli/itu_1770_4/loudness_calculator_factory_itu_1770_4.h"

#include <cstdint>
#include <memory>

#include "iamf/cli/itu_1770_4/loudness_calculator_itu_1770_4.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

std::unique_ptr<LoudnessCalculatorBase>
LoudnessCalculatorFactoryItu1770_4::CreateLoudnessCalculator(
    const MixPresentationLayout& layout, uint32_t num_samples_per_frame,
    int32_t rendered_sample_rate, int32_t rendered_bit_depth) const {
  return LoudnessCalculatorItu1770_4::CreateForLayout(
      layout, num_samples_per_frame, rendered_sample_rate, rendered_bit_depth);
}

}  // namespace iamf_tools
