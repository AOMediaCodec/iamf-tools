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

#ifndef CLI_ADM_TO_USER_METADATA_IAMF_IAMF_H_
#define CLI_ADM_TO_USER_METADATA_IAMF_IAMF_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/adm_to_user_metadata/iamf/audio_element_handler.h"
#include "iamf/cli/adm_to_user_metadata/iamf/audio_frame_handler.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/adm_to_user_metadata/iamf/mix_presentation_handler.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Helps maintain consistency within an IAMF stream.
 *
 * This class holds the mapping between ADM objects and IAMF OBUs. It also holds
 * several handlers which help maintain consistency between particular types of
 * OBUs.
 */
class IAMF {
 public:
  struct AudioObjectsAndMetadata {
    std::vector<AudioObject> audio_objects;
    int32_t original_audio_programme_index;
  };

  /*!\brief Creates an `IAMF` object.
   *
   * \param file_prefix File prefix to use when naming output wav files.
   * \param adm ADM data to initialize with.
   * \param max_frame_duration_ms Maximum frame duration in milliseconds. The
   *        actual frame duration may be shorter due to rounding.
   * \param samples_per_sec Sample rate of the input audio files in Hertz.
   * \return `IAMF` object or a specific error code on failure.
   */
  static absl::StatusOr<IAMF> Create(absl::string_view file_prefix,
                                     const ADM& adm, int32_t frame_duration_ms,
                                     uint32_t samples_per_sec);

  const std::map<int32_t, AudioObjectsAndMetadata>
      mix_presentation_id_to_audio_objects_and_metadata_;
  const std::map<std::string, uint32_t> audio_object_to_audio_element_;

  const std::string file_name_prefix_;
  const int64_t num_samples_per_frame_;
  const std::vector<IamfInputLayout> input_layouts_;

  AudioElementHandler audio_element_handler_;
  const AudioFrameHandler audio_frame_handler_;
  MixPresentationHandler mix_presentation_handler_;

 private:
  /*!\brief Constructor.
   *
   * \param file_prefix File prefix to use when naming output wav files.
   * \param mix_presentation_id_to_audio_objects_and_metadata Map of mix
   *        presentation IDs to audio objects and metadata to initialize with.
   * \param audio_object_to_audio_element Map of audio object reference IDs to
   *        audio element IDs.
   * \param num_samples_per_frame Number of samples per frame.
   * \param samples_per_sec Sample rate of the input audio files in Hertz.
   * \param input_layouts Vector of iamf input layouts format ids.
   */
  IAMF(absl::string_view file_prefix,
       const std::map<int32_t, AudioObjectsAndMetadata>&
           mix_presentation_id_to_audio_objects_and_metadata,
       const std::map<std::string, uint32_t>& audio_object_to_audio_element,
       int64_t num_samples_per_frame, uint32_t samples_per_sec,
       const std::vector<IamfInputLayout>& input_layouts);
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_IAMF_IAMF_H_
