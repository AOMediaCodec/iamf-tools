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
#include "iamf/cli/obu_sequencer_base.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/profile_filter.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

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

absl::Status WriteObusWithHook(
    ArbitraryObu::InsertionHook insertion_hook,
    const std::vector<const ArbitraryObu*>& arbitrary_obus,
    WriteBitBuffer& wb) {
  for (const auto& arbitrary_obu : arbitrary_obus) {
    if (arbitrary_obu->insertion_hook_ == insertion_hook) {
      RETURN_IF_NOT_OK(arbitrary_obu->ValidateAndWriteObu(wb));
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status ObuSequencerBase::GenerateTemporalUnitMap(
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<ArbitraryObu>& arbitrary_obus,
    TemporalUnitMap& temporal_unit_map) {
  // Initially, guess the temporal units by the start time. Deeper validation
  // and sanitization occurs when creating the TemporalUnitView.
  struct UnsanitizedTemporalUnit {
    std::vector<const ParameterBlockWithData*> parameter_blocks;
    std::vector<const AudioFrameWithData*> audio_frames;
    std::vector<const ArbitraryObu*> arbitrary_obus;
  };
  typedef absl::flat_hash_map<InternalTimestamp, UnsanitizedTemporalUnit>
      UnsanitizedTemporalUnitMap;
  UnsanitizedTemporalUnitMap unsanitized_temporal_unit_map;

  for (const auto& parameter_block : parameter_blocks) {
    unsanitized_temporal_unit_map[parameter_block.start_timestamp]
        .parameter_blocks.push_back(&parameter_block);
  }
  for (auto& audio_frame : audio_frames) {
    unsanitized_temporal_unit_map[audio_frame.start_timestamp]
        .audio_frames.push_back(&audio_frame);
  }
  for (const auto& arbitrary_obu : arbitrary_obus) {
    if (arbitrary_obu.insertion_tick_ == std::nullopt) {
      continue;
    }
    unsanitized_temporal_unit_map[*arbitrary_obu.insertion_tick_]
        .arbitrary_obus.push_back(&arbitrary_obu);
  }
  // Sanitize and build a map on the sanitized temporal units.
  for (const auto& [timestamp, unsanitized_temporal_unit] :
       unsanitized_temporal_unit_map) {
    auto temporal_unit_view = TemporalUnitView::CreateFromPointers(
        unsanitized_temporal_unit.parameter_blocks,
        unsanitized_temporal_unit.audio_frames,
        unsanitized_temporal_unit.arbitrary_obus);
    if (!temporal_unit_view.ok()) {
      return temporal_unit_view.status();
    }
    temporal_unit_map.emplace(timestamp, *std::move(temporal_unit_view));
  }

  return absl::OkStatus();
}

absl::Status ObuSequencerBase::WriteTemporalUnit(
    bool include_temporal_delimiters, const TemporalUnitView& temporal_unit,
    WriteBitBuffer& wb, int& num_samples) {
  num_samples += temporal_unit.num_untrimmed_samples_;

  if (include_temporal_delimiters) {
    // Temporal delimiter has no payload.
    const TemporalDelimiterObu obu((ObuHeader()));
    RETURN_IF_NOT_OK(obu.ValidateAndWriteObu(wb));
  }

  RETURN_IF_NOT_OK(
      WriteObusWithHook(ArbitraryObu::kInsertionHookBeforeParameterBlocksAtTick,
                        temporal_unit.arbitrary_obus_, wb));

  // Write the Parameter Block OBUs.
  for (const auto& parameter_blocks : temporal_unit.parameter_blocks_) {
    const auto& parameter_block = parameter_blocks;
    RETURN_IF_NOT_OK(parameter_block->obu->ValidateAndWriteObu(wb));
  }

  RETURN_IF_NOT_OK(
      WriteObusWithHook(ArbitraryObu::kInsertionHookAfterParameterBlocksAtTick,
                        temporal_unit.arbitrary_obus_, wb));

  // Write Audio Frame OBUs.
  for (const auto& audio_frame : temporal_unit.audio_frames_) {
    RETURN_IF_NOT_OK(audio_frame->obu.ValidateAndWriteObu(wb));
    LOG_FIRST_N(INFO, 10) << "wb.bit_offset= " << wb.bit_offset()
                          << " after Audio Frame";
  }

  RETURN_IF_NOT_OK(
      WriteObusWithHook(ArbitraryObu::kInsertionHookAfterAudioFramesAtTick,
                        temporal_unit.arbitrary_obus_, wb));

  if (!wb.IsByteAligned()) {
    return absl::InvalidArgumentError("Write buffer not byte-aligned");
  }

  return absl::OkStatus();
}

// Writes the descriptor OBUs. Section 5.1.1
// (https://aomediacodec.github.io/iamf/#standalone-descriptor-obus) orders the
// OBUs by type.
//
// For Codec Config OBUs and Audio Element OBUs, the order is arbitrary. For
// determinism this implementation orders them by ascending ID.
//
// For Mix Presentation OBUs, the order is the same as the original order.
// Because the original ordering may be used downstream when selecting the mix
// presentation
// (https://aomediacodec.github.io/iamf/#processing-mixpresentation-selection).
//
// For Arbitrary OBUs, they are inserted in an order implied by the insertion
// hook. Ties are broken by the original order, when multiple OBUs have the same
// hook.
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
  // TODO(b/332956880): Support customizing the ordering.
  const std::vector<uint32_t> codec_config_ids =
      SortedKeys(codec_config_obus, std::less<uint32_t>());
  for (const auto id : codec_config_ids) {
    RETURN_IF_NOT_OK(codec_config_obus.at(id).ValidateAndWriteObu(wb));
    LOG(INFO) << "wb.bit_offset= " << wb.bit_offset() << " after Codec Config";
  }

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterCodecConfigs, arbitrary_obus, wb));

  // Write Audio Element OBUs in ascending order of Audio Element IDs.
  // TODO(b/332956880): Support customizing the ordering.
  const std::vector<uint32_t> audio_element_ids =
      SortedKeys(audio_elements, std::less<uint32_t>());
  for (const auto id : audio_element_ids) {
    RETURN_IF_NOT_OK(audio_elements.at(id).obu.ValidateAndWriteObu(wb));
    LOG(INFO) << "wb.bit_offset= " << wb.bit_offset() << " after Audio Element";
  }

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterAudioElements, arbitrary_obus, wb));

  // TODO(b/269708630): Ensure at least one the profiles in the IA Sequence
  //                    Header supports all of the layers for scalable audio
  //                    elements.
  // Maintain the original order of Mix Presentation OBUs.
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    // Make sure the mix presentation is valid for at least one of the profiles
    // in the sequence header before writing it.
    absl::flat_hash_set<ProfileVersion> profile_version = {
        ia_sequence_header_obu.GetPrimaryProfile(),
        ia_sequence_header_obu.GetAdditionalProfile()};
    RETURN_IF_NOT_OK(ProfileFilter::FilterProfilesForMixPresentation(
        audio_elements, mix_presentation_obu, profile_version));

    RETURN_IF_NOT_OK(mix_presentation_obu.ValidateAndWriteObu(wb));
    LOG(INFO) << "wb.bit_offset= " << wb.bit_offset()
              << " after Mix Presentation";
  }
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterMixPresentations, arbitrary_obus, wb));

  return absl::OkStatus();
}

}  // namespace iamf_tools
