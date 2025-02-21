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
#ifndef CLI_OBU_SEQUENCER_IAMF_H_
#define CLI_OBU_SEQUENCER_IAMF_H_

#include <cstdint>
#include <list>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

class ObuSequencerIamf : public ObuSequencerBase {
 public:
  /*!\brief Constructor.
   * \param iamf_filename Name of the output standalone .iamf file or an empty
   *        string to disable output.
   * \param include_temporal_delimiters Whether the serialized data should
   *        include a temporal delimiter.
   * \param leb_generator Leb generator to use when writing OBUs.
   */
  ObuSequencerIamf(const std::string& iamf_filename,
                   bool include_temporal_delimiters,
                   const LebGenerator& leb_generator)
      : ObuSequencerBase(leb_generator),
        iamf_filename_(iamf_filename),
        include_temporal_delimiters_(include_temporal_delimiters) {}

  ~ObuSequencerIamf() override = default;

  /*!\brief Pick and place OBUs and write to the standalone .iamf file.
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
  absl::Status PickAndPlace(
      const IASequenceHeaderObu& ia_sequence_header_obu,
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const std::list<MixPresentationObu>& mix_presentation_obus,
      const std::list<AudioFrameWithData>& audio_frames,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      const std::list<ArbitraryObu>& arbitrary_obus) override;

 private:
  const std::string iamf_filename_;
  const bool include_temporal_delimiters_;
};

}  // namespace iamf_tools

#endif  // CLI_OBU_SEQUENCER_H_
