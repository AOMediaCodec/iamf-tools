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

#ifndef CLI_ADM_TO_USER_METADATA_IAMF_AUDIO_FRAME_HANDLER_H_
#define CLI_ADM_TO_USER_METADATA_IAMF_AUDIO_FRAME_HANDLER_H_

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/proto/audio_frame.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

class AudioFrameHandler {
 public:
  /*\!brief Constructor.
   *
   * \param file_prefix Prefix for associated wav files.
   * \param num_samples_to_trim_at_end Common number of samples to trim at end
   *     for all audio frame metadata.
   */
  AudioFrameHandler(absl::string_view file_prefix,
                    int32_t num_samples_to_trim_at_end)
      : file_prefix_(file_prefix),
        num_samples_to_trim_at_end_(num_samples_to_trim_at_end) {};

  /*\!brief Populates a `AudioFrameMetadata`.
   *
   * \param audio_element_id ID of the associated audio element.
   * \param input_layout Input layout of the associated audio element.
   * \param file_suffix Suffix to include in the file name
   * \param audio_frame_metadata Data to populate.
   */
  absl::Status PopulateAudioFrameMetadata(
      absl::string_view file_suffix, int32_t audio_element_id,
      IamfInputLayout input_layout,
      iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_obu_metadata)
      const;

  const std::string file_prefix_;
  const int32_t num_samples_to_trim_at_end_;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_IAMF_AUDIO_FRAME_HANDLER_H_
