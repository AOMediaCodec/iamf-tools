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
#include "iamf/cli/sample_processor_base.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"

namespace iamf_tools {

SampleProcessorBase::~SampleProcessorBase() {};

absl::Status SampleProcessorBase::PushFrame(
    absl::Span<const std::vector<int32_t>> time_channel_samples) {
  if (state_ != State::kTakingSamples) {
    return absl::FailedPreconditionError(absl::StrCat(
        "Do not use PushFrame() after Flush() is called. State= ", state_));
  }

  // Check the shape of the input data.
  if (time_channel_samples.size() > max_input_samples_per_frame_) {
    return absl::InvalidArgumentError(
        "Too many samples per frame. The maximum number of samples per frame "
        "is: " +
        absl::StrCat(max_input_samples_per_frame_) +
        ". The number of samples per frame received is: " +
        absl::StrCat(time_channel_samples.size()));
  }
  for (const auto& channel_samples : time_channel_samples) {
    if (channel_samples.size() != num_channels_) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Number of channels does not match the expected number of channels, "
          "num_channels_",
          num_channels_, " vs. ", channel_samples.size()));
    }
  }

  num_valid_ticks_ = 0;
  return PushFrameDerived(time_channel_samples);
}

absl::Status SampleProcessorBase::Flush() {
  if (state_ == State::kFlushCalled) {
    return absl::FailedPreconditionError(
        "Flush() called in unexpected state. Do not call Flush() twice.");
  }

  state_ = State::kFlushCalled;
  num_valid_ticks_ = 0;
  return FlushDerived();
}

absl::Span<const std::vector<int32_t>>
SampleProcessorBase::GetOutputSamplesAsSpan() const {
  return absl::MakeConstSpan(output_time_channel_samples_)
      .first(num_valid_ticks_);
}

}  // namespace iamf_tools
