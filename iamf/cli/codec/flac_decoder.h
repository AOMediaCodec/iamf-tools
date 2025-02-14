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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "include/FLAC/format.h"
#include "include/FLAC/ordinals.h"
#include "include/FLAC/stream_decoder.h"
namespace iamf_tools {

/*!brief Decoder for FLAC audio streams.
 */
class FlacDecoder : public DecoderBase {
 public:
  /*!brief Constructor.
   *
   * \param num_channels Number of channels for this stream.
   * \param num_samples_per_frame Number of samples per frame.
   */
  FlacDecoder(int num_channels, uint32_t num_samples_per_frame);

  ~FlacDecoder() override;

  /*!\brief Initializes the underlying libflac decoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize() override;

  /*!\brief Gets the number of samples per channel.
   *
   * \return Number of samples per channel.
   */
  int GetNumSamplesPerChannel() { return num_samples_per_channel_; }

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

  /*!\brief Reads an encoded flac frame into the libflac decoder
   *
   * This callback function is used whenever the decoder needs more input data.
   *
   * \param decoder Unused libflac stream decoder. This parameter is not used in
   *        this implementation, but is included to override the libflac
   *        signature.
   * \param buffer Output buffer for the encoded frame.
   * \param bytes Maximum size of the buffer; in the case of a successful read,
   *        this will be set to the actual number of bytes read.
   * \param client_data universal pointer, which in this case should point to
   *        FlacDecoder.
   *
   * \return A libflac read status indicating whether the read was successful.
   */
  static FLAC__StreamDecoderReadStatus LibFlacReadCallback(
      const FLAC__StreamDecoder* /*decoder*/, FLAC__byte buffer[],
      size_t* bytes, void* client_data);

  /*!\brief Writes a decoded flac frame to an instance of FlacDecoder.
   *
   * This callback function is used to write out a decoded frame from the
   * libflac decoder.
   *
   * \param decoder Unused libflac stream decoder. This parameter is not used in
   *        this implementation, but is included to override the libflac
   *        signature.
   * \param frame libflac encoded frame metadata.
   * \param buffer Array of pointers to decoded channels of data. Each pointer
   *        will point to an array of signed samples of length
   *        `frame->header.blocksize`. Channels will be ordered according to the
   *        FLAC specification.
   * \param client_data Universal pointer, which in this case should point to
   *        FlacDecoder.
   *
   * \return A libflac write status indicating whether the write was successful.
   */
  static FLAC__StreamDecoderWriteStatus LibFlacWriteCallback(
      const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame,
      const FLAC__int32* const buffer[], void* client_data);

  /*!\brief Logs an error from the libflac decoder.
   *
   *  This function will be called whenever an error occurs during libflac
   *  decoding.
   *
   * \param decoder Unused libflac stream decoder. This parameter is not used in
   *        this implementation, but is included to override the libflac
   *        signature.
   * \param status The error encountered by the decoder.
   * \param client_data Universal pointer, which in this case should point to
   *        FlacDecoder. Unused in this implementation.
   */
  static void LibFlacErrorCallback(const FLAC__StreamDecoder* /*decoder*/,
                                   FLAC__StreamDecoderErrorStatus status,
                                   void* /*client_data*/);

 private:
  std::vector<uint8_t> encoded_frame_ = {};
  std::vector<std::vector<int32_t>> decoded_frame_ = {};
  // A pointer to the `libflac` decoder.
  FLAC__StreamDecoder* decoder_ = nullptr;
};

}  // namespace iamf_tools

#endif  // CLI_CODEC_FLAC_DECODER_H_
