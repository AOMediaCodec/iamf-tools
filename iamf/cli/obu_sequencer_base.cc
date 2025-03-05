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

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/profile_filter.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/leb_generator.h"
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

// Write buffer. Let's start with 64 KB. The buffer will resize for larger
// OBUs if needed.
constexpr int64_t kBufferStartSize = 65536;

/*!\brief Map of start timestamp -> OBUs in that temporal unit.
 *
 * Map of temporal unit start time -> OBUs that overlap this temporal unit.
 * Using absl::btree_map for convenience as this allows iterating by
 * timestamp (which is the key).
 */
typedef absl::btree_map<int32_t, TemporalUnitView> TemporalUnitMap;

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
// Some IA Sequences can be "trivial" and missing descriptor OBUs or audio
// frames. These would decode to an empty stream. Fallback to some reasonable,
// but arbitrary default values, when the true value is undefined.

// Fallback sample rate when there are no Codec Config OBUs.
constexpr uint32_t kFallbackSampleRate = 48000;
// Fallback bit-depth when there are no Codec Config OBUs.
constexpr uint8_t kFallbackBitDepth = 16;
// Fallback number of channels when there are no audio elements.
constexpr uint32_t kFallbackNumChannels = 2;
// Fallback first PTS when there are no audio frames.
constexpr int64_t kFallbackFirstPts = 0;

// Gets the sum of the number of channels for the given audio elements. Or falls
// back to a default value if there are no audio elements.
int32_t GetNumberOfChannels(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements) {
  if (audio_elements.empty()) {
    // The muxer fails if we return the true value (0 channels).
    return kFallbackNumChannels;
  }

  int32_t num_channels = 0;
  for (const auto& [audio_element_id, audio_element] : audio_elements) {
    // Add the number of channels for every substream in every audio element.
    for (const auto& [substream_id, labels] :
         audio_element.substream_id_to_labels) {
      num_channels += static_cast<int32_t>(labels.size());
    }
  }
  return num_channels;
}

// Gets the first Presentation Timestamp (PTS); the timestamp of the first
// sample that is not trimmed. Or zero of there are no untrimmed samples.
absl::StatusOr<int64_t> GetFirstUntrimmedTimestamp(
    const TemporalUnitMap& temporal_unit_map) {
  if (temporal_unit_map.empty()) {
    return kFallbackFirstPts;
  }

  std::optional<int64_t> first_untrimmed_timestamp;
  for (const auto& [start_timestamp, temporal_unit] : temporal_unit_map) {
    if (temporal_unit.num_untrimmed_samples_ == 0) {
      // Fully trimmed frame. Wait for more.
      continue;
    }
    if (temporal_unit.num_samples_to_trim_at_start_ > 0 &&
        first_untrimmed_timestamp.has_value()) {
      return absl::InvalidArgumentError(
          "Temporal units must not have samples trimmed from the start, after "
          "the first untrimmed sample.");
    }

    // Found the first untrimmed sample. Get the timestamp. We only continue
    // looping to check that no more temporal units have samples trimmed from
    // the start, after the first untrimmed sample.
    first_untrimmed_timestamp =
        start_timestamp + temporal_unit.num_samples_to_trim_at_start_;
  }

  return first_untrimmed_timestamp.has_value() ? *first_untrimmed_timestamp
                                               : kFallbackFirstPts;
}

