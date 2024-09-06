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

#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/adm_to_user_metadata/adm/xml_to_adm.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

const int32_t kExtraOffset = 8;
const int32_t kRiffHeaderLength = 12;

// Reads and validates the RIFF chunk data of WAV file.
absl::Status ReadRiffChunk(std::istream& buffer) {
  buffer.clear();
  buffer.seekg(0);

  // Validate the "RIFF" header and skip past the chunk size.
  std::vector<char> file_format(Bw64Reader::kChunkNameSize);
  buffer.read(file_format.data(), Bw64Reader::kChunkNameSize);
  if (std::memcmp(file_format.data(), "RIFF", Bw64Reader::kChunkNameSize) !=
      0) {
    return absl::InvalidArgumentError("Could not find `RIFF` header.");
  }
  buffer.ignore(Bw64Reader::kChunkLengthSize);

  // Validate the type is "WAVE".
  std::vector<char> riff_type(Bw64Reader::kChunkNameSize);
  buffer.read(riff_type.data(), Bw64Reader::kChunkNameSize);
  if (std::memcmp(riff_type.data(), "WAVE", Bw64Reader::kChunkNameSize) != 0) {
    return absl::InvalidArgumentError("Could not find `WAVE` header.");
  }
  return absl::OkStatus();
}

// Reads the chunk data of the WAV file. It returns a vector containing the name
// of WAV header such as "fmt " and the corresponding and data size.
std::pair<std::vector<char>, int32_t> ReadChunkHeader(std::istream& buffer) {
  std::pair<std::vector<char>, int32_t> chunk_header;
  chunk_header.first.resize(Bw64Reader::kChunkNameSize);
  if (buffer.read(chunk_header.first.data(), Bw64Reader::kChunkNameSize) &&
      buffer.read(reinterpret_cast<char*>(&chunk_header.second),
                  Bw64Reader::kChunkLengthSize)) {
    return chunk_header;
  }
  chunk_header.second = -1;
  return chunk_header;
}

// Constructs an index of chunk name to information about the chunk.
Bw64Reader::ChunksOffsetMap CreateChunksOffsetMap(std::istream& buffer) {
  buffer.clear();
  buffer.seekg(kRiffHeaderLength);

  Bw64Reader::ChunksOffsetMap chunks_offset_map;
  while (true) {
    const auto [chunk_id, chunk_size] = ReadChunkHeader(buffer);
    if (chunk_size == -1) {
      break;
    }
    int32_t current_file_pointer = buffer.tellg();
    chunks_offset_map[std::string(chunk_id.begin(), chunk_id.end())] = {
        chunk_size, current_file_pointer - kExtraOffset};
    buffer.seekg(chunk_size + (chunk_size & 1), std::ios::cur);
  }
  return chunks_offset_map;
}

// Returns the chunk information if present.
absl::StatusOr<Bw64Reader::ChunkInfo> GetChunkInfo(
    absl::string_view chunk_name,
    const Bw64Reader::ChunksOffsetMap& chunks_offset_map) {
  const auto iter = chunks_offset_map.find(chunk_name);
  if (iter == chunks_offset_map.end()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Could not find `", chunk_name, "` header."));
  }
  return iter->second;
}

// Parses the "fmt "  data to the output `FormatInfoChunk`.
absl::StatusOr<FormatInfoChunk> ReadFmtChunk(
    const Bw64Reader::ChunksOffsetMap& chunks_offset_map,
    std::istream& buffer) {
  const auto& fmt_chunk_info = GetChunkInfo("fmt ", chunks_offset_map);
  if (!fmt_chunk_info.ok()) {
    return fmt_chunk_info.status();
  }

  buffer.clear();
  buffer.seekg(fmt_chunk_info->offset + Bw64Reader::kChunkHeaderOffset);

  FormatInfoChunk format_info;
  buffer.read(reinterpret_cast<char*>(&format_info), sizeof(format_info));
  return format_info;
}

// Extracts the "axml" data to the output string.
absl::StatusOr<std::string> ReadAxml(
    const Bw64Reader::ChunksOffsetMap& chunks_offset_map,
    std::istream& buffer) {
  const auto& axml_chunk_info = GetChunkInfo("axml", chunks_offset_map);
  if (!axml_chunk_info.ok()) {
    return axml_chunk_info.status();
  }

  const int32_t axml_size = axml_chunk_info->size;
  const int32_t axml_data_position =
      axml_chunk_info->offset + Bw64Reader::kChunkHeaderOffset;
  std::vector<char> axml_data(axml_size);
  buffer.clear();

  buffer.seekg(axml_data_position);
  buffer.read(axml_data.data(), axml_size);

  return std::string(axml_data.data(), axml_size);
}

}  // namespace

absl::StatusOr<Bw64Reader::ChunkInfo> Bw64Reader::GetChunkInfo(
    absl::string_view chunk_name) const {
  return ::iamf_tools::adm_to_user_metadata::GetChunkInfo(chunk_name,
                                                          chunks_offset_map_);
}

absl::StatusOr<int64_t> Bw64Reader::GetTotalSamplesPerChannel() const {
  const int64_t kBitsPerByte = 8;

  const int64_t bits_per_sample_per_channel =
      format_info_.bits_per_sample * format_info_.num_channels;
  if (bits_per_sample_per_channel == 0 ||
      (bits_per_sample_per_channel % kBitsPerByte != 0)) {
    return absl::InvalidArgumentError(
        "Cannot compute number of samples per frame.");
  }

  const auto& chunk_info = GetChunkInfo("data");
  if (!chunk_info.ok()) {
    return absl::FailedPreconditionError("Missing `data` chunk.");
  }

  const int64_t data_chunk_size = static_cast<int64_t>(chunk_info->size);
  return data_chunk_size / (bits_per_sample_per_channel / kBitsPerByte);
}

absl::StatusOr<Bw64Reader> Bw64Reader::BuildFromStream(
    int32_t importance_threshold, std::istream& buffer) {
  const auto read_riff_chunk_status = ReadRiffChunk(buffer);
  if (!read_riff_chunk_status.ok()) {
    return read_riff_chunk_status;
  }

  // Index the chunks.
  const auto chunks_offset_map = CreateChunksOffsetMap(buffer);

  const auto format_info = ReadFmtChunk(chunks_offset_map, buffer);
  if (!format_info.ok()) {
    return format_info.status();
  }

  const auto axml_data = ReadAxml(chunks_offset_map, buffer);
  if (!axml_data.ok()) {
    return axml_data.status();
  }

  const auto adm = ParseXmlToAdm(*axml_data, importance_threshold);
  if (!adm.ok()) {
    return adm.status();
  }

  return Bw64Reader(*adm, *format_info, chunks_offset_map);
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
