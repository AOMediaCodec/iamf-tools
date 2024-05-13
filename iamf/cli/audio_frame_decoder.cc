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

#include <cstdint>
#include <filesystem>
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
#include "iamf/cli/cli_util.h"
#include "iamf/cli/codec/aac_encoder_decoder.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/codec/opus_encoder_decoder.h"
#include "iamf/cli/wav_writer.h"
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
      break;
    case kCodecIdOpus:
      decoder = std::make_unique<OpusDecoder>(codec_config, num_channels);
      break;
    case kCodecIdAacLc:
      decoder = std::make_unique<AacDecoder>(codec_config, num_channels);
      break;
    case kCodecIdFlac:
      // TODO(b/280490947): Support FLAC fully by decoding with `libflac`.
      //                    Although since it is lossless it *should* be
      //                    equivalent.
      break;
    default:
      LOG(ERROR) << "Unrecognized `codec_id`= "
                 << codec_config.GetCodecConfig().codec_id;
      return absl::InvalidArgumentError("");
  }

  if (decoder) {
    RETURN_IF_NOT_OK(decoder->Initialize());
  }
  return absl::OkStatus();
}

// Configures a wav writer for a given substream ID.
absl::Status InitializeWavWriterForSubstreamId(
    uint32_t substream_id, absl::string_view output_wav_directory,
    absl::string_view file_prefix, int num_channels, int sample_rate,
    int bit_depth, absl::node_hash_map<uint32_t, WavWriter>& wav_writers) {
  if (wav_writers.contains(substream_id)) {
    return absl::OkStatus();
  }

  const std::filesystem::path file_directory =
      std::filesystem::path(output_wav_directory);
  const std::filesystem::path file_name(
      absl::StrCat(file_prefix, "_decoded_substream_", substream_id, ".wav"));
  // Write directly to special files (e.g. `/dev/null`). Otherwise append the
  // filename.
  const std::filesystem::path wav_path =
      std::filesystem::is_character_file(output_wav_directory)
          ? file_directory
          : file_directory / file_name;

  wav_writers.emplace(
      substream_id, WavWriter(wav_path, num_channels, sample_rate, bit_depth));
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
  decoded_audio_frame.audio_element_with_data =
      encoded_frame.audio_element_with_data;

  // Decode the samples with the specific decoder associated with this
  // substream.
  if (decoder != nullptr) {
    RETURN_IF_NOT_OK(decoder->DecodeAudioFrame(
        encoded_frame.obu.audio_frame_, decoded_audio_frame.decoded_samples));
  } else {
    // Currently `decoder` remains `nullptr` for LPCM and FLAC, which are
    // lossless decoders and the decoding is skipped.
    // TODO(b/280490947): Support FLAC fully by decoding with `libflac`.
    //                    Although since it is lossless it *should* be
    //                    equivalent.
    decoded_audio_frame.decoded_samples = encoded_frame.raw_samples;
  }

  return absl::OkStatus();
}

/*\!brief Writes interlaced PCM samples into a WAV file.
 *
 * \param samples Input frames arranged in (time, channel) axes.
 * \param samples_to_trim_at_start Samples to trim at the beginning.
 * \param samples_to_trim_at_end Samples to trim at the end.
 * \param wav_writer `WavWriter` to write WAV file with.
 * \param substream_id Substream ID of substream being written.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status WriteInterlacedSamplesToWav(
    const std::vector<std::vector<int32_t>>& samples,
    uint32_t samples_to_trim_at_start, uint32_t samples_to_trim_at_end,
    WavWriter& wav_writer, uint32_t substream_id) {
  // Buffer of raw input PCM with channels interlaced and no padding.
  std::vector<uint8_t> buffer;
  RETURN_IF_NOT_OK(WritePcmFrameToBuffer(samples, samples_to_trim_at_start,
                                         samples_to_trim_at_end,
                                         wav_writer.bit_depth(),
                                         /*big_endian=*/false, buffer));

  // Write the raw PCM to a ".wav" file.
  if (!wav_writer.WriteSamples(buffer)) {
    LOG_FIRST_N(WARNING, 1)
        << "Failed to write to wav file for substream ID: " << substream_id
        << ".";
  }

  return absl::OkStatus();
}

// Trims and writes out the `decoded_audio_frame ` to the associated wav writer
// if it can be found. Or does nothing if the wav writer cannot be found.
absl::Status DumpDecodedAudioFrameToWavWriter(
    const DecodedAudioFrame& decoded_audio_frame,
    absl::node_hash_map<uint32_t, WavWriter>& substream_id_to_wav_writers) {
  const auto substream_id = decoded_audio_frame.substream_id;
  auto wav_writer_iter = substream_id_to_wav_writers.find(substream_id);
  if (wav_writer_iter != substream_id_to_wav_writers.end()) {
    // Write this frame to a WAV file.
    RETURN_IF_NOT_OK(WriteInterlacedSamplesToWav(
        decoded_audio_frame.decoded_samples,
        decoded_audio_frame.samples_to_trim_at_start,
        decoded_audio_frame.samples_to_trim_at_end, wav_writer_iter->second,
        substream_id));
  }

  return absl::OkStatus();
}

void AbortAllWavWriters(
    absl::node_hash_map<uint32_t, WavWriter>& substream_id_to_wav_writers) {
  for (auto& [unused_substream_id, wav_writer] : substream_id_to_wav_writers) {
    wav_writer.Abort();
  }
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
    RETURN_IF_NOT_OK(InitializeWavWriterForSubstreamId(
        substream_id, output_wav_directory_, file_prefix_, num_channels,
        static_cast<int>(codec_config.GetOutputSampleRate()),
        static_cast<int>(codec_config.GetBitDepthToMeasureLoudness()),
        substream_id_to_wav_writer_));
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
    auto decode_status = DecodeAudioFrame(
        audio_frame, decoder_iter->second.get(), decoded_audio_frame);
    if (!decode_status.ok()) {
      LOG(ERROR) << "Failed to decode audio streams. decode_status: "
                 << decode_status;
      AbortAllWavWriters(substream_id_to_wav_writer_);
      return decode_status;
    }
    RETURN_IF_NOT_OK(DumpDecodedAudioFrameToWavWriter(
        decoded_audio_frame, substream_id_to_wav_writer_));
    decoded_audio_frames.push_back(decoded_audio_frame);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
