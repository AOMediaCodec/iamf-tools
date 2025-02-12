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
#ifndef CLI_OPUS_ENCODER_DECODER_H_
#define CLI_OPUS_ENCODER_DECODER_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "include/opus.h"

namespace iamf_tools {

class OpusDecoder : public DecoderBase {
 public:
  /*!\brief Constructor
   *
   * \param codec_config_obu Codec Config OBU with initialization settings.
   * \param num_channels Number of channels for this stream.
   */
  OpusDecoder(const CodecConfigObu& codec_config_obu, int num_channels);

  /*!\brief Destructor
   */
  ~OpusDecoder() override;

  /*!\brief Initializes the underlying decoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize() override;

  /*!\brief Decodes an Opus audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame) override;

 private:
  // The decoder from `libopus` is in the global namespace.
  typedef ::OpusDecoder LibOpusDecoder;

  const OpusDecoderConfig& opus_decoder_config_;
  const uint32_t output_sample_rate_;

  LibOpusDecoder* decoder_ = nullptr;
};

}  // namespace iamf_tools

#endif  // CLI_OPUS_ENCODER_DECODER_H_
