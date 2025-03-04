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
#ifndef CLI_OBU_SEQUENCER_BASE_H_
#define CLI_OBU_SEQUENCER_BASE_H_

#include <cstdint>
#include <list>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Map of start timestamp -> OBUs in that temporal unit.*/
typedef absl::btree_map<int32_t, TemporalUnitView> TemporalUnitMap;

class ObuSequencerBase {
 public:
  /*!\brief Constructor.
   *
   * \param leb_generator Leb generator to use when writing OBUs.
   */
  ObuSequencerBase(const LebGenerator& leb_generator)
      : leb_generator_(leb_generator) {}

  /*!\brief Destructor.*/
  virtual ~ObuSequencerBase() = default;

  /*!\brief Generates a map of start timestamp -> OBUs in that temporal unit.
   *
   * \param audio_frames Data about Audio Frame OBUs to write.
   * \param parameter_blocks Data about Parameter Block OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \param temporal_unit_map Output map of start timestamp -> `TemporalUnit`.
   *        The `TemporalUnit` has the `audio_frames` vector with all audio
   *        frames starting at that timestamp.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  [[deprecated(
      "Process one temporal unit at a time with `TemporalUnitView::Create`")]]
  static absl::Status GenerateTemporalUnitMap(
      const std::list<AudioFrameWithData>& audio_frames,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      const std::list<ArbitraryObu>& arbitrary_obus,
      TemporalUnitMap& temporal_unit_map);

  /*!\brief Serializes and writes out a temporal unit.
   *
   * Write out the OBUs contained within the input arguments to the output write
   * buffer.
   *
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param temporal_unit Temporal unit to write out.
   * \param wb Write buffer to write to.
   * \param num_samples Number of samples written out.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status WriteTemporalUnit(bool include_temporal_delimiters,
                                        const TemporalUnitView& temporal_unit,
                                        WriteBitBuffer& wb, int& num_samples);

  /*!\brief Writes the input descriptor OBUs.
   *
   * Write out the OBUs contained within the input arguments to the output write
   * buffer.
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU to write.
   * \param codec_config_obus Codec Config OBUs to write.
   * \param audio_elements Audio Element OBUs with data to write.
   * \param mix_presentation_obus Mix Presentation OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \param wb Write buffer to write to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status WriteDescriptorObus(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const std::list<MixPresentationObu>& mix_presentation_obus,
      const std::list<ArbitraryObu>& arbitrary_obus, WriteBitBuffer& wb);

  /*!\brief Pick and place OBUs and write to some output.
   *
   * \param ia_sequence_header_obu IA Sequence Header OBU to write.
   * \param codec_config_obus Codec Config OBUs to write.
   * \param audio_elements Audio Element OBUs with data to write.
   * \param mix_presentation_obus Mix Presentation OBUs to write.
   * \param audio_frames Data about Audio Frame OBUs to write.
   * \param parameter_blocks Data about Parameter Block OBUs to write.
   * \param arbitrary_obus Arbitrary OBUs to write.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  virtual absl::Status PickAndPlace(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const std::list<MixPresentationObu>& mix_presentation_obus,
      const std::list<AudioFrameWithData>& audio_frames,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      const std::list<ArbitraryObu>& arbitrary_obus) = 0;

 protected:
  const LebGenerator leb_generator_;
};

}  // namespace iamf_tools

#endif  // CLI_OBU_SEQUENCER_H_
