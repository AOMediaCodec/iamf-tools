/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_RENDERER_OAR_LAYOUT_RENDERER_H_
#define CLI_RENDERER_OAR_LAYOUT_RENDERER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_manager.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/renderer/layout_renderer_base.h"
#include "iamf/cli/renderer/oar_wrapper.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Class for rendering audio elements to a layout using OAR.
 *
 * The class hosts one `OarWrapper` instance, which adapts the OAR library
 * and renders all audio elements.
 */
class OarLayoutRenderer : public LayoutRendererBase {
 public:
  /*!\brief Creates `OarLayoutRenderer`.
   *
   * \param audio_elements_in_sub_mix Audio elements in sub-mix to render.
   * \param sub_mix_audio_elements Sub-mix audio elements containing rendering
   *        configurations.
   * \param output_mix_gain Output mix gain parameter definition.
   * \param layout Target output layout.
   * \param num_channels Number of output channels.
   * \param common_sample_rate Common sample rate of input audio elements.
   * \param common_num_samples_per_frame Common number of samples per frame.
   * \param trimming_settings Trimming settings to apply.
   * \return Unique pointer to created `OarLayoutRenderer` instance on success,
   *         or `nullptr` on failure.
   */
  static std::unique_ptr<OarLayoutRenderer> Create(
      const std::vector<const AudioElementWithData*>& audio_elements_in_sub_mix,
      const std::vector<SubMixAudioElement>& sub_mix_audio_elements,
      const MixGainParamDefinition& output_mix_gain, const Layout& layout,
      int32_t num_channels, uint32_t common_sample_rate,
      uint32_t common_num_samples_per_frame,
      TrimmingSettings trimming_settings);

  /*!\brief Destructor. */
  ~OarLayoutRenderer() override = default;

 private:
  typedef std::optional<std::vector<InternalSampleType>> OptionalDemixingMatrix;

  /*!\brief Constructor.
   *
   * \param audio_element_ids Audio element IDs to render.
   * \param element_mix_gains Element mix gain parameter definitions.
   * \param output_mix_gain Output mix gain parameter definition.
   * \param num_channels Number of output channels.
   * \param common_sample_rate Common sample rate of input audio elements.
   * \param common_num_samples_per_frame Common number of samples per frame.
   * \param audio_element_id_to_ordered_labels Mapping from audio element ID to
   *        ordered channel labels.
   * \param audio_element_id_to_demixing_matrices Mapping from audio element ID
   *        to optional demixing matrices.
   * \param audio_element_id_to_demixing_param_definitions Mapping from audio
   *        element ID to demixing parameter definitions.
   * \param trimming_settings Trimming settings to apply.
   * \param oar Unique pointer to underlying `OarWrapper` instance.
   */
  OarLayoutRenderer(
      const std::vector<DecodedUleb128>& audio_element_ids,
      const std::vector<MixGainParamDefinition>& element_mix_gains,
      const MixGainParamDefinition& output_mix_gain, int32_t num_channels,
      uint32_t common_sample_rate, uint32_t common_num_samples_per_frame,
      const absl::flat_hash_map<DecodedUleb128,
                                std::vector<ChannelLabel::Label>>&
          audio_element_id_to_ordered_labels,
      const absl::flat_hash_map<DecodedUleb128, OptionalDemixingMatrix>&
          audio_element_id_to_demixing_matrices,
      const absl::flat_hash_map<DecodedUleb128, DemixingParamDefinition>&
          audio_element_id_to_demixing_param_definitions,
      const TrimmingSettings& trimming_settings,
      std::unique_ptr<OarWrapper> oar);

  /*!\brief Prepares rendering session state before processing elements.
   *
   * \param id_to_labeled_frame Map from audio element ID to labeled frame.
   * \param id_to_parameter_block Map from parameter ID to parameter block.
   * \return `absl::OkStatus()` on success, or specific status on
   *         failure.
   */
  absl::Status PrepareOperation(
      const IdLabeledFrameMap& id_to_labeled_frame,
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block) override;

