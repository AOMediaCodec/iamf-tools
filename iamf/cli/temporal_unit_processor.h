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

#ifndef CLI_TEMPORAL_UNIT_PROCESSOR_H_
#define CLI_TEMPORAL_UNIT_PROCESSOR_H_

#include <list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/metadata_obu.h"
#include "iamf/obu/param_definitions/param_definition_variant.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Processes Temporal Unit OBUs from an IA Sequence bitstream. */
class TemporalUnitProcessor {
 public:
  /*!\brief Output of processing a Temporal Unit. */
  struct OutputTemporalUnit {
    std::list<AudioFrameWithData> output_audio_frames;
    std::list<ParameterBlockWithData> output_parameter_blocks;
    std::list<MetadataObu> output_metadata_obus;
    InternalTimestamp output_timestamp;
  };

  /*!\brief Creates a `TemporalUnitProcessor`.
   *
   * \param audio_elements Audio Element OBUs with data from the Descriptor
   *        OBUs. The elements must outlive the `TemporalUnitProcessor`.
   * \param mix_presentation_obus Mix Presentation OBUs from the Descriptor
   *        OBUs.
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream. Must outlive the `TemporalUnitProcessor`.
   * \return `TemporalUnitProcessor` on success, or a specific status on
   *         failure.
   */
  static absl::StatusOr<TemporalUnitProcessor> Create(
      const DescriptorObus::AudioElementsById& audio_elements,
      const DescriptorObus::MixPresentationObus& mix_presentation_obus,
      ReadBitBuffer* absl_nonnull read_bit_buffer);

  /*!\brief Move constructor. */
  TemporalUnitProcessor(TemporalUnitProcessor&&) = default;

  // TODO(b/379819959): Also handle Temporal Delimiter OBUs.
  /*!\brief Processes all OBUs from a Temporal Unit from the stored IA Sequence.
   *
   * \param eos_is_end_of_sequence Whether reaching the end of the stream
   *        should be considered as the end of the sequence, and therefore the
   *        end of the temporal unit.
   * \param output_temporal_unit Contains the data from the temporal unit that
   *        is processed.
   * \param continue_processing Whether the processing should be continued.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  absl::Status ProcessTemporalUnit(
      bool eos_is_end_of_sequence,
      std::optional<OutputTemporalUnit>& output_temporal_unit,
      bool& continue_processing);

 private:
  struct TemporalUnitData {
    std::list<ParameterBlockWithData> parameter_blocks;
    std::list<AudioFrameWithData> audio_frames;
    std::list<MetadataObu> metadata_obus;

    std::optional<TemporalDelimiterObu> temporal_delimiter;
    std::optional<InternalTimestamp> timestamp;

    bool Empty() const {
      return parameter_blocks.empty() && audio_frames.empty() &&
             metadata_obus.empty();
    }

    void Clear() {
      audio_frames.clear();
      parameter_blocks.clear();
      metadata_obus.clear();
      temporal_delimiter.reset();
      timestamp.reset();
    }

    template <class T>
    static void AddDataToCorrectTemporalUnit(
        TemporalUnitData& current_temporal_unit,
        TemporalUnitData& next_temporal_unit, T&& obu_with_data) {
      const InternalTimestamp new_timestamp = obu_with_data.start_timestamp;
      if (!current_temporal_unit.timestamp.has_value()) {
        current_temporal_unit.timestamp = new_timestamp;
      }
      if (*current_temporal_unit.timestamp == new_timestamp) {
        current_temporal_unit.GetList<T>().push_back(
            std::forward<T>(obu_with_data));
      } else {
        next_temporal_unit.GetList<T>().push_back(
            std::forward<T>(obu_with_data));
        next_temporal_unit.timestamp = new_timestamp;
      }
    }

   private:
    template <class T>
    std::list<T>& GetList() {
      if constexpr (std::is_same_v<T, ParameterBlockWithData>) {
        return parameter_blocks;
      } else if constexpr (std::is_same_v<T, AudioFrameWithData>) {
        return audio_frames;
      } else if constexpr (std::is_same_v<T, MetadataObu>) {
        return metadata_obus;
      }
    }
  };

  TemporalUnitProcessor(
      absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
          param_definition_variants,
      absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
          substream_id_to_audio_element,
      absl::flat_hash_set<DecodedUleb128> audio_element_ids,
      std::unique_ptr<GlobalTimingModule> absl_nonnull global_timing_module,
      std::unique_ptr<ParametersManager> absl_nonnull parameters_manager,
      ReadBitBuffer* absl_nonnull read_bit_buffer)
      : param_definition_variants_(std::move(param_definition_variants)),
        substream_id_to_audio_element_(
            std::move(substream_id_to_audio_element)),
        audio_element_ids_(std::move(audio_element_ids)),
        global_timing_module_(std::move(global_timing_module)),
        parameters_manager_(std::move(parameters_manager)),
        read_bit_buffer_(read_bit_buffer) {}

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants_;
  absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element_;
  absl::flat_hash_set<DecodedUleb128> audio_element_ids_;
  std::unique_ptr<GlobalTimingModule> global_timing_module_;
  std::unique_ptr<ParametersManager> parameters_manager_;
  ReadBitBuffer* absl_nonnull read_bit_buffer_;

  // Cached data when processing temporal units.
  TemporalUnitData current_temporal_unit_;
  TemporalUnitData next_temporal_unit_;
};

}  // namespace iamf_tools

#endif  // CLI_TEMPORAL_UNIT_PROCESSOR_H_
