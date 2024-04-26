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
#include <memory>
#include <string>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/codec_config.h"

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

/*\!brief Decodes Audio Frame OBUs based on the associated codec.
 *
 * This class is related to the "Codec Decoder" as used in the IAMF
 * specification. "The Codec Decoder for each Audio Substream outputs the
 * decoded channels."
 *
 * This class manages the underlying codec decoders for all substreams. Codec
 * decoders may be stateful; this class manages a one-to-one mapping between
 * codec decoders and substream.
 *
 * Call `InitDecodersForSubstreams` with pairs of `SubstreamIdLabelsMap` and
 * `CodecConfigObu`. This typically will require one call per Audio Element OBU.
 *
 * Then call `Decode` repeatedly with a list of `AudioFrameWithData`. There may
 * be multiple `AudioFrameWithData`s in a single call to this function. Each
 * substream in the list is assumed to be self-consistent in temporal order. It
 * is permitted in any order relative to other substreams.
 */
class AudioFrameDecoder {
 public:
  /*\!brief Constructor.
   *
   * \param output_wav_directory Directory to write debugging wav files to.
   * \param file_prefix File name prefix for debugging files.
   */
  AudioFrameDecoder(absl::string_view output_wav_directory,
                    absl::string_view file_prefix)
      : output_wav_directory_(output_wav_directory),
        file_prefix_(file_prefix) {}

  /*\!brief Initialize codec decoders for each substream.
   *
   * \param substream_id_to_labels Substreams and their associated labels to
   *     initialize. The number of channels is determined by the number of
   *     labels.
   * \param codec_config Codec Config OBU to use for all substreams.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitDecodersForSubstreams(
      const SubstreamIdLabelsMap& substream_id_to_labels,
      const CodecConfigObu& codec_config);

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

  // A map of substream IDs to the relevant decoder and codec config. This is
  // necessary to process streams with stateful decoders correctly.
  absl::node_hash_map<uint32_t, std::unique_ptr<DecoderBase>>
      substream_id_to_decoder_;

  // A map of substream IDs to the relevant wav writer.
  absl::node_hash_map<uint32_t, WavWriter> substream_id_to_wav_writer_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_DECODER_H_
