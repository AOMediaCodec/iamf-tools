/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/proto_conversion/proto_to_obu/parameter_block_metadata_converter.h"

#include <cstdint>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/parameter_block_generator.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/animated_parameter_data.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {
namespace {

absl::StatusOr<MixGainParameterData> ConvertMixGainParameterData(
    const iamf_tools_cli_proto::MixGainParameterData& proto_mix_gain) {
  switch (proto_mix_gain.param_data().parameter_data_case()) {
    using enum iamf_tools_cli_proto::AnimatedParameterDataInt16::
        ParameterDataCase;
    case kStep: {
      const auto& metadata_animation = proto_mix_gain.param_data().step();
      int16_t start_point_value;
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "AnimationStepInt16.start_point_value",
          metadata_animation.start_point_value(), start_point_value));
      return MixGainParameterData::Make(
          AnimatedParameterData<int16_t>::MakeStep(start_point_value));
    }
    case kLinear: {
      const auto& metadata_animation = proto_mix_gain.param_data().linear();
      int16_t start_point_value;
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "AnimationLinearInt16.start_point_value",
          metadata_animation.start_point_value(), start_point_value));
      int16_t end_point_value;
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "AnimationLinearInt16.end_point_value",
          metadata_animation.end_point_value(), end_point_value));
      return MixGainParameterData::Make(
          AnimatedParameterData<int16_t>::MakeLinear(start_point_value,
                                                     end_point_value));
    }
    case kBezier: {
      const auto& metadata_animation = proto_mix_gain.param_data().bezier();
      int16_t start_point_value;
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "AnimationBezierInt16.start_point_value",
          metadata_animation.start_point_value(), start_point_value));
      int16_t end_point_value;
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "AnimationBezierInt16.end_point_value",
          metadata_animation.end_point_value(), end_point_value));
      int16_t control_point_value;
      RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
          "AnimationBezierInt16.control_point_value",
          metadata_animation.control_point_value(), control_point_value));
      uint8_t control_point_relative_time;
      RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
          "AnimationBezierInt16.control_point_relative_time",
          metadata_animation.control_point_relative_time(),
          control_point_relative_time));
      return MixGainParameterData::Make(
          AnimatedParameterData<int16_t>::MakeBezier(
              start_point_value, end_point_value, control_point_value,
              control_point_relative_time));
    }
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unrecognized parameter data case= ",
                       proto_mix_gain.param_data().parameter_data_case()));
  }
}

absl::StatusOr<ReconGainInfoParameterData> ConvertReconGainInfoParameterData(
    const iamf_tools_cli_proto::ReconGainInfoParameterData& proto_recon_gain) {
  ReconGainInfoParameterData recon_gain_data;
  recon_gain_data.recon_gain_elements.reserve(
      proto_recon_gain.recon_gains_for_layer_size());
  for (const auto& recon_gains_layer :
       proto_recon_gain.recon_gains_for_layer()) {
    ReconGainElement element{};
    for (const auto& [bit_position, user_recon_gain] :
         recon_gains_layer.recon_gain()) {
      if (bit_position >= element.recon_gain.size()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Recon gain bit position out of range: ", bit_position));
      }
      uint8_t recon_gain_byte = 0;
      RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
          "ReconGains.recon_gain", user_recon_gain, recon_gain_byte));
      element.recon_gain_flag |= 1 << bit_position;
      element.recon_gain[bit_position] = recon_gain_byte;
    }
    recon_gain_data.recon_gain_elements.push_back(element);
  }
  return recon_gain_data;
}

}  // namespace

absl::StatusOr<ParameterBlockMetadata> ConvertToParameterBlockMetadata(
    const iamf_tools_cli_proto::ParameterBlockObuMetadata& proto_metadata) {
  ParameterBlockMetadata result = {
      .parameter_id = proto_metadata.parameter_id(),
      .duration = proto_metadata.duration(),
      .constant_subblock_duration = proto_metadata.constant_subblock_duration(),
      .start_timestamp = proto_metadata.start_timestamp(),
      .obu_header = GetHeaderFromMetadata(proto_metadata.obu_header()),
  };

  result.subblocks.reserve(proto_metadata.subblocks_size());
  for (const auto& proto_subblock : proto_metadata.subblocks()) {
    ParameterSubblockMetadata subblock_metadata = {
        .subblock_duration = proto_subblock.subblock_duration(),
    };
    switch (proto_subblock.parameter_data_case()) {
      using enum iamf_tools_cli_proto::ParameterSubblock::ParameterDataCase;
      case kMixGainParameterData: {
        auto mix_gain_data = ConvertMixGainParameterData(
            proto_subblock.mix_gain_parameter_data());
        if (!mix_gain_data.ok()) {
          return mix_gain_data.status();
        }
        subblock_metadata.parameter_data = *mix_gain_data;
        break;
      }
      case kDemixingInfoParameterData: {
        DemixingInfoParameterData demixing_data;
        RETURN_IF_NOT_OK(CopyDemixingInfoParameterData(
            proto_subblock.demixing_info_parameter_data(), demixing_data));
        subblock_metadata.parameter_data = demixing_data;
        break;
      }
      case kReconGainInfoParameterData: {
        auto recon_gain_data = ConvertReconGainInfoParameterData(
            proto_subblock.recon_gain_info_parameter_data());
        if (!recon_gain_data.ok()) {
          return recon_gain_data.status();
        }
        subblock_metadata.parameter_data = std::move(*recon_gain_data);
        break;
      }
      case kParameterDataExtension:
      case PARAMETER_DATA_NOT_SET:
        break;
    }
    result.subblocks.push_back(std::move(subblock_metadata));
  }

  return result;
}

}  // namespace iamf_tools