// Gets the common sample rate and bit depth for the given codec config OBUs. Or
// falls back to default values if there are no codec configs.
absl::Status GetCommonSampleRateAndBitDepth(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling) {
  if (codec_config_obus.empty()) {
    // The true value is undefined, but the muxer requires non-zero values.
    common_sample_rate = kFallbackSampleRate;
    common_bit_depth = kFallbackBitDepth;
    requires_resampling = false;
    return absl::OkStatus();
  }

  requires_resampling = false;
  absl::flat_hash_set<uint32_t> sample_rates;
  absl::flat_hash_set<uint8_t> bit_depths;
  for (const auto& [unused_id, obu] : codec_config_obus) {
    sample_rates.insert(obu.GetOutputSampleRate());
    bit_depths.insert(obu.GetBitDepthToMeasureLoudness());
  }

  return ::iamf_tools::GetCommonSampleRateAndBitDepth(
      sample_rates, bit_depths, common_sample_rate, common_bit_depth,
      requires_resampling);
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

absl::Status GenerateTemporalUnitMap(
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
}  // namespace

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

ObuSequencerBase::ObuSequencerBase(
    const LebGenerator& leb_generator, bool include_temporal_delimiters,
    bool delay_descriptors_until_first_untrimmed_sample)
    : leb_generator_(leb_generator),
      delay_descriptors_until_first_untrimmed_sample_(
          delay_descriptors_until_first_untrimmed_sample),
      include_temporal_delimiters_(include_temporal_delimiters) {}

ObuSequencerBase::~ObuSequencerBase() {};

absl::Status ObuSequencerBase::PickAndPlace(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  switch (state_) {
    case kInitialized:
      break;
    case kFlushed:
      return absl::FailedPreconditionError(
          "`PickAndPlace` should only be called once per instance.");
  }

  uint32_t common_sample_rate;
  uint8_t common_bit_depth;
  bool requires_resampling;
  RETURN_IF_NOT_OK(
      GetCommonSampleRateAndBitDepth(codec_config_obus, common_sample_rate,
                                     common_bit_depth, requires_resampling));
  if (requires_resampling) {
    return absl::UnimplementedError(
        "Codec Config OBUs with different bit-depths and/or sample "
        "rates are not in base-enhanced/base/simple profile; they are not "
        "allowed in ISOBMFF.");
  }

  // This assumes all Codec Configs have the same sample rate and frame size.
  // We may need to be more careful if IA Samples do not all (except the
  // final) have the same duration in the future.
  uint32_t common_samples_per_frame = 0;
  RETURN_IF_NOT_OK(
      GetCommonSamplesPerFrame(codec_config_obus, common_samples_per_frame));

  // Write the descriptor OBUs.
  WriteBitBuffer wb(kBufferStartSize, leb_generator_);

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookBeforeDescriptors, arbitrary_obus, wb));
  // Write out the descriptor OBUs.
  RETURN_IF_NOT_OK(ObuSequencerBase::WriteDescriptorObus(
      ia_sequence_header_obu, codec_config_obus, audio_elements,
      mix_presentation_obus, arbitrary_obus, wb));
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterDescriptors, arbitrary_obus, wb));

  TemporalUnitMap temporal_unit_map;
  RETURN_IF_NOT_OK(GenerateTemporalUnitMap(audio_frames, parameter_blocks,
                                           arbitrary_obus, temporal_unit_map));

  // If `delay_descriptors_until_first_untrimmed_sample` is true, then concrete
  // class needs `first_untrimmed_timestamp`. Otherwise, it would cause an
  // unnecessary delay, because the PTS cannot be determined until the first
  // untrimmed sample is received.
  std::optional<int64_t> first_untrimmed_timestamp;
  if (delay_descriptors_until_first_untrimmed_sample_) {
    // TODO(b/397637224): When this class can be used iteratively, we need to
    //                    determine the first PTS from the initial audio frames
    //                    only.
    const auto temp_first_untrimmed_timestamp =
        GetFirstUntrimmedTimestamp(temporal_unit_map);
    if (!temp_first_untrimmed_timestamp.ok()) {
      return temp_first_untrimmed_timestamp.status();
    }
    first_untrimmed_timestamp = *temp_first_untrimmed_timestamp;
  }

  int64_t cumulative_num_samples_for_logging = 0;
  int64_t num_temporal_units_for_logging = 0;
  const auto wrote_ia_sequence = [&]() -> absl::Status {
    RETURN_IF_NOT_OK(PushSerializedDescriptorObus(
        common_samples_per_frame, common_sample_rate, common_bit_depth,
        first_untrimmed_timestamp, GetNumberOfChannels(audio_elements),
        absl::MakeConstSpan(wb.bit_buffer())));
    wb.Reset();

    for (const auto& [timestamp, temporal_unit] : temporal_unit_map) {
      // Write the IA Sample to a `MediaSample`.
      int num_samples = 0;

      RETURN_IF_NOT_OK(WriteTemporalUnit(include_temporal_delimiters_,
                                         temporal_unit, wb, num_samples));
      RETURN_IF_NOT_OK(PushSerializedTemporalUnit(
          static_cast<int64_t>(timestamp), num_samples, wb.bit_buffer()));

      cumulative_num_samples_for_logging += num_samples;
      num_temporal_units_for_logging++;
      wb.Reset();
    }
    return absl::OkStatus();
  }();
  if (!wrote_ia_sequence.ok()) {
    // Something failed when writing the IA Sequence. Signal to clean up the
    // output, such as removing a bad file.
    Abort();
    return wrote_ia_sequence;
  }

  LOG(INFO) << "Wrote " << num_temporal_units_for_logging
            << " temporal units with a total of "
            << cumulative_num_samples_for_logging
            << " samples excluding padding.";

  Flush();
  state_ = kFlushed;
  return absl::OkStatus();
}

}  // namespace iamf_tools
