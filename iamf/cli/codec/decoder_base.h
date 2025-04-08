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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace iamf_tools {

/*!\brief A common interfaces for all decoders.
 */
class DecoderBase {
 public:
  /*!\brief Constructor.
   *
   * After constructing `Initialize` MUST be called and return successfully
   * before using most functionality of the decoder.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_channel Number of samples per channel.
   */
  DecoderBase(int num_channels, uint32_t num_samples_per_channel)
      : num_channels_(num_channels),
        num_samples_per_channel_(num_samples_per_channel),
        decoded_samples_(num_samples_per_channel_,
                         std::vector<int32_t>(num_channels)),
        num_valid_ticks_(0) {}

  /*!\brief Destructor.
   */
  virtual ~DecoderBase() = default;

  /*!\brief Decodes an audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame) = 0;

  /*!\brief Outputs valid decoded samples.
   *
   * \return Valid decoded samples.
   */
  std::vector<std::vector<int32_t>> ValidDecodedSamples() const {
    return {decoded_samples_.begin(),
            decoded_samples_.begin() + num_valid_ticks_};
  }

 protected:
  const int num_channels_;
  const uint32_t num_samples_per_channel_;

  // Stores the output decoded frames arranged in (time, sample) axes. That
  // is to say, each inner vector has one sample for per channel and the outer
  // vector contains one inner vector for each time tick. When the decoded
  // samples is shorter than a frame, only the first `num_valid_ticks_` ticks
  // should be used.
  std::vector<std::vector<int32_t>> decoded_samples_;

  // Number of ticks (time samples) in `decoded_samples_` that are valid.
  size_t num_valid_ticks_;
};

}  // namespace iamf_tools

#endif  // CLI_DECODER_BASE_H_
