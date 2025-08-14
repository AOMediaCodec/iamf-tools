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

#include <list>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class DescriptorObuParser {
 public:
  struct ParsedDescriptorObus {
    // IA sequence header processed from the bitstream.
    IASequenceHeaderObu sequence_header;
    // Map of Codec Config OBUs processed from the bitstream.
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
    // Map of Audio Elements and metadata processed from the bitstream.
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
        audio_elements_with_data;
    // List of Mix Presentation OBUs processed from the bitstream.
    std::list<MixPresentationObu> mix_presentation_obus;
  };

  /*!\brief Processes the Descriptor OBUs of an IA Sequence.
   *
   * If insufficient data to process all descriptor OBUs is provided, a failing
   * status will be returned. `output_insufficient_data` will be set to true and
   * the read_bit_buffer will not be consumed. A user should call this function
   * again after providing more data within the read_bit_buffer.
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Buffer containing a portion of an iamf bitstream
   *        containing a sequence of OBUs. The buffer will be consumed up to the
   *        end of the descriptor OBUs if processing is successful.
   * \param output_insufficient_data Whether the bitstream provided is
   *        insufficient to process all descriptor OBUs.
   * \return `ParsedDescriptorObus` if the process is successful. A specific
   *        status on failure.
   */
  static absl::StatusOr<ParsedDescriptorObus> ProcessDescriptorObus(
      bool is_exhaustive_and_exact, ReadBitBuffer& read_bit_buffer,
      bool& output_insufficient_data);
};

}  // namespace iamf_tools

#endif  // CLI_DESCRIPTOR_OBU_PARSER_H_
