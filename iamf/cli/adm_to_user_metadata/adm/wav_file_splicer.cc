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
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/adm_to_user_metadata/adm/panner.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

constexpr int32_t kBitsPerByte = 8;
constexpr size_t kSizeToFlush = 4096;

// Arbitrary limit on how many samples will be written to the wav file at
// once. Chosen to agree with `kSizeToFlush`, even if there are 16-bit
// samples and one channel.
constexpr size_t kMaxNumSamplesPerFrame = kSizeToFlush / 2;

// Error tolerance set to the minimum precision allowed by ADM file to describe
// timing related parameters.
constexpr double kErrorTolerance = 1e-5;
// Offset for data chunk within the extensible format wav file.
constexpr int32_t kExtensibleOffset = 72;
// Standard size for a wav header.
constexpr int32_t kHeaderSize = 8;
// Total number of channels allowed per mix for the IAMF base enhanced profile.
constexpr int kMaxChannelsPerMixBaseEnhanced = 28;
// Max LFE channels allowed per mix for the IAMF base enhanced profile.
constexpr int kMaxLfeChannelsAllowed =
    kMaxChannelsPerMixBaseEnhanced - kOutputWavChannels;

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
    std::vector<std::unique_ptr<WavWriter>>& audio_object_index_to_wav_writer) {
  for (auto& wav_writer : audio_object_index_to_wav_writer) {
    wav_writer->Abort();
  }
}

absl::Status FlushToWavWriter(std::vector<uint8_t>& samples_to_flush,
                              WavWriter& wav_writer) {
  RETURN_IF_NOT_OK(wav_writer.WritePcmSamples(samples_to_flush));
  samples_to_flush.clear();
  return absl::OkStatus();
}

// Returns a vector of pairs, each with a segment size and wav writer index.
// Non-LFE segments use index 0, and LFE segments are indexed starting from 1.
//
// For e.g., consider an input wav with layout 3.1.2 (where the 4th channel
// corresponds to LFE).
// Channel layout : {L3, R3, Centre, LFE, Ltf3, Rtf3}
// Input LFE-Id list : {4}
//
// The segmentation required:[L3, R3, Centre]; [LFE]; [Ltf3, Rtf3]
// Segment layout obtained: <3,0>, <1,1>, <2,0>
std::vector<std::pair<int, int>> GenerateSegmentLayout(
    const std::vector<int>& lfe_ids, const int num_channels) {
  std::vector<std::pair<int, int>> segment_layout;
  for (int lfe_index = 0; lfe_index <= lfe_ids.size(); ++lfe_index) {
    const int start_index = (lfe_index == 0) ? 0 : lfe_ids[lfe_index - 1];
    const int end_index =
        (lfe_index < lfe_ids.size()) ? lfe_ids[lfe_index] - 1 : num_channels;

    // Store the segment length corresponding to non-LFE channels and update the
    // wav writer index as 0.
    segment_layout.push_back({end_index - start_index, 0});

    // Store the segment length corresponding to LFE channel (always 1) and
    // update the wav writer index incrementally starting from 1.
    if (lfe_index < lfe_ids.size()) {
      segment_layout.push_back({1, lfe_index + 1});
    }
  }
  return segment_layout;
}

