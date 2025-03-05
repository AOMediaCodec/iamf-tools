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
namespace {
constexpr Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};
}

Layout ApiToInternalType(api::OutputLayout api_output_layout) {
  switch (api_output_layout) {
    case api::OutputLayout::kOutputStereo:
      return kStereoLayout;
  };
  // Switch above is exhaustive.
  LOG(FATAL) << "Invalid output layout.";
}

}  // namespace iamf_tools
