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

#ifndef CLI_AMBISONICS_MIXER_H_
#define CLI_AMBISONICS_MIXER_H_

#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/codec_config.h"

namespace iamf_tools {

/*!\brief Represents an Ambisonics mixer. */
class AmbisonicsMixer {
 public:
  /*!\brief Preset configuration for Ambisonics.
   *
   * The "best practice" series of preset configurations are intended to provide
   * reasonable default settings for a given use case. The best practices may
   * evolve over time, and the final settings may vary on other IA Sequence
   * settings (e.g. codec).
   */
  enum class Preset {
    kBestPracticeForOrder0 = 0,
    kBestPracticeForOrder1 = 1,
    kBestPracticeForOrder2 = 2,
    kBestPracticeForOrder3 = 3,
  };

  /*!\brief Makes an Ambisonics mixer from a preset.
   *
   * \param codec_id Codec ID.
   * \param preset Preset configuration.
   * \return AmbisonicsMixer.
   */
  static AmbisonicsMixer MakeFromPreset(CodecConfig::CodecId codec_id,
                                        Preset preset);

  /*!\brief Makes an Ambisonics mixer from an existing Ambisonics configuration.
   *
   * \param config Ambisonics configuration.
   * \return AmbisonicsMixer.
   */
  static AmbisonicsMixer MakeFromAmbisonicsConfig(
      const AmbisonicsConfig& config);

  /*!\brief Returns the Ambisonics configuration.
   *
   * \return Ambisonics configuration.
   */
  AmbisonicsConfig GetAmbisonicsConfig() const;

 private:
  /*!\brief Private constructor.
   *
   * \param config Ambisonics configuration.
   */
  explicit AmbisonicsMixer(AmbisonicsConfig config);

  AmbisonicsConfig ambisonics_config_;
};

}  // namespace iamf_tools

#endif  // CLI_AMBISONICS_MIXER_H_
