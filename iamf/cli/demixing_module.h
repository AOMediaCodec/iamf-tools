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
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

struct SubstreamData {
  uint32_t substream_id;

  // Samples arranged in a FIFO queue with a vector of channels. There can only
  // be one or two channels. Includes "virtual" samples that are output from the
  // encoder, but are not passed to the encoder.
  std::deque<std::vector<InternalSampleType>> samples_obu;

  // Samples to pass to encoder.
  std::deque<std::vector<int32_t>> samples_encode;
  // One or two elements; corresponding to the output gain to be applied to
  // each channel.
  std::vector<double> output_gains_linear;
  uint32_t num_samples_to_trim_at_end;
  uint32_t num_samples_to_trim_at_start;
};

// Mapping from channel label to a frame of samples.
typedef absl::node_hash_map<ChannelLabel::Label,
                            std::vector<InternalSampleType>>
    LabelSamplesMap;

struct LabeledFrame {
  InternalTimestamp end_timestamp;
  uint32_t samples_to_trim_at_end;
  uint32_t samples_to_trim_at_start;
  LabelSamplesMap label_to_samples;
  DownMixingParams demixing_params;
  ReconGainInfoParameterData recon_gain_info_parameter_data;
  // Vector of length `num_layers`. Only populated for scalable channel audio.
  std::vector<ChannelAudioLayerConfig::LoudspeakerLayout>
      loudspeaker_layout_per_layer;
};

// Mapping from audio element ids to `LabeledFrame`s.
typedef absl::flat_hash_map<DecodedUleb128, LabeledFrame> IdLabeledFrameMap;

typedef absl::Status (*Demixer)(const DownMixingParams&, LabelSamplesMap&);

/*!\brief Manages data and processing to down-mix and demix audio elements.
 *
 * This class relates to the "Element Reconstructor" as used in the IAMF
 * specifications. "An Element Reconstructor re-assembles the Audio Elements by
 * combining the Channel Group(s) guided by Descriptors and Parameter
 * Substream(s)." This class does not apply the reconstruction gain, so
 * additional post processing is needed to finish audio element reconstruction.
 *
 * Down-mixers are used to down-mix the input channels to the substream
 * channels. Typically there are down-mixers for scalable channel audio
 * elements with more than one layer. Down-mixers are created according to
 * https://aomediacodec.github.io/iamf/#iamfgeneration-scalablechannelaudio-downmixmechanism
 *
 * Demixers are used to recreate the original audio from the substreams.
 * Demixers are created according to
 * https://aomediacodec.github.io/iamf/#processing-scalablechannelaudio.
 */
class DemixingModule {
 public:
  struct DemixingMetadataForAudioElementId {
    std::list<Demixer> demixers;
    std::list<Demixer> down_mixers;
    SubstreamIdLabelsMap substream_id_to_labels;
    LabelGainMap label_to_output_gain;
  };

  struct DownmixingAndReconstructionConfig {
    absl::flat_hash_set<ChannelLabel::Label> user_labels;
    SubstreamIdLabelsMap substream_id_to_labels;
    LabelGainMap label_to_output_gain;
  };

  /*!\brief Creates a `DemixingModule` for down-mixing and reconstruction.
   *
   * This is most useful from the context of an encoder. For example, to encode
   * a scalable channel audio element with two layers, the input channels are
   * down-mixed according to various rules in the spec.
   *
   * Initializes metadata for each input audio element ID. The metadata includes
   * information about the channels and the specific down-mixers and demixers
   * needed for that audio element.
   *
   * \param id_to_config_map Map of Audio Element IDs to
   *        `DownmixingAndReconstructionConfig`, which contains the
   *        user-provided labels and the `substream_id_to_labels` and
   *        `label_to_output_gain` from the corresponding
   *        `AudioElementWithData`.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::StatusOr<DemixingModule> CreateForDownMixingAndReconstruction(
      const absl::flat_hash_map<DecodedUleb128,
                                DownmixingAndReconstructionConfig>&&
          id_to_config_map);

  /*!\brief Initializes for reconstruction (demixing) the input audio elements.
   *
   * This is most useful from the context of a decoder. For example, to decode
   * a scalable channel audio element with two layers, the substreams are
   * demixed according to various rules in the spec.
   *
   * Initializes metadata for each input audio element ID. The metadata includes
   * information about the channels and the specific down-mixers and demixers
   * needed for that audio element.
   *
   * \param audio_elements Audio elements.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::StatusOr<DemixingModule> CreateForReconstruction(
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements);

  /*!\brief Searches the input map for the target samples or demixed samples.
   *
   * \param label Label of the channel (or its demixed version) to search for.
   * \param label_to_samples Map of label to samples to search.
   * \param samples Output span to the samples if found.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the search failed.
   */
  static absl::Status FindSamplesOrDemixedSamples(
      ChannelLabel::Label label, const LabelSamplesMap& label_to_samples,
      absl::Span<const InternalSampleType>& samples);

