/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/mix_presentation_finalizer.h"

#include <cstdint>
#include <list>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/mix_presentation_generator.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/common/macros.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

absl::Status
MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizer::Finalize(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>&,
    const IdTimeLabeledFrameMap&, const std::list<ParameterBlockWithData>&,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  LOG(INFO) << "Calling "
               "MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizer"
               "::Finalize():";
  LOG(INFO) << "  Loudness information may be copied from user "
            << "provided values.";

  // TODO(b/332567539): Use `RendererFactory` to render certain layouts.
  // TODO(b/302273947): Once layouts are rendered and mixed then use a
  //                    `LoudnessCalculatorFactory` to measure loudness.
  int metadata_index = 0;
  for (auto& mix_presentation_obu : mix_presentation_obus) {
    for (int sub_mix_index = 0;
         sub_mix_index < mix_presentation_obu.sub_mixes_.size();
         ++sub_mix_index) {
      MixPresentationSubMix& sub_mix =
          mix_presentation_obu.sub_mixes_[sub_mix_index];
      for (int layout_index = 0; layout_index < sub_mix.layouts.size();
           layout_index++) {
        const auto& user_loudness =
            mix_presentation_metadata_.at(metadata_index)
                .sub_mixes(sub_mix_index)
                .layouts(layout_index)
                .loudness();
        auto& output_loudness = sub_mix.layouts[layout_index].loudness;

        // The `info_type` should already be copied over in the
        // `MixPresentationGenerator`. Check it is equivalent for extra safety.
        uint8_t user_info_type;
        RETURN_IF_NOT_OK(MixPresentationGenerator::CopyInfoType(
            user_loudness, user_info_type));
        if (user_info_type != output_loudness.info_type) {
          LOG(ERROR) << "Mismatching loudness info types: ("
                     << static_cast<uint32_t>(user_info_type) << " vs "
                     << static_cast<uint32_t>(output_loudness.info_type) << ")";
          return absl::InvalidArgumentError("");
        }
        RETURN_IF_NOT_OK(
            MixPresentationGenerator::CopyUserIntegratedLoudnessAndPeaks(
                user_loudness, output_loudness));
        RETURN_IF_NOT_OK(MixPresentationGenerator::CopyUserAnchoredLoudness(
            user_loudness, output_loudness));
        RETURN_IF_NOT_OK(MixPresentationGenerator::CopyUserLayoutExtension(
            user_loudness, output_loudness));
      }
    }

    metadata_index++;
  }

  // Examine Mix Presentation OBUs.
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    mix_presentation_obu.PrintObu();
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
