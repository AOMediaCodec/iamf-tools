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
#include <memory>
#include <vector>

#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

struct DecodedAudioFrame {
  uint32_t substream_id;
  InternalTimestamp start_timestamp;  // Start time of this frame. Measured in
                                      // ticks from the Global Timing Module.
  InternalTimestamp end_timestamp;    // End time of this frame. Measured in
                                      // ticks from the Global Timing Module.
  uint32_t samples_to_trim_at_end;
  uint32_t samples_to_trim_at_start;

  // Decoded samples. Includes any samples that will be trimmed in processing.
  // Points to the memory location where samples were first produced.
  // TODO(b/4107595837): Find a more robust data model so that the span is
  //                     guaranteed to point to correct samples.
  absl::Span<const std::vector<int32_t>> decoded_samples;

  // Down-mixing parameters used to create this audio frame.
  DownMixingParams down_mixing_params;

  // Recon gain info parameter data used to adjust the gain of this audio frame.
  ReconGainInfoParameterData recon_gain_info_parameter_data;

  // The audio element with data associated with this frame.
  const AudioElementWithData* audio_element_with_data;
};

/*!\brief Decodes Audio Frame OBUs based on the associated codec.
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
  /*!\brief Constructor. */
  AudioFrameDecoder() = default;

  /*!\brief Initialize codec decoders for each substream.
   *
   * \param substream_id_to_labels Substreams and their associated labels to
   *        initialize. The number of channels is determined by the number of
   *        labels.
   * \param codec_config Codec Config OBU to use for all substreams.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status InitDecodersForSubstreams(
      const SubstreamIdLabelsMap& substream_id_to_labels,
      const CodecConfigObu& codec_config);

  /*!\brief Decodes an Audio Frame OBU.
   *
   * \param encoded_audio_frame Input Audio Frame OBU.
   * \return Decoded audio frame on success. A specific status on failure.
   */
  absl::StatusOr<DecodedAudioFrame> Decode(
      const AudioFrameWithData& encoded_audio_frame);

 private:
  // A map of substream IDs to the relevant decoder and codec config. This is
  // necessary to process streams with stateful decoders correctly.
  absl::node_hash_map<uint32_t, std::unique_ptr<DecoderBase>>
      substream_id_to_decoder_;
};

}  // namespace iamf_tools

#endif  // CLI_AUDIO_FRAME_DECODER_H_
