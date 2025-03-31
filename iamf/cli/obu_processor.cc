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

#include "iamf/cli/obu_processor.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/profile_filter.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

// Gets a CodecConfigObu from `read_bit_buffer` and stores it into
// `codec_config_obu_map`, using the `codec_config_id` as the unique key.
absl::Status GetAndStoreCodecConfigObu(
    const ObuHeader& header, int64_t payload_size,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obu_map,
    ReadBitBuffer& read_bit_buffer) {
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
  LOG(INFO) << "Mix Presentation OBU successfully parsed.";
  mix_presentation_obu->PrintObu();
  mix_presentation_obus.push_back(*std::move(mix_presentation_obu));
  return absl::OkStatus();
}

absl::Status UpdateParameterStatesIfNeeded(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements_with_data,
    const GlobalTimingModule& global_timing_module,
    ParametersManager& parameters_manager) {
  std::optional<InternalTimestamp> global_timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module.GetGlobalAudioFrameTimestamp(global_timestamp));
  // Not ready to update the states yet.
  if (!global_timestamp.has_value()) {
    return absl::OkStatus();
  }

  // The audio frames for all audio elements are finished; update the
  // parameters manager.
  for (const auto& [audio_element_id, unused_element] :
       audio_elements_with_data) {
    RETURN_IF_NOT_OK(parameters_manager.UpdateDemixingState(audio_element_id,
                                                            *global_timestamp));
    RETURN_IF_NOT_OK(parameters_manager.UpdateReconGainState(
        audio_element_id, *global_timestamp));
  }
  return absl::OkStatus();
}

absl::Status GetAndStoreAudioFrameWithData(
    const ObuHeader& header, const int64_t payload_size,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements_with_data,
    const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>&
        substream_id_to_audio_element,
    ReadBitBuffer& read_bit_buffer, GlobalTimingModule& global_timing_module,
    ParametersManager& parameters_manager,
    std::optional<AudioFrameWithData>& output_audio_frame_with_data) {
  output_audio_frame_with_data.reset();
  auto audio_frame_obu =
      AudioFrameObu::CreateFromBuffer(header, payload_size, read_bit_buffer);
  if (!audio_frame_obu.ok()) {
    return audio_frame_obu.status();
  }
  const auto substream_id = audio_frame_obu->GetSubstreamId();
  const auto audio_element_iter =
      substream_id_to_audio_element.find(substream_id);
  if (audio_element_iter == substream_id_to_audio_element.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "No audio element found having substream ID: ", substream_id));
  }
  const auto& audio_element_with_data = *audio_element_iter->second;
  auto audio_frame_with_data = ObuWithDataGenerator::GenerateAudioFrameWithData(
      audio_element_with_data, *audio_frame_obu, global_timing_module,
      parameters_manager);
  if (!audio_frame_with_data.ok()) {
    return audio_frame_with_data.status();
  }
  output_audio_frame_with_data = *audio_frame_with_data;

  RETURN_IF_NOT_OK(UpdateParameterStatesIfNeeded(
      audio_elements_with_data, global_timing_module, parameters_manager));

  return absl::OkStatus();
}

absl::Status GetAndStoreParameterBlockWithData(
    const ObuHeader& header, const int64_t payload_size,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ReadBitBuffer& read_bit_buffer, GlobalTimingModule& global_timing_module,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data) {
  auto parameter_block_obu = ParameterBlockObu::CreateFromBuffer(
      header, payload_size, param_definition_variants, read_bit_buffer);
  if (!parameter_block_obu.ok()) {
    return parameter_block_obu.status();
  }

  std::optional<InternalTimestamp> global_timestamp;
  RETURN_IF_NOT_OK(
      global_timing_module.GetGlobalAudioFrameTimestamp(global_timestamp));
  if (!global_timestamp.has_value()) {
    return absl::InvalidArgumentError(
        "Global timestamp has no value while generating a parameter "
        "block");
  }

  // Process the newly parsed parameter block OBU.
  auto parameter_block_with_data =
      ObuWithDataGenerator::GenerateParameterBlockWithData(
          *global_timestamp, global_timing_module,
          std::move(*parameter_block_obu));
  if (!parameter_block_with_data.ok()) {
    return parameter_block_with_data.status();
  }
  output_parameter_block_with_data = std::move(*parameter_block_with_data);

  return absl::OkStatus();
}

