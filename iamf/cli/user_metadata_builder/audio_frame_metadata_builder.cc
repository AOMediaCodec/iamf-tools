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

#include "iamf/cli/user_metadata_builder/audio_frame_metadata_builder.h"

#include <cstdint>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"
#include "iamf/common/obu_util.h"

namespace iamf_tools {

namespace {

absl::StatusOr<std::vector<iamf_tools_cli_proto::ChannelLabel>>
LookupLabelsFromInputLayout(IamfInputLayout input_layout) {
  // Map which holds the channel labels corresponding to the loudspeaker
  // layout/ambisonics.
  using enum IamfInputLayout;
  using enum iamf_tools_cli_proto::ChannelLabel;
  static const absl::NoDestructor<absl::flat_hash_map<
      IamfInputLayout, std::vector<iamf_tools_cli_proto::ChannelLabel>>>
      kIamfInputLayoutToProtoLabels({
          {kMono, {CHANNEL_LABEL_MONO}},
          {kStereo, {CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2}},
          {k5_1,
           {CHANNEL_LABEL_L_5, CHANNEL_LABEL_R_5, CHANNEL_LABEL_CENTRE,
            CHANNEL_LABEL_LFE, CHANNEL_LABEL_LS_5, CHANNEL_LABEL_RS_5}},
          {k5_1_2,
           {CHANNEL_LABEL_L_5, CHANNEL_LABEL_R_5, CHANNEL_LABEL_CENTRE,
            CHANNEL_LABEL_LFE, CHANNEL_LABEL_LS_5, CHANNEL_LABEL_RS_5,
            CHANNEL_LABEL_LTF_2, CHANNEL_LABEL_RTF_2}},
          {k5_1_4,
           {CHANNEL_LABEL_L_5, CHANNEL_LABEL_R_5, CHANNEL_LABEL_CENTRE,
            CHANNEL_LABEL_LFE, CHANNEL_LABEL_LS_5, CHANNEL_LABEL_RS_5,
            CHANNEL_LABEL_LTF_4, CHANNEL_LABEL_RTF_4, CHANNEL_LABEL_LTB_4,
            CHANNEL_LABEL_RTB_4}},
          {k7_1,
           {CHANNEL_LABEL_L_7, CHANNEL_LABEL_R_7, CHANNEL_LABEL_CENTRE,
            CHANNEL_LABEL_LFE, CHANNEL_LABEL_LSS_7, CHANNEL_LABEL_RSS_7,
            CHANNEL_LABEL_LRS_7, CHANNEL_LABEL_RRS_7}},
          {k7_1_4,
           {CHANNEL_LABEL_L_7, CHANNEL_LABEL_R_7, CHANNEL_LABEL_CENTRE,
            CHANNEL_LABEL_LFE, CHANNEL_LABEL_LSS_7, CHANNEL_LABEL_RSS_7,
            CHANNEL_LABEL_LRS_7, CHANNEL_LABEL_RRS_7, CHANNEL_LABEL_LTF_4,
            CHANNEL_LABEL_RTF_4, CHANNEL_LABEL_LTB_4, CHANNEL_LABEL_RTB_4}},
          {kBinaural, {CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2}},
          {kAmbisonicsOrder1,
           {CHANNEL_LABEL_A_0, CHANNEL_LABEL_A_1, CHANNEL_LABEL_A_2,
            CHANNEL_LABEL_A_3}},
          {kAmbisonicsOrder2,
           {CHANNEL_LABEL_A_0, CHANNEL_LABEL_A_1, CHANNEL_LABEL_A_2,
            CHANNEL_LABEL_A_3, CHANNEL_LABEL_A_4, CHANNEL_LABEL_A_5,
            CHANNEL_LABEL_A_6, CHANNEL_LABEL_A_7, CHANNEL_LABEL_A_8}},
          {kAmbisonicsOrder3,
           {CHANNEL_LABEL_A_0, CHANNEL_LABEL_A_1, CHANNEL_LABEL_A_2,
            CHANNEL_LABEL_A_3, CHANNEL_LABEL_A_4, CHANNEL_LABEL_A_5,
            CHANNEL_LABEL_A_6, CHANNEL_LABEL_A_7, CHANNEL_LABEL_A_8,
            CHANNEL_LABEL_A_9, CHANNEL_LABEL_A_10, CHANNEL_LABEL_A_11,
            CHANNEL_LABEL_A_12, CHANNEL_LABEL_A_13, CHANNEL_LABEL_A_14,
            CHANNEL_LABEL_A_15}},
      });

  return LookupInMap(*kIamfInputLayoutToProtoLabels, input_layout,
                     "Proto-based channels labels for `IamfInputLayout`");
}

}  // namespace

// Sets the required textproto fields for audio_frame_metadata.
absl::Status AudioFrameMetadataBuilder::PopulateAudioFrameMetadata(
    absl::string_view wav_filename, uint32_t audio_element_id,
    IamfInputLayout input_layout,
    iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_obu_metadata) {
  audio_frame_obu_metadata.set_wav_filename(wav_filename);
  // Let the encoder determine how much codec delay and padding is required. We
  // just want to preserve the original audio.
  audio_frame_obu_metadata.set_samples_to_trim_at_start_includes_codec_delay(
      false);
  audio_frame_obu_metadata.set_samples_to_trim_at_end_includes_padding(false);
  audio_frame_obu_metadata.set_samples_to_trim_at_start(0);
  audio_frame_obu_metadata.set_samples_to_trim_at_end(0);
  audio_frame_obu_metadata.set_audio_element_id(audio_element_id);

  const auto& labels = LookupLabelsFromInputLayout(input_layout);
  if (!labels.ok()) {
    return labels.status();
  }
  int32_t channel_id = 0;
  for (const auto& label : *labels) {
    auto* channel_metadata = audio_frame_obu_metadata.add_channel_metadatas();
    channel_metadata->set_channel_label(label);
    channel_metadata->set_channel_id(channel_id++);
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
