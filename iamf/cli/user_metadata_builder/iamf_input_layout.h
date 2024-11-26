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

#ifndef CLI_USER_METADATA_BUILDER_IAMF_INPUT_LAYOUT_H_
#define CLI_USER_METADATA_BUILDER_IAMF_INPUT_LAYOUT_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
namespace iamf_tools {

/*!\brief Input layout of an IAMF Audio Element.
 *
 * Used as a generalization of types an audio element can represent. Even when
 * the OBU may represent them using different structures (i.e. this type
 * captures both `AUDIO_ELEMENT_CHANNEL_BASED` and `AUDIO_ELEMENT_SCENE_BASED`).
 */
enum class IamfInputLayout {
  kMono,
  kStereo,
  k5_1,
  k5_1_2,
  k5_1_4,
  k7_1,
  k7_1_4,
  kBinaural,
  kAmbisonicsOrder1,
  kAmbisonicsOrder2,
  kAmbisonicsOrder3,
};

/*!\brief Lookup IAMF input layout from the ADM audio pack format ID.
 *
 * In ADM, audioPackFormatID has the format AP_yyyyxxxx, where 'yyyy' digits
 * represent the type of audio and 'xxxx' gives the description within a
 * particular type.
 *
 * yyyy    typeDefinition
 * 0001    DirectSpeakers
 * 0002    Matrix
 * 0003    Objects
 * 0004    HOA
 * 0005    Binaural
 *
 * IAMF supports typeDefinitions ='DirectSpeakers'/'HOA'/'Binaural'.
 */
absl::StatusOr<IamfInputLayout> LookupInputLayoutFromAudioPackFormatId(
    absl::string_view audio_pack_format_id);

}  // namespace iamf_tools

#endif  // CLI_USER_METADATA_BUILDER_IAMF_INPUT_LAYOUT_H_
