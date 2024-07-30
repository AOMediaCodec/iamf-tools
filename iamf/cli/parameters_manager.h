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

#ifndef CLI_PARAMETERS_MANAGER_H_
#define CLI_PARAMETERS_MANAGER_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

/*!\brief Manages parameters and supports easy query.
 *
 * The class operates iteratively; holding one set of parameter blocks
 * corresponding to the same frame (with the same start/end timestamps).
 *
 * For each frame:
 *   - Parameter blocks are added via `AddDemixingParameterBlock()`,
 *   - Parameter values can be queried via `GetDownMixingParameters()`.
 *   - Caller (usually the audio frame generator) is responsible to tell this
 *     manager to advance to the next frame via `UpdateDemixingState()`.
 */
class ParametersManager {
 public:
  /*!\brief Constructor.
   *
   * \param audio_elements Input Audio Element OBUs with data.
   * \param parameter_blocks Input Parameter Block OBUs with data.
   */
  ParametersManager(
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements);

  /*!\brief Initializes some internal data.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize();

  /*!\brief Checks if a `DemixingParamDefinition` exists for an audio element.
   *
   * \param audio_element_id ID of the audio element to query.
   * \return True if a `DemixingParamDefinition` is available for the audio
   *     element queried.
   */
  bool DemixingParamDefinitionAvailable(DecodedUleb128 audio_element_id);

  /*!\brief Gets current down-mixing parameters for an audio element.
   *
   * \param audio_element_id ID of the audio element that the parameters are
   *     to be applied.
   * \param down_mixing_params Output down mixing parameters.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDownMixingParameters(DecodedUleb128 audio_element_id,
                                       DownMixingParams& down_mixing_params);

  /*!\brief Adds a new demixing parameter block.
   *
   * \param parameter_block Pointer to the new demixing parameter block to add.
   */
  void AddDemixingParameterBlock(const ParameterBlockWithData* parameter_block);

  /*!\brief Adds a new recon gain parameter block.
   *
   * \param parameter_block Pointer to the new recon gain parameter block to
   * add.
   */
  void AddReconGainParameterBlock(
      const ParameterBlockWithData* parameter_block);

  /*!\brief Updates the state of demixing parameters for an audio element.
   *
   * Also validates the timestamp is as expected.
   *
   * \param audio_element_id Audio Element ID whose corresponding demixing
   *     state are to be updated.
   * \param expected_timestamp Expected timestamp of the next set of
   *     demixing parameter blocks.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateDemixingState(DecodedUleb128 audio_element_id,
                                   int32_t expected_timestamp);

  /*!\brief Updates the state of recon gain parameters for an audio element.
   *
   * Also validates the timestamp is as expected.
   *
   * \param audio_element_id Audio Element ID whose corresponding recon gain
   *     state are to be updated.
   * \param expected_timestamp Expected timestamp of the next set of
   *     recon gain parameter blocks.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateReconGainState(DecodedUleb128 audio_element_id,
                                    int32_t expected_timestamp);

 private:
  // State used when generating demixing parameters for an audio element.
  struct DemixingState {
    const DemixingParamDefinition* param_definition;

    // `w_idx` for the frame just processed, i.e. `wIdx(k - 1)` in the Spec.
    int previous_w_idx;

    // `w_idx` used to process the current frame, i.e. `wIdx(k)` in the Spec.
    int w_idx;

    // Timestamp for the next frame to be processed.
    int32_t next_timestamp;

    // Update rule of the currently tracked demixing parameters, because the
    // first frame needs some special treatment.
    DemixingInfoParameterData::WIdxUpdateRule update_rule;
  };

  struct ReconGainState {
    const ReconGainParamDefinition* param_definition;

    // Timestamp for the next frame to be processed.
    int32_t next_timestamp;
  };

  // Mapping from Audio Element ID to audio element data.
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
      audio_elements_;

  // Mapping from Parameter ID to demixing parameter blocks.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      demixing_parameter_blocks_;

  // Mapping from Parameter ID to recon gain parameter blocks.
  absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>
      recon_gain_parameter_blocks_;

  // Mapping from Audio Element ID to the demixing state.
  absl::flat_hash_map<DecodedUleb128, DemixingState> demixing_states_;

  // Mapping from Audio Element ID to the recon gain state.
  absl::flat_hash_map<DecodedUleb128, ReconGainState> recon_gain_states_;
};

}  // namespace iamf_tools

#endif  // CLI_PARAMETERS_MANAGER_H_
