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
#ifndef CLI_AAC_ENCODER_DECODER_H_
#define CLI_AAC_ENCODER_DECODER_H_

#include <cstdint>
#include <vector>

// This symbol conflicts with a macro in fdk_aac.
#ifdef IS_LITTLE_ENDIAN
#undef IS_LITTLE_ENDIAN
#endif

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "libAACdec/include/aacdecoder_lib.h"

namespace iamf_tools {

// TODO(b/277731089): Test all of `aac_encoder_decoder.h`.
class AacDecoder : public DecoderBase {
 public:
  /*!\brief Constructor.
   *
   * \param codec_config_obu Codec Config OBU with initialization settings.
   * \param num_channels Number of channels for this stream.
   */
  AacDecoder(const CodecConfigObu& codec_config_obu, int num_channels);

  /*!\brief Destructor.
   */
  ~AacDecoder() override;

  /*!\brief Initializes the underlying decoder.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize() override;

  /*!\brief Decodes an AAC audio frame.
   *
   * \param encoded_frame Frame to decode.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DecodeAudioFrame(
      const std::vector<uint8_t>& encoded_frame) override;

 private:
  const AacDecoderConfig& aac_decoder_config_;
  AAC_DECODER_INSTANCE* decoder_ = nullptr;
};

}  // namespace iamf_tools

#endif  // CLI_AAC_ENCODER_DECODER_H_
