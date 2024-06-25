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

#ifndef CLI_WAV_SAMPLE_PROVIDER_H_
#define CLI_WAV_SAMPLE_PROVIDER_H_

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/obu/leb128.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

class WavSampleProvider {
 public:
  /*!\brief Constructor.
   *
   * \param audio_frame_metadata Input audio frame metadata.
   */
  WavSampleProvider(
      const ::google::protobuf::RepeatedPtrField<
          iamf_tools_cli_proto::AudioFrameObuMetadata>& audio_frame_metadata) {
    for (const auto& audio_frame_obu_metadata : audio_frame_metadata) {
      audio_frame_metadata_[audio_frame_obu_metadata.audio_element_id()] =
          audio_frame_obu_metadata;
    }
  }

  /*!\brief Initializes WAV readers that provide samples for the audio frames.
   *
   * \param input_wav_directory Directory containing the input WAV files.
   * \param audio_elements Input Audio Element OBUs with data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize(
      const std::string& input_wav_directory,
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements);

  /*!\brief Read frames from WAV files corresponding to an Audio Element.
   *
   * \param audio_element_id ID of the Audio Element whose corresponding frames
   *      are to be read from WAV files.
   * \param labeled_samples Output samples organized by their channel labels.
   * \param finished_reading Whether the reading is finished.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ReadFrames(DecodedUleb128 audio_element_id,
                          LabelSamplesMap& labeled_samples,
                          bool& finished_reading);

 private:
  // Mapping from Audio Element ID to `WavReader`.
  absl::flat_hash_map<DecodedUleb128, WavReader> wav_readers_;

  // Mapping from Audio Element ID to audio frame metadata.
  absl::flat_hash_map<DecodedUleb128,
                      iamf_tools_cli_proto::AudioFrameObuMetadata>
      audio_frame_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_WAV_SAMPLE_PROVIDER_H_
