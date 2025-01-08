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
#ifndef CLI_SAMPLE_PROCESSOR_BASE_H_
#define CLI_SAMPLE_PROCESSOR_BASE_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace iamf_tools {

/*!\brief Abstract class to process PCM samples.
 *
 * This class represents an abstract interface to reprocess PCM samples. In
 * general, processors could introduce delay or could result in a different
 * number of samples per frame.
 *
 * Usage pattern:
 *   - While input samples are available:
 *     - Call `PushFrame()` to push in samples.
 *     - Call `GetOutputSamplesAsSpan()` to retrieve the samples.
 *   - Call `Flush()` to signal that no more frames will be pushed.
 *   - Call `GetOutputSamplesAsSpan()` one last time to retrieve any remaining
 *     samples.
 *
 *   - Note: Results from `GetOutputSamplesAsSpan()` are invalidated by
 *     further calls to `PushFrame()` or `Flush()`.
 */
class SampleProcessorBase {
 public:
  /*!\brief Constructor.
   *
   * \param max_input_samples_per_frame Maximum number of samples per frame in
   *        the input timescale.
   * \param num_channels Number of channels. Later calls to `PushFrame()` must
   *        contain this many channels.
   * \param max_output_samples_per_frame Maximum number of samples per frame in
   *        the output timescale.
   */
  SampleProcessorBase(size_t max_input_samples_per_frame, size_t num_channels,
                      size_t max_output_samples_per_frame)
      : max_input_samples_per_frame_(max_input_samples_per_frame),
        num_channels_(num_channels),
        output_time_channel_samples_(max_output_samples_per_frame,
                                     std::vector<int32_t>(num_channels)) {}

  /*!\brief Destructor. */
  virtual ~SampleProcessorBase() = 0;

  /*!\brief Pushes a frame of samples to the processor.
   *
   * \param time_channel_samples Samples to push arranged in (time, channel).
   * \return `absl::OkStatus()` on success. `absl::FailedPreconditionError` if
   *         called after `Flush()`. Other specific statuses on failure.
   */
  absl::Status PushFrame(
      absl::Span<const std::vector<int32_t>> time_channel_samples);

  /*!\brief Signals to close the processor and flush any remaining samples.
   *
   * After calling `Flush()`, it is invalid to call `PushFrame()`
   * or `Flush()` again.
   *
   * \return `absl::OkStatus()` on success. `absl::FailedPreconditionError` if
   *         called after `Flush()`. Other specific statuses on failure.
   */
  absl::Status Flush();

  /*!\brief Gets a span of the output samples.
   *
   * \return Span of the output samples. The span will be invalidated when
   *         `PushFrame()` or `Flush()` is called.
   */
  absl::Span<const std::vector<int32_t>> GetOutputSamplesAsSpan() const;

 protected:
  /*!\brief Pushes a frame of samples to the processor.
   *
   * \param time_channel_samples Samples to push arranged in (time, channel).
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PushFrameDerived(
      absl::Span<const std::vector<int32_t>> time_channel_samples) = 0;

  /*!\brief Signals to close the processor and flush any remaining samples.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status FlushDerived() = 0;

  const size_t max_input_samples_per_frame_;
  const size_t num_channels_;

  // Stores the output decoded frames arranged in (time, sample) axes. That
  // is to say, each inner vector has one sample for per channel and the outer
  // vector contains one inner vector for each time tick. When the decoded
  // samples is shorter than a frame, only the first `num_valid_ticks_` ticks
  // should be used.
  std::vector<std::vector<int32_t>> output_time_channel_samples_;

  // Number of ticks (time samples) in `output_time_channel_samples_` that are
  // valid.
  size_t num_valid_ticks_ = 0;

 private:
  enum class State {
    kTakingSamples,
    kFlushCalled,
  } state_ = State::kTakingSamples;
};

}  // namespace iamf_tools

#endif  // CLI_SAMPLE_PROCESSOR_BASE_H_
