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

#include "iamf/cli/wav_writer.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "src/dsp/write_wav_file.h"

namespace iamf_tools {

WavWriter::WavWriter(const std::string& wav_filename, int num_channels,
                     int sample_rate_hz, int bit_depth)
    : num_channels_(num_channels),
      sample_rate_hz_(sample_rate_hz),
      bit_depth_(bit_depth),
      total_samples_written_(0),
      filename_(wav_filename) {
  // Open the file to write to.
  LOG(INFO) << "Writer \"" << filename_ << "\"";
  file_ = std::fopen(filename_.c_str(), "wb");
  if (file_ == nullptr) {
    LOG(ERROR) << "Error opening file \"" << filename_ << "\"";
    return;
  }

  // Write a dummy header. This will be overwritten in the user's subsequent
  // call to `Finalize`.
  switch (bit_depth_) {
    case 16:
      CHECK_NE(WriteWavHeader(file_, 0, sample_rate_hz_, num_channels_), 0)
          << "Error writing header of file \"" << filename_ << "\"";
      return;
    case 24:
      CHECK_NE(WriteWavHeader24Bit(file_, 0, sample_rate_hz_, num_channels_), 0)
          << "Error writing header of file \"" << filename_ << "\"";
      return;
    case 32:
      CHECK_NE(WriteWavHeader32Bit(file_, 0, sample_rate_hz_, num_channels_), 0)
          << "Error writing header of file \"" << filename_ << "\"";
      return;
    default:
      LOG(WARNING) << "This implementation does not support writing "
                   << bit_depth_ << "-bit wav files.";
      // Abort to avoid leaving an empty wav file.
      Abort();
      return;
  }
}

WavWriter::WavWriter(WavWriter&& original)
    : num_channels_(original.num_channels_),
      sample_rate_hz_(original.sample_rate_hz_),
      bit_depth_(original.bit_depth_),
      total_samples_written_(original.total_samples_written_),
      file_(original.file_),
      filename_(original.filename_) {
  // Invalidate the file pointer on the original copy to prevent it from being
  // closed on destruction.
  original.file_ = nullptr;
}

// Write samples for all channels.
bool WavWriter::WriteSamples(uint8_t* buffer, size_t buffer_size) {
  if (file_ == nullptr) {
    return false;
  }

  if (buffer_size == 0) {
    // Nothing to write.
    return true;
  }

  if (buffer_size % (bit_depth_ * num_channels_ / 8) != 0) {
    LOG(ERROR) << "Must write an integer number of samples.";
    return false;
  }

  // Calculate how many samples there are.
  const int bytes_per_sample = bit_depth_ / 8;
  const size_t num_total_samples = (buffer_size) / bytes_per_sample;

  int result = 0;
  if (bit_depth_ == 16) {
    // Arrange the input samples into an int16_t to match the expected input of
    // `WriteWavSamples`.
    std::vector<int16_t> samples(num_total_samples, 0);
    for (int i = 0; i < num_total_samples * bytes_per_sample;
         i += bytes_per_sample) {
      samples[i / bytes_per_sample] = (buffer[i + 1] << 8) | buffer[i];
    }

    result = WriteWavSamples(file_, samples.data(), samples.size());
  } else if (bit_depth_ == 24) {
    // Arrange the input samples into an int32_t to match the expected input of
    // `WriteWavSamples24Bit` with the lowest byte unused.
    std::vector<int32_t> samples(num_total_samples, 0);
    for (int i = 0; i < num_total_samples * bytes_per_sample;
         i += bytes_per_sample) {
      samples[i / bytes_per_sample] =
          (buffer[i + 2] << 24) | buffer[i + 1] << 16 | buffer[i] << 8;
    }
    result = WriteWavSamples24Bit(file_, samples.data(), samples.size());
  } else if (bit_depth_ == 32) {
    // Arrange the input samples into an int32_t to match the expected input of
    // `WriteWavSamples32Bit`.
    std::vector<int32_t> samples(num_total_samples, 0);
    for (int i = 0; i < num_total_samples * bytes_per_sample;
         i += bytes_per_sample) {
      samples[i / bytes_per_sample] = buffer[i + 3] << 24 |
                                      buffer[i + 2] << 16 | buffer[i + 1] << 8 |
                                      buffer[i];
    }
    result = WriteWavSamples32Bit(file_, samples.data(), samples.size());
  } else {
    LOG(ERROR) << "WavWriter only supports 16, 24, and 32-bit samples."
               << bit_depth_;
  }

  if (result == 1) {
    total_samples_written_ += num_total_samples;
    return true;
  }

  return false;
}

WavWriter::~WavWriter() {
  if (file_ == nullptr) {
    return;
  }
  // Finalize the temporary header based on the total number of samples written
  // and close the file.
  std::fseek(file_, 0, SEEK_SET);
  if (bit_depth_ == 16) {
    WriteWavHeader(file_, total_samples_written_, sample_rate_hz_,
                   num_channels_);
  } else if (bit_depth_ == 24) {
    WriteWavHeader24Bit(file_, total_samples_written_, sample_rate_hz_,
                        num_channels_);
  } else if (bit_depth_ == 32) {
    WriteWavHeader32Bit(file_, total_samples_written_, sample_rate_hz_,
                        num_channels_);
  } else {
    LOG(WARNING) << "This implementation does not support writing "
                 << bit_depth_ << "-bit wav files.";
    return;
  }
  std::fclose(file_);
}

void WavWriter::Abort() {
  std::fclose(file_);
  std::remove(filename_.c_str());
  file_ = nullptr;
}

}  // namespace iamf_tools
