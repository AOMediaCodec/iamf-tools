/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/codec/encoder_base.h"

#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/common/macros.h"

namespace iamf_tools {

EncoderBase::~EncoderBase() {}

absl::Status EncoderBase::Initialize() {
  RETURN_IF_NOT_OK(InitializeEncoder());

  // Some encoders depend on `InitializeEncoder` being called before
  // `SetNumberOfSamplesToDelayAtStart`.
  RETURN_IF_NOT_OK(SetNumberOfSamplesToDelayAtStart());
  return absl::OkStatus();
}

absl::Status EncoderBase::ValidateInputSamples(
    const std::vector<std::vector<int32_t>>& samples) const {
  if (!supports_partial_frames_ && samples.size() != num_samples_per_frame_) {
    LOG(ERROR) << "Found " << samples.size()
               << " samples per channels. Expected " << num_samples_per_frame_
               << ".";
    return absl::InvalidArgumentError("");
  }
  if (samples.empty()) {
    return absl::InvalidArgumentError("");
  }
  if (samples[0].size() != num_channels_) {
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
