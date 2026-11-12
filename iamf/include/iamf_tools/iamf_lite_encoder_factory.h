/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef INCLUDE_IAMF_TOOLS_IAMF_LITE_ENCODER_FACTORY_H_
#define INCLUDE_IAMF_TOOLS_IAMF_LITE_ENCODER_FACTORY_H_

#include <memory>

#include "iamf_encoder_interface.h"
#include "iamf_tools_encoder_api_types.h"

namespace iamf_tools {
namespace api {

/*!\brief Factory functions for creating IAMF simplified encoders which removes
 * the dependency on protocol buffers.*/
class IamfLiteEncoderFactory {
 public:
  /*!\brief Factory function to create a 48 kHz Opus streaming encoder with
   *        one audio element.
   *
   * Creates an encoder configured with default descriptor OBUs for a single
   * Opus-encoded audio element at 48 kHz without requiring protocol buffers.
   *
   * \param audio_element_type Scene-based or channel-based layout type.
   * \return Encoder on success, or `nullptr` on failure.
   */
  static std::unique_ptr<IamfEncoderInterface> CreateOneElementOpusIamfEncoder(
      AudioElementType audio_element_type);

  /*!\brief Factory function to create a 16-bit little-endian linear PCM
   *        streaming encoder with one audio element.
   *
   * Creates an encoder configured with default descriptor OBUs for a single
   * PCM-encoded audio element without requiring protocol buffers.
   *
   * \param sample_rate Input and output audio sample rate.
   * \param audio_element_type Scene-based or channel-based layout type.
   * \return Encoder on success, or `nullptr` on failure.
   */
  static std::unique_ptr<IamfEncoderInterface>
  CreateOneElementInt16LePcmIamfEncoder(PcmSampleRate sample_rate,
                                        AudioElementType audio_element_type);

  IamfLiteEncoderFactory() = delete;
};

}  // namespace api
}  // namespace iamf_tools

#endif  // INCLUDE_IAMF_TOOLS_IAMF_LITE_ENCODER_FACTORY_H_