  /*!\brief Processes single audio element within rendering loop.
   *
   * \param index Index of audio element in sub-mix list.
   * \param audio_element_id ID of audio element.
   * \param labeled_frame Labeled frame of audio element.
   * \param element_mix_gain Mix gain parameter definition for audio
   *        element.
   * \param id_to_parameter_block Map from parameter ID to parameter block.
   * \return `absl::OkStatus()` on success, or specific status on
   *         failure.
   */
  absl::Status PerElementOperation(
      size_t index, DecodedUleb128 audio_element_id,
      const LabeledFrame& labeled_frame,
      const MixGainParamDefinition& element_mix_gain,
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block) override;

  /*!\brief Finalizes rendering operation after processing elements.
   *
   * \param id_to_parameter_block Map from parameter ID to parameter block.
   * \param rendered_samples Output buffer for mixed and rendered samples.
   * \param valid_rendered_samples Output span view of valid rendered
   *        samples.
   * \return `absl::OkStatus()` on success, or specific status on
   *         failure.
   */
  absl::Status FinalizeOperation(
      const absl::flat_hash_map<DecodedUleb128, const ParameterBlockWithData*>&
          id_to_parameter_block,
      std::vector<std::vector<InternalSampleType>>& rendered_samples,
      std::vector<absl::Span<const InternalSampleType>>& valid_rendered_samples)
      override;

  /*!\brief Prepares the input buffer for OAR rendering.
   *
   * Arranges the input samples according to the given ordered labels, applies
   * projection if a demixing matrix is provided, and populates the output
   * buffer (which is the input buffer fed to OAR) with interleaved samples.
   *
   * \param labeled_frame Labeled frame containing the input audio samples.
   * \param ordered_labels Span of ordered channel labels.
   * \param demixing_matrix Optional demixing matrix for projecting the samples.
   * \param oar_input_buffer Output buffer populated with interleaved samples.
   * \param num_input_channels_to_oar Output number of channels fed to OAR.
   * \return `absl::OkStatus()` on success, or a specific status on failure.
   */
  absl::Status PrepareOarInputBuffer(
      const LabeledFrame& labeled_frame,
      absl::Span<const ChannelLabel::Label> ordered_labels,
      const std::optional<absl::Span<const InternalSampleType>>&
          demixing_matrix,
      std::vector<OarWrapper::OarSampleType>& oar_input_buffer,
      uint32_t& num_input_channels_to_oar);

  // Mapping from Audio Element ID to ordered channel labels.
  const absl::flat_hash_map<DecodedUleb128, std::vector<ChannelLabel::Label>>
      audio_element_id_to_ordered_labels_;

  // Mapping from Audio Element ID to (optional) demixing matrices.
  const absl::flat_hash_map<DecodedUleb128, OptionalDemixingMatrix>
      audio_element_id_to_demixing_matrices_;

  // Mapping from Audio Element ID to demixing infos.
  const absl::flat_hash_map<DecodedUleb128, DemixingParamDefinition>
      audio_element_id_to_demixing_param_definitions_;

  // Determines whether frame start/end sample trimming is applied on rendered
  // samples.
  const TrimmingSettings trimming_settings_;

  // Pre-initialized audio frame of an empty channel that can be reused (for
  // example to represent an omitted channel).
  const std::vector<InternalSampleType> empty_channel_;

  // Wrapper that interfaces the underlying OAR library.
  std::unique_ptr<OarWrapper> oar_;

  // The input/output buffers can be reused because the content is immediately
  // copied to somewhere else after adding input to OAR or rendering.
  std::vector<OarWrapper::OarSampleType> oar_input_buffer_;
  std::vector<OarWrapper::OarSampleType> oar_output_buffer_;

  // Local state to store trimming values accumulated during the element loop.
  std::optional<uint32_t> common_num_samples_to_trim_at_start_;
  std::optional<uint32_t> common_num_samples_to_trim_at_end_;
  std::optional<size_t> common_num_valid_ticks_;

  // Various intermediate buffers used during rendering.
  std::vector<absl::Span<const InternalSampleType>> arranged_samples_;
  std::vector<std::vector<InternalSampleType>> projected_samples_;
  std::vector<absl::Span<InternalSampleType>> projected_samples_spans_;
  std::vector<absl::Span<const InternalSampleType>> input_spans_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERER_OAR_LAYOUT_RENDERER_H_
