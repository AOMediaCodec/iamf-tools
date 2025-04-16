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
#ifndef CLI_ITU_1770_4_LOUDNESS_CALCULATOR_FACTORY_ITU_1770_4_H_
#define CLI_ITU_1770_4_LOUDNESS_CALCULATOR_FACTORY_ITU_1770_4_H_

#include <cstdint>
#include <memory>

#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Factory which returns ITU 1770-4 loudness calculators.
 *
 * This factory creates `LoudnessCalculatorItu1770_4` calculators. It can be
 * used to measure the loudness of any layout defined in IAMF v1 (excluding
 * extensions).
 *
 * This factory is intended to be used when the user wants "accurate" loudness
 * measurements for a signal when played on a particular layout. It should only
 * be used when the user expects to pass in samples which are representative of
 * the signal the end user would receive.
 */
class LoudnessCalculatorFactoryItu1770_4
    : public LoudnessCalculatorFactoryBase {
 public:
  /*!\brief Creates an ITU 1770-4 loudness calculator.
   *
   * \param layout Layout to measure loudness on.
   * \param num_samples_per_frame Number of samples per frame for the calculator
   *        to process.
   * \param rendered_sample_rate Sample rate of the rendered audio.
   * \param rendered_bit_depth Bit-depth of the rendered audio.
   * \return Unique pointer to a loudness calculator or `nullptr` if it could
   *         not be created.
   */
  std::unique_ptr<LoudnessCalculatorBase> CreateLoudnessCalculator(
      const MixPresentationLayout& layout, uint32_t num_samples_per_frame,
      int32_t rendered_sample_rate, int32_t rendered_bit_depth) const override;

  /*!\brief Destructor. */
  ~LoudnessCalculatorFactoryItu1770_4() override = default;
};

}  // namespace iamf_tools

#endif  // CLI_ITU_1770_4_LOUDNESS_CALCULATOR_FACTORY_ITU_1770_4_H_
