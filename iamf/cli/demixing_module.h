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

#ifndef CLI_DEMIXING_MODULE_H_
#define CLI_DEMIXING_MODULE_H_

#include <cstdint>
#include <deque>
#include <list>
#include <string>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"

namespace iamf_tools {

struct SubstreamData {
  uint32_t substream_id;

  // Samples arranged in a FIFO queue with a vector of channels. There can only
  // be one or two channels. Includes "virtual" samples that are output from the
  // encoder, but are not passed to the encoder.
  std::deque<std::vector<int32_t>> samples_obu;
  // Samples to pass to encoder.
  std::deque<std::vector<int32_t>> samples_encode;
  // One or two elements; corresponding to the output gain to be applied to
  // each channel.
  std::vector<double> output_gains_linear;
  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
};

// Mapping from channel label to a frame of samples.
typedef absl::node_hash_map<std::string, std::vector<int32_t>> LabelSamplesMap;

struct LabeledFrame {
  int32_t end_timestamp;
  uint32_t samples_to_trim_at_end;
  uint32_t samples_to_trim_at_start;
  LabelSamplesMap label_to_samples;
  DownMixingParams demixing_params;
};

// Mapping from starting timestamp to a `LabeledFrame`.
typedef absl::btree_map<int32_t, LabeledFrame> TimeLabeledFrameMap;

// Mapping from audio element id to a `TimeLabeledFrameMap`.
typedef absl::flat_hash_map<DecodedUleb128, TimeLabeledFrameMap>
    IdTimeLabeledFrameMap;

typedef absl::Status (*Demixer)(const DownMixingParams&, LabelSamplesMap*);

class DemixingModule {
 public:
  /*\!brief Mapping from Audio Element ID to the demixers.
   */
  typedef absl::flat_hash_map<DecodedUleb128, std::list<Demixer>> DemixerMap;

  /*\!brief Constructor.
   *
   * \param user_metadata Input user metadata.
   * \param audio_elements Audio elements. Used only for `audio_element_id`,
   *     `substream_id_to_labels`, and `label_to_output_gain`.
   */
  DemixingModule(
      const iamf_tools_cli_proto::UserMetadata& user_metadata,
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements);

  /*\!brief Searches the input map for the target samples or demixed samples.
   *
   * \param label Label to directly search for or prefix with "D_" and search
   *     for.
   * \param label_to_samples Map of label to samples to search.
   * \param samples Output argument for the samples if found.
   * \return `absl::OkStatus()` on success. `absl::UnknownError()` if the search
   *     failed.
   */
  static absl::Status FindSamplesOrDemixedSamples(
      const std::string& label, const LabelSamplesMap& label_to_samples,
      const std::vector<int32_t>** samples);

  /*\!brief Down-mixes samples of input channels to substreams.
   *
   * \param audio_element_id Audio Element ID of these substreams.
   * \param down_mixing_params Down mixing parameters to use. Ignored when
   *     there is no associated down-mixer.
   * \param input_label_to_samples Samples in input channels organized by the
   *     channel labels.
   * \param substream_id_to_substream_data Mapping from substream IDs to
   *     substream data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DownMixSamplesToSubstreams(
      DecodedUleb128 audio_element_id,
      const DownMixingParams& down_mixing_params,
      LabelSamplesMap& input_label_to_samples,
      absl::flat_hash_map<uint32_t, SubstreamData>&
          substream_id_to_substream_data) const;

  /*\!brief Demix audio samples.
   *
   * \param audio_frames Audio Frames.
   * \param decoded_audio_frames Decoded Audio Frames.
   * \param id_to_time_to_labeled_frame Output data structure for samples.
   * \param id_to_time_to_labeled_decoded_frame Output data structure for
   *     decoded samples.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DemixAudioSamples(
      const std::list<AudioFrameWithData>& audio_frames,
      const std::list<DecodedAudioFrame>& decoded_audio_frames,
      IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
      IdTimeLabeledFrameMap& id_to_time_to_labeled_decoded_frame) const;

  /*\!brief Gets the down-mixers associated with an Audio Element ID.
   *
   * \param audio_element_id Audio Element ID
   * \param down_mixers Output pointer to the list of down-mixers.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDownMixers(DecodedUleb128 audio_element_id,
                             const std::list<Demixer>*& down_mixers) const;

  /*\!brief Gets the demixers associated with an Audio Element ID.
   *
   * \param audio_element_id Audio Element ID
   * \param demixers Output pointer to the list of demixers.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDemixers(DecodedUleb128 audio_element_id,
                           const std::list<Demixer>*& demixers) const;

 private:
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
      audio_elements_;
  absl::Status init_status_;
  DemixerMap demixer_map_;
  DemixerMap down_mixer_map_;
};

}  // namespace iamf_tools

#endif  // CLI_DEMIXING_MODULE_H_
