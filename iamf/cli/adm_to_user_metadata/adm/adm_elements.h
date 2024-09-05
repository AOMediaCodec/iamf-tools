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

#ifndef CLI_ADM_TO_USER_METADATA_ADM_ADM_ELEMENTS_H_
#define CLI_ADM_TO_USER_METADATA_ADM_ADM_ELEMENTS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

// This struct holds the Audio Definition Model (ADM) elements.
struct ADM {
  std::vector<struct AudioProgramme> audio_programmes;
  std::vector<struct AudioContent> audio_contents;
  std::vector<struct AudioObject> audio_objects;
};

// This structure holds the sub-elements of loudness metadata.
struct LoudnessMetadata {
  static constexpr float kDefaultIntegratedLoudness = 0.0;

  float integrated_loudness = kDefaultIntegratedLoudness;
  std::optional<float> max_true_peak;
  std::optional<float> dialogue_loudness;
};

// This structure holds the reference layout of an audio programme.
struct ReferenceLayout {
  std::vector<std::string> audio_pack_format_id_ref;
};

// This structure holds the authoring information of an audio programme.
struct AuthoringInformation {
  ReferenceLayout reference_layout;
};

// This structure holds the attributes of an audio programme in ADM.
struct AudioProgramme {
  std::string id;
  std::string name;
  std::string audio_programme_label;
  std::vector<std::string> audio_content_id_refs;
  LoudnessMetadata loudness_metadata;
  AuthoringInformation authoring_information;
};

// This structure holds the attributes of an audio content in ADM.
struct AudioContent {
  std::string id;
  std::string name;
  std::vector<std::string> audio_object_id_ref;
};

// This structure holds the attributes of an audio object in ADM.
struct AudioObject {
  static constexpr absl::string_view kDefaultLocalizedElementAnnotations =
      "test_sub_mix_0_audio_element_0";
  static constexpr int32_t kDefaultADMImportance = 10;
  static constexpr float kDefaultADMGain = 0.0;

  std::string id;
  std::string name;
  std::string audio_object_label =
      std::string(kDefaultLocalizedElementAnnotations);
  int32_t importance = kDefaultADMImportance;
  float gain = kDefaultADMGain;
  std::vector<std::string> audio_pack_format_id_refs;
  std::vector<std::string> audio_comple_object_id_ref;
  std::vector<std::string> audio_track_uid_ref;
};

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_ADM_ADM_ELEMENTS_H_