// Distributes audio samples from the input buffer to WavWriter objects,
// segmenting them by LFE and non-LFE channels based on the provided layout.
// Samples are transformed and periodically flushed to each WavWriter upon
// reaching kSizeToFlush.
absl::Status FlushLfeNonLfeWavs(
    const std::vector<char>& buffer, const size_t bytes_to_read,
    const int num_channels, const int32_t bytes_per_sample,
    const std::vector<std::pair<int, int>>& segment_layout,
    std::vector<std::unique_ptr<WavWriter>>& writers) {
  // A vector of buffers to store the samples corresponding to non-LFE and LFE
  // channels respectively.
  std::vector<std::vector<uint8_t>> nonlfe_lfe_buffer(writers.size(),
                                                      std::vector<uint8_t>());
  for (size_t sample_index = 0; sample_index < bytes_to_read;
       sample_index += static_cast<size_t>(bytes_per_sample) * num_channels) {
    int channel_offset = 0;
    for (const auto& [segment_size, writer_index] : segment_layout) {
      const size_t offset =
          sample_index + static_cast<size_t>(channel_offset) * bytes_per_sample;
      std::transform(buffer.begin() + offset,
                     buffer.begin() + offset +
                         static_cast<size_t>(segment_size) * bytes_per_sample,
                     std::back_inserter(nonlfe_lfe_buffer[writer_index]),
                     [](char c) { return static_cast<uint8_t>(c); });

      channel_offset += segment_size;
    }

    // Occasionally flush the buffer to the corresponding wav writer.
    // To avoid intermittent padding, ensure that the samples to flush is always
    // even.
    for (int index = 0; index < writers.size(); ++index) {
      auto buffer_size = nonlfe_lfe_buffer[index].size();
      if (buffer_size >= kSizeToFlush && buffer_size % 2 == 0) {
        RETURN_IF_NOT_OK(
            FlushToWavWriter(nonlfe_lfe_buffer[index], *writers[index]));
      }
    }
  }

  // Flush the remaining buffers.
  for (int index = 0; index < writers.size(); ++index) {
    RETURN_IF_NOT_OK(
        FlushToWavWriter(nonlfe_lfe_buffer[index], *writers[index]));
  }

  return absl::OkStatus();
}

// Splices the wav to obtain the wav segment.
absl::Status SpliceWavSegment(std::istream& input_stream,
                              const size_t& sample_length,
                              const size_t& total_channel_size,
                              std::vector<uint8_t>& samples_buffer,
                              WavWriter& wav_writer) {
  for (size_t data_chunk_pos = 0;
       data_chunk_pos < sample_length * total_channel_size;
       data_chunk_pos += total_channel_size) {
    std::vector<char> sample(total_channel_size);
    if (!input_stream.read(sample.data(), sample.size())) {
      wav_writer.Abort();
      return absl::OutOfRangeError(
          "Reached end of stream before the implied end of the `data` "
          "chunk.");
    }
    std::transform(sample.begin(), sample.end(),
                   std::back_inserter(samples_buffer),
                   [](char c) { return static_cast<uint8_t>(c); });
    auto buffer_size = samples_buffer.size();
    if (buffer_size >= kSizeToFlush && buffer_size % 2 == 0) {
      RETURN_IF_NOT_OK(FlushToWavWriter(samples_buffer, wav_writer));
    }
  }
  RETURN_IF_NOT_OK(FlushToWavWriter(samples_buffer, wav_writer));
  return absl::OkStatus();
}

// Calculates the total duration of the wav file.
double CalculateTotalDuration(const size_t& data_chunk_size,
                              const FormatInfoChunk& wav_file_fmt,
                              const size_t& total_channel_size) {
  const auto& total_samples_per_channel = data_chunk_size / total_channel_size;
  double total_duration = static_cast<double>(total_samples_per_channel) /
                          static_cast<double>(wav_file_fmt.samples_per_sec);
  return total_duration;
}

// Computes the duration in seconds.
double ConvertTimeToSeconds(const BlockTime& time) {
  return time.hour * 3600.0 + time.minute * 60.0 + time.second;
}

// Computes the audio block duration as the diff of start time between 2
// consecutive blocks.
double CalculateBlockDuration(const std::vector<AudioBlockFormat>& audio_block,
                              const int& block_index) {
  double seg_duration = 0.0;
  if (block_index < audio_block.size() - 1) {
    const auto block_rtime =
        ConvertTimeToSeconds(audio_block[block_index].rtime);
    const auto next_block_rtime =
        ConvertTimeToSeconds(audio_block[block_index + 1].rtime);
    seg_duration = (next_block_rtime - block_rtime);
  } else {
    seg_duration = ConvertTimeToSeconds(audio_block[block_index].duration);
  }
  return seg_duration;
}

