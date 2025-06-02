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
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
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
  /*!brief Factory function.
   *
   * \param decoder_config Decoder config for this stream.
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_frame Number of samples per frame for this stream.
   * \return LPCM decoder on success. A specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<DecoderBase>> Create(
      const LpcmDecoderConfig& decoder_config, int num_channels,
      uint32_t num_samples_per_frame);

  /*!brief Destructor. */
  ~LpcmDecoder() override = default;

  /*!\brief Decodes an LPCM audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      absl::Span<const uint8_t> encoded_frame) override;

 private:
  /* Private constructor.
   *
   * Used only by the factory function.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_frame Number of samples per frame for this stream.
   * \param little_endian Whether the samples are little endian.
   * \param bytes_per_sample Number of bytes per sample.
   */
  LpcmDecoder(int num_channels, uint32_t num_samples_per_frame,
              bool little_endian, size_t bytes_per_sample)
      : DecoderBase(num_channels, num_samples_per_frame),
        little_endian_(little_endian),
        bytes_per_sample_(bytes_per_sample) {}
  const bool little_endian_;
  const size_t bytes_per_sample_;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_LPCM_DECODER_H_
