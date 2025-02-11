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

#ifndef CLI_PROTO_CONVERSION_DOWNMIXING_RECONSTRUCTION_UTIL_H_
#define CLI_PROTO_CONVERSION_DOWNMIXING_RECONSTRUCTION_UTIL_H_

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Creates a map of ID to DownmixingAndReconstructionConfig.
 *
 * \param user_metadata Proto UserMetadata, the source of ChannelLabels.
 * \param audio_elements AudioElements to source SubStreamIdsToLabels and
 *        LabelToOutputGains.
 * \return Map of Audio Element ID to DemixingMetadata on success. An error if
 *         any Audio Element ID is not found in `audio_elements`. An error if
 *         any labels fail to be converted.
 */
absl::StatusOr<absl::flat_hash_map<
    DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
CreateAudioElementIdToDemixingMetadata(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements);

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_DOWNMIXING_RECONSTRUCTION_UTIL_H_