// Retrieves LFE channel IDs from the audio channels list, adds them to
// "lfe_ids" vector, and checks if the count exceeds the allowed limit.
std::vector<int> GetLfeChannelIDs(
    const std::vector<AudioChannelFormat>& audio_channels) {
  std::vector<int> lfe_ids;
  for (int index = 0; index < audio_channels.size(); ++index) {
    if (audio_channels[index].name == "RoomCentricLFE") {
      if (lfe_ids.size() < kMaxLfeChannelsAllowed) {
        lfe_ids.push_back(index + 1);
      } else {
        LOG(WARNING)
            << "The number of LFE channels exceeds the allowed limit. Only the "
               "first "
            << kMaxLfeChannelsAllowed
            << " LFE channels will be processed as unique audio element(s). "
               "The remaining LFE channels would be panned with rest of the "
               "channels to obtain 3OA.";
        break;
      }
    }
  }
  return lfe_ids;
}

// Updates wav splicing parameters such as remaining durations and block indices
// for each audio channel.
void UpdateWavSplicingParams(
    const double& this_seg_duration,
    const std::vector<AudioChannelFormat>& audio_channels,
    std::vector<double>& seg_duration,
    std::vector<size_t>& audio_block_indices) {
  for (size_t ch = 0; ch < audio_channels.size(); ++ch) {
    if (seg_duration[ch] > kErrorTolerance)
      seg_duration[ch] -= this_seg_duration;
    if (seg_duration[ch] <= kErrorTolerance) {
      size_t next_index = audio_block_indices[ch] + 1;
      const auto& this_channel_block = audio_channels[ch].audio_blocks;
      if (next_index < this_channel_block.size()) {
        audio_block_indices[ch] = next_index;
        seg_duration[ch] =
            CalculateBlockDuration(this_channel_block, next_index);
      } else {
        seg_duration[ch] = 0.0;
      }
    }
  }
}

