/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/temporal_unit_processor.h"

#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/obu_with_data_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/param_definition_base.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status UpdateParameterStatesIfNeeded(
    const absl::flat_hash_set<DecodedUleb128>& audio_element_ids,
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
  for (const auto& audio_element_id : audio_element_ids) {
    RETURN_IF_NOT_OK(parameters_manager.UpdateDemixingState(audio_element_id,
                                                            *global_timestamp));
    RETURN_IF_NOT_OK(parameters_manager.UpdateReconGainState(
        audio_element_id, *global_timestamp));
  }
  return absl::OkStatus();
}

absl::Status GetAndStoreAudioFrameWithData(
    const ObuHeader& header, const int64_t payload_size,
    const absl::flat_hash_set<DecodedUleb128>& audio_element_ids,
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
      audio_element_ids, global_timing_module, parameters_manager));

  return absl::OkStatus();
}

const ParamDefinition* PeekParameterIdAndLookupParamDefinition(
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ReadBitBuffer& read_bit_buffer) {
  auto parameter_id = ParameterBlockObu::PeekParameterId(read_bit_buffer);
  if (!parameter_id.ok()) {
    return nullptr;
  }
  const auto parameter_definition_it =
      param_definition_variants.find(*parameter_id);
  if (parameter_definition_it == param_definition_variants.end()) {
    return nullptr;
  }

  const auto cast_to_base_pointer = [](const auto& param_definition) {
    return static_cast<const ParamDefinition*>(&param_definition);
  };
  return std::visit(cast_to_base_pointer, parameter_definition_it->second);
}

