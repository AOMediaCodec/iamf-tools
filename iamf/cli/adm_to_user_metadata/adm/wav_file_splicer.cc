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

#include "iamf/cli/adm_to_user_metadata/adm/wav_file_splicer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/wav_writer.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

constexpr int32_t kBitsPerByte = 8;

// Creates a map for the audioObject(s) and the audioTrack(s) present within.
std::vector<std::vector<int32_t>> GetAudioTracksForAudioObjects(
    const std::vector<struct AudioObject>& audio_objects) {
  std::vector<std::vector<int32_t>> audio_tracks_for_audio_objects(
      audio_objects.size(), std::vector<int32_t>());
  int32_t audio_object_index = -1;
  int32_t audio_track_index = -1;
  for (const auto& audio_object : audio_objects) {
    auto& audio_tracks_for_audio_object =
        audio_tracks_for_audio_objects[++audio_object_index];
    for (auto unused_audio_track : audio_object.audio_track_uid_ref) {
      audio_tracks_for_audio_object.push_back(++audio_track_index);
    }
  }
  return audio_tracks_for_audio_objects;
}

void AbortAllWavWriters(
    std::vector<WavWriter>& audio_object_index_to_wav_writer) {
  for (auto& wav_writer : audio_object_index_to_wav_writer) {
    wav_writer.Abort();
  }
}

void FlushToWavWriter(std::vector<uint8_t>& samples_to_flush,
                      WavWriter& wav_writer) {
  wav_writer.WriteSamples(samples_to_flush.data(), samples_to_flush.size());
  samples_to_flush.clear();
}

}  // namespace

// Separates the input wav file to 'n' number of wav file(s), where 'n' is the
// number of audioObject(s) present in the XML metadata.
absl::Status SpliceWavFilesFromAdm(
    const std::filesystem::path& output_file_path,
    absl::string_view file_prefix, const Bw64Reader& reader,
    std::istream& input_stream) {
  const auto& data_chunk_info = reader.GetChunkInfo("data");
  const auto& fmt_chunk_info = reader.GetChunkInfo("fmt ");
  if (!data_chunk_info.ok() || !fmt_chunk_info.ok()) {
    return absl::NotFoundError("Missing data or fmt chunk.");
  }

  const auto audio_tracks_for_audio_objects =
      GetAudioTracksForAudioObjects(reader.adm_.audio_objects);

  if (audio_tracks_for_audio_objects.empty()) {
    return absl::NotFoundError("No audioObject present.");
  };

  // Construct the wav writers to use a file name of the form 'converted'
  // followed by the 1-indexed content.
  std::vector<WavWriter> audio_object_index_to_wav_writer;
  audio_object_index_to_wav_writer.reserve(
      audio_tracks_for_audio_objects.size());
  const FormatInfoChunk& wav_file_fmt = reader.format_info_;
  for (int audio_object_index = 0;
       audio_object_index < audio_tracks_for_audio_objects.size();
       ++audio_object_index) {
    audio_object_index_to_wav_writer.emplace_back(
        (output_file_path / absl::StrCat(file_prefix, "_converted",
                                         audio_object_index + 1, ".wav"))
            .string(),
        audio_tracks_for_audio_objects[audio_object_index].size(),
        wav_file_fmt.samples_per_sec, wav_file_fmt.bits_per_sample);
  }

  // Write audio samples into the corresponding output wav file(s).
  const int32_t audio_data_position =
      data_chunk_info->offset + Bw64Reader::kChunkHeaderOffset;
  input_stream.seekg(audio_data_position);

  // Buffer to store samples per audio object. They will be flushed occasionally
  // when the buffer is full. The buffer will expand, so it is OK if it goes
  // over the target size to flush.
  static const size_t kSizeToFlush = 4096;
  std::vector<std::vector<uint8_t>> interlaced_samples_for_audio_objects(
      audio_tracks_for_audio_objects.size(), std::vector<uint8_t>());

  // Read audio samples from the buffer and organize them into individual audio
  // tracks, based on the mapping specified in
  // 'audio_tracks_for_audio_objects'. Write the audio track data to
  // corresponding `WavWriter`s.
  const int32_t bytes_per_channel_per_samples =
      wav_file_fmt.bits_per_sample / kBitsPerByte;
  const int32_t channels = wav_file_fmt.num_channels;
  for (int data_chunk_pos = 0; data_chunk_pos < data_chunk_info->size;
       data_chunk_pos += bytes_per_channel_per_samples * channels) {
    for (int audio_object_index = 0;
         audio_object_index < audio_tracks_for_audio_objects.size();
         ++audio_object_index) {
      // Read in the samples for the current audio object.
      std::vector<char> sample(
          bytes_per_channel_per_samples *
          audio_tracks_for_audio_objects[audio_object_index].size());

      if (!input_stream.read(sample.data(), sample.size())) {
        AbortAllWavWriters(audio_object_index_to_wav_writer);
        return absl::OutOfRangeError(
            "Reached end of stream before the implied end of the `data` "
            "chunk.");
      }

      // Store the samples in the buffer.
      auto& samples_for_audio_object =
          interlaced_samples_for_audio_objects[audio_object_index];
      std::transform(sample.begin(), sample.end(),
                     std::back_inserter(samples_for_audio_object),
                     [](char c) { return static_cast<uint8_t>(c); });

      // Occasionally flush the buffer to the corresponding wav writer.
      if (samples_for_audio_object.size() >= kSizeToFlush) {
        FlushToWavWriter(samples_for_audio_object,
                         audio_object_index_to_wav_writer[audio_object_index]);
      }
    }
  }

  // Flush the remaining buffers.
  for (int audio_object_index = 0;
       audio_object_index < audio_tracks_for_audio_objects.size();
       ++audio_object_index) {
    FlushToWavWriter(interlaced_samples_for_audio_objects[audio_object_index],
                     audio_object_index_to_wav_writer[audio_object_index]);
  }
  return absl::OkStatus();
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
