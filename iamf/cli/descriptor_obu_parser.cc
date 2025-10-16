/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/descriptor_obu_parser.h"

#include <cstdint>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// The size of a Codec Config OBU payload (after header) if all fields are
// minimal size, and `DecoderConfig` is empty. Real Codec Config OBUs would have
// a non-empty `DecoderConfig` and always be a few bytes larger.
constexpr size_t kSmallestAcceptedCodecConfigSize = 8;

// Gets a CodecConfigObu from `read_bit_buffer` and stores it into
// `codec_config_obu_map`, using the `codec_config_id` as the unique key.
absl::Status GetAndStoreCodecConfigObu(
    const ObuHeader& header, int64_t payload_size,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obu_map,
    ReadBitBuffer& read_bit_buffer) {
  if (payload_size < kSmallestAcceptedCodecConfigSize) {
    // The OBU is implausibly small. It is likely the source file is corrupted.
    // For maximum compatibility, silently skip over the OBU.
    ABSL_LOG(WARNING)
        << "Possible bitstream corruption. Skipping over an "
           "implausibly small Codec Config OBU with a payload size of: "
        << payload_size << " bytes.";
    return read_bit_buffer.IgnoreBytes(payload_size);
  }

  absl::StatusOr<CodecConfigObu> codec_config_obu =
      CodecConfigObu::CreateFromBuffer(header, payload_size, read_bit_buffer);
  if (!codec_config_obu.ok()) {
    return codec_config_obu.status();
  }
  codec_config_obu->PrintObu();
  codec_config_obu_map.insert(
      {codec_config_obu->GetCodecConfigId(), *std::move(codec_config_obu)});
  return absl::OkStatus();
}

absl::Status GetAndStoreAudioElementObu(
    const ObuHeader& header, int64_t payload_size,
    absl::flat_hash_map<DecodedUleb128, AudioElementObu>& audio_element_obu_map,
    ReadBitBuffer& read_bit_buffer) {
  absl::StatusOr<AudioElementObu> audio_element_obu =
      AudioElementObu::CreateFromBuffer(header, payload_size, read_bit_buffer);
  if (!audio_element_obu.ok()) {
    return audio_element_obu.status();
  }
  audio_element_obu->PrintObu();
  audio_element_obu_map.insert(
      {audio_element_obu->GetAudioElementId(), *std::move(audio_element_obu)});
  return absl::OkStatus();
}

absl::Status GetAndStoreMixPresentationObu(
    const ObuHeader& header, int64_t payload_size,
    std::list<MixPresentationObu>& mix_presentation_obus,
    ReadBitBuffer& read_bit_buffer) {
  absl::StatusOr<MixPresentationObu> mix_presentation_obu =
      MixPresentationObu::CreateFromBuffer(header, payload_size,
                                           read_bit_buffer);
  if (!mix_presentation_obu.ok()) {
    return mix_presentation_obu.status();
  }
  ABSL_LOG(INFO) << "Mix Presentation OBU successfully parsed.";
  mix_presentation_obu->PrintObu();
  mix_presentation_obus.push_back(*std::move(mix_presentation_obu));
  return absl::OkStatus();
}

// Resets the buffer to `start_position` and sets the `insufficient_data`
// flag to `true`.
absl::Status InsufficientDataReset(ReadBitBuffer& read_bit_buffer,
                                   const int64_t start_position,
                                   bool& insufficient_data) {
  ABSL_LOG(INFO) << "Insufficient data to process all descriptor OBUs.";
  insufficient_data = true;
  RETURN_IF_NOT_OK(read_bit_buffer.Seek(start_position));
  ABSL_LOG(INFO) << "Reset the buffer to the beginning.";
  return absl::ResourceExhaustedError(
      "Insufficient data to process all descriptor OBUs. Please provide "
      "more data and try again.");
}

}  // namespace

