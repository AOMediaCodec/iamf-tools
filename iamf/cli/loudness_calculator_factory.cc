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
#include "iamf/cli/loudness_calculator_factory.h"

#include <cstdint>
#include <memory>

#include "iamf/cli/loudness_calculator.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

LoudnessCalculatorFactoryBase::~LoudnessCalculatorFactoryBase() {}

std::unique_ptr<LoudnessCalculatorBase>
LoudnessCalculatorFactoryUserProvidedLoudness::CreateLoudnessCalculator(
    const MixPresentationLayout& layout, int32_t, int32_t) const {
  return std::make_unique<LoudnessCalculatorUserProvidedLoudness>(
      layout.loudness);
}

}  // namespace iamf_tools
