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
#include "iamf/cli/obu_sequencer.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/arbitrary_obu.h"
#include "iamf/audio_element.h"
#include "iamf/audio_frame.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/codec_config.h"
#include "iamf/ia.h"
#include "iamf/ia_sequence_header.h"
#include "iamf/mix_presentation.h"
#include "iamf/obu_header.h"
#include "iamf/parameter_block.h"
#include "iamf/temporal_delimiter.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

namespace {

template <typename KeyValueMap, typename KeyComparator>
std::vector<uint32_t> SortedKeys(const KeyValueMap& map,
                                 const KeyComparator& comparator) {
  std::vector<uint32_t> keys;
  keys.reserve(map.size());
  for (const auto& [key, value] : map) {
    keys.push_back(key);
  }
  std::sort(keys.begin(), keys.end(), comparator);
  return keys;
}

}  // namespace

absl::Status ObuSequencerBase::GenerateTemporalUnitMap(
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    TemporalUnitMap& temporal_unit_map) {
  // Put all audio frames into the map based on their start time.
  for (auto& audio_frame : audio_frames) {
    auto& temporal_unit_audio_frames =
        temporal_unit_map[audio_frame.start_timestamp].audio_frames;
    if (!temporal_unit_audio_frames.empty() &&
        temporal_unit_audio_frames.back()->end_timestamp !=
            audio_frame.end_timestamp) {
      LOG(ERROR)
          << "Temporal units must have the same start time and duration.";
      return absl::InvalidArgumentError("");
    }
    temporal_unit_audio_frames.push_back(&audio_frame);
  }

  // Sort within each temporal unit, first by Audio Element ID and then
  // by Audio Substream ID.
  auto compare_audio_element_id_audio_substream_id =
      [](const AudioFrameWithData* a, const AudioFrameWithData* b) {
        const auto audio_element_id_a =
            a->audio_element_with_data->obu.audio_element_id_;
        const auto audio_element_id_b =
            b->audio_element_with_data->obu.audio_element_id_;
        if (audio_element_id_a == audio_element_id_b) {
          return a->obu.GetSubstreamId() < b->obu.GetSubstreamId();
        } else {
          return audio_element_id_a < audio_element_id_b;
        }
      };

  for (auto& [unused_timestamp, temporal_unit] : temporal_unit_map) {
    std::sort(temporal_unit.audio_frames.begin(),
              temporal_unit.audio_frames.end(),
              compare_audio_element_id_audio_substream_id);
  }

  // Put all parameter blocks into every temporal unit they overlap.
  for (const auto& parameter_block : parameter_blocks) {
    // Get the start and end time of the parameter block.
    const int32_t obu_start_time = parameter_block.start_timestamp;
    const int32_t obu_end_time = parameter_block.end_timestamp;

    for (auto& temporal_unit : temporal_unit_map) {
      const int32_t temporal_unit_start = temporal_unit.first;
      if (temporal_unit.second.audio_frames.empty()) {
        // This should never happen.
        return absl::UnknownError("");
      }
      const int32_t temporal_unit_end =
          temporal_unit.second.audio_frames[0]->end_timestamp;

      // Check if the temporal unit starts or ends during the parameter block.
      if ((obu_start_time < temporal_unit_start &&
           temporal_unit_start < obu_end_time) ||
          (temporal_unit_start <= obu_start_time &&
           obu_start_time < temporal_unit_end)) {
        temporal_unit.second.parameter_blocks.push_back(&parameter_block);
      }
    }
  }

  // Sort within each temporal unit by Parameter ID.
  // TODO(b/302470464): Test the output Parameter Blocks are in order.
  auto compare_parameter_id = [](const ParameterBlockWithData* a,
                                 const ParameterBlockWithData* b) {
    return a->obu->parameter_id_ < b->obu->parameter_id_;
  };

  for (auto& [unused_timestamp, temporal_unit] : temporal_unit_map) {
    std::sort(temporal_unit.parameter_blocks.begin(),
              temporal_unit.parameter_blocks.end(), compare_parameter_id);
  }

  return absl::OkStatus();
}

