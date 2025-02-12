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

#ifndef CLI_OBU_WITH_DATA_GENERATOR_H_
#define CLI_OBU_WITH_DATA_GENERATOR_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A collection of utility functions to generate OBUs with data.
 */
class ObuWithDataGenerator {
 public:
  /*!\brief Creates a map of `AudioElementWithData` instances.
   *
   * \param codec_config_obus Map of Codec Config OBUs.
   * \param audio_element_obus Map of Audio Element OBUs used to generate the
   *        result. OBU ownership is transferred to the returned map and
   *        `audio_element_obus` is cleared upon success.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  static absl::StatusOr<
      absl::flat_hash_map<DecodedUleb128, AudioElementWithData>>
  GenerateAudioElementsWithData(
      const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
          codec_config_obus,
      absl::flat_hash_map<DecodedUleb128, AudioElementObu>& audio_element_obus);

  /*!\brief Creates an `AudioFrameWithData` instance.
   *
   * \param audio_element_with_data `AudioElementWithData` associated to the
   *        audio frame.
   * \param audio_frame_obu Input audio frame OBU.
   * \param global_timing_module Module to keep track of frame-by-frame
   *        timestamps.
   * \param parameters_manager Maintains the state of the parameters.
   * \return Audio frame with data if the process is successful. A specific
   *         status on failure.
   */
  static absl::StatusOr<AudioFrameWithData> GenerateAudioFrameWithData(
      const AudioElementWithData& audio_element_with_data,
      const AudioFrameObu& audio_frame_obu,
      GlobalTimingModule& global_timing_module,
      ParametersManager& parameters_manager);

  /*!\brief Creates a `ParameterBlockWithData` instance.
   *
   * \param input_start_timestamp Expected start timestamp of this parameter
   *        block to check that there is no gap in the parameter substream.
   * \param global_timing_module Module to keep track of frame-by-frame
   *        timestamps.
   * \param parameter_block_obu Unique pointer to a parameter block OBU.
   *        Ownership will be transferred to the output argument.
   * \return Parameter block with data if the process is successful. A specific
   *         status on failure.
   */
  static absl::StatusOr<ParameterBlockWithData> GenerateParameterBlockWithData(
      InternalTimestamp input_start_timestamp,
      GlobalTimingModule& global_timing_module,
      std::unique_ptr<ParameterBlockObu> parameter_block_obu);

  /*!\brief Populates metadata about the layout config into the output params.
   *
   * \param audio_substream_ids Ordered list of substream IDs in the OBU.
   * \param config Scalable channel layout config to process.
   * \param substream_id_to_labels `audio_substream_id` to output label map.
   * \param label_to_output_gain Output param populated by this function.
   * \param channel_numbers_for_layers Output param populated by this function.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status FinalizeScalableChannelLayoutConfig(
      const std::vector<DecodedUleb128>& audio_substream_ids,
      const ScalableChannelLayoutConfig& config,
      SubstreamIdLabelsMap& substream_id_to_labels,
      LabelGainMap& label_to_output_gain,
      std::vector<ChannelNumbers>& channel_numbers_for_layers);

  /*!\brief Populates substream_id_to_labels for the ambisonics config.
   *
   * \param audio_element_obu Mono ambisonics config OBU to process.
   * \param substream_id_to_labels Output map of substream IDs to labels.
   * \return `absl::OkStatus()` on success. An error if the input OBU is not an
   *         ambisonics config. A specific status on failure.
   */
  static absl::Status FinalizeAmbisonicsConfig(
      const AudioElementObu& audio_element_obu,
      SubstreamIdLabelsMap& substream_id_to_labels);
};
}  // namespace iamf_tools

#endif  // CLI_OBU_WITH_DATA_GENERATOR_H_
