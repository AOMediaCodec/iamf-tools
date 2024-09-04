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
#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/codec/flac_encoder.h"

namespace iamf_tools {

FlacEncoder::~FlacEncoder() {}

absl::Status FlacEncoder::EncodeAudioFrame(
    int /*input_bit_depth*/,
    const std::vector<std::vector<int32_t>>& /*samples*/,
    std::unique_ptr<AudioFrameWithData> /*partial_audio_frame_with_data*/) {
  return absl::UnimplementedError(
      "Encoding FLAC on native Windows is not yet implemented.");
}

absl::Status FlacEncoder::Finalize() {
  return absl::UnimplementedError(
      "Encoding FLAC on native Windows is not yet implemented.");
}

absl::Status FlacEncoder::InitializeEncoder() {
  return absl::UnimplementedError(
      "Encoding FLAC on native Windows is not yet implemented.");
}

}  // namespace iamf_tools
