/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef COMMON_UTILS_SAMPLE_PROCESSING_UTILS_H_
#define COMMON_UTILS_SAMPLE_PROCESSING_UTILS_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace iamf_tools {

/*!\brief Writes the input PCM sample to a buffer.
 *
 * Writes the most significant `sample_size` bits of `sample` starting at
 * `buffer[write_position]`. It is up to the user to ensure the buffer is valid.
 *
 * \param sample Sample to write the upper `sample_size` bits of.
 * \param sample_size Sample size in bits. MUST be one of {8, 16, 24, 32}.
 * \param big_endian `true` to write the sample as big endian. `false` to write
 *        it as little endian.
 * \param buffer Start of the buffer to write to.
 * \param write_position Offset of the buffer to write to. Incremented to one
 *        after the last byte written on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 *         `sample_size` is invalid.
 */
absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* buffer,
                            size_t& write_position);

/*!\brief Arranges the input samples by time and channel.
 *
 * \param samples Interleaved samples to arrange.
 * \param num_channels Number of channels.
 * \param transform_samples Function to transform each sample to the output
 *        type.
 * \param output Output vector to write the samples to. The size is not
 *        modified in this function even if the number of input samples do
 *        not fill the entire output vector. In that case, only the first
 *        `num_ticks` are filled.
 * \param num_ticks Number of ticks (time samples) of the output vector that
 *        are filled in this function.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *         number of samples is not a multiple of the number of channels. An
 *         error propagated from `transform_samples` if it fails.
 */
template <typename InputType, typename OutputType>
absl::Status ConvertInterleavedToTimeChannel(
    absl::Span<const InputType> samples, size_t num_channels,
    const absl::AnyInvocable<absl::Status(InputType, OutputType&) const>&
        transform_samples,
    std::vector<std::vector<OutputType>>& output, size_t& num_ticks) {
  if (samples.size() % num_channels != 0) [[unlikely]] {
    return absl::InvalidArgumentError(absl::StrCat(
        "Number of samples must be a multiple of the number of "
        "channels. Found ",
        samples.size(), " samples and ", num_channels, " channels."));
  }

  num_ticks = samples.size() / num_channels;
  if (num_ticks > output.size()) [[unlikely]] {
    return absl::InvalidArgumentError(absl::StrCat(
        "Number of ticks does not fit into the output vector: (num_ticks= ",
        num_ticks, " > output.size()= ", output.size(), ")"));
  }

  for (int t = 0; t < num_ticks; ++t) {
    auto& output_for_time = output[t];
    if (output_for_time.size() != num_channels) [[unlikely]] {
      return absl::InvalidArgumentError(absl::StrCat(
          "Number of channels is not equal to the output vector at tick ", t,
          ": (", num_channels, " != ", output[t].size(), ")"));
    }
    for (int c = 0; c < num_channels; ++c) {
      const auto status =
          transform_samples(samples[t * num_channels + c], output_for_time[c]);
      if (!status.ok()) [[unlikely]] {
        return status;
      }
    }
  }
  return absl::OkStatus();
}

/*!\brief Interleaves the input samples.
 *
 * \param samples Samples in (time, channel) axes to arrange.
 * \param transform_samples Function to transform each sample to the output
 *        type.
 * \param output Output vector to write the interleaved samples to.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if the
 *         input has an inconsistent number of channels. An error propagated
 *         from `transform_samples` if it fails.
 */
template <typename InputType, typename OutputType>
absl::Status ConvertTimeChannelToInterleaved(
    absl::Span<const std::vector<InputType>> input,
    const absl::AnyInvocable<absl::Status(InputType, OutputType&) const>&
        transform_samples,
    std::vector<OutputType>& output) {
  const size_t num_channels = input.empty() ? 0 : input[0].size();
  if (!std::all_of(input.begin(), input.end(), [&](const auto& tick) {
        return tick.size() == num_channels;
      })) {
    return absl::InvalidArgumentError(
        "All ticks must have the same number of channels.");
  }

  // TODO(b/382197581): avoid resizing inside this function.
  output.clear();
  output.reserve(input.size() * num_channels);
  for (const auto& tick : input) {
    for (const auto& sample : tick) {
      OutputType transformed_sample;
      auto status = transform_samples(sample, transformed_sample);
      if (!status.ok()) {
        return status;
      }
      output.emplace_back(transformed_sample);
    }
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools

#endif  // COMMON_UTILS_SAMPLE_PROCESSING_UTILS_H_