// This function handles the splicing of wav data into segments to respect the
// positional metadata defined by audioBlockFormat and invokes the panner to
// obtain 3OA. The panned wav for each segment is appended to obtain the final
// output wav.
absl::Status ConvertFromObjectsTo3OA(
    const std::filesystem::path& output_file_path,
    absl::string_view file_prefix, const ADM& input_adm,
    const FormatInfoChunk& wav_file_fmt, std::istream& input_stream,
    const iamf_tools::adm_to_user_metadata::Bw64Reader::ChunkInfo&
        data_chunk_info) {
  const std::streamoff audio_data_position =
      data_chunk_info.offset + Bw64Reader::kChunkHeaderOffset;
  input_stream.seekg(audio_data_position);

  // Buffer to temporarily store audio samples before writing to file.
  std::vector<uint8_t> samples_buffer;
  samples_buffer.reserve(kSizeToFlush);

  // Prepare the file paths and initialize necessary file handling.
  const auto temp_file_dir = std::filesystem::temp_directory_path();
  std::filesystem::path input_file =
      temp_file_dir / absl::StrCat(file_prefix, "_adm_segment.wav");
  std::filesystem::path output_file =
      output_file_path / absl::StrCat(file_prefix, "_converted1.wav");

  // Output channels set to 16 as objects get panned to 3OA.
  auto output_wav_writer = WavWriter::Create(
      output_file.string(), kOutputWavChannels, wav_file_fmt.samples_per_sec,
      wav_file_fmt.bits_per_sample, kMaxNumSamplesPerFrame);

  // Calculate number of bytes per sample based on bits per sample.
  const int32_t bytes_per_sample =
      static_cast<int32_t>(wav_file_fmt.bits_per_sample) / kBitsPerByte;
  const int32_t total_channels = wav_file_fmt.num_channels;
  const size_t total_channel_size =
      static_cast<size_t>(bytes_per_sample) * wav_file_fmt.num_channels;
  const size_t data_chunk_size = data_chunk_info.size;
  const auto total_duration =
      CalculateTotalDuration(data_chunk_size, wav_file_fmt, total_channel_size);
  const size_t total_samples =
      total_duration * static_cast<size_t>(wav_file_fmt.samples_per_sec);

  // Initialize vectors required to hold intermediate values.
  std::vector<size_t> audio_block_indices(total_channels, 0);
  std::vector<double> seg_duration(total_channels, 0);

  // Holds the duration of current segment.
  double this_seg_duration = 0.0;
  double total_processed_duration = 0.0;
  // Holds the number of samples left over from the previous segment due to
  // rounding error.
  float leftover_sample_duration = 0.0f;
  int num_samples_count = 0;

  // Initialize segment duration for all channels with the corresponding first
  // audio block duration.
  auto& audio_channels = input_adm.audio_channels;
  for (int ch = 0; ch < total_channels; ++ch) {
    seg_duration[ch] =
        CalculateBlockDuration(audio_channels[ch].audio_blocks, 0);
    if (seg_duration[ch] <= kErrorTolerance) {
      seg_duration[ch] = total_duration;
    }
  }

  // Iterate over the audio blocks within the audio channel which holds
  // time-varying positional metadata. Splice the channels into segments such
  // that segments in each channels have a constant metadata. Invoke the panner
  // for the wav segments to obtain 3OA and later append the output wav file.
  //
  // For e.g., Consider 2 channels (containing 10 samples each) having varying
  // positional metadata associated with it during different time duration as
  // below:
  //
  // CH1 -----|---|--|
  // CH2 ---|---|----|
  //
  // The above wav data will be spliced to 5 wav segments as below:
  //
  //     seg1|seg2|seg3|seg4|seg5
  // CH1  ---| -- | -  | -- | --
  // CH2  ---| -- | -  | -- | --
  while (true) {
    // Find the minimum non-zero segment duration.
    auto min_duration = std::min_element(
        seg_duration.begin(), seg_duration.end(), [](double a, double b) {
          return (a > kErrorTolerance && (b <= kErrorTolerance || a < b));
        });

    if (*min_duration > kErrorTolerance) {
      this_seg_duration = *min_duration;
    } else if (*min_duration < kErrorTolerance) {
      break;
    } else {
      CHECK_GE(*min_duration, -kErrorTolerance)
          << "Minimum duration should not be negative";
    }

    total_processed_duration += this_seg_duration;

    // Read audio samples corresponding to the minimum segment duration
    // and write to an intermediate wav file which will be input to the
    // panner.
    {
      auto wav_writer = WavWriter::Create(
          input_file.string(), wav_file_fmt.num_channels,
          wav_file_fmt.samples_per_sec, wav_file_fmt.bits_per_sample,
          kMaxNumSamplesPerFrame);
      // Compute the length of audio samples corresponding to the current
      // segment duration. The samples excluded due the rounding error at each
      // segment is accounted in the next segment.
      const float this_seg_length =
          (this_seg_duration * wav_file_fmt.samples_per_sec) +
          leftover_sample_duration;
      // Length of the processed audio segment. Samples are rounded off for the
      // current segment.
      const auto processed_seg_length = std::floor(this_seg_length);
      leftover_sample_duration = this_seg_length - processed_seg_length;

      num_samples_count += processed_seg_length;

      CHECK_LE(processed_seg_length, total_samples)
          << "Samples in segment should not be greater than actual samples in "
             "the wav file";

      RETURN_IF_NOT_OK(SpliceWavSegment(input_stream, processed_seg_length,
                                        total_channel_size, samples_buffer,
                                        *wav_writer));
    }

    // Pan the current wav segment to 3OA and append the output wav.
    RETURN_IF_NOT_OK(PanObjectsToAmbisonics(input_file.string(), input_adm,
                                            audio_block_indices,
                                            *output_wav_writer));

    UpdateWavSplicingParams(this_seg_duration, audio_channels, seg_duration,
                            audio_block_indices);
  }

  CHECK_LE(fabs(total_processed_duration - total_duration), kErrorTolerance);
  CHECK_LE(fabs(num_samples_count - total_samples), kErrorTolerance);

  // Delete the temporary files.
  if (!std::filesystem::remove(input_file)) {
    return absl::InternalError("Error while removing temporary file.");
  }
  return absl::OkStatus();
}

