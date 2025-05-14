/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/api/internal_utils/internal_utils.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/include/iamf_tools/iamf_decoder.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace iamf_tools {

// Configure the wav writer and reusable sample buffer, based on output
// properties of the decoder.
api::IamfStatus SetupAfterDescriptors(
    const api::IamfDecoder& decoder, const std::string& output_filename,
    std::unique_ptr<WavWriter>& wav_writer,
    std::vector<uint8_t>& reusable_sample_buffer) {
  // Gather statistics about the output.
  uint32_t frame_size;
  iamf_tools::api::IamfStatus frame_size_status =
      decoder.GetFrameSize(frame_size);
  if (!frame_size_status.ok()) {
    return frame_size_status;
  }
  int sample_size_bytes = 0;
  switch (decoder.GetOutputSampleType()) {
    case iamf_tools::api::OutputSampleType::kInt16LittleEndian:
      sample_size_bytes = 2;
      break;
    case iamf_tools::api::OutputSampleType::kInt32LittleEndian:
      sample_size_bytes = 4;
      break;
    default:
      return api::IamfStatus::ErrorStatus("Unsupported output sample type.");
  }

  int num_channels;
  iamf_tools::api::IamfStatus num_channels_status =
      decoder.GetNumberOfOutputChannels(num_channels);
  if (!num_channels_status.ok()) {
    return num_channels_status;
  }
  uint32_t sample_rate;
  iamf_tools::api::IamfStatus sample_rate_status =
      decoder.GetSampleRate(sample_rate);
  if (!sample_rate_status.ok()) {
    return sample_rate_status;
  }

  LOG(INFO) << "Output sample rate: " << sample_rate;
  LOG(INFO) << "Output frame size: " << frame_size;
  LOG(INFO) << "Output number of channels: " << num_channels;
  LOG(INFO) << "Output sample size bytes: " << sample_size_bytes;

  // Now that we know output properties, create the wav writer, and configure
  // the size of the reusable sample buffer.
  const int sample_size_bits = sample_size_bytes * 8;
  wav_writer = WavWriter::Create(output_filename, num_channels, sample_rate,
                                 sample_size_bits, frame_size,
                                 /*write_wav_header=*/true);
  if (wav_writer == nullptr) {
    return api::IamfStatus::ErrorStatus("Failed to create wav writer.");
  }
  const size_t buffer_size_bytes =
      frame_size * num_channels * sample_size_bytes;
  reusable_sample_buffer.resize(buffer_size_bytes);
  return api::IamfStatus::OkStatus();
}

}  // namespace iamf_tools
