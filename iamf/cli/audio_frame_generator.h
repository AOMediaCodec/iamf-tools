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

#ifndef CLI_AUDIO_FRAME_GENERATOR_H_
#define CLI_AUDIO_FRAME_GENERATOR_H_

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/leb128.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

/*!\brief Generator of audio frames.
 *
 * The generation of audio frames can be done asynchronously, where
 * samples are added on one thread and completed frames are consumed on another.
 *
 * The use pattern of this class is:
 *
 *   - Initialize (`Initialize()`).
 *
 *   Thread 1:
 *   - Repeat until no new sample to add (by checking `TakingSamples()`):
 *     - Add samples for each audio element (`AddSamples()`).
 *   - Finalize the sample-adding process (`Finalize()`).
 *
 *   Thread 2:
 *   - Repeat until no frame to generate (by checking `GeneratingFrames()`):
 *     - Output generated frames (`OutputFrames()`).
 *     - If the output is empty, wait.
 *     - Otherwise, add the output of this round to the final result.
 */
class AudioFrameGenerator {
 public:
  /*!\brief Constructor.
   *
   * \param audio_frame_metadata Input audio frame metadata.
   * \param codec_config_metadata Input codec config metadata.
   * \param audio_elements Input Audio Element OBUs with data.
   * \param demixing_module Demixng module.
   * \param parameters_manager Manager of parameters.
   * \param global_timing_module Global Timing Module.
   */
  AudioFrameGenerator(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::AudioFrameObuMetadata>& audio_frame_metadata,
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata,
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements,
      const DemixingModule& demixing_module,
      ParametersManager& parameters_manager,
      GlobalTimingModule& global_timing_module)
      : audio_elements_(audio_elements),
        demixing_module_(demixing_module),
        parameters_manager_(parameters_manager),
        global_timing_module_(global_timing_module) {
    for (const auto& audio_frame_obu_metadata : audio_frame_metadata) {
      audio_frame_metadata_[audio_frame_obu_metadata.audio_element_id()] =
          audio_frame_obu_metadata;
    }

    for (const auto& codec_config_obu_metadata : codec_config_metadata) {
      codec_config_metadata_[codec_config_obu_metadata.codec_config_id()] =
          codec_config_obu_metadata.codec_config();
    }
  }

  /*!\brief Data structure to track the user requested trimming.
   */
  struct TrimmingState {
    int64_t user_samples_left_to_trim_at_end;
    int64_t user_samples_left_to_trim_at_start;
  };

  /*!\brief Initializes encoders and relevant data structures.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize();

  /*!\brief Returns whether the generator is still taking audio samples.
   *
   * \return True if the generator is still taking audio samples.
   */
  bool TakingSamples() const {
    return !substream_id_to_substream_data_.empty();
  }

  /*!\brief Adds samples for an Audio Element and a channel label.
   *
   * Calling this function with empty input `samples` will signal the
   * underlying encoder that the a substream has ended. Eventually when all
   * substreams are ended, `TakingSamples()` will return false.
   *
   * \param audio_element_id Audio Element ID that the added samples belong to.
   * \param label Channel label of the added samples.
   * \param samples Samples to add.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status AddSamples(DecodedUleb128 audio_element_id,
                          const std::string& label,
                          const std::vector<int32_t>& samples);

  /*!\brief Finalizes the sample-adding process.
   *
   * This will signal all underlying encoders that there are no more samples
   * to come.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Finalize();

  /*!\brief Returns whether there still are audio frames being generated.
   *
   * \return True until all underlying encoders have finished encoding, and
   *         all audio frames have been generated.
   */
  bool GeneratingFrames() const {
    absl::MutexLock lock(&mutex_);
    return !substream_id_to_encoder_.empty();
  }

  /*!\brief Outputs a list of generated Audio Frame OBUs (and associated data).
   *
   * The output frames all belong to the same temporal unit, sharing the same
   * start and end timestamps.
   *
   * \param audio_frames Output list of audio frames.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status OutputFrames(std::list<AudioFrameWithData>& audio_frames);

 private:
  // Mapping from Audio Element ID to audio frame metadata.
  absl::flat_hash_map<DecodedUleb128,
                      iamf_tools_cli_proto::AudioFrameObuMetadata>
      audio_frame_metadata_;

  // Mapping from Audio Element ID to audio element data.
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
      audio_elements_;

  // Mapping from Codec Config ID to additional codec config metadata used
  // to configure encoders.
  absl::flat_hash_map<DecodedUleb128, iamf_tools_cli_proto::CodecConfig>
      codec_config_metadata_;

  // Mapping from audio substream IDs to encoders.
  absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>
      substream_id_to_encoder_ ABSL_GUARDED_BY(mutex_);

  // Mapping from Audio Element ID to labeled samples.
  absl::flat_hash_map<DecodedUleb128, LabelSamplesMap> id_to_labeled_samples_;

  // Mapping from substream IDs to number of samples that the user requested
  // to trim at end.
  absl::flat_hash_map<uint32_t, uint32_t>
      substream_id_to_user_samples_trim_end_;

  // Mapping from substream IDs to substream data.
  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data_;

  // Mapping from substream IDs to trimming states.
  absl::flat_hash_map<uint32_t, TrimmingState> substream_id_to_trimming_state_
      ABSL_GUARDED_BY(mutex_);

  const DemixingModule& demixing_module_;
  ParametersManager& parameters_manager_;
  GlobalTimingModule& global_timing_module_;

  // Mutex to protect data accessed in different threads.
  mutable absl::Mutex mutex_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_GENERATOR_H_
