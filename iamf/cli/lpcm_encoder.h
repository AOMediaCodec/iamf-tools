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
#ifndef CLI_LPCM_ENCODER_H_
#define CLI_LPCM_ENCODER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/encoder_base.h"
#include "iamf/codec_config.h"
#include "iamf/lpcm_decoder_config.h"

namespace iamf_tools {

class LpcmEncoder : public EncoderBase {
 public:
  LpcmEncoder(const CodecConfigObu& codec_config, int num_channels)
      : EncoderBase(true, codec_config, num_channels),
        decoder_config_(std::get<LpcmDecoderConfig>(
            codec_config.codec_config_.decoder_config)) {}

  ~LpcmEncoder() override = default;

 private:
  /*!\brief Initializes the underlying encoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitializeEncoder() override;

  /*!\brief Initializes `required_samples_to_delay_at_start_`.
   *
   * \return `absl::OkStatus()` always.
   */
  absl::Status SetNumberOfSamplesToDelayAtStart() override {
    required_samples_to_delay_at_start_ = 0;
    return absl::OkStatus();
  }

  /*!\brief Encodes an audio frame.
   *
   * \param input_bit_depth Ignored.
   * \param samples Samples arranged in (time x channel) axes. The samples are
   *     left-justified and stored in the upper `input_bit_depth` bits.
   *
   * \param partial_audio_frame_with_data Unique pointer to take ownership of.
   *     The underlying `audio_frame_` is modifed. All other fields are blindly
   *     passed along.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status EncodeAudioFrame(
      int /*input_bit_depth*/, const std::vector<std::vector<int32_t>>& samples,
      std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data)
      override;

  const LpcmDecoderConfig decoder_config_;
};
}  // namespace iamf_tools

#endif  // CLI_LPCM_ENCODER_H_
