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
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
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

  // Examine Mix Presentation OBUs.
  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    mix_presentation_obu.PrintObu();
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