// Returns a list of pointers to the supported mix presentations. Empty if none
// are supported.
std::list<MixPresentationObu*> GetSupportedMixPresentations(
    const absl::flat_hash_set<ProfileVersion> requested_profiles,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  // Find a mix presentation and layout that agrees with the requested profiles.
  std::list<MixPresentationObu*> supported_mix_presentations;
  std::string cumulative_error_message;
  for (auto iter = mix_presentation_obus.begin();
       iter != mix_presentation_obus.end(); ++iter) {
    auto profiles = requested_profiles;
    const auto status = ProfileFilter::FilterProfilesForMixPresentation(
        audio_elements, *iter, profiles);
    if (status.ok()) {
      supported_mix_presentations.push_back(&*iter);
    }
    absl::StrAppend(&cumulative_error_message, status.message(), "\n");
  }
  LOG(INFO) << "Filtered mix presentations: " << cumulative_error_message;
  return supported_mix_presentations;
}

// Searches for the desired layout in the supported mix presentations. If found,
// the output_playback_layout is the same as the desired_layout. Otherwise, we
// default to the first layout in the first unsupported mix presentation.
absl::StatusOr<MixPresentationObu*> GetPlaybackLayoutAndMixPresentation(
    const std::list<MixPresentationObu*>& supported_mix_presentations,
    const Layout& desired_layout, Layout& output_playback_layout) {
  for (const auto& mix_presentation : supported_mix_presentations) {
    for (const auto& sub_mix : mix_presentation->sub_mixes_) {
      for (const auto& layout : sub_mix.layouts) {
        if (layout.loudness_layout == desired_layout) {
          output_playback_layout = layout.loudness_layout;
          return mix_presentation;
        }
      }
    }
  }
  // If we get here, we didn't find the desired layout in any of the supported
  // mix presentations. We default to the first layout in the first mix
  // presentation.
  MixPresentationObu* output_mix_presentation =
      supported_mix_presentations.front();
  if (output_mix_presentation->sub_mixes_.empty()) {
    return absl::InvalidArgumentError(
        "No submixes found in the first mix presentation.");
  }
  if (output_mix_presentation->sub_mixes_.front().layouts.empty()) {
    return absl::InvalidArgumentError(
        "No layouts found in the first submix of the first mix presentation.");
  }
  // We add a "virtual" layout here that matches the desired layout if it wasn't
  // found. This allows us to decode to the user-requested layout even if it
  // wasn't present in the mix presentation.
  output_mix_presentation->sub_mixes_.front().layouts.front().loudness_layout =
      desired_layout;
  output_playback_layout = desired_layout;
  return output_mix_presentation;
}

// Resets the buffer to `start_position` and sets the `insufficient_data`
// flag to `true`. Clears the output maps.
absl::Status InsufficientDataReset(
    ReadBitBuffer& read_bit_buffer, const int64_t start_position,
    bool& insufficient_data,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        output_codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        output_audio_elements_with_data,
    std::list<MixPresentationObu>& output_mix_presentation_obus) {
  LOG(INFO) << "Insufficient data to process all descriptor OBUs.";
  insufficient_data = true;
  output_codec_config_obus.clear();
  output_audio_elements_with_data.clear();
  output_mix_presentation_obus.clear();
  RETURN_IF_NOT_OK(read_bit_buffer.Seek(start_position));
  LOG(INFO) << "Reset the buffer to the beginning.";
  return absl::ResourceExhaustedError(
      "Insufficient data to process all descriptor OBUs. Please provide "
      "more data and try again.");
}

