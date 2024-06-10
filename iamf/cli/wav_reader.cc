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

#include "iamf/cli/wav_reader.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "src/dsp/read_wav_file.h"
#include "src/dsp/read_wav_info.h"

namespace iamf_tools {

namespace {
const int kAudioToTactileFailure = 0;
}

absl::StatusOr<WavReader> WavReader::CreateFromFile(
    const std::string& wav_filename, const size_t num_samples_per_frame) {
  if (num_samples_per_frame == 0) {
    return absl::InvalidArgumentError("num_samples_per_frame must be > 0");
  }
  LOG(INFO) << "Reading \"" << wav_filename << "\"";
  FILE* file = std::fopen(wav_filename.c_str(), "rb");
  if (file == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to open file: \"", wav_filename,
                     "\" with error: ", std::strerror(errno), "."));
  }

  ReadWavInfo info;
  if (ReadWavHeader(file, &info) == kAudioToTactileFailure) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to read header of file: \"", wav_filename,
                     "\". Maybe it is not a valid RIFF WAV."));
  }

  // Overwrite `info_.destination_alignment_bytes` to 4 to always store results
  // in 4 bytes (32 bits), so we can handle 16-, 24-, and 32-bit PCMs.
  info.destination_alignment_bytes = 4;

  // Log the header info.
  LOG(INFO) << "WAV header info:";
  LOG(INFO) << "  num_channels= " << info.num_channels;
  LOG(INFO) << "  sample_rate_hz= " << info.sample_rate_hz;
  LOG(INFO) << "  remaining_samples= " << info.remaining_samples;
  LOG(INFO) << "  bit_depth= " << info.bit_depth;
  LOG(INFO) << "  destination_alignment_bytes= "
            << info.destination_alignment_bytes;
  LOG(INFO) << "  encoding= " << info.encoding;
  LOG(INFO) << "  sample_format= " << info.sample_format;

  return WavReader(num_samples_per_frame, file, info);
}

WavReader::WavReader(const size_t num_samples_per_frame, FILE* file,
                     const ReadWavInfo& info)
    : buffers_(num_samples_per_frame,
               std::vector<int32_t>(info.num_channels, 0)),
      num_samples_per_frame_(num_samples_per_frame),
      file_(file),
      info_(info) {}

WavReader::WavReader(WavReader&& original)
    : buffers_(std::move(original.buffers_)),
      num_samples_per_frame_(original.num_samples_per_frame_),
      file_(original.file_),
      info_(original.info_) {
  // Invalidate the file pointer on the original copy to prevent it from being
  // closed on destruction.
  original.file_ = nullptr;
}

WavReader::~WavReader() {
  if (file_ != nullptr) {
    std::fclose(file_);
  }
}

size_t WavReader::ReadFrame() {
  size_t samples_read = 0;
  for (int i = 0; i < buffers_.size(); i++) {
    samples_read +=
        ReadWavSamples(file_, &info_, buffers_[i].data(), buffers_[i].size());
  }
  return samples_read;
}

}  // namespace iamf_tools
