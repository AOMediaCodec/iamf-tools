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
#include "iamf/cli/obu_to_proto/parameter_block_metadata_generator.h"

#include <utility>
#include <variant>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using ParameterSubblockMetadata = iamf_tools_cli_proto::ParameterSubblock;

// Returns a proto representation of the input `AnimationStepInt16`.
absl::StatusOr<iamf_tools_cli_proto::MixGainParameterData>
AnimatedParameterDataInt16ToMetadata(
    MixGainParameterData::AnimationType animation_type,
    const AnimationStepInt16& step) {
  RETURN_IF_NOT_OK(ValidateEqual(
      animation_type, MixGainParameterData::AnimationType::kAnimateStep,
      "Expected a step. Got animation_type= "));
  iamf_tools_cli_proto::MixGainParameterData result;
  result.set_animation_type(iamf_tools_cli_proto::ANIMATE_STEP);

  result.mutable_param_data()->mutable_step()->set_start_point_value(
      step.start_point_value);
  return result;
}

// Returns a proto representation of the input `AnimationLinearInt16`.
absl::StatusOr<iamf_tools_cli_proto::MixGainParameterData>
AnimatedParameterDataInt16ToMetadata(
    MixGainParameterData::AnimationType animation_type,
    const AnimationLinearInt16& linear) {
  RETURN_IF_NOT_OK(ValidateEqual(
      animation_type, MixGainParameterData::AnimationType::kAnimateLinear,
      "Expected a linear. Got animation_type= "));
  iamf_tools_cli_proto::MixGainParameterData result;
  result.set_animation_type(iamf_tools_cli_proto::ANIMATE_LINEAR);

  result.mutable_param_data()->mutable_linear()->set_start_point_value(
      linear.start_point_value);
  result.mutable_param_data()->mutable_linear()->set_end_point_value(
      linear.end_point_value);
  return result;
}

// Returns a proto representation of the input `AnimationBezierInt16`.
absl::StatusOr<iamf_tools_cli_proto::MixGainParameterData>
AnimatedParameterDataInt16ToMetadata(
    MixGainParameterData::AnimationType animation_type,
    const AnimationBezierInt16& bezier) {
  RETURN_IF_NOT_OK(ValidateEqual(
      animation_type, MixGainParameterData::AnimationType::kAnimateBezier,
      "Expected a bezier. Got animation_type= "));
  iamf_tools_cli_proto::MixGainParameterData result;
  result.set_animation_type(iamf_tools_cli_proto::ANIMATE_BEZIER);

  result.mutable_param_data()->mutable_bezier()->set_start_point_value(
      bezier.start_point_value);
  result.mutable_param_data()->mutable_bezier()->set_end_point_value(
      bezier.end_point_value);
  result.mutable_param_data()->mutable_bezier()->set_control_point_value(
      bezier.control_point_value);
  result.mutable_param_data()
      ->mutable_bezier()
      ->set_control_point_relative_time(bezier.control_point_relative_time);
  return result;
}

// Copies the input `obu_dmixp_mode` to the output proto `dmixp_mode`.
absl::Status CopyDMixPMode(DemixingInfoParameterData::DMixPMode obu_dmixp_mode,
                           iamf_tools_cli_proto::DMixPMode& dmixp_mode) {
  using enum DemixingInfoParameterData::DMixPMode;
  using enum iamf_tools_cli_proto::DMixPMode;
  static const absl::NoDestructor<absl::flat_hash_map<
      DemixingInfoParameterData::DMixPMode, iamf_tools_cli_proto::DMixPMode>>
      kObuDmixPModeToMetadataDMixPMode({
          {kDMixPMode1, DMIXP_MODE_1},
          {kDMixPMode2, DMIXP_MODE_2},
          {kDMixPMode3, DMIXP_MODE_3},
          {kDMixPModeReserved1, DMIXP_MODE_RESERVED_A},
          {kDMixPMode1_n, DMIXP_MODE_1_N},
          {kDMixPMode2_n, DMIXP_MODE_2_N},
          {kDMixPMode3_n, DMIXP_MODE_3_N},
          {kDMixPModeReserved2, DMIXP_MODE_RESERVED_B},
      });
  if (!LookupInMap(*kObuDmixPModeToMetadataDMixPMode, obu_dmixp_mode,
                   dmixp_mode)
           .ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown dmixp_mode = ", obu_dmixp_mode));
  }

  return absl::OkStatus();
}