void GetSampleRateAndFrameSize(
    const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        output_codec_config_obus,
    std::optional<uint32_t>& output_sample_rate,
    std::optional<uint32_t>& output_frame_size) {
  if (output_codec_config_obus.size() != 1) {
    LOG(WARNING) << "Expected exactly one codec config OBUs, but found "
                 << output_codec_config_obus.size();
    return;
  }
  const auto& first_codec_config_obu = output_codec_config_obus.begin()->second;
  output_sample_rate = first_codec_config_obu.GetOutputSampleRate();
  output_frame_size = first_codec_config_obu.GetNumSamplesPerFrame();
}

}  // namespace

absl::Status ObuProcessor::InitializeInternal(bool is_exhaustive_and_exact,
                                              bool& output_insufficient_data) {
  // Process the descriptor OBUs.
  LOG(INFO) << "Starting Descriptor OBU processing";
  RETURN_IF_NOT_OK(ObuProcessor::ProcessDescriptorObus(
      is_exhaustive_and_exact, *read_bit_buffer_, ia_sequence_header_,
      codec_config_obus_, audio_elements_, mix_presentations_,
      output_insufficient_data));
  LOG(INFO) << "Processed Descriptor OBUs";
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      audio_elements_, mix_presentations_, param_definition_variants_));
  GetSampleRateAndFrameSize(codec_config_obus_, output_sample_rate_,
                            output_frame_size_);
  // Mapping from substream IDs to pointers to audio element with data.
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements_) {
    for (const auto& [substream_id, unused_labels] :
         audio_element_with_data.substream_id_to_labels) {
      auto [unused_iter, inserted] = substream_id_to_audio_element_.insert(
          {substream_id, &audio_element_with_data});
      if (!inserted) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Duplicated substream ID: ", substream_id,
            " associated with audio element ID: ", audio_element_id));
      }
    }
  }
  global_timing_module_ =
      GlobalTimingModule::Create(audio_elements_, param_definition_variants_);
  if (global_timing_module_ == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize the global timing module");
  }
  parameters_manager_.emplace(audio_elements_);
  RETURN_IF_NOT_OK(parameters_manager_->Initialize());
  return absl::OkStatus();
}

absl::Status ObuProcessor::ProcessDescriptorObus(
    bool is_exhaustive_and_exact, ReadBitBuffer& read_bit_buffer,
    IASequenceHeaderObu& output_sequence_header,
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        output_codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        output_audio_elements_with_data,
    std::list<MixPresentationObu>& output_mix_presentation_obus,
    bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
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
        return InsufficientDataReset(
            read_bit_buffer, global_position_before_all_obus,
            output_insufficient_data, output_codec_config_obus,
            output_audio_elements_with_data, output_mix_presentation_obus);
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
        LOG(ERROR) << error_status;
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
      return InsufficientDataReset(
          read_bit_buffer, global_position_before_all_obus,
          output_insufficient_data, output_codec_config_obus,
          output_audio_elements_with_data, output_mix_presentation_obus);
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
          LOG(WARNING) << "Detected an IA Sequence without temporal units.";
          continue_processing = false;
          break;
        }
        auto ia_sequence_header_obu = IASequenceHeaderObu::CreateFromBuffer(
            header, payload_size, read_bit_buffer);
        if (!ia_sequence_header_obu.ok()) {
          return ia_sequence_header_obu.status();
        }
        output_sequence_header = *std::move(ia_sequence_header_obu);
        output_sequence_header.PrintObu();
        processed_ia_header = true;
        break;
      }
      case kObuIaCodecConfig: {
        RETURN_IF_NOT_OK(GetAndStoreCodecConfigObu(
            header, payload_size, output_codec_config_obus, read_bit_buffer));
        break;
      }
      case kObuIaAudioElement: {
        RETURN_IF_NOT_OK(GetAndStoreAudioElementObu(
            header, payload_size, audio_element_obu_map, read_bit_buffer));
        break;
      }
      case kObuIaMixPresentation: {
        RETURN_IF_NOT_OK(GetAndStoreMixPresentationObu(
            header, payload_size, output_mix_presentation_obus,
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
        LOG(INFO) << "Detected a reserved OBU while parsing Descriptor OBUs. "
                  << "Safely ignoring it.";
        std::vector<uint8_t> buffer_to_discard(payload_size);
        RETURN_IF_NOT_OK(
            read_bit_buffer.ReadUint8Span(absl::MakeSpan(buffer_to_discard)));
        break;
      }
      default:
        /// TODO(b/387550488): Handle reserved OBUs.
        continue_processing = false;
        break;
    }
    if (!continue_processing) {
      // Rewind the position to before the last header was read.
      LOG(INFO) << "position_before_header: " << position_before_header;
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
            output_codec_config_obus, audio_element_obu_map);
    if (!audio_elements_with_data.ok()) {
      return audio_elements_with_data.status();
    }
    output_audio_elements_with_data = std::move(*audio_elements_with_data);
  }
  return absl::OkStatus();
}

