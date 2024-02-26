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

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/encoder_base.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/ia.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

/*\!brief Generator of audio frames.
 *
 * The use pattern of this class is:
 *   1. Initialize (`Initialize()`).
 *   2. Repeat until finished (by checking `Finished()`):
 *     2.1. Add samples for each audio element (`AddSamples()`).
 *     2.2. Generate frames for all audio elements (`GenerateFrames()`).
 *   3. Finalize (`Finalize()`).
 */
class AudioFrameGenerator {
 public:
  /*\!brief Constructor.
   *
   * \param audio_frame_metadata Input audio frame metadata.
   * \param codec_config_metadata Input codec config metadata.
   * \param audio_elements Input Audio Element OBUs with data.
   * \param output_wav_directory Directory to write the output wav files.
   * \param file_name_prefix Prefix of output wav file names.
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
      const std::string& output_wav_directory,
      const std::string& file_name_prefix,
      const DemixingModule& demixing_module,
      ParametersManager& parameters_manager,
      GlobalTimingModule& global_timing_module)
      : audio_elements_(audio_elements),
        output_wav_directory_(output_wav_directory),
        file_name_prefix_(file_name_prefix),
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

  /*\!brief Initializes encoders and relevant data structures.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize();

  /*\!brief Adds samples for an Audio Element and a channel label.
   *
   * \param audio_element_id Audio Element ID that the added samples belong to.
   * \param label Channel label of the added samples.
   * \param samples Samples to add.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status AddSamples(DecodedUleb128 audio_element_id,
                          const std::string& label,
                          const std::vector<int32_t>& samples);

  // TODO(b/306319126): Generate one audio frame at a time.
  /*\!brief Generates a list of Audio Frame OBUs from the input metadata.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GenerateFrames();

  // TODO(b/306319126): Modify this to append only the last few frames to
  //                    the output list.
  /*\!brief Finalizes and outputs all audio frames.
   *
   * \param audio_frames Output list of OBUs.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Finalize(std::list<AudioFrameWithData>& audio_frames);

  /*\!brief Whether the generation process is finished.
   *
   * \return True if there is nothing else to be generated.
   */
  bool Finished() const { return substream_id_to_substream_data_.empty(); }

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
  const std::string output_wav_directory_;
  const std::string file_name_prefix_;

  // Mapping from audio substream IDs to encoders.
  absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>
      substream_id_to_encoder_;

  // Mapping from Audio Element ID to labeled samples.
  absl::flat_hash_map<DecodedUleb128, LabelSamplesMap> id_to_labeled_samples_;

  // Mapping from substream IDs to number of samples left to pad at the end.
  absl::flat_hash_map<uint32_t, uint32_t> substream_id_to_samples_pad_end_;

  // Mapping from substream IDs to substream data.
  absl::flat_hash_map<uint32_t, SubstreamData> substream_id_to_substream_data_;

  const DemixingModule& demixing_module_;
  ParametersManager& parameters_manager_;
  GlobalTimingModule& global_timing_module_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_GENERATOR_H_
