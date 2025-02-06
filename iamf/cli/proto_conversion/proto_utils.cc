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
#include "iamf/cli/proto_conversion/proto_utils.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/lookup_tables.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::Status CopyParamDefinition(
    const iamf_tools_cli_proto::ParamDefinition& input_param_definition,
    ParamDefinition& param_definition) {
  param_definition.parameter_id_ = input_param_definition.parameter_id();
  param_definition.parameter_rate_ = input_param_definition.parameter_rate();

  param_definition.param_definition_mode_ =
      input_param_definition.param_definition_mode();
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "ParamDefinition.reserved", input_param_definition.reserved(),
      param_definition.reserved_));
  param_definition.duration_ = input_param_definition.duration();
  param_definition.constant_subblock_duration_ =
      input_param_definition.constant_subblock_duration();

  if (input_param_definition.constant_subblock_duration() != 0) {
    // Nothing else to be done. Return.
    return absl::OkStatus();
  }

  if (input_param_definition.num_subblocks() <
      input_param_definition.subblock_durations_size()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected at least ", input_param_definition.num_subblocks(),
        "subblock durations for parameter id = ",
        input_param_definition.parameter_id()));
  }

  param_definition.InitializeSubblockDurations(
      static_cast<DecodedUleb128>(input_param_definition.num_subblocks()));
  for (int i = 0; i < input_param_definition.num_subblocks(); ++i) {
    RETURN_IF_NOT_OK(param_definition.SetSubblockDuration(
        i, input_param_definition.subblock_durations(i)));
  }

  return absl::OkStatus();
}

ObuHeader GetHeaderFromMetadata(
    const iamf_tools_cli_proto::ObuHeaderMetadata& input_obu_header) {
  std::vector<uint8_t> extension_header_bytes(
      input_obu_header.extension_header_bytes().size());
  std::transform(input_obu_header.extension_header_bytes().begin(),
                 input_obu_header.extension_header_bytes().end(),
                 extension_header_bytes.begin(),
                 [](char c) { return static_cast<uint8_t>(c); });

  return ObuHeader{
      .obu_redundant_copy = input_obu_header.obu_redundant_copy(),
      .obu_trimming_status_flag = input_obu_header.obu_trimming_status_flag(),
      .obu_extension_flag = input_obu_header.obu_extension_flag(),
      .num_samples_to_trim_at_end =
          input_obu_header.num_samples_to_trim_at_end(),
      .num_samples_to_trim_at_start =
          input_obu_header.num_samples_to_trim_at_start(),
      .extension_header_size = input_obu_header.extension_header_size(),
      .extension_header_bytes = extension_header_bytes};
}

absl::Status CopyDemixingInfoParameterData(
    const iamf_tools_cli_proto::DemixingInfoParameterData&
        input_demixing_info_parameter_data,
    DemixingInfoParameterData& obu_demixing_param_data) {
  static const auto kProtoToInternalDMixPMode =
      BuildStaticMapFromPairs(LookupTables::kProtoAndInternalDMixPModes);

  RETURN_IF_NOT_OK(CopyFromMap(*kProtoToInternalDMixPMode,
                               input_demixing_info_parameter_data.dmixp_mode(),
                               "Internal version of proto `dmixp_mode`",
                               obu_demixing_param_data.dmixp_mode));

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "DemixingInfoParameterData.reserved",
      input_demixing_info_parameter_data.reserved(),
      obu_demixing_param_data.reserved));

  return absl::OkStatus();
}

absl::Status CopyDMixPMode(DemixingInfoParameterData::DMixPMode obu_dmixp_mode,
                           iamf_tools_cli_proto::DMixPMode& dmixp_mode) {
  static const auto kInternalToProtoDMixPMode = BuildStaticMapFromInvertedPairs(
      LookupTables::kProtoAndInternalDMixPModes);

  return CopyFromMap(*kInternalToProtoDMixPMode, obu_dmixp_mode,
                     "Proto version of internal `DMixPMode`", dmixp_mode);
}

std::unique_ptr<LebGenerator> CreateLebGenerator(
    const iamf_tools_cli_proto::Leb128Generator& user_config) {
  // Transform the enum and possibly `fixed_size` to call LebGenerator::Create.
  using enum iamf_tools_cli_proto::Leb128GeneratorMode;
  switch (user_config.mode()) {
    case GENERATE_LEB_MINIMUM:
      return LebGenerator::Create(LebGenerator::GenerationMode::kMinimum);
    case GENERATE_LEB_FIXED_SIZE: {
      int8_t fixed_size_int8;
      auto status =
          StaticCastIfInRange("user_metadata.leb_generator.fixed_size",
                              user_config.fixed_size(), fixed_size_int8);
      if (!status.ok()) {
        LOG(ERROR) << status;
        return nullptr;
      }
      return LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize,
                                  fixed_size_int8);
    }
    default:
      LOG(ERROR) << "Invalid generation mode: " << user_config.mode();
      return nullptr;
  }
}

}  // namespace iamf_tools
