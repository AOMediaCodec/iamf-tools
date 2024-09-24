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

#include <algorithm>
#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/macros.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

template <typename StateType, typename StateUpdater>
absl::Status UpdateParameterState(
    DecodedUleb128 audio_element_id, int32_t expected_timestamp,
    absl::flat_hash_map<DecodedUleb128, StateType>& parameter_states,
    absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
        parameter_blocks,
    absl::string_view parameter_name, StateUpdater additional_state_updater) {
  const auto parameter_states_iter = parameter_states.find(audio_element_id);
  if (parameter_states_iter == parameter_states.end()) {
    // No parameter definition found for the audio element ID, so
    // nothing to update.
    return absl::OkStatus();
  }

  // Validate the timestamps before updating.
  auto& parameter_state = parameter_states_iter->second;

  // Using `.at()` here is safe because if the parameter state exists for the
  // `audio_element_id`, an entry in `parameter_blocks` with the key
  // `parameter_state.param_definition->parameter_id_` has already been
  // created during `Initialize()`.
  auto& parameter_block =
      parameter_blocks.at(parameter_state.param_definition->parameter_id_);
  if (parameter_block == nullptr) {
    // No parameter block found for this ID. Do not validate the timestamp
    // or update anything else.
    return absl::OkStatus();
  }

  if (expected_timestamp != parameter_state.next_timestamp) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Mismatching timestamps for ", parameter_name, " parameters: (",
        parameter_state.next_timestamp, " vs ", expected_timestamp, ")"));
  }

  // Update the next timestamp for the next frame.
  parameter_state.next_timestamp = parameter_block->end_timestamp;

  // Clear out the parameter block, which should not be used before a new
  // one is added via `Add*ParameterBlock()`.
  parameter_block = nullptr;

  // Additional update steps.
  additional_state_updater(parameter_state);

  return absl::OkStatus();
}

}  // namespace

ParametersManager::ParametersManager(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements)
    : audio_elements_(audio_elements) {}

