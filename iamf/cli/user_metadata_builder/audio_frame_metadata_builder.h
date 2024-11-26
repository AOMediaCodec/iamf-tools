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

#ifndef CLI_USER_METADATA_BUILDER_AUDIO_FRAME_METADATA_BUILDER_H_
#define CLI_USER_METADATA_BUILDER_AUDIO_FRAME_METADATA_BUILDER_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"

namespace iamf_tools {

/*!\brief Helps create consistent audio frame metadatas for an IAMF stream.
 *
 * In `iamf-tools` this metadata is typically associated in a 1:1 mapping with
 * an audio element.
 *
 * `PopulateAudioFrameMetadata()` will generate a single audio frame metadata.
 * It can be called multiple times to generate additional audio frame
 * metadatas.
 *
 * The generated metadatas have a channel mapping consistent with an ITU-2051-3
 * layout.
 */
class AudioFrameMetadataBuilder {
 public:
  /*!\brief Populates a `AudioFrameMetadata`.
   *
   * \param wav_filename Name of the associated wav file.
   * \param audio_element_id ID of the associated audio element.
   * \param input_layout Input layout of the associated audio element.
   * \param file_suffix Suffix to include in the file name
   * \param audio_frame_metadata Data to populate.
   */
  static absl::Status PopulateAudioFrameMetadata(
      absl::string_view wav_filename, uint32_t audio_element_id,
      IamfInputLayout input_layout,
      iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_obu_metadata);
};

}  // namespace iamf_tools

#endif  // CLI_USER_METADATA_BUILDER_AUDIO_FRAME_METADATA_BUILDER_H_
