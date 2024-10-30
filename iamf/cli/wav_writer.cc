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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "src/dsp/write_wav_file.h"

namespace iamf_tools {

std::unique_ptr<WavWriter> WavWriter::Create(const std::string& wav_filename,
                                             int num_channels,
                                             int sample_rate_hz, int bit_depth,
                                             bool write_header) {
  // Open the file to write to.
  LOG(INFO) << "Writer \"" << wav_filename << "\"";
  auto* file = std::fopen(wav_filename.c_str(), "wb");
  if (file == nullptr) {
    LOG(ERROR).WithPerror() << "Error opening file \"" << wav_filename << "\"";
    return nullptr;
  }

  // Write a dummy header. This will be overwritten in the destructor.
  WavHeaderWriter wav_header_writer;
  switch (bit_depth) {
    case 16:
      wav_header_writer = WriteWavHeader;
      break;
    case 24:
      wav_header_writer = WriteWavHeader24Bit;
      break;
    case 32:
      wav_header_writer = WriteWavHeader32Bit;
      break;
    default:
      LOG(WARNING) << "This implementation does not support writing "
                   << bit_depth << "-bit wav files.";
      std::fclose(file);
      std::remove(wav_filename.c_str());
      return nullptr;
  }

  // Set to an empty writer. The emptiness can be checked to skip writing the
  // header.
  if (!write_header) {
    wav_header_writer = WavHeaderWriter();
  } else if (wav_header_writer(file, 0, sample_rate_hz, num_channels) == 0) {
    LOG(ERROR) << "Error writing header of file \"" << wav_filename << "\"";
    return nullptr;
  }

  return absl::WrapUnique(new WavWriter(wav_filename, num_channels,
                                        sample_rate_hz, bit_depth, file,
                                        std::move(wav_header_writer)));
}

WavWriter::~WavWriter() {
  if (file_ == nullptr) {
    return;
  }

  // Finalize the temporary header based on the total number of samples written
  // and close the file.
  if (wav_header_writer_) {
    std::fseek(file_, 0, SEEK_SET);
    wav_header_writer_(file_, total_samples_written_, sample_rate_hz_,
                       num_channels_);
  }
  std::fclose(file_);
}

// Write samples for all channels.
bool WavWriter::WriteSamples(const std::vector<uint8_t>& buffer) {
  if (file_ == nullptr) {
    return false;
  }

  const auto buffer_size = buffer.size();

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
    // This should never happen because the factory method would never create
    // an object with disallowed `bit_depth_` values.
    LOG(FATAL) << "WavWriter only supports 16, 24, and 32-bit samples; got "
               << bit_depth_;
  }

  if (result == 1) {
    total_samples_written_ += num_total_samples;
    return true;
  }

  return false;
}

void WavWriter::Abort() {
  std::fclose(file_);
  std::remove(filename_to_remove_.c_str());
  file_ = nullptr;
}

WavWriter::WavWriter(const std::string& filename_to_remove, int num_channels,
                     int sample_rate_hz, int bit_depth, FILE* file,
                     WavHeaderWriter wav_header_writer)
    : num_channels_(num_channels),
      sample_rate_hz_(sample_rate_hz),
      bit_depth_(bit_depth),
      total_samples_written_(0),
      file_(file),
      filename_to_remove_(filename_to_remove),
      wav_header_writer_(std::move(wav_header_writer)) {}

}  // namespace iamf_tools
