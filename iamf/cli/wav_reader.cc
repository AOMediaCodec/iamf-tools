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

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "src/dsp/read_wav_file.h"

namespace iamf_tools {

WavReader::WavReader(const std::string& wav_filename,
                     const size_t num_samples_per_frame)
    : num_samples_per_frame_(num_samples_per_frame) {
  LOG(INFO) << "Reading \"" << wav_filename << "\"";
  file_ = std::fopen(wav_filename.c_str(), "rb");
  CHECK_NE(ReadWavHeader(file_, &info_), 0)
      << "Error reading header of file \"" << wav_filename << "\"";

  // Overwrite `info_.destination_alignment_bytes` to 4 to always
  // store results in 4 bytes (32 bits), so we can handle 16-, 24-, and 32-bit
  // PCMs.
  info_.destination_alignment_bytes = 4;

  // Log the header info.
  LOG(INFO) << "WAV header info:";
  LOG(INFO) << "  num_channels= " << info_.num_channels;
  LOG(INFO) << "  sample_rate_hz= " << info_.sample_rate_hz;
  LOG(INFO) << "  remaining_samples= " << info_.remaining_samples;
  LOG(INFO) << "  bit_depth= " << info_.bit_depth;
  LOG(INFO) << "  destination_alignment_bytes= "
            << info_.destination_alignment_bytes;
  LOG(INFO) << "  encoding= " << info_.encoding;
  LOG(INFO) << "  sample_format= " << info_.sample_format;

  // Initialize the buffers.
  buffers_.resize(num_samples_per_frame_);
  for (auto& buffer : buffers_) {
    buffer.resize(info_.num_channels);
  }
}

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