// Separates each LFE channel present in the channel bed to individual wav
// file(s).
absl::Status SeparateLfeChannels(const std::filesystem::path& output_file_path,
                                 absl::string_view file_prefix,
                                 const std::string& non_lfe_file_path,
                                 std::istream& input_stream,
                                 const FormatInfoChunk& wav_file_fmt,
                                 const Bw64Reader::ChunkInfo& data_chunk_info,
                                 const std::vector<int>& lfe_ids) {
  const size_t bits_per_sample = wav_file_fmt.bits_per_sample;
  const int32_t bytes_per_sample = bits_per_sample / kBitsPerByte;
  const int num_channels = wav_file_fmt.num_channels;
  const size_t samples_per_sec = wav_file_fmt.samples_per_sec;
  const int lfe_count = lfe_ids.size();
  const int non_lfe_count = num_channels - lfe_count;

  // Create wav writers to separate LFE and non-LFE channels. Index 0 holds
  // the wav writer corresponding to non-LFE channels and subsequent indices
  // correspond to each LFE channel present.
  std::vector<std::unique_ptr<WavWriter>> nonlfe_lfe_wav_writer;
  nonlfe_lfe_wav_writer.emplace_back(
      WavWriter::Create(non_lfe_file_path, non_lfe_count, samples_per_sec,
                        bits_per_sample, kMaxNumSamplesPerFrame));
  for (int lfe_index = 1; lfe_index <= lfe_ids.size(); ++lfe_index) {
    nonlfe_lfe_wav_writer.emplace_back(WavWriter::Create(
        (output_file_path /
         absl::StrCat(file_prefix, "_converted", lfe_index + 1, ".wav"))
            .string(),
        1, samples_per_sec, bits_per_sample, kMaxNumSamplesPerFrame));
  }

  // The samples in the input wav are packed in a channel-interleaved fashion.
  // To facilitate the splicing of LFE channels from non-LFE channels, a
  // segment layout is generated, which is a vector of size equal to the total
  // number of channels, and each element in the vector contains a pair that
  // holds the size of the segment (which equals the number of channels in a
  // segment) and the writer index respectively. The writer index for non-LFE
  // channels is 0 and LFE channels have a writer index starting from 1
  // (increasing in 1 increments). The channels are grouped together in
  // sequence if they are non-LFE.
  std::vector<std::pair<int, int>> segment_layout =
      GenerateSegmentLayout(lfe_ids, num_channels);

  const std::streamoff audio_data_position =
      data_chunk_info.offset + Bw64Reader::kChunkHeaderOffset;
  input_stream.seekg(audio_data_position);

  size_t num_samples_to_read = kSizeToFlush;
  std::vector<char> temp_buffer(num_samples_to_read * bytes_per_sample *
                                num_channels);

  // Perform the file read in chunks and use the temporary buffer for further
  // processing.
  for (size_t data_chunk_pos = 0; data_chunk_pos < data_chunk_info.size;
       data_chunk_pos += temp_buffer.capacity()) {
    const size_t bytes_to_read =
        std::min(temp_buffer.capacity(), data_chunk_info.size - data_chunk_pos);
    if (!input_stream.read(temp_buffer.data(), bytes_to_read)) {
      AbortAllWavWriters(nonlfe_lfe_wav_writer);
      return absl::OutOfRangeError(
          "Reached end of stream before the implied end of the `data` "
          "chunk.");
    }

    RETURN_IF_NOT_OK(FlushLfeNonLfeWavs(temp_buffer, bytes_to_read,
                                        num_channels, bytes_per_sample,
                                        segment_layout, nonlfe_lfe_wav_writer));
    temp_buffer.clear();
  }
  return absl::OkStatus();
}