// Gets the proto representation of the input `mix_gain_parameter_data`.
absl::StatusOr<ParameterSubblockMetadata> ParamDataToMetadata(
    const MixGainParameterData& mix_gain_parameter_data) {
  ParameterSubblockMetadata result;

  auto mix_gain_parameter_data_metadata = std::visit(
      [=](const auto& param_data) {
        return AnimatedParameterDataInt16ToMetadata(
            mix_gain_parameter_data.animation_type, param_data);
      },
      mix_gain_parameter_data.param_data);
  if (!mix_gain_parameter_data_metadata.ok()) {
    return mix_gain_parameter_data_metadata.status();
  }
  *result.mutable_mix_gain_parameter_data() =
      *std::move(mix_gain_parameter_data_metadata);

  return result;
}

// Gets the proto representation of the input `demixing_info_parameter_data`.
absl::StatusOr<ParameterSubblockMetadata> ParamDataToMetadata(
    const DemixingInfoParameterData& demixing_info_parameter_data) {
  ParameterSubblockMetadata result;
  iamf_tools_cli_proto::DMixPMode dmixp_mode;
  RETURN_IF_NOT_OK(
      CopyDMixPMode(demixing_info_parameter_data.dmixp_mode, dmixp_mode));
  result.mutable_demixing_info_parameter_data()->set_dmixp_mode(dmixp_mode);
  result.mutable_demixing_info_parameter_data()->set_reserved(
      demixing_info_parameter_data.reserved);
  return result;
}

// Gets the proto representation of the input `recon_gain_parameter_data`.
absl::StatusOr<ParameterSubblockMetadata> ParamDataToMetadata(
    const ReconGainInfoParameterData& recon_gain_parameter_data) {
  ParameterSubblockMetadata result;
  for (const auto& recon_gain_element :
       recon_gain_parameter_data.recon_gain_elements) {
    auto& recon_gains_for_layer =
        *result.mutable_recon_gain_info_parameter_data()
             ->add_recon_gains_for_layer()
             ->mutable_recon_gain();
    DecodedUleb128 bitmask = 1;
    for (int counter = 0; counter < recon_gain_element.recon_gain.size();
         ++counter) {
      if (recon_gain_element.recon_gain_flag & bitmask) {
        recon_gains_for_layer[counter] = recon_gain_element.recon_gain[counter];
      }

      bitmask <<= 1;
    }
  }

  return result;
}

// Gets the proto representation of the input `extension_parameter_data`.
absl::StatusOr<ParameterSubblockMetadata> ParamDataToMetadata(
    const ExtensionParameterData& extension_parameter_data) {
  ParameterSubblockMetadata result;

  result.mutable_parameter_data_extension()->set_parameter_data_size(
      extension_parameter_data.parameter_data_size);
  result.mutable_parameter_data_extension()
      ->mutable_parameter_data_bytes()
      ->assign(extension_parameter_data.parameter_data_bytes.begin(),
               extension_parameter_data.parameter_data_bytes.end());
  return result;
}

}  // namespace

absl::StatusOr<iamf_tools_cli_proto::ParameterSubblock>
ParameterBlockMetadataGenerator::GenerateParameterSubblockMetadata(
    const ParameterSubblock& parameter_subblock) {
  // Get the proto representation filled in with the specific type of
  // param_data.
  auto metadata_subblock = std::visit(
      [](const auto& param_data) { return ParamDataToMetadata(param_data); },
      parameter_subblock.param_data);
  if (!metadata_subblock.ok()) {
    return metadata_subblock.status();
  }
  metadata_subblock->set_subblock_duration(
      parameter_subblock.subblock_duration);

  return metadata_subblock;
}

}  // namespace iamf_tools
