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
#include "iamf/cli/parameters_manager.h"

#include <cstdint>
#include <list>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/param_definitions.h"

namespace iamf_tools {

ParametersManager::ParametersManager(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const std::list<ParameterBlockWithData>& parameter_blocks)
    : audio_elements_(audio_elements) {
  for (const auto& parameter_block : parameter_blocks) {
    parameter_blocks_[parameter_block.obu->parameter_id_].insert(
        {parameter_block.start_timestamp, &parameter_block});
  }
}

absl::Status ParametersManager::Initialize() {
  // Collect all `DemixingParamDefinition`s in all Audio Elements. Validate
  // there is no more than one per Audio Element.
  for (const auto& [audio_element_id, audio_element] : audio_elements_) {
    const DemixingParamDefinition* demixing_param_definition = nullptr;
    for (const auto& param : audio_element.obu.audio_element_params_) {
      if (param.param_definition_type !=
          ParamDefinition::kParameterDefinitionDemixing) {
        continue;
      }

      if (demixing_param_definition != nullptr) {
        LOG(ERROR) << "Not allowed to have multiple demixing parameters in a "
                      "single Audio Element.";
        return absl::InvalidArgumentError("");
      }

      demixing_param_definition =
          static_cast<DemixingParamDefinition*>(param.param_definition.get());

      // Continue searching. Only to validate that there is at most one
      // `DemixingParamDefinition`.
    }

    if (demixing_param_definition != nullptr) {
      // Insert an empty `btree_map` when a non-existent parameter ID is
      // recorded in the param definition. Default values will be used in this
      // case.
      auto [btree_map_iter, inserted] = parameter_blocks_.insert(
          {demixing_param_definition->parameter_id_, {}});
      demixing_states_[audio_element_id] = {
          .param_definition = demixing_param_definition,
          .parameter_blocks_iter = btree_map_iter->second.begin(),
          .previous_w_idx = 0,
          .next_timestamp = 0,
      };
    }
  }

  return absl::OkStatus();
}

bool ParametersManager::DemixingParamDefinitionAvailable(
    const DecodedUleb128 audio_element_id) {
  return demixing_states_.find(audio_element_id) != demixing_states_.end();
}

absl::Status ParametersManager::GetDownMixingParameters(
    const DecodedUleb128 audio_element_id,
    DownMixingParams& down_mixing_params) {
  const auto demixing_states_iter = demixing_states_.find(audio_element_id);
  if (demixing_states_iter == demixing_states_.end()) {
    LOG_FIRST_N(WARNING, 1)
        << "No demixing parameter definition found for Audio "
        << "Element with ID= " << audio_element_id
        << "; using some sensible values.";

    down_mixing_params = {
        0.707, 0.707, 0.707, 0.707, 0, 0, /*in_bitstream=*/false};
    return absl::OkStatus();
  }
  auto& demixing_state = demixing_states_iter->second;

  const auto* param_definition = demixing_state.param_definition;
  auto& parameter_blocks_iter = demixing_state.parameter_blocks_iter;
  const auto& parameter_blocks_for_id =
      parameter_blocks_.at(param_definition->parameter_id_);
  if (parameter_blocks_iter == parameter_blocks_for_id.end()) {
    // Failed to find a parameter block that overlaps this frame. Use the
    // default value from the parameter definition. This is OK when there are no
    // parameter blocks covering this substream. If there is only partial
    // coverage this will be marked invalid when the coverage of parameter
    // blocks is checked.
    LOG_FIRST_N(WARNING, 10)
        << "Failed to find a parameter block; using the default values";
    RETURN_IF_NOT_OK(DemixingInfoParameterData::DMixPModeToDownMixingParams(
        param_definition->default_demixing_info_parameter_data_.dmixp_mode,
        param_definition->default_demixing_info_parameter_data_.default_w,
        DemixingInfoParameterData::kDefault, down_mixing_params));
    return absl::OkStatus();
  }

  const auto [start_timestamp, parameter_block] = *(parameter_blocks_iter);
  CHECK_EQ(start_timestamp, demixing_state.next_timestamp);

  RETURN_IF_NOT_OK(DemixingInfoParameterData::DMixPModeToDownMixingParams(
      std::get<DemixingInfoParameterData>(
          parameter_block->obu->subblocks_[0].param_data)
          .dmixp_mode,
      demixing_state.previous_w_idx,
      (parameter_blocks_iter == parameter_blocks_for_id.begin()
           ? DemixingInfoParameterData::kFirstFrame
           : DemixingInfoParameterData::kNormal),
      down_mixing_params));
  demixing_state.w_idx = down_mixing_params.w_idx_used;
  return absl::OkStatus();
}

absl::Status ParametersManager::UpdateDownMixingParameters(
    const DecodedUleb128 audio_element_id, const int32_t expected_timestamp) {
  const auto demixing_states_iter = demixing_states_.find(audio_element_id);
  if (demixing_states_iter == demixing_states_.end()) {
    // No demixing parameter definition found for the audio element ID, so
    // nothing to update.
    return absl::OkStatus();
  }

  // Validate the timestamps before updating.
  auto& demixing_state = demixing_states_iter->second;

  const auto parameter_id = demixing_state.param_definition->parameter_id_;
  if (demixing_state.parameter_blocks_iter ==
      parameter_blocks_.at(parameter_id).end()) {
    // No parameter block found for this ID. Do not validate the timestamp
    // or update anything else.
    return absl::OkStatus();
  }

  if (expected_timestamp != demixing_state.next_timestamp) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Mismatching timestamps for down-mixing parameters: (",
        demixing_state.next_timestamp, " vs ", expected_timestamp, ")"));
  }

  // Update `previous_w_idx` for the next frame.
  demixing_state.previous_w_idx = demixing_state.w_idx;

  // Update the iterator and the next timestamp for the next frame.
  demixing_state.next_timestamp =
      demixing_state.parameter_blocks_iter->second->end_timestamp;
  demixing_state.parameter_blocks_iter++;

  return absl::OkStatus();
}

}  // namespace iamf_tools
