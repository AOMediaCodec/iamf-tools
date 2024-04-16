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
#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/demixing_module.h"

namespace iamf_tools {
/*\!brief Abstract class to render a demixed audio element to a playback layout.
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
  /*\!brief Arranges the samples to be rendered in (time, channel) axes.
   *
   * \param labeled_frame Labeled frame determine which original or demixed
   *     samples to trim and render.
   * \param ordered_labels Ordered list of original labels. Samples are arranged
   *     based on the original or demixed label samples in each time tick. Slots
   *     corresponding with empty labels ("") will create zeroed-out samples.
   * \param samples_to_render Output samples to render in (time, channel) axes.
   *     Samples which should be trimmed are omitted from the output.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status ArrangeSamplesToRender(
      const LabeledFrame& labeled_frame,
      const std::vector<std::string>& ordered_labels,
      std::vector<std::vector<int32_t>>& samples_to_render);

  /*\!brief Destructor. */
  virtual ~AudioElementRendererBase() = 0;

  /*\!brief Accumulates samples to be rendered.
   *
   * \param labeled_frame Labeled frame to render.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status RenderLabeledFrame(
      const LabeledFrame& labeled_frame) = 0;

  /*!\brief Flush finished audio frames.
   *
   * \param rendered_samples Vector to append rendered samples to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Flush(std::vector<int32_t>& rendered_samples);

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
  /*\!brief Constructor. */
  AudioElementRendererBase() = default;

  // Mutex to guard simultaneous access to data members.
  mutable absl::Mutex mutex_;
  std::vector<int32_t> rendered_samples_ ABSL_GUARDED_BY(mutex_);
  bool is_finalized_ ABSL_GUARDED_BY(mutex_) = false;
};

}  // namespace iamf_tools
#endif  // CLI_RENDERER_AUDIO_ELEMENT_RENDERER_BASE_H_
