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

class AudioFrameGenerator {
 public:
  /*\!brief Constructor.
   *
   * \param audio_frame_metadata Input audio frame metadata.
   * \param codec_config_metadata Input codec config metadata.
   * \param audio_elements Input Audio Element OBUs with data.
   * \param input_wav_directory Directory containing the input wav files.
   * \param output_wav_directory Directory to write the output wav files.
   * \param file_name_prefix Prefix of output wav file names.
   * \param parameters_manager Manager of parameters.
   */
  AudioFrameGenerator(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::AudioFrameObuMetadata>& audio_frame_metadata,
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadata,
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements,
      const std::string& input_wav_directory,
      const std::string& output_wav_directory,
      const std::string& file_name_prefix,
      ParametersManager& parameters_manager)
      : audio_frame_metadata_(audio_frame_metadata),
        audio_elements_(audio_elements),
        input_wav_directory_(input_wav_directory),
        output_wav_directory_(output_wav_directory),
        file_name_prefix_(file_name_prefix),
        parameters_manager_(parameters_manager) {
    for (const auto& metadata : codec_config_metadata) {
      codec_config_metadata_[metadata.codec_config_id()] =
          metadata.codec_config();
    }
  }

  // TODO(b/306319126): Generate one audio frame at a time.
  /*\!brief Generates a list of Audio Frame OBUs from the input metadata.
   *
   * \param demixing_module Demixng module.
   * \param global_timing_module Global Timing Module.
   * \param audio_frames Output list of OBUs.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Generate(const DemixingModule& demixing_module,
                        GlobalTimingModule& global_timing_module,
                        std::list<AudioFrameWithData>& audio_frames);

 private:
  const ::google::protobuf::RepeatedPtrField<
      iamf_tools_cli_proto::AudioFrameObuMetadata>
      audio_frame_metadata_;

  // Mapping from Audio Element ID to audio element data.
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
      audio_elements_;

  // Mapping from Codec Config ID to additional codec config metadata used
  // to configure encoders.
  absl::flat_hash_map<DecodedUleb128, iamf_tools_cli_proto::CodecConfig>
      codec_config_metadata_;
  const std::string input_wav_directory_;
  const std::string output_wav_directory_;
  const std::string file_name_prefix_;

  // Mapping from audio substream IDs to encoders.
  absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>
      substream_id_to_encoder_;

  ParametersManager& parameters_manager_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_GENERATOR_H_
