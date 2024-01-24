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
#include <list>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/param_definitions.h"

namespace iamf_tools {

// TODO(b/306319126): Make this class operate iteratively; holding one set
//                    of parameter blocks at a time. Maybe see if how the
//                    Parameter Block Generator can generate one frame's worth
//                    of parameters at a time.

class ParametersManager {
 public:
  /*\!brief Constructor.
   *
   * \param audio_elements Input Audio Element OBUs with data.
   * \param parameter_blocks Input Parameter Block OBUs with data.
   */
  ParametersManager(const absl::flat_hash_map<
                        DecodedUleb128, AudioElementWithData>& audio_elements,
                    const std::list<ParameterBlockWithData>& parameter_blocks);

  /*\!brief Initializes some internal data.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize();

  /*\!brief Checks if a `DemixingParamDefinition` exists for an audio element.
   *
   * \param audio_element_id ID of the audio element to query.
   * \return True if a `DemixingParamDefinition` is available for the audio
   *     element queried.
   */
  bool DemixingParamDefinitionAvailable(DecodedUleb128 audio_element_id);

  /*\!brief Gets current down-mixing parameters for an audio element.
   *
   * \param audio_element_id ID of the audio element that the parameters are
   *     to be applied.
   * \param down_mixing_params Output down mixing parameters.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDownMixingParameters(DecodedUleb128 audio_element_id,
                                       DownMixingParams& down_mixing_params);

  /*\!brief Updates the down-mixing parameters for an audio element.
   *
   * Also validates the timestamp is as expected before updating.
   *
   * \param audio_element_id Audio Element ID whose corresponding parameters are
   *     to be updated.
   * \param expected_timestamp Expected timestamp parameters before updating.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateDownMixingParameters(DecodedUleb128 audio_element_id,
                                          int32_t expected_timestamp);

 private:
  // State used when generating demixing parameters for an audio element.
  struct DemixingState {
    const DemixingParamDefinition* param_definition;

    // Iterator to the next parameter block with a start timestamp <=
    // `next_timestamp`.
    // TODO(b/315924757): Remove this once the class tracks one parameter block
    //                    per audio element at a time.
    absl::btree_map<uint32_t, const ParameterBlockWithData*>::const_iterator
        parameter_blocks_iter;

    // `w_idx` for the frame just processed, i.e. `wIdx(k - 1)` in the Spec.
    int previous_w_idx;

    // `w_idx` used to process the current frame, i.e. `wIdx(k)` in the Spec.
    int w_idx;

    // Timestamp for the next frame to be processed.
    int32_t next_timestamp;
  };

  // Mapping from Audio Element ID to audio element data.
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
      audio_elements_;

  // Mapping from Parameter ID to parameter blocks, sorted by their start
  // timestamps.
  // TODO(b/315924757): To support an iterative structure, only the parameter
  //                    block corresponding to the current frame needs to be
  //                    tracked.
  absl::flat_hash_map<DecodedUleb128,
                      absl::btree_map<uint32_t, const ParameterBlockWithData*>>
      parameter_blocks_;

  // Mapping from Audio Element ID to the demixing state.
  absl::flat_hash_map<DecodedUleb128, DemixingState> demixing_states_;
};

}  // namespace iamf_tools

#endif  // CLI_PARAMETERS_MANAGER_H_
