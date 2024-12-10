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
#ifndef CLI_RESAMPLER_BASE_H_
#define CLI_RESAMPLER_BASE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace iamf_tools {

/*!\brief Abstract class to resample PCM samples.
 *
 * This class does not represent a normative portion of the IAMF spec and is
 * simply used for convenience when interfacing with or between components which
 * may have sample rate differences for a variety of reasons.
 *
 * This class represents an abstract interface to resample PCM samples. This
 * class is useful for post-processing or preprocessing audio depending on the
 * underlying IAMF codec.
 *
 * Usage pattern:
 *   - While input samples are available:
 *     - Call `PushFrame()` to push in samples
 *     - Call `GetOutputSamplesAsSpan()` to retrieve the samples.
 *   - Call `Flush()` to signal that no more frames will be pushed.
 *   - Call `GetOutputSamplesAsSpan()` one last time to retrieve any remaining
 *     samples.
 *
 *   - Note: Results from `GetOutputSamplesAsSpan()` must always be used before
 *     further calls to `PushFrame()` or `Flush()`.
 */
// TODO(b/382257677): Return an error, or at least test that it is safe, to push
//                    further frames after `Flush()` calls.
class ResamplerBase {
 public:
  /*!\brief Constructor.
   *
   * \param max_output_ticks Maximum number of ticks in the output timescale.
   * \param num_channels Number of channels.
   */
  ResamplerBase(size_t max_output_ticks, size_t num_channels)
      : output_time_channel_samples_(max_output_ticks,
                                     std::vector<int32_t>(num_channels)) {}

  /*!\brief Destructor. */
  virtual ~ResamplerBase() = 0;

  /*!\brief Pushes a frame of samples to the resampler.
   *
   * \param time_channel_samples Samples to push arranged in (time, channel).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushFrame(
      absl::Span<const std::vector<int32_t>> time_channel_samples) = 0;

  /*!\brief Signals to close the resampler and flush any remaining samples.
   *
   * After calling `Flush()`, it is implementation-defined to call `PushFrame()`
   * or `Flush()` again.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Flush() = 0;

  /*!\brief Gets a span of the output samples.
   *
   * \return Span of the output samples. The span will be invalidated when
   *         `PushFrame()` or `Flush()` is called.
   */
  absl::Span<const std::vector<int32_t>> GetOutputSamplesAsSpan() const {
    return absl::MakeConstSpan(output_time_channel_samples_)
        .first(num_valid_ticks_);
  }

 protected:
  std::vector<std::vector<int32_t>> output_time_channel_samples_;
  size_t num_valid_ticks_ = 0;
};

}  // namespace iamf_tools

#endif  // CLI_RESAMPLER_BASE_H_
