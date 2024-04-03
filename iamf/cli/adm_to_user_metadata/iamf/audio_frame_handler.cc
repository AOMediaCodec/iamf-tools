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

#include "iamf/cli/adm_to_user_metadata/iamf/audio_frame_handler.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

namespace {

absl::StatusOr<std::vector<std::string>> LookupLabelsFromInputLayout(
    IamfInputLayout input_layout) {
  // Map which holds the channel labels corresponding to the loudspeaker
  // layout/ambisonics.
  using enum IamfInputLayout;
  static const absl::NoDestructor<
      absl::flat_hash_map<IamfInputLayout, std::vector<std::string>>>
      kInputLayoutToLabels({
          {kMono, {"M"}},
          {kStereo, {"L2", "R2"}},
          {k5_1, {"L5", "R5", "C", "LFE", "Ls5", "Rs5"}},
          {k5_1_2, {"L5", "R5", "C", "LFE", "Ls5", "Rs5", "Ltf2", "Rtf2"}},
          {k5_1_4,
           {"L5", "R5", "C", "LFE", "Ls5", "Rs5", "Ltf4", "Rtf4", "Ltb4",
            "Rtb4"}},
          {k7_1, {"L7", "R7", "C", "LFE", "Lss7", "Rss7", "Lrs7", "Rrs7"}},
          {k7_1_4,
           {"L7", "R7", "C", "LFE", "Lss7", "Rss7", "Lrs7", "Rrs7", "Ltf4",
            "Rtf4", "Ltb4", "Rtb4"}},
          {kBinaural, {"L", "R"}},
          {kAmbisonicsOrder1, {"A0", "A1", "A2", "A3"}},
          {kAmbisonicsOrder2,
           {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8"}},
          {kAmbisonicsOrder3,
           {"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10",
            "A11", "A12", "A13", "A14", "A15"}},
      });

  auto it = kInputLayoutToLabels->find(input_layout);
  if (it == kInputLayoutToLabels->end()) {
    return absl::NotFoundError(
        absl::StrCat("Labels not found for input_layout= ", input_layout));
  }
  return it->second;
}

}  // namespace

// Sets the required textproto fields for audio_frame_metadata.
absl::Status AudioFrameHandler::PopulateAudioFrameMetadata(
    absl::string_view file_suffix, int32_t audio_element_id,
    IamfInputLayout input_layout,
    iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_obu_metadata)
    const {
  audio_frame_obu_metadata.set_wav_filename(
      absl::StrCat(file_prefix_, "_converted", file_suffix, ".wav"));
  audio_frame_obu_metadata.set_samples_to_trim_at_start(0);
  audio_frame_obu_metadata.set_samples_to_trim_at_end(
      num_samples_to_trim_at_end_);
  audio_frame_obu_metadata.set_audio_element_id(audio_element_id);

  const auto& labels = LookupLabelsFromInputLayout(input_layout);
  if (!labels.ok()) {
    return labels.status();
  }
  int32_t channel_id = 0;
  for (const auto& label : *labels) {
    audio_frame_obu_metadata.add_channel_labels(label);
    audio_frame_obu_metadata.add_channel_ids(channel_id++);
  }

  return absl::OkStatus();
}

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
