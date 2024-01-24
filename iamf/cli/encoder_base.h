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

#ifndef CLI_ENCODER_BASE_H_
#define CLI_ENCODER_BASE_H_

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/codec_config.h"

namespace iamf_tools {

class EncoderBase {
 public:
  /*\!brief Constructor.
   *
   * After constructing `Initialize` MUST be called and return successfully
   * before using most functionality of the encoder. Call `FinalizeAndFlush` to
   * close the encoder and retrieve flushed frames. Frames are flushed in the
   * order they were received by `EncodeAudioFrame`.
   *
   * \param supports_partial_frames `true` for encoders that support encoding
   *     frames shorter than `num_samples_per_frame_`. `false` otherwise.
   * \param codec_config Codec Config OBU for the encoder.
   * \num_channels Number of channels for the encoder.
   */
  EncoderBase(bool supports_partial_frames, const CodecConfigObu& codec_config,
              int num_channels)
      : supports_partial_frames_(supports_partial_frames),
        num_samples_per_frame_(codec_config.GetNumSamplesPerFrame()),
        input_sample_rate_(codec_config.GetInputSampleRate()),
        output_sample_rate_(codec_config.GetOutputSampleRate()),
        input_pcm_bit_depth_(codec_config.GetBitDepthToMeasureLoudness()),
        num_channels_(num_channels) {}

  /*\!brief Destructor. */
  virtual ~EncoderBase() = 0;

  /*!\brief Initializes `EncoderBase`.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize();

  /*!\brief Encodes an audio frame.
   *
   * \param input_bit_depth Bit-depth of the input data.
   * \param samples Samples arranged in (time x channel) axes. The samples are
   *     left-justified and stored in the upper `input_bit_depth` bits.
   * \param partial_audio_frame_with_data Unique pointer to take ownership of.
   *     The underlying `audio_frame_` is modifed. All other fields are blindly
   *     passed along.
   * \return `absl::OkStatus()` on success. Success does not necessarily mean
   *     the frame was finished. A specific status on failure.
   */
  virtual absl::Status EncodeAudioFrame(
      int input_bit_depth, const std::vector<std::vector<int32_t>>& samples,
      std::unique_ptr<AudioFrameWithData> partial_audio_frame_with_data) = 0;

  /*!\brief Finalizes and flushes all audio frames to output argument.
   *
   * This function MUST be called at most once to retrieve encoder audio frames.
   *
   * \param audio_frames List to append finished frames to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status FinalizeAndFlush(
      std::list<AudioFrameWithData>& audio_frames) {
    audio_frames.splice(audio_frames.end(), finalized_audio_frames_);
    return absl::OkStatus();
  }

  /*!\brief Gets the required number of samples to delay at the start.
   *
   * Sometimes this is called "pre-skip". This represents the number of initial
   * "junk" samples output from the encoder. In IAMF this represents the
   * recommended amount of samples to trim at the start of a substream.
   *
   * \return Number of samples to delay at the start of the substream.
   */
  uint32_t GetNumberOfSamplesToDelayAtStart() const {
    return required_samples_to_delay_at_start_;
  }

  const bool supports_partial_frames_;
  const uint32_t num_samples_per_frame_;
  const uint32_t input_sample_rate_;
  const uint32_t output_sample_rate_;
  const uint8_t input_pcm_bit_depth_;
  const int num_channels_;

 protected:
  /*!\brief Initializes the child class.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status InitializeEncoder() = 0;

  /*!\brief Initializes `required_samples_to_delay_at_start_`.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status SetNumberOfSamplesToDelayAtStart() = 0;

  /*!\brief Validates `samples` has the correct number of ticks and channels.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ValidateInputSamples(
      const std::vector<std::vector<int32_t>>& samples) const;

  uint32_t required_samples_to_delay_at_start_ = 0;
  std::list<AudioFrameWithData> finalized_audio_frames_ = {};
};

}  // namespace iamf_tools

#endif  // CLI_ENCODER_BASE_H_
