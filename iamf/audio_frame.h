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
#ifndef AUDIO_FRAME_H_
#define AUDIO_FRAME_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/ia.h"
#include "iamf/obu_base.h"
#include "iamf/obu_header.h"
#include "iamf/read_bit_buffer.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {
/*!\brief The Audio Frame OBU.
 *
 * The length and meaning of the `audio_frame` field depends on the associated
 * `CodecConfigObu` and `AudioElementObu`.
 *
 * For IAMF-OPUS the field represents an opus packet of RFC6716.
 * For IAMF-AAC-LC the field represents an raw_data_block() of the AAC spec.
 * For IAMF-FLAC the field represents an FRAME of the FLAC spec.
 * For IAMF-LPCM the field represents PCM samples. When more than one byte is
 * used to represent a PCM sample, the byte order (i.e. its endianness) is
 * indicated in `sample_format_flags` from the corresponding `CodecConfigObu`.
 */
class AudioFrameObu : public ObuBase {
 public:
  /*\!brief Constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param substream_id Substream ID.
   */
  AudioFrameObu(const ObuHeader& header, DecodedUleb128 substream_id,
                const std::vector<uint8_t>& audio_frame);

  /*\!brief Move constructor.*/
  AudioFrameObu(AudioFrameObu&& other) = default;

  /*\!brief Destructor.*/
  ~AudioFrameObu() = default;

  friend bool operator==(const AudioFrameObu& lhs,
                         const AudioFrameObu& rhs) = default;

  /*\!brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  /*\!brief Gets the substream ID of the OBU.
   * \return Substream ID.
   */
  DecodedUleb128 GetSubstreamId() const { return audio_substream_id_; }

  std::vector<uint8_t> audio_frame_;

 private:
  // This field is not serialized when in the range [0, 17].
  const DecodedUleb128 audio_substream_id_;

  /*\!brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*\!brief Reads the OBU payload from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndReadPayload(ReadBitBuffer& rb) override;
};

}  // namespace iamf_tools

#endif  // AUDIO_FRAME_H_