absl::StatusOr<DescriptorObuParser::ParsedDescriptorObus>
DescriptorObuParser::ProcessDescriptorObus(bool is_exhaustive_and_exact,
                                           ReadBitBuffer& read_bit_buffer,
                                           bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;

  ParsedDescriptorObus parsed_obus;
  auto audio_element_obu_map =
      absl::flat_hash_map<DecodedUleb128, AudioElementObu>();
  const int64_t global_position_before_all_obus = read_bit_buffer.Tell();
  bool processed_ia_header = false;
  bool continue_processing = true;
  while (continue_processing) {
    auto header_metadata =
        ObuHeader::PeekObuTypeAndTotalObuSize(read_bit_buffer);
    if (!header_metadata.ok()) {
      if (header_metadata.status().code() ==
          absl::StatusCode::kResourceExhausted) {
        // Can't read header because there is not enough data.
        return InsufficientDataReset(read_bit_buffer,
                                     global_position_before_all_obus,
                                     output_insufficient_data);
      } else {
        // Some other error occurred, propagate it.
        return header_metadata.status();
      }
    }

    // Now, we know we were at least able to read obu_type and the total size of
    // the obu.
    if (ObuHeader::IsTemporalUnitObuType(header_metadata->obu_type)) {
      if (is_exhaustive_and_exact) {
        auto error_status = absl::InvalidArgumentError(
            "Descriptor OBUs must not contain a temporal unit OBU when "
            "is_exhaustive_and_exact is true.");
        ABSL_LOG(ERROR) << error_status;
        RETURN_IF_NOT_OK(read_bit_buffer.Seek(global_position_before_all_obus));
        return error_status;
      }
      // Since it's a temporal unit, we know we are done reading descriptor
      // OBUs. Since we've only peeked on this iteration of the loop, no need to
      // rewind the buffer.
      // Check that we've processed an IA header to ensure it's a valid IA
      // Sequence.
      if (!processed_ia_header) {
        return absl::InvalidArgumentError(
            "An IA Sequence and/or descriptor OBUs must always start with an "
            "IA Header.");
      }
      // Break out of the while loop since we've reached the end of the
      // descriptor OBUs; should not seek back to the beginning of the buffer
      // since this is a successful termination.
      break;
    }

    // Now, we know that this is not a temporal unit OBU.
    if (!read_bit_buffer.CanReadBytes(header_metadata->total_obu_size)) {
      // This is a descriptor OBU for which we don't have enough data.
      return InsufficientDataReset(read_bit_buffer,
                                   global_position_before_all_obus,
                                   output_insufficient_data);
    }
    // Now we know we can read the entire obu.
    const int64_t position_before_header = read_bit_buffer.Tell();
    ObuHeader header;
    // Note that `payload_size` is different from the total obu size calculated
    // by `PeekObuTypeAndTotalObuSize`.
    int64_t payload_size;
    RETURN_IF_NOT_OK(header.ReadAndValidate(read_bit_buffer, payload_size));
    switch (header.obu_type) {
      case kObuIaSequenceHeader: {
        if (processed_ia_header && !header.obu_redundant_copy) {
          ABSL_LOG(WARNING)
              << "Detected an IA Sequence without temporal units.";
          continue_processing = false;
          break;
        }
        auto ia_sequence_header_obu = IASequenceHeaderObu::CreateFromBuffer(
            header, payload_size, read_bit_buffer);
        if (!ia_sequence_header_obu.ok()) {
          return ia_sequence_header_obu.status();
        }
        parsed_obus.sequence_header = *std::move(ia_sequence_header_obu);
        parsed_obus.sequence_header.PrintObu();
        processed_ia_header = true;
        break;
      }
      case kObuIaCodecConfig: {
        RETURN_IF_NOT_OK(GetAndStoreCodecConfigObu(
            header, payload_size, parsed_obus.codec_config_obus,
            read_bit_buffer));
        break;
      }
      case kObuIaAudioElement: {
        RETURN_IF_NOT_OK(GetAndStoreAudioElementObu(
            header, payload_size, audio_element_obu_map, read_bit_buffer));
        break;
      }
      case kObuIaMixPresentation: {
        RETURN_IF_NOT_OK(GetAndStoreMixPresentationObu(
            header, payload_size, parsed_obus.mix_presentation_obus,
            read_bit_buffer));
        break;
      }
      case kObuIaReserved24:
      case kObuIaReserved25:
      case kObuIaReserved26:
      case kObuIaReserved27:
      case kObuIaReserved28:
      case kObuIaReserved29:
      case kObuIaReserved30: {
        // Reserved OBUs may occur in the sequence of Descriptor OBUs. For
        // now, ignore any reserved OBUs by skipping over their bits in the
        // buffer.
        continue_processing = true;
        ABSL_LOG(INFO)
            << "Detected a reserved OBU while parsing Descriptor OBUs. "
            << "Safely ignoring it.";
        RETURN_IF_NOT_OK(read_bit_buffer.IgnoreBytes(payload_size));
        break;
      }
      default:
        /// TODO(b/387550488): Handle reserved OBUs.
        continue_processing = false;
        break;
    }
    if (!continue_processing) {
      // Rewind the position to before the last header was read.
      ABSL_LOG(INFO) << "position_before_header: " << position_before_header;
      RETURN_IF_NOT_OK(read_bit_buffer.Seek(position_before_header));
    }
    if (!processed_ia_header) {
      return absl::InvalidArgumentError(
          "An IA Sequence and/or descriptor OBUs must always start with an IA "
          "Header.");
    }
    if (is_exhaustive_and_exact && !read_bit_buffer.IsDataAvailable()) {
      // We've reached the end of the bitstream and we've processed all
      // descriptor OBUs.
      break;
    }
  }
  if (!audio_element_obu_map.empty()) {
    auto audio_elements_with_data =
        ObuWithDataGenerator::GenerateAudioElementsWithData(
            parsed_obus.codec_config_obus, audio_element_obu_map);
    if (!audio_elements_with_data.ok()) {
      return audio_elements_with_data.status();
    }
    parsed_obus.audio_elements_with_data = std::move(*audio_elements_with_data);
  }
  return parsed_obus;
}

}  // namespace iamf_tools