absl::Status GetAndStoreParameterBlockWithData(
    const ObuHeader& header, const int64_t payload_size,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ReadBitBuffer& read_bit_buffer, GlobalTimingModule& global_timing_module,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data) {
  const ParamDefinition* parameter_definition =
      PeekParameterIdAndLookupParamDefinition(param_definition_variants,
                                              read_bit_buffer);
  if (parameter_definition == nullptr) {
    return absl::InvalidArgumentError(
        "Found a stray parameter block OBU (no matching parameter "
        "definition).");
  }

  auto parameter_block_obu = ParameterBlockObu::CreateFromBuffer(
      header, payload_size, *parameter_definition, read_bit_buffer);
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

absl::Status ProcessTemporalUnitObu(
    const absl::flat_hash_set<DecodedUleb128>& audio_element_ids,
    const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>&
        substream_id_to_audio_element,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ParametersManager& parameters_manager, ReadBitBuffer& read_bit_buffer,
    GlobalTimingModule& global_timing_module,
    std::optional<AudioFrameWithData>& output_audio_frame_with_data,
    std::optional<ParameterBlockWithData>& output_parameter_block_with_data,
    std::optional<TemporalDelimiterObu>& output_temporal_delimiter,
    std::optional<MetadataObu>& output_metadata_obu,
    bool& continue_processing) {
  continue_processing = true;
  output_audio_frame_with_data.reset();
  output_parameter_block_with_data.reset();
  output_temporal_delimiter.reset();
  output_metadata_obu.reset();

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

  if (read_bit_buffer.NumBytesAvailable() < header_metadata->total_obu_size) {
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
  // `kObuIaTemporalDelimiter`, `kObuIaMetadata`}. We also want to detect an
  // `kIaSequenceHeader` which would signal the start of a new IA Sequence, and
  // to gracefully handle "reserved" OBUs.
  absl::Status parsed_obu_status = absl::OkStatus();
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
    case kObuIaAudioFrameId17:
      parsed_obu_status = GetAndStoreAudioFrameWithData(
          header, payload_size, audio_element_ids,
          substream_id_to_audio_element, read_bit_buffer, global_timing_module,
          parameters_manager, output_audio_frame_with_data);
      break;

    case kObuIaParameterBlock:
      parsed_obu_status = GetAndStoreParameterBlockWithData(
          header, payload_size, param_definition_variants, read_bit_buffer,
          global_timing_module, output_parameter_block_with_data);
      break;
    case kObuIaTemporalDelimiter: {
      // This implementation does not process by temporal unit. Safely ignore
      // it.
      const auto& temporal_delimiter = TemporalDelimiterObu::CreateFromBuffer(
          header, payload_size, read_bit_buffer);
      if (!temporal_delimiter.ok()) {
        parsed_obu_status = temporal_delimiter.status();
        break;
      }
      output_temporal_delimiter = *temporal_delimiter;
      break;
    }
    case kObuIaMetadata: {
      if (header.obu_redundant_copy) {
        ABSL_LOG(INFO) << "Safely ignoring a redundant Metadata OBU.";
        RETURN_IF_NOT_OK(read_bit_buffer.IgnoreBytes(payload_size));
        break;
      }
      auto metadata_obu =
          MetadataObu::CreateFromBuffer(header, payload_size, read_bit_buffer);
      if (!metadata_obu.ok()) {
        parsed_obu_status = metadata_obu.status();
        break;
      }
      // Metadata OBU containing `metadata_iamf_tags` SHALL NOT be placed
      // within a Temporal Unit.
      if (metadata_obu->GetMetadataType() == kMetadataTypeIamfTags) {
        return absl::InvalidArgumentError(
            "Metadata OBU with `metadata_iamf_tags` is not allowed in a "
            "Temporal Unit.");
      }
      output_metadata_obu = *std::move(metadata_obu);
      break;
    }
    case kObuIaSequenceHeader:
      if (!header.obu_redundant_copy) {
        // OK. The user of this function will need to reconfigure its state to
        // process the next IA sequence.
        ABSL_LOG(INFO) << "Detected the start of the next IA Sequence.";
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
      ABSL_LOG(INFO) << "Safely ignoring a redundant OBU.";
      RETURN_IF_NOT_OK(read_bit_buffer.IgnoreBytes(payload_size));
      break;
    default:
      // TODO(b/329705373): Read in the data as an `ArbitraryOBU` and output
      //                    it from this function.
      ABSL_LOG(INFO) << "Detected a reserved OBU. Safely ignoring it.";
      RETURN_IF_NOT_OK(read_bit_buffer.IgnoreBytes(payload_size));
      break;
  }
  if (!parsed_obu_status.ok()) {
    // The spec is permissive in bypassing OBUs that we don't yet understand.
    // These may signal some future features. Ignore the OBU, downstream OBUs
    // that reference it will be ignored.
    ABSL_LOG(WARNING) << "Bypassing OBU: " << header.obu_type
                      << " with status: " << parsed_obu_status
                      << " and seeking past it.";
    RETURN_IF_NOT_OK(read_bit_buffer.Seek(position_before_header));
    RETURN_IF_NOT_OK(
        read_bit_buffer.IgnoreBytes(header_metadata->total_obu_size));
  }

  if (!continue_processing) {
    // Rewind the position to before the last header was read.
    ABSL_LOG(INFO) << "position_before_header: " << position_before_header;
    RETURN_IF_NOT_OK(read_bit_buffer.Seek(position_before_header));
  }

  return absl::OkStatus();
}

// Adds `metadata_obu` to `current_metadata_obus` if the current temporal unit
// has no audio frames yet; otherwise tentatively adds it to
// `next_metadata_obus`.
void AddMetadataObuToCorrectTemporalUnit(
    const std::list<AudioFrameWithData>& current_audio_frames,
    std::list<MetadataObu>& current_metadata_obus,
    std::list<MetadataObu>& next_metadata_obus, MetadataObu&& metadata_obu) {
  if (current_audio_frames.empty()) {
    current_metadata_obus.push_back(std::move(metadata_obu));
  } else {
    next_metadata_obus.push_back(std::move(metadata_obu));
  }
}

// Validates that no Metadata OBU was placed between Audio Frame OBUs of the
// current temporal unit.
//
// If `next_metadata_obus` already holds a tentatively added Metadata OBU
// (placed after the current temporal unit already had audio frames), the new
// Audio Frame OBU must belong to the next temporal unit (which sets
// `next_timestamp`). If `next_timestamp` is still unset, the Audio Frame OBU
// belonged to the current temporal unit, meaning the Metadata OBU was
// illegally placed between Audio Frame OBUs of the current temporal unit.
absl::Status ValidateNoMetadataBetweenAudioFrames(
    const std::list<MetadataObu>& next_metadata_obus,
    const std::optional<InternalTimestamp>& next_timestamp) {
  if (!next_metadata_obus.empty() && !next_timestamp.has_value()) {
    return absl::InvalidArgumentError(
        "Metadata OBU must not be placed after Audio Frame OBUs in a "
        "Temporal Unit.");
  }
  return absl::OkStatus();
}

// Validates that no Metadata OBU precedes a Temporal Delimiter OBU in the
// temporal unit started by that delimiter.
absl::Status ValidateNoMetadataBeforeTemporalDelimiter(
    const std::optional<TemporalDelimiterObu>& current_temporal_delimiter,
    const std::list<MetadataObu>& current_metadata_obus,
    const std::list<MetadataObu>& next_metadata_obus) {
  if ((!current_temporal_delimiter.has_value() &&
       !current_metadata_obus.empty()) ||
      !next_metadata_obus.empty()) {
    return absl::InvalidArgumentError(
        "Metadata OBU must be placed after the Temporal Delimiter OBU and "
        "before the end of the Temporal Unit.");
  }
  return absl::OkStatus();
}

// Validates that Metadata OBUs are followed by Audio Frame OBUs in the current
// temporal unit, and that no trailing Metadata OBUs remain at the end of the
// sequence.
absl::Status ValidateMetadataFollowedByAudioFrames(
    const std::list<MetadataObu>& current_metadata_obus,
    const std::list<AudioFrameWithData>& current_audio_frames,
    const std::list<MetadataObu>& next_metadata_obus, bool is_end_of_sequence) {
  if ((!current_metadata_obus.empty() && current_audio_frames.empty()) ||
      (is_end_of_sequence && !next_metadata_obus.empty())) {
    return absl::InvalidArgumentError(
        "Metadata OBU must not be the last OBU of a Temporal Unit and must "
        "be followed by Audio Frame OBUs.");
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<TemporalUnitProcessor> TemporalUnitProcessor::Create(
    const DescriptorObus::AudioElementsById& audio_elements,
    const DescriptorObus::MixPresentationObus& mix_presentation_obus,
    ReadBitBuffer* absl_nonnull read_bit_buffer) {
  RETURN_IF_NOT_OK(ValidateNotNull(read_bit_buffer, "read_bit_buffer"));

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants;
  RETURN_IF_NOT_OK(CollectAndValidateParamDefinitions(
      audio_elements, mix_presentation_obus, param_definition_variants));

  absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element;
  absl::flat_hash_set<DecodedUleb128> audio_element_ids;
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    audio_element_ids.insert(audio_element_id);
    for (const auto& [substream_id, unused_labels] :
         audio_element_with_data.substream_id_to_labels) {
      auto [unused_iter, inserted] = substream_id_to_audio_element.insert(
          {substream_id, &audio_element_with_data});
      if (!inserted) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Duplicated substream ID: ", substream_id,
            " associated with audio element ID: ", audio_element_id));
      }
    }
  }

  std::unique_ptr<GlobalTimingModule> global_timing_module =
      GlobalTimingModule::Create(audio_elements, param_definition_variants);
  if (global_timing_module == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to initialize the global timing module");
  }

  auto parameters_manager = ParametersManager::Create(audio_elements);
  if (!parameters_manager.ok()) {
    return parameters_manager.status();
  }
  ABSL_CHECK_NE(*parameters_manager, nullptr);

  return TemporalUnitProcessor(std::move(param_definition_variants),
                               std::move(substream_id_to_audio_element),
                               std::move(audio_element_ids),
                               std::move(global_timing_module),
                               *std::move(parameters_manager), read_bit_buffer);
}