absl::Status ObuSequencerBase::WriteTemporalUnit(
    bool include_temporal_delimiters, const TemporalUnit& temporal_unit,
    WriteBitBuffer& wb, int& num_samples) {
  if (temporal_unit.audio_frames.empty()) {
    LOG(ERROR) << "Every temporal unit must have an audio frame.";
    return absl::UnknownError("");
  }

  num_samples +=
      (temporal_unit.audio_frames[0]->raw_samples.size() -
       (temporal_unit.audio_frames[0]
            ->obu.header_.num_samples_to_trim_at_start +
        temporal_unit.audio_frames[0]->obu.header_.num_samples_to_trim_at_end));

  if (include_temporal_delimiters) {
    // Temporal delimiter has no payload.
    const TemporalDelimiterObu obu((ObuHeader()));
    RETURN_IF_NOT_OK(obu.ValidateAndWriteObu(wb));
  }

  // Write the Parameter Block OBUs.
  for (const auto& parameter_blocks : temporal_unit.parameter_blocks) {
    const auto& parameter_block = parameter_blocks;
    RETURN_IF_NOT_OK(parameter_block->obu->ValidateAndWriteObu(wb));
  }

  // Write Audio Frame OBUs.
  for (const auto& audio_frame : temporal_unit.audio_frames) {
    RETURN_IF_NOT_OK(audio_frame->obu.ValidateAndWriteObu(wb));
    LOG_FIRST_N(INFO, 10) << "wb.bit_offset= " << wb.bit_offset()
                          << " after Audio Frame";
  }

  if (!wb.IsByteAligned()) {
    LOG(INFO) << "Write buffer not byte-aligned";
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

absl::Status ObuSequencerBase::WriteDescriptorObus(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    const std::list<ArbitraryObu>& arbitrary_obus, WriteBitBuffer& wb) {
  // Write IA Sequence Header OBU.
  RETURN_IF_NOT_OK(ia_sequence_header_obu.ValidateAndWriteObu(wb));
  LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
            << " after IA Sequence Header";

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterIaSequenceHeader, arbitrary_obus, wb));

  // Write Codec Config OBUs in ascending order of Codec Config IDs.
  // TODO(b/302384688): Support a different ordering.
  const std::vector<uint32_t> codec_config_ids =
      SortedKeys(codec_config_obus, std::less<uint32_t>());
  for (const auto id : codec_config_ids) {
    RETURN_IF_NOT_OK(codec_config_obus.at(id).ValidateAndWriteObu(wb));
    LOG(INFO) << "wb.bit_offset= " << wb.bit_offset() << " after Codec Config";
  }

  // Write Audio Element OBUs in ascending order of Audio Element IDs.
  // TODO(b/302384688): Support a different ordering.
  const std::vector<uint32_t> audio_element_ids =
      SortedKeys(audio_elements, std::less<uint32_t>());
  for (const auto id : audio_element_ids) {
    RETURN_IF_NOT_OK(audio_elements.at(id).obu.ValidateAndWriteObu(wb));
    LOG(INFO) << "wb.bit_offset= " << wb.bit_offset() << " after Audio Element";
  }

  // Write Mix Presentation OBUs in ascending order of Mix Presentation IDs.
  // TODO(b/302384688): Support a different ordering.
  std::list<MixPresentationObu> sorted_mix_presentation_obus(
      mix_presentation_obus);
  sorted_mix_presentation_obus.sort(
      [](const MixPresentationObu& obu_1, const MixPresentationObu& obu_2) {
        return obu_1.mix_presentation_id_ < obu_2.mix_presentation_id_;
      });
  for (const auto& mix_presentation_obu : sorted_mix_presentation_obus) {
    RETURN_IF_NOT_OK(mix_presentation_obu.ValidateAndWriteObu(wb));
    LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
              << " after Mix Presentation";
  }

  // TODO(b/274065471): Check that the number of descriptor OBUs is allowed by
  //                    the current profile version.
  return absl::OkStatus();
}

absl::Status ObuSequencerIamf::PickAndPlace(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  // Write buffer. Let's start with 64 KB. The buffer will resize for larger
  // OBUs if needed.
  static const int64_t kBufferSize = 65536;
  WriteBitBuffer wb(kBufferSize, leb_generator_);

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookBeforeDescriptors, arbitrary_obus, wb));
  // Write out the descriptor OBUs.
  RETURN_IF_NOT_OK(ObuSequencerBase::WriteDescriptorObus(
      ia_sequence_header_obu, codec_config_obus, audio_elements,
      mix_presentation_obus, arbitrary_obus, wb));
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterDescriptors, arbitrary_obus, wb));

  // Map of temporal unit start time -> OBUs that overlap this temporal unit.
  // Using absl::btree_map for convenience as this allows iterating by
  // timestamp (which is the key).
  TemporalUnitMap temporal_unit_map;
  RETURN_IF_NOT_OK(ObuSequencerBase::GenerateTemporalUnitMap(
      audio_frames, parameter_blocks, temporal_unit_map));

  // Write all Audio Frame and Parameter Block OBUs ordered by temporal unit.
  int num_samples = 0;
  for (const auto& temporal_unit : temporal_unit_map) {
    // The temporal units will typically be the largest part of an IAMF
    // sequence. Occasionally flush to buffer to avoid keeping it all in memory.
    RETURN_IF_NOT_OK(wb.MaybeFlushIfCloseToCapacity(output_iamf_));

    RETURN_IF_NOT_OK(ObuSequencerBase::WriteTemporalUnit(
        include_temporal_delimiters_, temporal_unit.second, wb, num_samples));
  }
  LOG(INFO) << "Wrote " << temporal_unit_map.size()
            << " temporal units with a total of " << num_samples
            << " samples excluding padding.";

  // Flush any unwritten bytes before exiting.
  RETURN_IF_NOT_OK(wb.FlushAndWriteToFile(output_iamf_));

  return absl::OkStatus();
}

}  // namespace iamf_tools
