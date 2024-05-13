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

#include "absl/status/status.h"

namespace iamf_tools {

/*\!brief A common interfaces for all decoders.
 */
class DecoderBase {
 public:
  /*\!brief Constructor.
   *
   * After constructing `Initialize` MUST be called and return successfully
   * before using most functionality of the decoder.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_channel Number of samplers per channel.
   */
  DecoderBase(int num_channels, int num_samples_per_channel)
      : num_channels_(num_channels),
        num_samples_per_channel_(num_samples_per_channel) {}

  /*\!brief Destructor.
   */
  virtual ~DecoderBase() = default;

  /*\!brief Initializes the underlying decoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status Initialize() = 0;

  /*\!brief Decodes an audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \param decoded_samples Output decoded frames arranged in (time, sample)
   *     axes.  That is to say, each inner vector has one sample for per channel
   *     and the outer vector contains one inner vector for each time tick.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame,
      std::vector<std::vector<int32_t>>& decoded_samples) = 0;

 protected:
  const int num_channels_;
  const int num_samples_per_channel_;
};

}  // namespace iamf_tools

#endif  // CLI_DECODER_BASE_H_
