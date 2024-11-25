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

#ifndef CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_
#define CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_

#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class RenderingMixPresentationFinalizer {
 public:
  // -- Rendering Metadata struct definitions --

  // Common metadata for rendering an audio element and independent of
  // each frame.
  struct AudioElementRenderingMetadata {
    std::unique_ptr<AudioElementRendererBase> renderer;

    // Pointers to the audio element and the associated codec config. They
    // contain useful information for rendering.
    const AudioElementObu* audio_element;
    const CodecConfigObu* codec_config;
  };

  // Contains rendering metadata for all audio elements in a given layout.
  struct LayoutRenderingMetadata {
    bool can_render;
    // Controlled by the WavWriterFactory; may be nullptr if the user does not
    // want a wav file written for this layout.
    std::unique_ptr<WavWriter> wav_writer;
    // Controlled by the LoudnessCalculatorFactory; may be nullptr if the user
    // does not want loudness calculated for this layout.
    std::unique_ptr<LoudnessCalculatorBase> loudness_calculator;
    std::vector<AudioElementRenderingMetadata> audio_element_rendering_metadata;
    // The number of channels in this layout.
    int32_t num_channels;
    // The start time stamp of the current frames to be rendered within this
    // layout.
    int32_t start_timestamp;
  };

  // We need to store rendering metadata for each submix, layout, and audio
  // element. This metadata will then be used to render the audio frames at each
  // timestamp. Some metadata is common to all audio elements and all layouts
  // within a submix. We also want to optionally support writing to a wav file
  // and/or calculating loudness based on the rendered output.
  struct SubmixRenderingMetadata {
    uint32_t common_sample_rate;
    uint8_t wav_file_bit_depth;
    uint8_t loudness_calculator_bit_depth;
    std::vector<SubMixAudioElement> audio_elements_in_sub_mix;
    // Mix gain applied to the entire submix.
    MixGainParamDefinition mix_gain;
    // This vector will contain one LayoutRenderingMetadata per layout in the
    // submix.
    std::vector<LayoutRenderingMetadata> layout_rendering_metadata;
  };

  // Contains rendering metadata for all submixes in a given mix presentation.
  struct MixPresentationRenderingMetadata {
    DecodedUleb128 mix_presentation_id;
    std::vector<SubmixRenderingMetadata> submix_rendering_metadata;
  };

  /*!\brief Factory for a wav writer.
   *
   * Used to control whether or not wav writers are created and control their
   * filenames.
   *
   * For example, if the user only wants a particular layout (e.g. stereo), or a
   * particular mix presentation to be rendered, then a factory could filter out
   * irrelevant mix presentations or layouts.
   *
   * \param mix_presentation_id Mix presentation ID.
   * \param sub_mix_index Index of the sub mix within the mix presentation.
   * \param layout_index Index of the layout within the sub mix.
   * \param layout Associated layout.
   * \param prefix Prefix for the output file.
   * \param num_channels Number of channels.
   * \param sample_rate Sample rate.
   * \param bit_depth Bit depth.
   * \return Unique pointer to a wav writer or `nullptr` if none is desired.
   */
  typedef absl::AnyInvocable<std::unique_ptr<WavWriter>(
      DecodedUleb128 mix_presentation_id, int sub_mix_index, int layout_index,
      const Layout& layout, const std::filesystem::path& prefix,
      int num_channels, int sample_rate, int bit_depth) const>
      WavWriterFactory;

  /*!\brief Constructor.
   *
   * \param mix_presentation_metadata Input mix presentation metadata. Only the
   *        `loudness_metadata` fields are used.
   * \param file_path_prefix Prefix for the output WAV file names.
   * \param output_wav_file_bit_depth_override If present, overrides the output
   *        WAV file bit depth.
   * \param validate_loudness If true, validate the loudness against the user
   *        provided loudness.
   * \param renderer_factory Factory to take control of for creating renderers.
   * \param renderer_factory Factory to take control of for creating loudness
   *        calculators.
   */
  RenderingMixPresentationFinalizer(
      const std::filesystem::path& file_path_prefix,
      std::optional<uint8_t> output_wav_file_bit_depth_override,
      bool validate_loudness,
      std::unique_ptr<RendererFactoryBase> renderer_factory,
      std::unique_ptr<LoudnessCalculatorFactoryBase>
          loudness_calculator_factory)
      : file_path_prefix_(file_path_prefix),
        output_wav_file_bit_depth_override_(output_wav_file_bit_depth_override),
        validate_loudness_(validate_loudness),
        renderer_factory_(std::move(renderer_factory)),
        loudness_calculator_factory_(std::move(loudness_calculator_factory)) {}

  /*!\brief Initializes the rendering mix presentation finalizer.
   *
   * Rendering metadata is extracted from the mix presentation OBUs, which will
   * be used to render the mix presentations in PushTemporalUnit. This must be
   * called before PushTemporalUnit or Finalize.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize(
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const WavWriterFactory& wav_writer_factory,
      std::list<MixPresentationObu>& mix_presentation_obus);

  /*!\brief Renders and writes a single temporal unit.
   *
   * Renders a single temporal unit for all mix presentations. It also computes
   * the loudness of the rendered samples which can be used once Finalize() is
   * called.
   *
   * \param audio_elements Input Audio Element OBUs with data.
   * \param id_to_labeled_frame Data structure of samples for a given timestamp,
   *        keyed by audio element ID and channel label.
   * \param parameter_blocks_start Start of the Input Parameter Block OBUs
   *        associated with this temporal unit.
   * \param parameter_blocks_end End of the Input Parameter Block OBUs
   *        associated with this temporal unit.
   * \param mix_presentation_obus Output list of OBUs to finalize with initial
   *        user-provided loudness information.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushTemporalUnit(
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const IdLabeledFrameMap& id_to_labeled_frame,
      const std::list<ParameterBlockWithData>::const_iterator&
          parameter_blocks_start,
      const std::list<ParameterBlockWithData>::const_iterator&
          parameter_blocks_end,
      std::list<MixPresentationObu>& mix_presentation_obus);

  /*!\brief Finalizes the list of Mix Presentation OBUs.
   *
   * Populates the loudness information for each Mix Presentation OBU. This
   * requires rendering the data which depends on several types of input OBUs.
   *
   * \param audio_elements Input Audio Element OBUs with data.
   * \param id_to_time_to_labeled_frame Data structure of samples, keyed by
   *        audio element ID, starting timestamp, and channel label.
   * \param parameter_blocks Input Parameter Block OBUs.
   * \param wav_writer_factory Factory for creating output rendered wav files.
   * \param mix_presentation_obus Output list of OBUs to finalize with initial
   *        user-provided loudness information.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Finalize(
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const IdTimeLabeledFrameMap& id_to_time_to_labeled_frame,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      const WavWriterFactory& wav_writer_factory,
      std::list<MixPresentationObu>& mix_presentation_obus);

 private:
  const std::filesystem::path file_path_prefix_;
  const std::optional<uint8_t> output_wav_file_bit_depth_override_;
  const bool validate_loudness_;

  const std::unique_ptr<RendererFactoryBase> renderer_factory_;
  const std::unique_ptr<LoudnessCalculatorFactoryBase>
      loudness_calculator_factory_;

  std::list<MixPresentationRenderingMetadata> rendering_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_
