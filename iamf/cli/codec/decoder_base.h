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

#ifndef CLI_DECODER_BASE_H_
#define CLI_DECODER_BASE_H_

#include <cstdint>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A common interfaces for all decoders.
 */
class DecoderBase {
 public:
  /*!\brief Constructor.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_channel Number of samples per channel.
   */
  DecoderBase(int num_channels, uint32_t num_samples_per_channel)
      : num_channels_(num_channels),
        num_samples_per_channel_(num_samples_per_channel) {
    decoded_samples_.reserve(num_samples_per_channel_);
  }

  /*!\brief Destructor.
   */
  virtual ~DecoderBase() = default;

  /*!\brief Decodes an audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status DecodeAudioFrame(
      absl::Span<const uint8_t> encoded_frame) = 0;

  /*!\brief Outputs valid decoded samples as a span.
   *
   * \return Span of valid decoded samples.
   */
  absl::Span<const std::vector<InternalSampleType>> ValidDecodedSamples()
      const {
    return absl::MakeConstSpan(decoded_samples_);
  }

 protected:
  const int num_channels_;
  const uint32_t num_samples_per_channel_;

  // Stores the output decoded frames arranged in (channel, time) axes. That
  // is to say, each inner vector has one sample for per time tick and the outer
  // vector contains one inner vector for each channel. When the decoded
  // samples is shorter than a frame, the inner vectors will be resized to fit
  // the valid portion.
  std::vector<std::vector<InternalSampleType>> decoded_samples_;
};

}  // namespace iamf_tools

#endif  // CLI_DECODER_BASE_H_
