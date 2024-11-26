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

#ifndef CLI_USER_METADATA_BUILDER_AUDIO_ELEMENT_METADATA_BUILDER_H_
#define CLI_USER_METADATA_BUILDER_AUDIO_ELEMENT_METADATA_BUILDER_H_

#include <cstdint>

#include "absl/status/status.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"

namespace iamf_tools {

/*!\brief Helps create consistent audio element metadata for an IAMF stream.
 *
 * This class stores state information to avoid conflicts between audio
 * elements in a single IAMF stream. It helps generate audio streams that may
 * have multiple audio elements while ensuring they all have unique substream
 * IDs.
 *
 * `PopulateAudioElementMetadata()` will generate a single audio element
 * metadata. It can be called multiple times to generate additional audio
 * element metadata. The output audio elements are simplistically configured
 * based on the input layout.
 *
 * This class is intended to be used to generate simple audio elements for any
 * compatibility layers between non-IAMF formats and IAMF.
 */
class AudioElementMetadataBuilder {
 public:
  AudioElementMetadataBuilder() = default;

  /*!\brief Populates a simplistic `AudioElementObuMetadata`.
   *
   * The populated metadata will be based on the input layout, with various
   * settings (parameters, number of layers, etc.) set to simplistic default
   * values.
   *
   * \param codec_config_id Codec config ID.
   * \param audio_element_id Audio element ID.
   * \param input_layout Input layout of the audio element.
   * \param audio_element_obu_metadata Audio element OBU metadata.
   * \return `absl::OkStatus()` on success. A specific error code on failure.
   */
  absl::Status PopulateAudioElementMetadata(
      uint32_t audio_element_id, uint32_t codec_config_id,
      IamfInputLayout input_layout,
      iamf_tools_cli_proto::AudioElementObuMetadata&
          audio_element_obu_metadata);

 private:
  int64_t audio_stream_id_counter_ = 0;
};

}  // namespace iamf_tools

#endif  // CLI_USER_METADATA_BUILDER_AUDIO_ELEMENT_METADATA_BUILDER_H_
