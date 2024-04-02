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

#ifndef CLI_ADM_TO_USER_METADATA_IAMF_AUDIO_ELEMENT_HANDLER_H_
#define CLI_ADM_TO_USER_METADATA_IAMF_AUDIO_ELEMENT_HANDLER_H_

#include <cstdint>

#include "absl/status/status.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/ia_sequence_header.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

class AudioElementHandler {
 public:
  AudioElementHandler() = default;

  /*\!brief Populates an `AudioElementObuMetadata`.
   *
   * \param audio_element_id Audio element ID.
   * \param input_layout Input layout of the audio element.
   * \param audio_element_obu_metadata Audio element OBU metadata.
   * \return `absl::OkStatus()` on success. A specific error code on failure.
   */
  absl::Status PopulateAudioElementMetadata(
      int32_t audio_element_id, IamfInputLayout input_layout,
      iamf_tools_cli_proto::AudioElementObuMetadata&
          audio_element_obu_metadata);

 private:
  int32_t audio_stream_id_counter_ = 0;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_IAMF_IA_SEQUENCE_HEADER_OBU_METADATA_HANDLER_H_
