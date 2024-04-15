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

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
namespace iamf_tools {

AudioElementRendererBase::~AudioElementRendererBase() {}

absl::Status AudioElementRendererBase::Flush(
    std::vector<int32_t>& rendered_samples) {
  absl::MutexLock lock(&mutex_);
  rendered_samples.insert(rendered_samples.end(), rendered_samples_.begin(),
                          rendered_samples_.end());
  rendered_samples_.clear();
  return absl::OkStatus();
}

}  // namespace iamf_tools
