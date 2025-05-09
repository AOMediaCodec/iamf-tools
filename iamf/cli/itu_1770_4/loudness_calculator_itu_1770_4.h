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
#ifndef CLI_ITU_1770_4_LOUDNESS_CALCULATOR_ITU_1770_4_H_
#define CLI_ITU_1770_4_LOUDNESS_CALCULATOR_ITU_1770_4_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/obu/mix_presentation.h"
#include "include/ebur128_analyzer.h"

namespace iamf_tools {

/*!\brief Calculate loudness according the ITU 1770-4 for an input audio stream.
 *
 * - Call the builder with an input `MixPresentationLayout`
 * - Call `AccumulateLoudnessForSamples()` to accumulate interleaved audio
 * samples to measure loudness on.
 * - Call `QueryLoudness()` to query the current loudness. The types to be
 * measured are determined from the constructor argument.
 */
class LoudnessCalculatorItu1770_4 : public LoudnessCalculatorBase {
 public:
  /*!\brief Creates an ITU 1770-4 loudness calculator.
   *
   * \param layout Layout to measure loudness on.
   * \param num_samples_per_frame Number of samples per frame for the calculator
   *        to process. Subsequent calls to `AccumulateLoudnessForSamples()`
   *        must not have more sample than this.
   * \param rendered_sample_rate Sample rate of the rendered audio.
   * \param rendered_bit_depth Bit-depth of the rendered audio.
   */
  static std::unique_ptr<LoudnessCalculatorItu1770_4> CreateForLayout(
      const MixPresentationLayout& layout, uint32_t num_samples_per_frame,
      int32_t rendered_sample_rate, int32_t rendered_bit_depth);

  /*!\brief Destructor. */
  ~LoudnessCalculatorItu1770_4() override = default;

  /*!\brief Accumulates samples to be measured.
   *
   * \param channel_time_samples Samples to measure arranged in (channel, time).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status AccumulateLoudnessForSamples(
      absl::Span<const absl::Span<const int32_t>> channel_time_samples)
      override;

  /*!\brief Outputs the measured loudness.
   *
   * Outputs a `LoudnessInfo` with calculated values for `integrated_loudness`,
   * `digital_peak`, and (optionally) `true_peak` according to ITU-1770-4. Other
   * loudness values are copied over from the user-provided `LoudnessInfo`.
   *
   * \return Measured loudness on success. A specific status on failure.
   */
  absl::StatusOr<LoudnessInfo> QueryLoudness() const override;

 private:
  /*!\brief Constructor.
   *
   * \param num_samples_per_frame Number of samples per frame for the calculator
   *        to process.
   * \param num_channels Number of channels in the input audio.
   * \param weights Per-channel weights for each of the ITU-1770-4
   *        loudness-measurement bands.
   * \param rendered_sample_rate Sample rate of the rendered audio.
   * \param bit_depth_to_measure_loudness Bit-depth to use when measuring the
   *        rendered audio.
   * \param sample_format Sample format to use when measuring the rendered
   *        audio.
   * \param loudness_info User-provided loudness information.
   * \param enable_true_peak_measurement Whether to enable true peak
   *        measurement.
   */
  LoudnessCalculatorItu1770_4(
      uint32_t num_samples_per_frame, int32_t num_channels,
      const std::vector<float>& weights, int32_t rendered_sample_rate,
      int32_t bit_depth_to_measure_loudness,
      loudness::EbuR128Analyzer::SampleFormat sample_format,
      const LoudnessInfo& loudness_info, bool enable_true_peak_measurement)
      : num_samples_per_frame_(num_samples_per_frame),
        num_channels_(num_channels),
        bit_depth_to_measure_loudness_(bit_depth_to_measure_loudness),
        sample_format_(sample_format),
        user_provided_loudness_info_(loudness_info),
        interleaved_pcm_buffer_(num_samples_per_frame * num_channels *
                                bit_depth_to_measure_loudness / 8),
        ebu_r128_analyzer_(num_channels, weights, rendered_sample_rate,
                           enable_true_peak_measurement) {}

  const uint32_t num_samples_per_frame_;
  const int32_t num_channels_;
  const int32_t bit_depth_to_measure_loudness_;
  const loudness::EbuR128Analyzer::SampleFormat sample_format_;
  const LoudnessInfo user_provided_loudness_info_;

  // Reusable buffer between calls, to prevent excessive allocations.
  std::vector<uint8_t> interleaved_pcm_buffer_;

  loudness::EbuR128Analyzer ebu_r128_analyzer_;
};

}  // namespace iamf_tools

#endif  // CLI_ITU_1770_4_LOUDNESS_CALCULATOR_ITU_1770_4_H_
