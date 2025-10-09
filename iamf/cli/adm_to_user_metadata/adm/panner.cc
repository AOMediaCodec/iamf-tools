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

#include "iamf/cli/adm_to_user_metadata/adm/panner.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>

#include "Eigen/Core"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/ambisonic_encoder/ambisonic_encoder.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/types.h"
#include "src/dsp/read_wav_file.h"
#include "src/dsp/read_wav_info.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

using absl::MakeConstSpan;

constexpr int kDestinationAlignmentBytes = 4;
constexpr int kAmbisonicOrder = 3;
constexpr int kBufferSize = 256;
constexpr int kBitsPerByte = 8;
constexpr int kBitDepth16 = 16;
constexpr int kBitDepth24 = 24;
constexpr int kBitDepth32 = 32;

constexpr double kRadiansToDegrees = 180.0 / std::numbers::pi_v<double>;

}  // namespace

absl::Status PanObjectsToAmbisonics(const std::string& input_filename,
                                    const ADM& input_adm,
                                    const std::vector<size_t>& block_indices,
                                    WavWriter& wav_writer) {
  // Read input wav file.
  FILE* input_file = std::fopen(input_filename.c_str(), "rb");
  if (input_file == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrCat("Failed to open file: \"", input_filename,
                     "\" with error: ", std::strerror(errno), "."));
  }

  // Read header of input wav file.
  ReadWavInfo info;
  ABSL_CHECK_NE(ReadWavHeader(input_file, &info), 0)
      << "Error reading header of file \"" << input_file << "\"";

  const int ip_wav_bits_per_sample = info.bit_depth;
  const int ip_wav_nch = info.num_channels;
  const int op_wav_nch = kOutputWavChannels;
  const size_t ip_wav_total_num_samples = info.remaining_samples;
  info.destination_alignment_bytes = kDestinationAlignmentBytes;
  const size_t total_num_samples_per_channel =
      ip_wav_total_num_samples / ip_wav_nch;
  // Process the file in smaller chunks. Here we use "frames" in the same
  // context as the rest of the code base; one sample per frame implies one
  // sample for each channel.
  const size_t samples_per_frame = total_num_samples_per_channel < kBufferSize
                                       ? total_num_samples_per_channel
                                       : kBufferSize;

  if (ip_wav_bits_per_sample != kBitDepth16 &&
      ip_wav_bits_per_sample != kBitDepth24 &&
      ip_wav_bits_per_sample != kBitDepth32) {
    return absl::NotFoundError(absl::StrFormat(
        "Unsupported number of bits per sample: %d\n", ip_wav_bits_per_sample));
  }

  // Initialize the buffers to pass between various components.
  const size_t ip_buffer_alloc_size = samples_per_frame * ip_wav_nch;
  const size_t op_buffer_alloc_size = samples_per_frame * op_wav_nch;
  // Buffer of the samples from the input wav files.
  std::vector<int32_t> ip_buffer_int32(ip_buffer_alloc_size);
  // Float representation of the input buffer, to pass to the encoder.
  std::vector<float> ip_buffer_float(ip_buffer_alloc_size);
  // Float representation of the output buffer, to retrieve from the encoder.
  std::vector<float> op_buffer_float(op_buffer_alloc_size);
  // Internal representation of the output buffer.
  std::vector<std::vector<InternalSampleType>> op_buffer_internal_sample_type(
      op_wav_nch, std::vector<InternalSampleType>(samples_per_frame));
  // Channel-time 2-dimensional view backed by the internal representation, to
  // pass to wav_writer.
  std::vector<absl::Span<const InternalSampleType>> valid_channel_time_spans(
      op_wav_nch);

  // Create an Ambisonic encoder object.
  AmbisonicEncoder encoder(samples_per_frame, info.num_channels,
                           kAmbisonicOrder);

  // Assign sources to the encoder at all available input channels.
  for (int i = 0; i < ip_wav_nch; ++i) {
    auto& audio_block =
        input_adm.audio_channels[i].audio_blocks[block_indices[i]];
    auto x = audio_block.position.x;
    auto y = audio_block.position.y;
    auto z = audio_block.position.z;
    auto gain = audio_block.gain;

    Eigen::Vector3d position(x, y, z);
    auto azimuth = -((atan2(position[0], position[1])) * kRadiansToDegrees);
    auto elevation = (atan2(position[2], hypot(position[0], position[1]))) *
                     kRadiansToDegrees;
    auto distance = position.norm();

    encoder.SetSource(i, gain, azimuth, elevation, distance);
  }

  // Main processing loop.
  size_t samples_remaining = ip_wav_total_num_samples;
  size_t num_samples_to_read = samples_per_frame;
  auto max_value_db = 0.0f;
  while (samples_remaining > 0) {
    ABSL_CHECK_EQ(num_samples_to_read, samples_per_frame);
    // When remaining samples is below buffer capacity, pad unused buffer space
    // with zeros to ensure only valid sample data is processed.
    if (samples_remaining < ip_buffer_alloc_size) {
      num_samples_to_read = samples_remaining / ip_wav_nch;
      std::fill(ip_buffer_int32.begin() + samples_remaining,
                ip_buffer_int32.end(), 0);
    }
    // Read from the input file.
    const size_t samples_read = ReadWavSamples(
        input_file, &info, ip_buffer_int32.data(), ip_buffer_alloc_size);
    ABSL_CHECK_EQ(samples_read, num_samples_to_read * ip_wav_nch);

    // Convert int32 interleaved to float planar.
    for (size_t smp = 0; smp < samples_per_frame; ++smp) {
      for (size_t ch = 0; ch < ip_wav_nch; ++ch) {
        ip_buffer_float[ch * samples_per_frame + smp] =
            Int32ToNormalizedFloatingPoint<float>(
                ip_buffer_int32[smp * ip_wav_nch + ch]);
      }
    }

    // Process.
    encoder.ProcessPlanarAudioData(ip_buffer_float, op_buffer_float);

    // Warn if level exceeds 0 dBFS.
    for (size_t smp = 0; smp < samples_per_frame; ++smp) {
      auto ch = 0;  // Only look at the first channel, as the scene is SN3D
                    // normalized. Therefore, the first channel is the loudest.

      if (std::abs(op_buffer_float[ch * samples_per_frame + smp]) > 1.0f) {
        auto timestamp = ip_wav_total_num_samples - samples_remaining + smp;
        float level =
            20 *
            std::log10(std::abs(op_buffer_float[ch * samples_per_frame + smp]));
        max_value_db = std::max(max_value_db, level);

        ABSL_LOG_FIRST_N(WARNING, 5) << absl::StrFormat(
            "Clipping detected at sample %d. Sample exceeds 0 dBFS by: "
            "%.2f dB.",
            timestamp, level);
      }
    }

    // Create the internal representation of the data, a view of it, and pass it
    // to the wav writer.
    // Convert float planar to channel-time `InternalSampleType`.
    for (size_t smp = 0; smp < samples_per_frame; ++smp) {
      for (size_t ch = 0; ch < op_wav_nch; ++ch) {
        op_buffer_internal_sample_type[ch][smp] =
            static_cast<InternalSampleType>(
                op_buffer_float[ch * samples_per_frame + smp]);
      }
    }
    // Create a channel-time view of the first n ticks for each channel.
    for (size_t ch = 0; ch < op_wav_nch; ++ch) {
      valid_channel_time_spans[ch] =
          MakeConstSpan(op_buffer_internal_sample_type[ch])
              .subspan(0, num_samples_to_read);
    }
    // Write to the output file.
    RETURN_IF_NOT_OK(wav_writer.PushFrame(valid_channel_time_spans));

    samples_remaining -= samples_read;
  }

  if (max_value_db > 0.0f) {
    ABSL_LOG(WARNING) << absl::StrFormat(
        "Clipping detected during objects to Ambisonics panning. Maximum level "
        "exceeded 0 dBFS by: "
        "%.2f dB.",
        max_value_db);
  }

  std::fclose(input_file);

  return absl::OkStatus();
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
