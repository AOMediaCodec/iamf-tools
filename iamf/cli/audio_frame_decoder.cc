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

#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/aac_decoder.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/flac_decoder.h"
#include "iamf/cli/codec/lpcm_decoder.h"
#include "iamf/cli/codec/opus_decoder.h"
#include "iamf/common/macros.h"
#include "iamf/obu/codec_config.h"

namespace iamf_tools {

namespace {

absl::Status InitializeDecoder(const CodecConfigObu& codec_config,
                               int num_channels,
                               std::unique_ptr<DecoderBase>& decoder) {
  switch (codec_config.GetCodecConfig().codec_id) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
      decoder = std::make_unique<LpcmDecoder>(codec_config, num_channels);
      break;
    case kCodecIdOpus:
      decoder = std::make_unique<OpusDecoder>(codec_config, num_channels);
      break;
    case kCodecIdAacLc:
      decoder = std::make_unique<AacDecoder>(codec_config, num_channels);
      break;
    case kCodecIdFlac:
      decoder = std::make_unique<FlacDecoder>(codec_config, num_channels);
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unrecognized codec_id= ", codec_config.GetCodecConfig().codec_id));
  }

  if (decoder) {
    RETURN_IF_NOT_OK(decoder->Initialize());
  }
  return absl::OkStatus();
}

absl::Status DecodeAudioFrame(const AudioFrameWithData& encoded_frame,
                              DecoderBase* decoder,
                              DecodedAudioFrame& decoded_audio_frame) {
  // Copy over some fields from the encoded frame.
  decoded_audio_frame.substream_id = encoded_frame.obu.GetSubstreamId();
  decoded_audio_frame.start_timestamp = encoded_frame.start_timestamp;
  decoded_audio_frame.end_timestamp = encoded_frame.end_timestamp;
  decoded_audio_frame.samples_to_trim_at_end =
      encoded_frame.obu.header_.num_samples_to_trim_at_end;
  decoded_audio_frame.samples_to_trim_at_start =
      encoded_frame.obu.header_.num_samples_to_trim_at_start;
  decoded_audio_frame.down_mixing_params = encoded_frame.down_mixing_params;
  decoded_audio_frame.audio_element_with_data =
      encoded_frame.audio_element_with_data;

  // Decode the samples with the specific decoder associated with this
  // substream.
  RETURN_IF_NOT_OK(decoder->DecodeAudioFrame(
      encoded_frame.obu.audio_frame_, decoded_audio_frame.decoded_samples));

  return absl::OkStatus();
}

}  // namespace

// Initializes all decoders and wav writers based on the corresponding Audio
// Element and Codec Config OBUs.
absl::Status AudioFrameDecoder::InitDecodersForSubstreams(
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const CodecConfigObu& codec_config) {
  for (const auto& [substream_id, labels] : substream_id_to_labels) {
    auto& decoder = substream_id_to_decoder_[substream_id];
    if (decoder != nullptr) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Already initialized decoder for substream ID: ", substream_id,
          ". Maybe multiple Audio Element OBUs have the same substream ID?"));
    }

    const int num_channels = static_cast<int>(labels.size());

    // Initialize the decoder based on the found Codec Config OBU and number
    // of channels.
    RETURN_IF_NOT_OK(InitializeDecoder(codec_config, num_channels, decoder));
  }

  return absl::OkStatus();
}

absl::Status AudioFrameDecoder::Decode(
    const std::list<AudioFrameWithData>& encoded_audio_frames,
    std::list<DecodedAudioFrame>& decoded_audio_frames) {
  // Decode all frames in all substreams.
  for (const auto& audio_frame : encoded_audio_frames) {
    auto decoder_iter =
        substream_id_to_decoder_.find(audio_frame.obu.GetSubstreamId());
    if (decoder_iter == substream_id_to_decoder_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("No decoder found for substream ID: ",
                       audio_frame.obu.GetSubstreamId()));
    }

    DecodedAudioFrame decoded_audio_frame;
    RETURN_IF_NOT_OK(DecodeAudioFrame(audio_frame, decoder_iter->second.get(),
                                      decoded_audio_frame));
    decoded_audio_frames.push_back(std::move(decoded_audio_frame));
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
