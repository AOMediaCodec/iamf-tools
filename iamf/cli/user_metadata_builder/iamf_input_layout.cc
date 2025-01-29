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

#include "iamf/cli/user_metadata_builder/iamf_input_layout.h"

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/common/utils/map_utils.h"

namespace iamf_tools {

absl::StatusOr<IamfInputLayout> LookupInputLayoutFromAudioPackFormatId(
    absl::string_view audio_pack_format_id) {
  // Map which holds the audioPackFormatID in ADM and the corresponding
  // loudspeaker layout in IAMF.
  using enum IamfInputLayout;
  static const absl::NoDestructor<
      absl::flat_hash_map<absl::string_view, IamfInputLayout>>
      kAudioPackFormatIdToIamfInputLayoutInputLayout({
          {"AP_00010001", IamfInputLayout::kMono},
          {"AP_00010002", IamfInputLayout::kStereo},
          {"AP_00010003", IamfInputLayout::k5_1},
          {"AP_00010004", IamfInputLayout::k5_1_2},
          {"AP_00010005", IamfInputLayout::k5_1_4},
          {"AP_0001000f", IamfInputLayout::k7_1},
          {"AP_00010017", IamfInputLayout::k7_1_4},
          {"AP_00050001", IamfInputLayout::kBinaural},
          {"AP_00011FFF", IamfInputLayout::kLFE},
          {"AP_00040001", IamfInputLayout::kAmbisonicsOrder1},
          {"AP_00040002", IamfInputLayout::kAmbisonicsOrder2},
          {"AP_00040003", IamfInputLayout::kAmbisonicsOrder3},
      });

  return LookupInMap(*kAudioPackFormatIdToIamfInputLayoutInputLayout,
                     audio_pack_format_id,
                     "`IamfInputLayout` for `audio_pack_format_id`");
}

}  // namespace iamf_tools