  /*!\brief Down-mixes samples of input channels to substreams.
   *
   * \param audio_element_id Audio Element ID of these substreams.
   * \param down_mixing_params Down mixing parameters to use. Ignored when
   *        there is no associated down-mixer.
   * \param input_label_to_samples Samples in input channels organized by the
   *        channel labels.
   * \param substream_id_to_substream_data Mapping from substream IDs to
   *        substream data.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status DownMixSamplesToSubstreams(
      DecodedUleb128 audio_element_id,
      const DownMixingParams& down_mixing_params,
      LabelSamplesMap& input_label_to_samples,
      absl::flat_hash_map<uint32_t, SubstreamData>&
          substream_id_to_substream_data) const;

  /*!\brief Demix original audio samples.
   *
   * This is most useful when the original (before lossy codec) samples are
   * known, such as when encoding original audio.
   *
   * \param audio_frames Audio Frames.
   * \return Output data structure for samples, or a specific status on failure.
   */
  absl::StatusOr<IdLabeledFrameMap> DemixOriginalAudioSamples(
      const std::list<AudioFrameWithData>& audio_frames) const;

  /*!\brief Demix decoded audio samples.
   *
   * This is most useful when the decoded (after lossy codec) samples are
   * known, such as when decoding an IA Sequence, or when analyzing the effect
   * of a lossy codec to determine appropriate recon gain values.
   *
   * \param decoded_audio_frames Decoded Audio Frames.
   * \return Output data structure for samples, or a specific status on failure.
   */
  absl::StatusOr<IdLabeledFrameMap> DemixDecodedAudioSamples(
      const std::list<AudioFrameWithData>& decoded_audio_frames) const;

  /*!\brief Gets the down-mixers associated with an Audio Element ID.
   *
   * \param audio_element_id Audio Element ID
   * \param down_mixers Output pointer to the list of down-mixers.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDownMixers(DecodedUleb128 audio_element_id,
                             const std::list<Demixer>*& down_mixers) const;

  /*!\brief Gets the demixers associated with an Audio Element ID.
   *
   * \param audio_element_id Audio Element ID
   * \param demixers Output pointer to the list of demixers.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status GetDemixers(DecodedUleb128 audio_element_id,
                           const std::list<Demixer>*& demixers) const;

 private:
  enum class DemixingMode { kDownMixingAndReconstruction, kReconstruction };

  /*!\brief Private constructor.
   *
   * For use with `CreateForDownMixingAndReconstruction` and
   * `CreateForReconstruction`.
   *
   * \param demixing_mode Mode of the class.
   * \param audio_element_id_to_demixing_metadata Mapping from audio element ID
   *        to demixing metadata.
   */
  DemixingModule(
      DemixingMode demixing_mode,
      absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>&&
          audio_element_id_to_demixing_metadata)
      : demixing_mode_(demixing_mode),
        audio_element_id_to_demixing_metadata_(
            std::move(audio_element_id_to_demixing_metadata)) {}

  DemixingMode demixing_mode_;

  const absl::flat_hash_map<DecodedUleb128, DemixingMetadataForAudioElementId>
      audio_element_id_to_demixing_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_DEMIXING_MODULE_H_
