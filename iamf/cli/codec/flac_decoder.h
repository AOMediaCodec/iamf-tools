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
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/flac_decoder_stream_callbacks.h"
#include "include/FLAC/stream_decoder.h"
namespace iamf_tools {

/*!brief Decoder for FLAC audio streams.
 */
class FlacDecoder : public DecoderBase {
 public:
  /*!brief Factory function.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_frame Number of samples per frame.
   * \return Flac decoder on success. A specific status on failure.
   */
  static absl::StatusOr<std::unique_ptr<DecoderBase>> Create(
      int num_channels, uint32_t num_samples_per_frame);

  ~FlacDecoder() override;

  /*!\brief Finalizes the underlying libflac decoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Finalize();

  /*!\brief Decodes a FLAC audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame) override;

 private:
  /* Private constructor.
   *
   * Used only by the factory function.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_frame Number of samples per frame for this stream.
   * \param callback_data Backing data for the `libflac` decoder callbacks.
   * \param decoder `libflac` decoder to use.
   */
  FlacDecoder(
      int num_channels, uint32_t num_samples_per_frame,
      /* absl_nonnull */
      std::unique_ptr<flac_callbacks::LibFlacCallbackData> callback_data,
      FLAC__StreamDecoder* /* absl_nonnull */ decoder)
      : DecoderBase(num_channels, num_samples_per_frame),
        callback_data_(std::move(callback_data)),
        decoder_(decoder) {}

  // Backing data for the `libflac` decoder callbacks. Held in `unique_ptr` for
  // pointer stability after move.
  /* absl_nonnull */ std::unique_ptr<flac_callbacks::LibFlacCallbackData>
      callback_data_;
  // A pointer to the `libflac` decoder.
  FLAC__StreamDecoder* const /* absl_nonnull */ decoder_;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_FLAC_DECODER_H_
