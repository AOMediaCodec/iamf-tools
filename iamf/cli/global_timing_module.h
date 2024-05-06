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

#ifndef CLI_GLOBAL_TIMING_MODULE_H_
#define CLI_GLOBAL_TIMING_MODULE_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

class GlobalTimingModule {
 public:
  /*\!brief Constructor.
   */
  GlobalTimingModule() = default;

  /*\!brief Initializes a Global Timing Module.
   *
   * Must be called before calling `GetNextAudioFrameTimestamps()` and
   * `GetNextParameterBlockTimestamps()`.
   *
   * \param audio_elements Audio Element OBUs with data to search for sample
   *     rates.
   * \param codec_config_obus Codec Config OBUs to search for sample rates.
   * \param param_definitions Parameter definitions keyed by parameter IDs.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize(
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements,
      const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
      const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
          param_definitions);

  /*\!brief Gets the start and end timestamps of the next Audio Frame.
   *
   * \param audio_substream_id Substream ID of the Audio Frame.
   * \param duration Duration of this frame measured in ticks.
   * \param start_timestamp Output start timestamp.
   * \param end_timestamp Output end timestamp.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetNextAudioFrameTimestamps(DecodedUleb128 audio_substream_id,
                                           uint32_t duration,
                                           int32_t& start_timestamp,
                                           int32_t& end_timestamp);

  /*\!brief Gets the start and end timestamps of the next Parameter Block.
   *
   * \param parameter_id ID of the Parameter Block
   * \param input_start_timestamp Start timestamp specified by the user. Will be
   *     used to check if there are gaps.
   * \param duration Duration of this Parameter Block measured in ticks.
   * \param start_timestamp Output start timestamp.
   * \param end_timestamp Output end timestamp.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetNextParameterBlockTimestamps(DecodedUleb128 parameter_id,
                                               int32_t input_start_timestamp,
                                               uint32_t duration,
                                               int32_t& start_timestamp,
                                               int32_t& end_timestamp);

  /*\!brief Validates that a parameter block covers an audio frame's duration.
   *
   * \param parameter_id ID of the Parameter Block; for logging purposes.
   * \param parameter_block_start Start timestamp of the parameter block
   *
   * \param parameter_block_end End timestamp of the parameter block.
   * \param audio_substream_id Audio substream ID of the audio frame to be
   *     covered.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ValidateParameterBlockCoversAudioFrame(
      DecodedUleb128 parameter_id, int32_t parameter_block_start,
      int32_t parameter_block_end, DecodedUleb128 audio_substream_id) const;

 private:
  struct TimingData {
    // Ticks per second, used by audio sample rate and parameter rate.
    const uint32_t rate;

    // Starting timestamp of the entire stream.
    const int32_t global_start_timestamp;

    // Measured in ticks implied by `rate`.
    int32_t timestamp;
  };

  absl::Status GetTimestampsForId(
      DecodedUleb128 id, uint32_t duration,
      absl::flat_hash_map<DecodedUleb128, TimingData>& id_to_timing_data,
      int32_t& start_timestamp, int32_t& end_timestamp);

  absl::flat_hash_map<DecodedUleb128, TimingData> audio_frame_timing_data_;
  absl::flat_hash_map<DecodedUleb128, TimingData> parameter_block_timing_data_;
};

}  // namespace iamf_tools

#endif  // CLI_GLOBAL_TIMING_MODULE_H_
