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

#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<absl::flat_hash_map<
    DecodedUleb128, DemixingModule::DownmixingAndReconstructionConfig>>
CreateAudioElementIdToDemixingMetadata(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements) {
  absl::flat_hash_map<DecodedUleb128,
                      DemixingModule::DownmixingAndReconstructionConfig>
      result;
  // For each AudioFrameObuMetadata, we pull out the audio element ID, find
  // the matching AudioElementWithData, and convert the proto labels to internal
  // labels, and pair up the converted labels with `substream_id_to_labels` and
  // `label_to_output_gain` from the AudioElementWithData.
  for (const iamf_tools_cli_proto::AudioFrameObuMetadata&
           user_audio_frame_metadata : user_metadata.audio_frame_metadata()) {
    const auto audio_element_id = user_audio_frame_metadata.audio_element_id();
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>::const_iterator
        audio_element = audio_elements.find(audio_element_id);
    if (audio_element == audio_elements.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Audio Element ID= ", audio_element_id, " not found"));
    }
    absl::flat_hash_set<ChannelLabel::Label> user_channel_labels;
    RETURN_IF_NOT_OK(ChannelLabelUtils::SelectConvertAndFillLabels(
        user_audio_frame_metadata, user_channel_labels));
    const auto& audio_element_with_data = audio_element->second;
    result[audio_element_id] = {user_channel_labels,
                                audio_element_with_data.substream_id_to_labels,
                                audio_element_with_data.label_to_output_gain};
  }

  return result;
}

}  // namespace iamf_tools
