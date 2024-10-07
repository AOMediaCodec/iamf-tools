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

#ifndef CLI_ADM_TO_USER_METADATA_IAMF_MIX_PRESENTATION_HANDLER_H_
#define CLI_ADM_TO_USER_METADATA_IAMF_MIX_PRESENTATION_HANDLER_H_

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/proto/mix_presentation.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Helps create consistent mix presentation metadatas for an IAMF stream.
 *
 * This class stores information common between mix presentations in a single
 * IAMF stream.
 *
 * `PopulateMixPresentation()` will generate a single mix presentation metadata.
 * It can be called multiple times to generate additional mix presentation
 * metadatas.
 */
class MixPresentationHandler {
 public:
  /*!\brief Constructor.
   *
   * \param common_parameter_rate Common parameter rate for all generated OBUs.
   * \param audio_object_id_to_audio_element_id Mapping of audio object
   *        reference IDs to audio element IDs.
   */
  MixPresentationHandler(uint32_t common_parameter_rate,
                         const std::map<std::string, uint32_t>&
                             audio_object_id_to_audio_element_id)
      : common_parameter_rate_(common_parameter_rate),
        audio_object_id_to_audio_element_id_(
            audio_object_id_to_audio_element_id) {};

  /*!\brief Populates a `MixPresentationObuMetadata` proto.
   *
   * \param mix_presentation_id Mix presentation ID to generate.
   * \param audio_objects Audio objects for this mix presentation.
   * \param loudness_metadata Loudness metadata.
   * \param mix_presentation_obu_metadata Metadata to populate.
   * \return `absl::OkStatus()` on success. A specific error on failure.
   */
  absl::Status PopulateMixPresentation(
      int32_t mix_presentation_id,
      const std::vector<AudioObject>& audio_objects,
      const LoudnessMetadata& loudness_metadata,
      iamf_tools_cli_proto::MixPresentationObuMetadata&
          mix_presentation_obu_metadata);

 private:
  const uint32_t common_parameter_rate_;
  std::map<std::string, uint32_t> audio_object_id_to_audio_element_id_;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_IAMF_MIX_PRESENTATION_HANDLER_H_
