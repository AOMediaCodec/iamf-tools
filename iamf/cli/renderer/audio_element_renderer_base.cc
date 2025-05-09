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
#include "iamf/cli/renderer/audio_element_renderer_base.h"

#include <cstddef>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

AudioElementRendererBase::~AudioElementRendererBase() {}

absl::StatusOr<size_t> AudioElementRendererBase::RenderLabeledFrame(
    const LabeledFrame& labeled_frame) {
  absl::MutexLock lock(&mutex_);

  size_t num_valid_samples = 0;
  RETURN_IF_NOT_OK(iamf_tools::renderer_utils::ArrangeSamplesToRender(
      labeled_frame, ordered_labels_, kEmptyChannel, samples_to_render_,
      num_valid_samples));

  // Render samples in concrete subclasses.
  current_labeled_frame_ = &labeled_frame;

  std::vector<InternalSampleType> rendered_samples(
      num_output_channels_ * num_valid_samples, 0);
  RETURN_IF_NOT_OK(
      RenderSamples(absl::MakeConstSpan(samples_to_render_), rendered_samples));

  // Copy rendered samples to the output.
  rendered_samples_.insert(rendered_samples_.end(), rendered_samples.begin(),
                           rendered_samples.end());

  return num_valid_samples;
}

absl::Status AudioElementRendererBase::Flush(
    std::vector<InternalSampleType>& rendered_samples) {
  absl::MutexLock lock(&mutex_);
  rendered_samples.insert(rendered_samples.end(), rendered_samples_.begin(),
                          rendered_samples_.end());
  rendered_samples_.clear();
  return absl::OkStatus();
}

}  // namespace iamf_tools