// Separates each LFE channel present in the channel bed to individual wav
// file(s). The non-LFE channels and audio object(s) are panned to obtain
// 3rd-order ambisonics (3OA).
absl::Status SeparateLfeAndConvertTo3OA(
    const std::filesystem::path& output_file_path,
    absl::string_view file_prefix, const Bw64Reader& reader,
    std::istream& input_stream, const Bw64Reader::ChunkInfo& data_chunk_info,
    int& lfe_count) {
  std::string non_lfe_file_name = absl::StrCat(file_prefix, "_non_lfe.wav");
  const FormatInfoChunk& wav_file_fmt = reader.format_info_;
  const int num_channels = wav_file_fmt.num_channels;

  // Holds the track position corresponding to LFE channels.
  std::vector<int> lfe_ids = GetLfeChannelIDs(reader.adm_.audio_channels);
  lfe_count = lfe_ids.size();

  if (lfe_count == 0) {
    // If no LFE channels are present, pan all the channels to 3OA.
    return ConvertFromObjectsTo3OA(output_file_path, file_prefix, reader.adm_,
                                   reader.format_info_, input_stream,
                                   data_chunk_info);
  }

  CHECK_LT(lfe_count, num_channels);
  const int non_lfe_count = num_channels - lfe_count;
  const auto& non_lfe_file_path =
      (output_file_path / non_lfe_file_name).string();

  // Separate LFE channels to individual wavs
  RETURN_IF_NOT_OK(SeparateLfeChannels(output_file_path, file_prefix,
                                       non_lfe_file_path, input_stream,
                                       wav_file_fmt, data_chunk_info, lfe_ids));

  std::ifstream non_lfe_file(non_lfe_file_path,
                             std::ios::binary | std::ios::in);

  // Remove LFE channel related info from ADM before invoking the panner for
  // non-LFE channels. The vector lfe_ids is sorted, so erasing in reverse does
  // not invalidate the iterators, allows safe removal of lfe channels from the
  // ADM.
  ADM non_lfe_adm = reader.adm_;
  for (int index = lfe_count - 1; index >= 0; --index) {
    non_lfe_adm.audio_channels.erase(non_lfe_adm.audio_channels.begin() +
                                     lfe_ids[index]);
  }

  // Modify FormatInfoChunk with non-LFE channel count before invoking the
  // panner.
  FormatInfoChunk non_lfe_format_info = reader.format_info_;
  non_lfe_format_info.num_channels = non_lfe_count;

  // Calculate data chunk size and set data chunk info for the generated non-LFE
  // file and invoke the panner for the non-LFE file.
  const size_t file_size = std::filesystem::file_size(non_lfe_file_path);
  const size_t data_chunk_size = file_size - kExtensibleOffset - kHeaderSize;
  Bw64Reader::ChunkInfo non_lfe_data_chunk_info = {data_chunk_size,
                                                   kExtensibleOffset};
  RETURN_IF_NOT_OK(ConvertFromObjectsTo3OA(
      output_file_path, file_prefix, non_lfe_adm, non_lfe_format_info,
      non_lfe_file, non_lfe_data_chunk_info));
  non_lfe_file.close();

  // Delete the temporary file.
  if (!std::filesystem::remove(non_lfe_file_path)) {
    return absl::InternalError("Error while removing temporary file.");
  }
  return absl::OkStatus();
}

}  // namespace