absl::Status TemporalUnitProcessor::ProcessTemporalUnit(
    bool eos_is_end_of_sequence,
    std::optional<OutputTemporalUnit>& output_temporal_unit,
    bool& continue_processing) {
  // Various checks that should have been handled by the factory function.
  ABSL_CHECK(parameters_manager_ != nullptr);
  ABSL_CHECK(global_timing_module_ != nullptr);
  ABSL_CHECK(read_bit_buffer_ != nullptr);

  continue_processing = true;
  while (continue_processing) {
    std::optional<AudioFrameWithData> audio_frame_with_data;
    std::optional<ParameterBlockWithData> parameter_block_with_data;
    std::optional<TemporalDelimiterObu> temporal_delimiter;
    std::optional<MetadataObu> metadata_obu;
    RETURN_IF_NOT_OK(ProcessTemporalUnitObu(
        audio_element_ids_, substream_id_to_audio_element_,
        param_definition_variants_, *parameters_manager_, *read_bit_buffer_,
        *global_timing_module_, audio_frame_with_data,
        parameter_block_with_data, temporal_delimiter, metadata_obu,
        continue_processing));

    // Collect OBUs into a temporal unit.
    bool delimiter_end_condition = false;
    if (audio_frame_with_data.has_value()) {
      TemporalUnitData::AddDataToCorrectTemporalUnit(
          current_temporal_unit_, next_temporal_unit_,
          *std::move(audio_frame_with_data));
      RETURN_IF_NOT_OK(ValidateNoMetadataBetweenAudioFrames(
          next_temporal_unit_.metadata_obus, next_temporal_unit_.timestamp));
    } else if (parameter_block_with_data.has_value()) {
      TemporalUnitData::AddDataToCorrectTemporalUnit(
          current_temporal_unit_, next_temporal_unit_,
          *std::move(parameter_block_with_data));
    } else if (metadata_obu.has_value()) {
      AddMetadataObuToCorrectTemporalUnit(current_temporal_unit_.audio_frames,
                                          current_temporal_unit_.metadata_obus,
                                          next_temporal_unit_.metadata_obus,
                                          *std::move(metadata_obu));
    } else if (temporal_delimiter.has_value()) {
      RETURN_IF_NOT_OK(ValidateNoMetadataBeforeTemporalDelimiter(
          current_temporal_unit_.temporal_delimiter,
          current_temporal_unit_.metadata_obus,
          next_temporal_unit_.metadata_obus));
      if (current_temporal_unit_.temporal_delimiter.has_value()) {
        delimiter_end_condition = true;
        next_temporal_unit_.temporal_delimiter = *temporal_delimiter;
      } else {
        current_temporal_unit_.temporal_delimiter = *temporal_delimiter;
      }
    }

    // The current temporal unit is considered finished if any of the
    // following conditions is met:
    // - The end of sequence is reached.
    // - The timestamp has advanced (i.e. when the next temporal unit gets its
    //   timestamp).
    // - A second temporal delimiter is encountered.
    const bool is_end_of_sequence =
        !continue_processing && eos_is_end_of_sequence;
    if (is_end_of_sequence || next_temporal_unit_.timestamp.has_value() ||
        delimiter_end_condition) {
      RETURN_IF_NOT_OK(ValidateMetadataFollowedByAudioFrames(
          current_temporal_unit_.metadata_obus,
          current_temporal_unit_.audio_frames,
          next_temporal_unit_.metadata_obus, is_end_of_sequence));
      if (current_temporal_unit_.audio_frames.empty() &&
          current_temporal_unit_.parameter_blocks.empty()) {
        break;
      }
      output_temporal_unit = OutputTemporalUnit();
      output_temporal_unit->output_audio_frames =
          std::move(current_temporal_unit_.audio_frames);
      output_temporal_unit->output_parameter_blocks =
          std::move(current_temporal_unit_.parameter_blocks);
      output_temporal_unit->output_metadata_obus =
          std::move(current_temporal_unit_.metadata_obus);
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

}  // namespace iamf_tools
