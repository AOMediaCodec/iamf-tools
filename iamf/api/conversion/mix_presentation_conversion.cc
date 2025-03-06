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

#include "iamf/api/conversion/mix_presentation_conversion.h"

#include "absl/log/log.h"
#include "iamf/api/types.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
Layout MakeLayout(LoudspeakersSsConventionLayout::SoundSystem sound_system) {
  return {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
          .specific_layout =
              LoudspeakersSsConventionLayout{.sound_system = sound_system}};
};

Layout ApiToInternalType(api::OutputLayout api_output_layout) {
  switch (api_output_layout) {
    case api::OutputLayout::kItu2051_SoundSystemA_0_2_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0);
    case api::OutputLayout::kItu2051_SoundSystemB_0_5_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0);
    case api::OutputLayout::kItu2051_SoundSystemC_2_5_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0);
    case api::OutputLayout::kItu2051_SoundSystemD_4_5_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemD_4_5_0);
    case api::OutputLayout::kItu2051_SoundSystemE_4_5_1:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemE_4_5_1);
    case api::OutputLayout::kItu2051_SoundSystemF_3_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemF_3_7_0);
    case api::OutputLayout::kItu2051_SoundSystemG_4_9_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemG_4_9_0);
    case api::OutputLayout::kItu2051_SoundSystemH_9_10_3:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemH_9_10_3);
    case api::OutputLayout::kItu2051_SoundSystemI_0_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemI_0_7_0);
    case api::OutputLayout::kItu2051_SoundSystemJ_4_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem10_2_7_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem11_2_3_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0);
    case api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0:
      return MakeLayout(LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0);
  };
  // Switch above is exhaustive.
  LOG(FATAL) << "Invalid output layout.";
}

}  // namespace iamf_tools
