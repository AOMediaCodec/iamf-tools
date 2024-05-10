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

#ifndef CLI_CODEC_LPCM_DECODER_H_
#define CLI_CODEC_LPCM_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"

namespace iamf_tools {

/*!brief Decoder for LPCM audio streams.
 *
 * Class designed to decode one audio substream per instance when the
 * `codec_config_id` is "ipcm" and formatted as per IAMF Spec §3.5 and §3.11.4.
 * See https://aomediacodec.github.io/iamf/#lpcm-specific.
 */
class LpcmDecoder : public DecoderBase {
 public:
  /*!brief Constructor.
   *
   * \param codec_config_obu Codec Config OBU with initialization settings.
   * \param num_channels Number of channels for this stream.
   */
  LpcmDecoder(const CodecConfigObu& codec_config_obu, int num_channels);

  ~LpcmDecoder() override = default;

  absl::Status Initialize() override;

  absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame,
      std::vector<std::vector<int32_t>>& decoded_frames) override;

 private:
  const LpcmDecoderConfig decoder_config_;
  // We don't need the audio_roll_distance_ for decoding, but needed to validate
  // the LpcmDecoderConfig.
  int16_t audio_roll_distance_;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_LPCM_DECODER_H_
