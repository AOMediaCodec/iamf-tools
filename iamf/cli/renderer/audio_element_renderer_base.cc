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

#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/renderer/renderer_utils.h"
#include "iamf/common/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

AudioElementRendererBase::~AudioElementRendererBase() {}

absl::Status AudioElementRendererBase::Flush(
    std::vector<InternalSampleType>& rendered_samples) {
  absl::MutexLock lock(&mutex_);
  rendered_samples.insert(rendered_samples.end(), rendered_samples_.begin(),
                          rendered_samples_.end());
  rendered_samples_.clear();
  return absl::OkStatus();
}

absl::StatusOr<int> AudioElementRendererBase::RenderLabeledFrame(
    const LabeledFrame& labeled_frame) {
  std::vector<std::vector<InternalSampleType>> samples_to_render;
  RETURN_IF_NOT_OK(iamf_tools::renderer_utils::ArrangeSamplesToRender(
      labeled_frame, ordered_labels_, samples_to_render));

  // Render samples in concrete subclasses.
  mutex_.Lock();
  current_labeled_frame_ = &labeled_frame;
  mutex_.Unlock();
  std::vector<InternalSampleType> rendered_samples(
      num_output_channels_ * samples_to_render.size(), 0);
  RETURN_IF_NOT_OK(RenderSamples(samples_to_render, rendered_samples));

  // Copy rendered samples to the output.
  absl::MutexLock lock(&mutex_);
  rendered_samples_.insert(rendered_samples_.end(), rendered_samples.begin(),
                           rendered_samples.end());

  return samples_to_render.size();
}

}  // namespace iamf_tools
