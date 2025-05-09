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
#include "iamf/cli/audio_frame_decoder.h"

#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/lpcm_decoder.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/codec_config.h"

// These defines are not part of an official API and are likely to change or be
// removed.  Please do not depend on them.
// TODO(b/401063570): Remove these statements when no longer disabling FLAC/AAC.
#ifndef IAMF_TOOLS_DISABLE_AAC_DECODER
#include "iamf/cli/codec/aac_decoder.h"
#endif

#ifndef IAMF_TOOLS_DISABLE_FLAC_DECODER
#include "iamf/cli/codec/flac_decoder.h"
#endif

#ifndef IAMF_TOOLS_DISABLE_OPUS_DECODER
#include "iamf/cli/codec/opus_decoder.h"
#endif

namespace iamf_tools {

namespace {

absl::StatusOr<std::unique_ptr<DecoderBase>> CreateDecoder(
    const CodecConfigObu& codec_config, int num_channels) {
  switch (codec_config.GetCodecConfig().codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
      return LpcmDecoder::Create(codec_config, num_channels);
#ifndef IAMF_TOOLS_DISABLE_OPUS_DECODER
    case kCodecIdOpus:
      return OpusDecoder::Create(codec_config, num_channels);
#endif
#ifndef IAMF_TOOLS_DISABLE_AAC_DECODER
    case kCodecIdAacLc:
      return AacDecoder::Create(codec_config, num_channels);
#endif
#ifndef IAMF_TOOLS_DISABLE_FLAC_DECODER
    case kCodecIdFlac:
      return FlacDecoder::Create(num_channels,
                                 codec_config.GetNumSamplesPerFrame());
#endif
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unrecognized or disabled codec_id= ",
                       codec_config.GetCodecConfig().codec_id));
  }
}

}  // namespace

// Initializes all decoders based on the corresponding Audio Element and Codec
// Config OBUs.
absl::Status AudioFrameDecoder::InitDecodersForSubstreams(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const CodecConfigObu& codec_config) {
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    auto& decoder_for_substream = substream_id_to_decoder_[substream_id];
    if (decoder_for_substream != nullptr) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Already initialized decoder for substream ID: ", substream_id,
          ". Maybe multiple Audio Element OBUs have the same substream ID?"));
    }

    const int num_channels = static_cast<int>(labels.size());

    // Create the decoder, unwrap it and move it into the map, if it is valid.
    absl::StatusOr<std::unique_ptr<DecoderBase>> new_decoder =
        CreateDecoder(codec_config, num_channels);
    if (!new_decoder.ok()) {
      return new_decoder.status();
    }
    // The factories are not supposed to return an OK and `nullptr` decoder.
    // For defensive programming, check for that case.
    CHECK_NE(*new_decoder, nullptr);
    decoder_for_substream = std::move(*new_decoder);
  }

  return absl::OkStatus();
}

absl::StatusOr<DecodedAudioFrame> AudioFrameDecoder::Decode(
    const AudioFrameWithData& audio_frame) {
  auto decoder_iter =
      substream_id_to_decoder_.find(audio_frame.obu.GetSubstreamId());
  if (decoder_iter == substream_id_to_decoder_.end() ||
      decoder_iter->second == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("No decoder found for substream ID: ",
                     audio_frame.obu.GetSubstreamId()));
  }
  // Decode the samples with the specific decoder associated with this
  // substream.
  auto& decoder = *decoder_iter->second;
  RETURN_IF_NOT_OK(decoder.DecodeAudioFrame(audio_frame.obu.audio_frame_));

  // Return a frame. Most fields are copied from the encoded frame.
  return DecodedAudioFrame{
      .substream_id = audio_frame.obu.GetSubstreamId(),
      .start_timestamp = audio_frame.start_timestamp,
      .end_timestamp = audio_frame.end_timestamp,
      .samples_to_trim_at_end =
          audio_frame.obu.header_.num_samples_to_trim_at_end,
      .samples_to_trim_at_start =
          audio_frame.obu.header_.num_samples_to_trim_at_start,
      .decoded_samples = decoder.ValidDecodedSamples(),
      .down_mixing_params = audio_frame.down_mixing_params,
      .recon_gain_info_parameter_data =
          audio_frame.recon_gain_info_parameter_data,
      .audio_element_with_data = audio_frame.audio_element_with_data,
  };
}

}  // namespace iamf_tools