// Splices the input wav file depending on the ADM file type.
absl::Status SpliceWavFilesFromAdm(
    const std::filesystem::path& output_file_path,
    absl::string_view file_prefix, ProfileVersion profile_version,
    const Bw64Reader& reader, std::istream& input_stream, int& lfe_count) {
  const auto& data_chunk_info = reader.GetChunkInfo("data");
  const auto& fmt_chunk_info = reader.GetChunkInfo("fmt ");
  if (!data_chunk_info.ok() || !fmt_chunk_info.ok()) {
    return absl::NotFoundError("Missing data or fmt chunk.");
  }
  auto adm_file_type = reader.adm_.file_type;

  // Separates the input wav file to 'n' number of wav file(s), where 'n' is the
  // number of audioObject(s) present in the XML metadata.
  if (adm_file_type == kAdmFileTypeDefault) {
    const auto audio_tracks_for_audio_objects =
        GetAudioTracksForAudioObjects(reader.adm_.audio_objects);

    if (audio_tracks_for_audio_objects.empty()) {
      return absl::NotFoundError("No audioObject present.");
    };

    // Construct the wav writers to use a file name of the form 'converted'
    // followed by the 1-indexed content.
    std::vector<std::unique_ptr<WavWriter>> audio_object_index_to_wav_writer;
    audio_object_index_to_wav_writer.reserve(
        audio_tracks_for_audio_objects.size());
    const FormatInfoChunk& wav_file_fmt = reader.format_info_;
    for (int audio_object_index = 0;
         audio_object_index < audio_tracks_for_audio_objects.size();
         ++audio_object_index) {
      audio_object_index_to_wav_writer.emplace_back(WavWriter::Create(
          (output_file_path / absl::StrCat(file_prefix, "_converted",
                                           audio_object_index + 1, ".wav"))
              .string(),
          audio_tracks_for_audio_objects[audio_object_index].size(),
          wav_file_fmt.samples_per_sec, wav_file_fmt.bits_per_sample,
          kMaxNumSamplesPerFrame));
    }

    // Write audio samples into the corresponding output wav file(s).
    const std::streamoff audio_data_position =
        data_chunk_info->offset + Bw64Reader::kChunkHeaderOffset;
    input_stream.seekg(audio_data_position);

    // Buffer to store samples per audio object. They will be flushed
    // occasionally when the buffer is full. The buffer will expand, so it is OK
    // if it goes over the target size to flush.
    std::vector<std::vector<uint8_t>> interlaced_samples_for_audio_objects(
        audio_tracks_for_audio_objects.size(), std::vector<uint8_t>());

    // Read audio samples from the buffer and organize them into individual
    // audio tracks, based on the mapping specified in
    // 'audio_tracks_for_audio_objects'. Write the audio track data to
    // corresponding `WavWriter`s.
    const int32_t bytes_per_sample =
        static_cast<int32_t>(wav_file_fmt.bits_per_sample) / kBitsPerByte;
    const int32_t channels = wav_file_fmt.num_channels;
    for (size_t data_chunk_pos = 0; data_chunk_pos < data_chunk_info->size;
         data_chunk_pos += static_cast<size_t>(bytes_per_sample) * channels) {
      for (int audio_object_index = 0;
           audio_object_index < audio_tracks_for_audio_objects.size();
           ++audio_object_index) {
        // Read in the samples for the current audio object.
        std::vector<char> sample(
            static_cast<size_t>(bytes_per_sample) *
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
          RETURN_IF_NOT_OK(FlushToWavWriter(
              samples_for_audio_object,
              *audio_object_index_to_wav_writer[audio_object_index]));
        }
      }
    }

    // Flush the remaining buffers.
    for (int audio_object_index = 0;
         audio_object_index < audio_tracks_for_audio_objects.size();
         ++audio_object_index) {
      RETURN_IF_NOT_OK(FlushToWavWriter(
          interlaced_samples_for_audio_objects[audio_object_index],
          *audio_object_index_to_wav_writer[audio_object_index]));
    }
  } else {
    CHECK_EQ(adm_file_type, kAdmFileTypeDolby);
    using enum iamf_tools::ProfileVersion;
    if (profile_version == kIamfBaseProfile) {
      // For base profile version, convert the channel beds and audio objects
      // present to 3OA (16 channels) to facilitate IAMF encoding.
      RETURN_IF_NOT_OK(ConvertFromObjectsTo3OA(
          output_file_path, file_prefix, reader.adm_, reader.format_info_,
          input_stream, data_chunk_info.value()));
    } else {
      CHECK_EQ(static_cast<int>(profile_version),
               static_cast<int>(kIamfBaseEnhancedProfile));
      // For base enhanced profile version, convert the LFE channel(s) (if
      // present) to separate wav file(s) and the remaining channels to 3OA (16
      // channels) to facilitate IAMF encoding.
      RETURN_IF_NOT_OK(SeparateLfeAndConvertTo3OA(
          output_file_path, file_prefix, reader, input_stream,
          data_chunk_info.value(), lfe_count));
    }
  }
  return absl::OkStatus();
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