absl::Status ObuProcessor::ProcessTemporalUnitObu(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements_with_data,
    const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        codec_config_obus,
    const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>&
        substream_id_to_audio_element,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ParametersManager& parameters_manager, ReadBitBuffer& read_bit_buffer,
    GlobalTimingModule& global_timing_module,
    std::optional<AudioFrameWithData>& output_audio_frame_with_data,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data,
    std::optional<TemporalDelimiterObu>& output_temporal_delimiter,
    bool& continue_processing) {
  continue_processing = true;
  output_audio_frame_with_data.reset();
  output_parameter_block_with_data.reset();
  output_temporal_delimiter.reset();

  auto header_metadata = ObuHeader::PeekObuTypeAndTotalObuSize(read_bit_buffer);
  if (!header_metadata.ok()) {
    if (header_metadata.status().code() ==
        absl::StatusCode::kResourceExhausted) {
      // Can't read header because there is not enough data. This is not an
      // error, but we're done processing for now.
      continue_processing = false;
      return absl::OkStatus();
    } else {
      // Some other error occurred, propagate it.
      return header_metadata.status();
    }
  }

  if (!read_bit_buffer.CanReadBytes(header_metadata->total_obu_size)) {
    // This is a temporal unit OBU for which we don't have enough data. This is
    // not an error, but we're done processing for now.
    continue_processing = false;
    return absl::OkStatus();
  }

  const int64_t position_before_header = read_bit_buffer.Tell();

  // Read in the header and determines the size of the payload in bytes.
  ObuHeader header;
  int64_t payload_size;
  RETURN_IF_NOT_OK(header.ReadAndValidate(read_bit_buffer, payload_size));

  // Typically we should expect {`kObuIaAudioFrameX`,`kObuIaParameterBlock`,
  // `kObuIaTemporalDelimiter`}. We also want to detect an `kIaSequenceHeader`
  // which would signal the start of a new IA Sequence, and to gracefully
  // handle "reserved" OBUs.
  switch (header.obu_type) {
    case kObuIaAudioFrame:
    case kObuIaAudioFrameId0:
    case kObuIaAudioFrameId1:
    case kObuIaAudioFrameId2:
    case kObuIaAudioFrameId3:
    case kObuIaAudioFrameId4:
    case kObuIaAudioFrameId5:
    case kObuIaAudioFrameId6:
    case kObuIaAudioFrameId7:
    case kObuIaAudioFrameId8:
    case kObuIaAudioFrameId9:
    case kObuIaAudioFrameId10:
    case kObuIaAudioFrameId11:
    case kObuIaAudioFrameId12:
    case kObuIaAudioFrameId13:
    case kObuIaAudioFrameId14:
    case kObuIaAudioFrameId15:
    case kObuIaAudioFrameId16:
    case kObuIaAudioFrameId17: {
      RETURN_IF_NOT_OK(GetAndStoreAudioFrameWithData(
          header, payload_size, audio_elements_with_data,
          substream_id_to_audio_element, read_bit_buffer, global_timing_module,
          parameters_manager, output_audio_frame_with_data));
      break;
    }
    case kObuIaParameterBlock: {
      RETURN_IF_NOT_OK(GetAndStoreParameterBlockWithData(
          header, payload_size, param_definition_variants, read_bit_buffer,
          global_timing_module, output_parameter_block_with_data));
      break;
    }
    case kObuIaTemporalDelimiter: {
      // This implementation does not process by temporal unit. Safely ignore
      // it.
      const auto& temporal_delimiter = TemporalDelimiterObu::CreateFromBuffer(
          header, payload_size, read_bit_buffer);
      if (!temporal_delimiter.ok()) {
        return temporal_delimiter.status();
      }
      output_temporal_delimiter = *temporal_delimiter;
      break;
    }
    case kObuIaSequenceHeader:
      if (!header.obu_redundant_copy) {
        // OK. The user of this function will need to reconfigure its state to
        // process the next IA sequence.
        LOG(INFO) << "Detected the start of the next IA Sequence.";
        continue_processing = false;
        break;
      }
      // Ok for any IAMF v1.1.0 descriptor OBUs we can skip over redundant
      // copies.
      [[fallthrough]];
    case kObuIaCodecConfig:
    case kObuIaAudioElement:
    case kObuIaMixPresentation:
      if (!header.obu_redundant_copy) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Unexpected non-reserved OBU obu_type= ", header.obu_type));
      }
      // Consume and discard the OBU. IAMF allows us to ignore it (even if the
      // redundant flag is misleading).
      [[fallthrough]];
    default:
      // TODO(b/329705373): Read in the data as an `ArbitraryOBU` and output
      //                    it from this function.
      LOG(INFO) << "Detected a reserved or redundant OBU. Safely ignoring it.";
      std::vector<uint8_t> buffer_to_discard(payload_size);
      RETURN_IF_NOT_OK(
          read_bit_buffer.ReadUint8Span(absl::MakeSpan(buffer_to_discard)));
      break;
  }

  if (!continue_processing) {
    // Rewind the position to before the last header was read.
    LOG(INFO) << "position_before_header: " << position_before_header;
    RETURN_IF_NOT_OK(read_bit_buffer.Seek(position_before_header));
  }

  return absl::OkStatus();
}

