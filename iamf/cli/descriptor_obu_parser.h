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

#ifndef CLI_DESCRIPTOR_OBU_PARSER_H_
#define CLI_DESCRIPTOR_OBU_PARSER_H_

#include <cstdint>
#include <list>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class DescriptorObuParser {
 public:
  /*!\brief Processes the Descriptor OBUs of an IA Sequence.
   *
   * If insufficient data to process all descriptor OBUs is provided, a failing
   * status will be returned. `output_insufficient_data` will be set to true,
   * the read_bit_buffer will not be consumed, and the output parameters will
   * not be populated. A user should call this function again after providing
   * more data within the read_bit_buffer.
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Buffer containing a portion of an iamf bitstream
   *        containing a sequence of OBUs. The buffer will be consumed up to the
   *        end of the descriptor OBUs if processing is successful.
   * \param output_sequence_header IA sequence header processed from the
   *        bitstream.
   * \param output_codec_config_obus Map of Codec Config OBUs processed from the
   *        bitstream.
   * \param output_audio_elements_with_data Map of Audio Elements and metadata
   *        processed from the bitstream.
   * \param output_mix_presentation_obus List of Mix Presentation OBUs processed
   *        from the bitstream.
   * \param output_insufficient_data Whether the bitstream provided is
   *        insufficient to process all descriptor OBUs.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  static absl::Status ProcessDescriptorObus(
      bool is_exhaustive_and_exact, ReadBitBuffer& read_bit_buffer,
      IASequenceHeaderObu& output_sequence_header,
      absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
          output_codec_config_obus,
      absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          output_audio_elements_with_data,
      std::list<MixPresentationObu>& output_mix_presentation_obus,
      bool& output_insufficient_data);
};

}  // namespace iamf_tools

#endif  // CLI_DESCRIPTOR_OBU_PARSER_H_
