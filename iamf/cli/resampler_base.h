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
 *   - Repeatedly call `PushFrame()` to push in samples. Available samples will
 *     be located via the output argument.
 *
 *   - After all samples have been pushed it is RECOMMENDED to call `Flush()` to
 *     retrieve any remaining ticks. Available samples will be located via the
 *     output argument.
 *   - It is safe and encouraged to reuse the buffers between calls.
 *     Implementations are free to clear and resize the buffers as needed.
 */
class ResamplerBase {
 public:
  /*!\brief Destructor. */
  virtual ~ResamplerBase() = 0;

  /*!\brief Pushes a frame of samples to the resampler.
   *
   * \param time_channel_samples Samples to push arranged in (time, channel).
   * \param output_time_channel_samples Output samples arranged in (time,
   *        channel).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushFrame(
      absl::Span<const std::vector<int32_t>> time_channel_samples,
      std::vector<std::vector<int32_t>>& output_time_channel_samples) = 0;

  /*!\brief Signals to close the resampler and flush any remaining samples.
   *
   * It is bad practice to reuse the resampler after calling this function.
   *
   * \param output_time_channel_samples Output samples arranged in (time,
   *        channel).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Flush(
      std::vector<std::vector<int32_t>>& output_time_channel_samples) = 0;
};

}  // namespace iamf_tools

#endif  // CLI_RESAMPLER_BASE_H_