std::unique_ptr<ObuProcessor> ObuProcessor::Create(
    bool is_exhaustive_and_exact, ReadBitBuffer* read_bit_buffer,
    bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
  if (read_bit_buffer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ObuProcessor> obu_processor =
      absl::WrapUnique(new ObuProcessor(read_bit_buffer));
  if (const auto status = obu_processor->InitializeInternal(
          is_exhaustive_and_exact, output_insufficient_data);
      !status.ok()) {
    LOG(ERROR) << status;
    return nullptr;
  }
  return obu_processor;
}

std::unique_ptr<ObuProcessor> ObuProcessor::CreateForRendering(
    const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
    const Layout& desired_layout,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    bool is_exhaustive_and_exact, ReadBitBuffer* read_bit_buffer,
    Layout& output_layout, bool& output_insufficient_data) {
  // `output_insufficient_data` indicates a specific error condition and so is
  // true iff we've received valid data but need more of it.
  output_insufficient_data = false;
  if (read_bit_buffer == nullptr) {
    return nullptr;
  }
  std::unique_ptr<ObuProcessor> obu_processor =
      absl::WrapUnique(new ObuProcessor(read_bit_buffer));
  if (const auto status = obu_processor->InitializeInternal(
          is_exhaustive_and_exact, output_insufficient_data);
      !status.ok()) {
    LOG(ERROR) << status;
    return nullptr;
  }

  if (const auto status = obu_processor->InitializeForRendering(
          desired_profile_versions, desired_layout, sample_processor_factory,
          output_layout);
      !status.ok()) {
    LOG(ERROR) << status;
    return nullptr;
  }
  return obu_processor;
}

absl::StatusOr<uint32_t> ObuProcessor::GetOutputSampleRate() const {
  RETURN_IF_NOT_OK(
      ValidateHasValue(output_sample_rate_,
                       "Output sample rate, was this a trivial IA Sequence?"));
  return *output_sample_rate_;
}

absl::StatusOr<uint32_t> ObuProcessor::GetOutputFrameSize() const {
  RETURN_IF_NOT_OK(
      ValidateHasValue(output_frame_size_,
                       "Output frame size, was this a trivial IA Sequence?"));
  return *output_frame_size_;
}

absl::Status ObuProcessor::InitializeForRendering(
    const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
    const Layout& desired_layout,
    const RenderingMixPresentationFinalizer::SampleProcessorFactory&
        sample_processor_factory,
    Layout& output_layout) {
  if (mix_presentations_.empty()) {
    return absl::InvalidArgumentError("No mix presentation OBUs found.");
  }
  if (audio_elements_.empty()) {
    return absl::InvalidArgumentError("No audio element OBUs found.");
  }

  // TODO(b/377747704): Decode only the frames selected for the playback
  //                    layout.
  audio_frame_decoder_.emplace();
  for (const auto& [unused_id, audio_element_with_data] : audio_elements_) {
    RETURN_IF_NOT_OK(audio_frame_decoder_->InitDecodersForSubstreams(
        audio_element_with_data.substream_id_to_labels,
        *audio_element_with_data.codec_config));
  }
  {
    auto temp_demixing_module =
        DemixingModule::CreateForReconstruction(audio_elements_);
    if (!temp_demixing_module.ok()) {
      return temp_demixing_module.status();
    }
    demixing_module_.emplace(*std::move(temp_demixing_module));
  }

  // TODO(b/340289717): Add a way to select the mix presentation if multiple
  //                    are supported.
  const std::list<MixPresentationObu*> supported_mix_presentations =
      GetSupportedMixPresentations(desired_profile_versions, audio_elements_,
                                   mix_presentations_);
  if (supported_mix_presentations.empty()) {
    return absl::NotFoundError("No supported mix presentation OBUs found.");
  }
  Layout playback_layout;
  auto mix_presentation_to_render = GetPlaybackLayoutAndMixPresentation(
      supported_mix_presentations, desired_layout, output_layout);
  if (!mix_presentation_to_render.ok()) {
    return mix_presentation_to_render.status();
  }
  int playback_sub_mix_index;
  int playback_layout_index;
  RETURN_IF_NOT_OK(GetIndicesForLayout(
      (*mix_presentation_to_render)->sub_mixes_, output_layout,
      playback_sub_mix_index, playback_layout_index));
  decoding_layout_info_ = {
      .mix_presentation_id =
          (*mix_presentation_to_render)->GetMixPresentationId(),
      .sub_mix_index = playback_sub_mix_index,
      .layout_index = playback_layout_index,
  };
  auto forward_on_desired_layout =
      [&sample_processor_factory, mix_presentation_to_render,
       playback_sub_mix_index, playback_layout_index](
          DecodedUleb128 mix_presentation_id, int sub_mix_index,
          int layout_index, const Layout& layout, int num_channels,
          int sample_rate, int bit_depth, size_t max_input_samples_per_frame)
      -> std::unique_ptr<SampleProcessorBase> {
    if (mix_presentation_id ==
            (*mix_presentation_to_render)->GetMixPresentationId() &&
        playback_sub_mix_index == sub_mix_index &&
        playback_layout_index == layout_index) {
      return sample_processor_factory(
          mix_presentation_id, sub_mix_index, layout_index, layout,
          num_channels, sample_rate, bit_depth, max_input_samples_per_frame);
    }
    return nullptr;
  };

  // Create the mix presentation finalizer which is used to render the output
  // files. We neither trust the user-provided loudness, nor care about the
  // calculated loudness.
  const RendererFactory renderer_factory;
  absl::StatusOr<RenderingMixPresentationFinalizer> mix_presentation_finalizer =
      RenderingMixPresentationFinalizer::Create(
          /*renderer_factory=*/&renderer_factory,
          /*loudness_calculator_factory=*/nullptr, audio_elements_,
          forward_on_desired_layout, mix_presentations_);
  if (!mix_presentation_finalizer.ok()) {
    return mix_presentation_finalizer.status();
  }
  mix_presentation_finalizer_.emplace(*std::move(mix_presentation_finalizer));

  return absl::OkStatus();
}

absl::Status ObuProcessor::ProcessTemporalUnitObu(
    std::optional<AudioFrameWithData>& output_audio_frame_with_data,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data,
    std::optional<TemporalDelimiterObu>& output_temporal_delimiter,
    bool& continue_processing) {
  if (!parameters_manager_.has_value()) {
    return absl::InvalidArgumentError(
        "Parameters manager is not constructed; "
        "remember to call `Initialize()` first.");
  }
  if (global_timing_module_ == nullptr) {
    return absl::InvalidArgumentError(
        "Global timing module is not constructed; "
        "remember to call `Initialize()` first.");
  }
  if (read_bit_buffer_ == nullptr) {
    return absl::InvalidArgumentError(
        "Read bit buffer is not constructed; "
        "remember to call `Initialize()` first.");
  }

  return ObuProcessor::ProcessTemporalUnitObu(
      audio_elements_, codec_config_obus_, substream_id_to_audio_element_,
      param_definition_variants_, *parameters_manager_, *read_bit_buffer_,
      *global_timing_module_, output_audio_frame_with_data,
      output_parameter_block_with_data, output_temporal_delimiter,
      continue_processing);
}

absl::Status ObuProcessor::ProcessTemporalUnit(
    bool eos_is_end_of_sequence,
    std::optional<OutputTemporalUnit>& output_temporal_unit,
    bool& continue_processing) {
  continue_processing = true;
  while (continue_processing) {
    std::optional<AudioFrameWithData> audio_frame_with_data;
    std::optional<ParameterBlockWithData> parameter_block_with_data;
    std::optional<TemporalDelimiterObu> temporal_delimiter;
    RETURN_IF_NOT_OK(
        ProcessTemporalUnitObu(audio_frame_with_data, parameter_block_with_data,
                               temporal_delimiter, continue_processing));

    // Collect OBUs into a temporal unit.
    if (audio_frame_with_data.has_value()) {
      TemporalUnitData::AddDataToCorrectTemporalUnit(
          current_temporal_unit_, next_temporal_unit_,
          *std::move(audio_frame_with_data));
    } else if (parameter_block_with_data.has_value()) {
      TemporalUnitData::AddDataToCorrectTemporalUnit(
          current_temporal_unit_, next_temporal_unit_,
          *std::move(parameter_block_with_data));
    } else if (temporal_delimiter.has_value()) {
      current_temporal_unit_.temporal_delimiter = *temporal_delimiter;
    }

    // The current temporal unit is considered finished if any of the
    // following conditions is met:
    // - The end of sequence is reached.
    // - The timestamp has advanced (i.e. when the next temporal unit gets its
    //   timestamp).
    // - A temporal delimiter is encountered.
    // TODO(b/405943120): Stop creating buggy first "empty" temporal units when
    //                    temporal delimiters are encountered.
    if ((!continue_processing && eos_is_end_of_sequence) ||
        next_temporal_unit_.timestamp.has_value() ||
        current_temporal_unit_.temporal_delimiter.has_value()) {
      output_temporal_unit = OutputTemporalUnit();
      output_temporal_unit->output_audio_frames =
          std::move(current_temporal_unit_.audio_frames);
      output_temporal_unit->output_parameter_blocks =
          std::move(current_temporal_unit_.parameter_blocks);
      if (current_temporal_unit_.timestamp.has_value()) {
        output_temporal_unit->output_timestamp =
            current_temporal_unit_.timestamp.value();
      }
      current_temporal_unit_ = std::move(next_temporal_unit_);
      next_temporal_unit_ = TemporalUnitData();
      break;
    }
  }

  return absl::OkStatus();
}

absl::Status ObuProcessor::RenderTemporalUnitAndMeasureLoudness(
    InternalTimestamp start_timestamp,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    absl::Span<const std::vector<int32_t>>& output_rendered_pcm_samples) {
  if (audio_frames.empty()) {
    // Nothing to decode, render, or measure loudness of.
    return absl::OkStatus();
  }

  if (!audio_frame_decoder_.has_value()) {
    return absl::InvalidArgumentError(
        "Audio frame decoder is not constructed; "
        "remember to call `InitializeForRendering()` first.");
  }
  if (!demixing_module_.has_value()) {
    return absl::InvalidArgumentError(
        "Demxing module is not constructed; "
        "remember to call `InitializeForRendering()` first.");
  }
  if (!mix_presentation_finalizer_.has_value()) {
    return absl::InvalidArgumentError(
        "Mix presentation finalizer is not constructed; "
        "remember to call `InitializeForRendering()` first.");
  }

  // Decode the temporal unit.
  std::optional<InternalTimestamp> end_timestamp;

  // This resizing should happen only once per IA sequence, since all the
  // temporal units should contain the same number of audio frames.
  decoded_frames_for_temporal_unit_.resize(audio_frames.size());
  auto decoded_frames_iter = decoded_frames_for_temporal_unit_.begin();
  for (const auto& audio_frame : audio_frames) {
    if (!end_timestamp.has_value()) {
      end_timestamp = audio_frame.end_timestamp;
    }
    RETURN_IF_NOT_OK(
        CompareTimestamps(start_timestamp, audio_frame.start_timestamp,
                          "Audio frame has a different start timestamp than "
                          "the temporal unit: "));
    RETURN_IF_NOT_OK(CompareTimestamps(*end_timestamp,
                                       audio_frame.end_timestamp,
                                       "Audio frame has a different end "
                                       "timestamp than the temporal unit: "));
    auto decoded_frame = audio_frame_decoder_->Decode(audio_frame);
    if (!decoded_frame.ok()) {
      return decoded_frame.status();
    }
    *decoded_frames_iter = std::move(*decoded_frame);
    decoded_frames_iter++;
  }

  // Reconstruct the temporal unit and store the result in the output map.
  const auto decoded_labeled_frames_for_temporal_unit =
      demixing_module_->DemixDecodedAudioSamples(
          decoded_frames_for_temporal_unit_);
  if (!decoded_labeled_frames_for_temporal_unit.ok()) {
    return decoded_labeled_frames_for_temporal_unit.status();
  }

  RETURN_IF_NOT_OK(mix_presentation_finalizer_->PushTemporalUnit(
      *decoded_labeled_frames_for_temporal_unit, start_timestamp,
      *end_timestamp, parameter_blocks));

  auto rendered_samples =
      mix_presentation_finalizer_->GetPostProcessedSamplesAsSpan(
          decoding_layout_info_.mix_presentation_id,
          decoding_layout_info_.sub_mix_index,
          decoding_layout_info_.layout_index);
  if (!rendered_samples.ok()) {
    return rendered_samples.status();
  }
  output_rendered_pcm_samples = *rendered_samples;

  // TODO(b/379122580): Add a call to `FinalizePushingTemporalUnits`, then a
  //                    final call to `GetPostProcessedSamplesAsSpan` when there
  //                    are no more temporal units to push. Those calls may
  //                    belong elsewhere in the class depending on the
  //                    interface.

  return absl::OkStatus();
}

}  // namespace iamf_tools
