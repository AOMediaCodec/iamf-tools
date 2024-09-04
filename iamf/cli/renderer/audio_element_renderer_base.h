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
#ifndef CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_
#define CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
/*!\brief Abstract class to render a demixed audio element to a playback layout.
 *
 * This class represents an abstract interface to render a single audio element
 * to a single layout according to IAMF Spec 7.3.2
 * (https://aomediacodec.github.io/iamf/#processing-mixpresentation-rendering).
 *
 * - Call `RenderAudioFrame()` to render a labeled frame. The rendering may
 *   happen asynchronously.
 * - Call `Flush()` to retrieve finished frames, in the order they were
 *   received by `RenderLabeledFrame()`.
 * - Call `Finalize()` to close the renderer, telling it to finish rendering
 *   any remaining frames. Afterwards `IsFinalized()` should be called until it
 *   returns true, then audio frames should be  retrieved one last time via
 *   `Flush()`. After calling `Finalize()`, any subsequent call to
 *   `RenderAudioFrame()` may fail.
 * - Call `IsFinalized()` to ensure the renderer is Finalized.
 */
class AudioElementRendererBase {
 public:
  /*!\brief Destructor. */
  virtual ~AudioElementRendererBase() = 0;

  /*!\brief Accumulates samples to be rendered.
   *
   * \param labeled_frame Labeled frame to render.
   * \return Number of ticks which will be rendered. A specific status on
   *     failure.
   */
  virtual absl::StatusOr<int> RenderLabeledFrame(
      const LabeledFrame& labeled_frame) = 0;

  /*!\brief Flush finished audio frames.
   *
   * \param rendered_samples Vector to append rendered samples to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Flush(std::vector<InternalSampleType>& rendered_samples);

  /*!\brief Finalizes the renderer. Waits for it to finish any remaining frames.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Finalize() {
    absl::MutexLock lock(&mutex_);
    is_finalized_ = true;
    return absl::OkStatus();
  }

  /*!\brief Checks if the renderer is finalized.
   *
   * Sub-classes should override this if the renderer is not finalized directly
   * in the body of `Finalize()`.
   *
   * \return `true` if the render is finalized. `false` otherwise.
   */
  virtual bool IsFinalized() const {
    absl::MutexLock lock(&mutex_);
    return is_finalized_;
  }

 protected:
  /*!\brief Constructor. */
  AudioElementRendererBase() = default;

  // Mutex to guard simultaneous access to data members.
  mutable absl::Mutex mutex_;
  std::vector<InternalSampleType> rendered_samples_ ABSL_GUARDED_BY(mutex_);
  bool is_finalized_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_