absl::Status ParametersManager::Initialize() {
  // Collect all `DemixingParamDefinition`s and all `ReconGainParamDefinitions`
  // in all Audio Elements. Validate there is no more than one per Audio
  // Element.
  for (const auto& [audio_element_id, audio_element] : audio_elements_) {
    const DemixingParamDefinition* demixing_param_definition = nullptr;
    const ReconGainParamDefinition* recon_gain_param_definition = nullptr;
    for (const auto& param : audio_element.obu.audio_element_params_) {
      if (param.param_definition_type ==
          ParamDefinition::kParameterDefinitionDemixing) {
        if (demixing_param_definition != nullptr) {
          return absl::InvalidArgumentError(
              "Not allowed to have multiple demixing parameters in a "
              "single Audio Element.");
        }

        demixing_param_definition =
            static_cast<DemixingParamDefinition*>(param.param_definition.get());

        // Continue searching. Only to validate that there is at most one
        // `DemixingParamDefinition`.
      } else if (param.param_definition_type ==
                 ParamDefinition::kParameterDefinitionReconGain) {
        if (recon_gain_param_definition != nullptr) {
          return absl::InvalidArgumentError(
              "Not allowed to have multiple recon gain parameters in a "
              "single Audio Element.");
        }
        recon_gain_param_definition = static_cast<ReconGainParamDefinition*>(
            param.param_definition.get());
      }
    }

    if (demixing_param_definition != nullptr) {
      // Insert a `nullptr` for a parameter ID. If no parameter blocks have
      // this parameter ID, then it will remain null and default values will
      // be used.
      demixing_parameter_blocks_.insert(
          {demixing_param_definition->parameter_id_, nullptr});
      demixing_states_[audio_element_id] = {
          .param_definition = demixing_param_definition,
          .previous_w_idx = 0,
          .next_timestamp = 0,
          .update_rule = DemixingInfoParameterData::kFirstFrame,
      };
    }
    if (recon_gain_param_definition != nullptr) {
      // Insert a `nullptr` for a parameter ID. If no parameter blocks have
      // this parameter ID, then it will remain null and default values will
      // be used.
      recon_gain_parameter_blocks_.insert(
          {recon_gain_param_definition->parameter_id_, nullptr});
      recon_gain_states_[audio_element_id] = {
          .param_definition = recon_gain_param_definition,
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

// This function will populate `down_mixing_params` as follows:
// 1) If no parameter definition of type kParameterDefinitionDemixing
//    exists, it will use sensible defaults.
// 2) If such a parameter definition exists but no demixing parameter
//    blocks are present, it will use the parameter definition defaults.
// 3) If such a parameter definition exists AND a demixing parameter block
//    is present, it will use the values provided in the parameter block.
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
  const auto* parameter_block =
      demixing_parameter_blocks_.at(param_definition->parameter_id_);
  if (parameter_block == nullptr) {
    // Failed to find a parameter block that overlaps this frame. Use the
    // default value from the parameter definition. This is OK when there are
    // no parameter blocks covering this substream. If there is only partial
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

  if (parameter_block->start_timestamp != demixing_state.next_timestamp) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Mismatching timestamps for down-mixing parameters for "
        "audio element ID= ",
        audio_element_id, ": expecting", demixing_state.next_timestamp,
        " but got ", parameter_block->start_timestamp));
  }

  RETURN_IF_NOT_OK(DemixingInfoParameterData::DMixPModeToDownMixingParams(
      static_cast<DemixingInfoParameterData*>(
          parameter_block->obu->subblocks_[0].param_data.get())
          ->dmixp_mode,
      demixing_state.previous_w_idx, demixing_state.update_rule,
      down_mixing_params));
  demixing_state.w_idx = down_mixing_params.w_idx_used;
  return absl::OkStatus();
}

absl::Status ParametersManager::GetReconGainInfoParameterData(
    DecodedUleb128 audio_element_id, int32_t num_layers,
    ReconGainInfoParameterData& recon_gain_info_parameter_data) {
  const auto recon_gain_states_iter = recon_gain_states_.find(audio_element_id);
  if (recon_gain_states_iter == recon_gain_states_.end()) {
    LOG_FIRST_N(WARNING, 1)
        << "No recon gain parameter definition found for Audio "
        << "Element with ID= " << audio_element_id
        << "; setting recon gain to 255 (which represents a multiplier of 1.0, "
           "i.e. a gain of 0 dB) in all layers";
    for (int i = 0; i < num_layers; ++i) {
      ReconGainElement recon_gain_element;
      recon_gain_element.recon_gain_flag = DecodedUleb128(0);
      std::fill(recon_gain_element.recon_gain.begin(),
                recon_gain_element.recon_gain.end(), 255);
      recon_gain_info_parameter_data.recon_gain_elements.push_back(
          recon_gain_element);
    }
    return absl::OkStatus();
  }

  auto& recon_gain_state = recon_gain_states_iter->second;
  const auto* param_definition = recon_gain_state.param_definition;
  const auto* recon_gain_parameter_block =
      recon_gain_parameter_blocks_.at(param_definition->parameter_id_);
  if (recon_gain_parameter_block == nullptr) {
    // Failed to find a parameter block that overlaps this frame. A default
    // recon gain value of 0 dB is implied when there are no Parameter Block
    // OBUs provided. This is OK when there are no parameter blocks covering
    // this substream. If there is only partial coverage this will be marked
    // invalid when the coverage of parameter blocks is checked.
    LOG_FIRST_N(WARNING, 10)
        << "Failed to find a recon gain parameter block; "
           "A default recon gain value of 0 dB is implied when there are no "
           "Parameter Block OBUs provided";

    for (int i = 0; i < num_layers; ++i) {
      ReconGainElement recon_gain_element;
      recon_gain_element.recon_gain_flag = DecodedUleb128(0);
      std::fill(recon_gain_element.recon_gain.begin(),
                recon_gain_element.recon_gain.end(), 255);
      recon_gain_info_parameter_data.recon_gain_elements.push_back(
          recon_gain_element);
    }

    return absl::OkStatus();
  }

  if (recon_gain_parameter_block->start_timestamp !=
      recon_gain_state.next_timestamp) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Mismatching timestamps for recon gain parameters for "
        "audio element ID= ",
        audio_element_id, ": expecting", recon_gain_state.next_timestamp,
        " but got ", recon_gain_parameter_block->start_timestamp));
  }

  auto recon_gain_info_parameter_data_in_obu =
      static_cast<ReconGainInfoParameterData*>(
          recon_gain_parameter_block->obu->subblocks_[0].param_data.get());
  recon_gain_info_parameter_data.recon_gain_elements =
      recon_gain_info_parameter_data_in_obu->recon_gain_elements;
  return absl::OkStatus();
}

void ParametersManager::AddDemixingParameterBlock(
    const ParameterBlockWithData* parameter_block) {
  demixing_parameter_blocks_[parameter_block->obu->parameter_id_] =
      parameter_block;
}

void ParametersManager::AddReconGainParameterBlock(
    const ParameterBlockWithData* parameter_block) {
  recon_gain_parameter_blocks_[parameter_block->obu->parameter_id_] =
      parameter_block;
}

absl::Status ParametersManager::UpdateDemixingState(
    DecodedUleb128 audio_element_id, int32_t expected_timestamp) {
  // Additional updating steps to perform at the end of
  // `UpdateParameterState()`.
  auto demixing_state_updater = [](DemixingState& demixing_state) {
    // Update `previous_w_idx` for the next frame.
    demixing_state.previous_w_idx = demixing_state.w_idx;

    // Update the `update_rule` of the first frame to be "normally updating".
    if (demixing_state.update_rule == DemixingInfoParameterData::kFirstFrame) {
      demixing_state.update_rule = DemixingInfoParameterData::kNormal;
    }
  };

  return UpdateParameterState(audio_element_id, expected_timestamp,
                              demixing_states_, demixing_parameter_blocks_,
                              "down-mixing", demixing_state_updater);

  return absl::OkStatus();
}

absl::Status ParametersManager::UpdateReconGainState(
    DecodedUleb128 audio_element_id, int32_t expected_timestamp) {
  return UpdateParameterState(
      audio_element_id, expected_timestamp, recon_gain_states_,
      recon_gain_parameter_blocks_, "recon gain",
      [](auto& state) {}  // No additional updating needed.
  );
}

}  // namespace iamf_tools
