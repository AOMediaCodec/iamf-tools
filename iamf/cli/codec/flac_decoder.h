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

#ifndef CLI_CODEC_FLAC_DECODER_H_
#define CLI_CODEC_FLAC_DECODER_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
namespace iamf_tools {

/*!brief Decoder for FLAC audio streams.
 */
class FlacDecoder : public DecoderBase {
 public:
  /*!brief Constructor.
   *
   * \param codec_config_obu Codec Config OBU with initialization settings.
   * \param num_channels Number of channels for this stream.
   */
  FlacDecoder(const CodecConfigObu& codec_config_obu, int num_channels);

  ~FlacDecoder() override = default;

  /*!\brief Initializes the underlying decoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize() override;

  /*!\brief Decodes a FLAC audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \param decoded_samples Output decoded frames arranged in (time, sample)
   *        axes.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame,
      std::vector<std::vector<int32_t>>& decoded_samples) override;

  /*!\brief Sets an encoded FLAC frame in decoder.encoded_frame_.
   *
   * \param encoded_frame Encoded FLAC frame.
   */
  void SetEncodedFrame(const std::vector<uint8_t>& encoded_frame) {
    encoded_frame_ = encoded_frame;
  }

  /*!\brief Retrieves the encoded frame in decoder.encoded_frame_.
   *
   * \return Vector of encoded FLAC bytes representing a single frame.
   */
  std::vector<uint8_t> GetEncodedFrame() const { return encoded_frame_; }

  /*!\brief Sets a decoded FLAC frame in decoder.decoded_frame_.
   *
   * \param decoded_frame Decoded FLAC frame.
   */
  void SetDecodedFrame(const std::vector<std::vector<int32_t>>& decoded_frame) {
    decoded_frame_ = decoded_frame;
  }

  /*!\brief Retrieves the decoded FLAC frame in decoder.decoded_frame_.
   *
   * \return Vector of decoded FLAC samples.
   */
  std::vector<std::vector<int32_t>> GetDecodedFrame() const {
    return decoded_frame_;
  }

 private:
  std::vector<uint8_t> encoded_frame_ = {};
  std::vector<std::vector<int32_t>> decoded_frame_ = {};
  const FlacDecoderConfig decoder_config_;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_FLAC_DECODER_H_
