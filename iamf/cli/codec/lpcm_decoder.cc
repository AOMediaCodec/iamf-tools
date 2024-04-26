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

#include "iamf/cli/codec/lpcm_decoder.h"

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"

namespace iamf_tools {

LpcmDecoder::LpcmDecoder(const CodecConfigObu& codec_config_obu,
                         int num_channels)
    : DecoderBase(num_channels,
                  static_cast<int>(codec_config_obu.GetNumSamplesPerFrame())),
      decoder_config_(std::get<LpcmDecoderConfig>(
          codec_config_obu.GetCodecConfig().decoder_config)) {}

absl::Status LpcmDecoder::Initialize() {
  return absl::UnimplementedError(
      "LPCMDecoder::Initialize() is not implemented.");
}

absl::Status LpcmDecoder::DecodeAudioFrame(
    const std::vector<uint8_t>& encoded_frame,
    std::vector<std::vector<int32_t>>& decoded_frames) {
  return absl::UnimplementedError(
      "LPCMDecoder::DecodeAudioFrame() is not implemented.");
}

}  // namespace iamf_tools
