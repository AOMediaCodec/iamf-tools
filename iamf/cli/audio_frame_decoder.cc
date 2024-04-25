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
#include <string>
#include <utility>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
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

absl::Status InitializeWavWriters(
    const std::string& output_wav_directory, const std::string& file_prefix,
    const std::list<DecodedAudioFrame>& decoded_audio_frames,
    absl::node_hash_map<uint32_t, WavWriter>& wav_writers) {
  for (const auto& decoded_audio_frame : decoded_audio_frames) {
    const uint32_t substream_id = decoded_audio_frame.substream_id;
    // Create one `WavWriter` for each substream.
    if (wav_writers.find(substream_id) != wav_writers.end()) {
      continue;
    }

    const auto& audio_element = decoded_audio_frame.audio_element_with_data;
    // Get all the arguments for the constructor. Based on the substream ID and
    // found Codec Config OBU.
    const auto& iter = audio_element->substream_id_to_labels.find(substream_id);
    if (iter == audio_element->substream_id_to_labels.end()) {
      LOG(ERROR) << "Unknown number of channels for substream id: "
                 << substream_id;
      return absl::UnknownError("");
    }
    const int num_channels = static_cast<int>(iter->second.size());

    const std::filesystem::path file_directory =
        std::filesystem::path(output_wav_directory);
    const std::filesystem::path file_name(
        absl::StrCat(file_prefix, "_decoded_substream_",
                     decoded_audio_frame.substream_id, ".wav"));
    // Write directly to special files (e.g. `/dev/null`). Otherwise append the
    // filename.
    const std::filesystem::path wav_path =
        std::filesystem::is_character_file(output_wav_directory)
            ? file_directory
            : file_directory / file_name;

    wav_writers.emplace(
        substream_id,
        iamf_tools::WavWriter(
            wav_path, num_channels,
            static_cast<int>(
                audio_element->codec_config->GetOutputSampleRate()),
            static_cast<int>(
                audio_element->codec_config->GetBitDepthToMeasureLoudness())));
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

// Dumps the interlaced `decoded_frames` field of the input
// `decoded_audio_frames` to a WAV file per substream.
absl::Status DumpDecodedAudioFramesWav(
    const std::string& output_wav_directory, const std::string& file_prefix,
    const std::list<DecodedAudioFrame>& decoded_audio_frames) {
  // Initialize all `WavWriter`s.
  absl::node_hash_map<uint32_t, WavWriter> wav_writers;
  RETURN_IF_NOT_OK(InitializeWavWriters(output_wav_directory, file_prefix,
                                        decoded_audio_frames, wav_writers));

  // Trim and write out all decoded audio frames to the `WavWriter`s.
  for (const auto& decoded_audio_frame : decoded_audio_frames) {
    const uint32_t substream_id = decoded_audio_frame.substream_id;
    auto wav_writer_iter = wav_writers.find(substream_id);
    if (wav_writer_iter == wav_writers.end()) {
      return absl::UnknownError("");
    }

    // Write this frame to a WAV file.
    RETURN_IF_NOT_OK(WriteInterlacedSamplesToWav(
        decoded_audio_frame.decoded_samples,
        decoded_audio_frame.samples_to_trim_at_start,
        decoded_audio_frame.samples_to_trim_at_end, wav_writer_iter->second,
        substream_id));
  }

  return absl::OkStatus();
}

}  // namespace

absl::Status AudioFrameDecoder::Decode(
    const std::list<AudioFrameWithData>& encoded_audio_frames,
    std::list<DecodedAudioFrame>& decoded_audio_frames) {
  // A map of substream IDs to the relevant decoder and codec config. This is
  // necessary to process streams with stateful decoders correctly.
  absl::node_hash_map<uint32_t, std::unique_ptr<DecoderBase>>
      substream_id_to_decoder;

  // Initialize all decoders and find all corresponding Codec Config OBUs.
  for (const auto& audio_frame : encoded_audio_frames) {
    const uint32_t substream_id = audio_frame.obu.GetSubstreamId();
    auto& decoder = substream_id_to_decoder[substream_id];
    if (decoder) {
      // Already found the information for this stream.
      continue;
    }
    if (audio_frame.audio_element_with_data == nullptr ||
        audio_frame.audio_element_with_data->codec_config == nullptr) {
      LOG(ERROR) << "Unexpected nullptr in an audio frame with id="
                 << substream_id;
      return absl::UnknownError("");
    }

    const auto& audio_element = *audio_frame.audio_element_with_data;

    const auto& iter = audio_element.substream_id_to_labels.find(substream_id);
    if (iter == audio_element.substream_id_to_labels.end()) {
      LOG(ERROR) << "Unknown number of channels for substream id: "
                 << substream_id;
      return absl::UnknownError("");
    }
    const int num_channels = static_cast<int>(iter->second.size());

    // Initialize the decoder based on the found Codec Config OBU and number of
    // channels.
    RETURN_IF_NOT_OK(
        InitializeDecoder(*audio_element.codec_config, num_channels, decoder));
  }

  // Decode all frames in all substreams.
  for (const auto& audio_frame : encoded_audio_frames) {
    DecodedAudioFrame decoded_audio_frame;
    auto decode_status = DecodeAudioFrame(
        audio_frame,
        substream_id_to_decoder.at(audio_frame.obu.GetSubstreamId()).get(),
        decoded_audio_frame);
    if (!decode_status.ok()) {
      LOG(ERROR) << "Failed to decode audio streams. decode_status: "
                 << decode_status;
      return decode_status;
    }
    decoded_audio_frames.push_back(decoded_audio_frame);
  }

  RETURN_IF_NOT_OK(DumpDecodedAudioFramesWav(
      output_wav_directory_, file_prefix_, decoded_audio_frames));

  return absl::OkStatus();
}

}  // namespace iamf_tools
