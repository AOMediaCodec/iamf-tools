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
#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "include/opus.h"

namespace iamf_tools {

class OpusDecoder : public DecoderBase {
 public:
  /*!brief Factory function.
   *
   * \param codec_config_obu Codec config for this stream.
   * \param num_channels Number of channels for this stream.
   * \return Opus decoder on success. A specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<DecoderBase>> Create(
      const CodecConfigObu& codec_config_obu, int num_channels);

  /*!\brief Destructor
   */
  ~OpusDecoder() override;

  /*!\brief Decodes an Opus audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      absl::Span<const uint8_t> encoded_frame) override;

 private:
  // The decoder from `libopus` is in the global namespace.
  typedef ::OpusDecoder LibOpusDecoder;

  /* Private constructor.
   *
   * Used only by the factory function.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_frame Number of samples per frame for this stream.
   * \param decoder `libopus` decoder to use.
   */
  OpusDecoder(int num_channels, uint32_t num_samples_per_frame,
              LibOpusDecoder* /* absl_nonnull */ decoder)
      : DecoderBase(num_channels, num_samples_per_frame),
        interleaved_float_from_libopus_(num_samples_per_frame * num_channels),
        decoder_(decoder) {}

  // Size fixed at construction time.
  std::vector<float> interleaved_float_from_libopus_;
  LibOpusDecoder* const /* absl_nonnull */ decoder_;
};

}  // namespace iamf_tools

#endif  // CLI_OPUS_ENCODER_DECODER_H_
