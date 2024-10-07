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
#ifndef CLI_LOUDNESS_CALCULATOR_H_
#define CLI_LOUDNESS_CALCULATOR_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Abstract class for calculate loudness from an input audio stream.
 *
 * - Call the constructor with an input `MixPresentationLayout`.
 * - Call `AccumulateLoudnessForSamples()` to accumlate interleaved audio
 * samples to measure loudness on.
 * - Call `QueryLoudness()` to query the current loudness. The types to be
 * measured are determined from the constructor argument.
 */
class LoudnessCalculatorBase {
 public:
  /*!\brief Destructor. */
  virtual ~LoudnessCalculatorBase() = 0;

  /*!\brief Accumulates samples to be measured.
   *
   * \param rendered_samples Samples interleaved in IAMF canonical order to
   *        measure loudness on.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status AccumulateLoudnessForSamples(
      const std::vector<int32_t>& rendered_samples) = 0;

  /*!\brief Outputs the measured loudness.
   *
   * \param rendered_samples Samples interleaved in IAMF canonical order to
   *        measure loudness on.
   * \return Measured loudness on success. A specific status on failure.
   */
  virtual absl::StatusOr<LoudnessInfo> QueryLoudness() const = 0;

 protected:
  /*!\brief Constructor. */
  LoudnessCalculatorBase() {}
};

/*!\brief Loudness calculator which always returns the user provided loudness.
 */
class LoudnessCalculatorUserProvidedLoudness : public LoudnessCalculatorBase {
 public:
  /*!\brief Constructor.
   *
   * \param user_provided_loudness User provided loudness to echo back.
   */
  LoudnessCalculatorUserProvidedLoudness(
      const LoudnessInfo& user_provided_loudness)
      : user_provided_loudness_(user_provided_loudness) {}

  /*!\brief Destructor. */
  ~LoudnessCalculatorUserProvidedLoudness() override = default;

  /*!\brief Ignores the input samples.
   *
   * \param rendered_samples Samples to ignore.
   * \return `absl::OkStatus()` always.
   */
  absl::Status AccumulateLoudnessForSamples(
      const std::vector<int32_t>& /*rendered_samples*/) override {
    return absl::OkStatus();
  }

  /*!\brief Outputs the user provided loudness.
   *
   * \return `LoudnessInfo` provided in the constructor.
   */
  absl::StatusOr<LoudnessInfo> QueryLoudness() const override {
    return user_provided_loudness_;
  }

 private:
  const LoudnessInfo user_provided_loudness_;
};

}  // namespace iamf_tools

#endif  // CLI_LOUDNESS_CALCULATOR_H_
