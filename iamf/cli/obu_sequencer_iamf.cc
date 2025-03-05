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
#include "iamf/cli/obu_sequencer_iamf.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <string>
#include <system_error>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

namespace {

constexpr int64_t kBufferStartSize = 65536;

// This sequencer does not care about the delay or timing information. It would
// be pointless to delay the descriptor OBUs.
constexpr bool kDoNotDelayDescriptorsUntilFirstUntrimmedSample = false;

void MaybeRemoveFile(const std::string& filename,
                     std::optional<std::fstream>& file_to_remove) {
  if (filename.empty() || !file_to_remove.has_value()) {
    return;
  }

  // Close and delete the file.
  file_to_remove->close();
  file_to_remove = std::nullopt;
  std::error_code error_code;
  std::filesystem::remove(filename, error_code);
  if (!error_code) {
    // File clean up failed somehow. Just log the error and move on.
    LOG(ERROR).WithPerror() << "Failed to remove " << filename;
  }
}

}  // namespace

ObuSequencerIamf::ObuSequencerIamf(const std::string& iamf_filename,
                                   bool include_temporal_delimiters,
                                   const LebGenerator& leb_generator)
    : ObuSequencerBase(leb_generator, include_temporal_delimiters,
                       kDoNotDelayDescriptorsUntilFirstUntrimmedSample),
      iamf_filename_(iamf_filename),
      wb_(kBufferStartSize, leb_generator) {}

absl::Status ObuSequencerIamf::PushSerializedDescriptorObus(
    uint32_t /*common_samples_per_frame*/, uint32_t /*common_sample_rate*/,
    uint8_t /*common_bit_depth*/,
    std::optional<int64_t> /*first_untrimmed_timestamp*/, int /*num_channels*/,
    absl::Span<const uint8_t> descriptor_obus) {
  if (!iamf_filename_.empty()) {
    LOG(INFO) << "Writing descriptor OBUs to " << iamf_filename_;

    output_iamf_.emplace(iamf_filename_, std::fstream::out | std::ios::binary);
  }

  RETURN_IF_NOT_OK(wb_.WriteUint8Span(descriptor_obus));
  return wb_.FlushAndWriteToFile(output_iamf_);
}

absl::Status ObuSequencerIamf::PushSerializedTemporalUnit(
    int64_t /*timestamp*/, int /*num_samples*/,
    absl::Span<const uint8_t> temporal_unit) {
  RETURN_IF_NOT_OK(wb_.WriteUint8Span(temporal_unit));
  return wb_.FlushAndWriteToFile(output_iamf_);
}

void ObuSequencerIamf::Flush() {
  if (output_iamf_.has_value() && output_iamf_->is_open()) {
    output_iamf_->close();
    output_iamf_ = std::nullopt;
  }
}

void ObuSequencerIamf::Abort() {
  LOG(INFO) << "Aborting ObuSequencerIamf.";
  MaybeRemoveFile(iamf_filename_, output_iamf_);
}

}  // namespace iamf_tools
