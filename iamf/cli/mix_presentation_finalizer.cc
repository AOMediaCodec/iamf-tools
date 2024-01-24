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
#include "iamf/ia.h"
#include "iamf/mix_presentation.h"
#include "iamf/obu_util.h"

namespace iamf_tools {

absl::Status MixPresentationFinalizerBase::CopyUserIntegratedLoudnessAndPeaks(
    const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
    LoudnessInfo& output_loudness) {
  RETURN_IF_NOT_OK(Int32ToInt16(user_loudness.integrated_loudness(),
                                output_loudness.integrated_loudness));
  RETURN_IF_NOT_OK(
      Int32ToInt16(user_loudness.digital_peak(), output_loudness.digital_peak));

  if ((output_loudness.info_type & LoudnessInfo::kTruePeak) != 0) {
    RETURN_IF_NOT_OK(
        Int32ToInt16(user_loudness.true_peak(), output_loudness.true_peak));
  }

  return absl::OkStatus();
}

absl::Status MixPresentationFinalizerBase::CopyUserAnchoredLoudness(
    const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
    LoudnessInfo& output_loudness) {
  if ((output_loudness.info_type & LoudnessInfo::kAnchoredLoudness) == 0) {
    // Not using anchored loudness.
    return absl::OkStatus();
  }

  RETURN_IF_NOT_OK(
      Uint32ToUint8(user_loudness.anchored_loudness().num_anchored_loudness(),
                    output_loudness.anchored_loudness.num_anchored_loudness));

  for (const auto& metadata_anchor_element :
       user_loudness.anchored_loudness().anchor_elements()) {
    AnchoredLoudnessElement::AnchorElement obu_anchor_element;
    switch (metadata_anchor_element.anchor_element()) {
      using enum iamf_tools_cli_proto::AnchorType;
      using enum AnchoredLoudnessElement::AnchorElement;
      case ANCHOR_TYPE_UNKNOWN:
        obu_anchor_element = kAnchorElementUnknown;
        break;
      case ANCHOR_TYPE_DIALOGUE:
        obu_anchor_element = kAnchorElementDialogue;
        break;
      case ANCHOR_TYPE_ALBUM:
        obu_anchor_element = kAnchorElementAlbum;
        break;
      default:
        LOG(ERROR) << "Unknown anchor_element: "
                   << metadata_anchor_element.anchor_element();
        return absl::InvalidArgumentError("");
    }

    int16_t obu_anchored_loudness;
    RETURN_IF_NOT_OK(Int32ToInt16(metadata_anchor_element.anchored_loudness(),
                                  obu_anchored_loudness));
    output_loudness.anchored_loudness.anchor_elements.push_back(
        {obu_anchor_element, obu_anchored_loudness});
  }

  return absl::OkStatus();
}

absl::Status MixPresentationFinalizerBase::CopyUserLayoutExtension(
    const iamf_tools_cli_proto::LoudnessInfo& user_loudness,
    LoudnessInfo& output_loudness) {
  if ((output_loudness.info_type & LoudnessInfo::kAnyLayoutExtension) == 0) {
    // Not using layout extension.
    return absl::OkStatus();
  }

  output_loudness.layout_extension.info_type_size =
      user_loudness.info_type_size();
  output_loudness.layout_extension.info_type_bytes.reserve(
      user_loudness.info_type_bytes().size());
  for (const char& c : user_loudness.info_type_bytes()) {
    output_loudness.layout_extension.info_type_bytes.push_back(
        static_cast<uint8_t>(c));
  }
  return absl::OkStatus();
}

absl::Status DummyMixPresentationFinalizer::Finalize(
    const absl::flat_hash_map<uint32_t, AudioElementWithData>&,
    const IdTimeLabeledFrameMap&, const std::list<ParameterBlockWithData>&,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  LOG(INFO) << "Calling DummyMixPresentationFinalizer::Finalize():";
  LOG(INFO) << "  Loudness information will be copied from user "
            << "provided values.";

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
            MixPresentationFinalizerBase::CopyUserIntegratedLoudnessAndPeaks(
                user_loudness, output_loudness));
        RETURN_IF_NOT_OK(MixPresentationFinalizerBase::CopyUserAnchoredLoudness(
            user_loudness, output_loudness));
        RETURN_IF_NOT_OK(MixPresentationFinalizerBase::CopyUserLayoutExtension(
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
