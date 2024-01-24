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
#ifndef CLI_AUDIO_FRAME_DECODER_H_
#define CLI_AUDIO_FRAME_DECODER_H_

#include <cstdint>
#include <list>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"

namespace iamf_tools {

struct DecodedAudioFrame {
  uint32_t substream_id;
  int32_t start_timestamp;  // Start time of this frame. Measured in ticks from
                            // the Global Timing Module.
  int32_t end_timestamp;  // End time of this frame. Measured in ticks from the
                          // Global Timing Module.
  uint32_t samples_to_trim_at_end;
  uint32_t samples_to_trim_at_start;

  // Decoded samples. Includes any samples that will be trimmed in processing.
  std::vector<std::vector<int32_t>> decoded_samples;

  // The audio element with data associated with this frame.
  const AudioElementWithData* audio_element_with_data;
};

class AudioFrameDecoder {
 public:
  /*\!brief Constructor.
   *
   * \param output_wav_directory Directory to write debugging wav files to.
   * \param file_prefix File name prefix for debugging files.
   */
  AudioFrameDecoder(const std::string& output_wav_directory,
                    const std::string& file_prefix)
      : output_wav_directory_(output_wav_directory),
        file_prefix_(file_prefix) {}

  // TODO(b/306319126): Decode one audio frame at a time.
  /*\!brief Decodes a list of Audio Frame OBUs.
   *
   * \param encoded_audio_frames Input Audio Frame OBUs.
   * \param decoded_audio_frames Output decoded audio frames.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Decode(const std::list<AudioFrameWithData>& encoded_audio_frames,
                      std::list<DecodedAudioFrame>& decoded_audio_frames);

 private:
  const std::string output_wav_directory_;
  const std::string file_prefix_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_DECODER_H_
