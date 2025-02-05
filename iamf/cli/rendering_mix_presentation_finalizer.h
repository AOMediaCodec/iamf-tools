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

#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_base.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/cli/sample_processor_base.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A class that renders and finalizes IAMF mixes.
 *
 * The use pattern of this class is:
 *   // Call the factory function and handle any errors.
 *   auto finalizer = RenderingMixPresentationFinalizer::Create(...);
 *   if(!finalizer.ok()) {
 *     // Handle error.
 *   }
 *
 *   while (source has temporal units) {
 *     // Push the next temporal unit.
 *     RETURN_IF_NOT_OK(finalizer->PushTemporalUnit(...));
 *   }
 *   // Signal that no more temporal units will be pushed.
 *   RETURN_IF_NOT_OK(finalizer->FinalizePushingTemporalUnits());
 *   // Optionally, if loudness measurements and/or validation is desired, get
 *   // the final OBUs.
 *   absl::StatusOr<...> mix_presentation_obus =
 *     finalizer->GetFinalizedMixPresentationOBUs();
 *   // Handle any errors, or use the output mix presentation OBUs.
 */
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
    // Controlled by the `SampleProcessorFactory`; may be `nullptr` if the user
    // does not want post-processing this layout.
    std::unique_ptr<SampleProcessorBase> sample_processor;
    // Controlled by the `LoudnessCalculatorFactory`; may be `nullptr` if the
    // user does not want loudness calculated for this layout.
    std::unique_ptr<LoudnessCalculatorBase> loudness_calculator;
    std::vector<AudioElementRenderingMetadata> audio_element_rendering_metadata;
    // The number of channels in this layout.
    int32_t num_channels;
    // The start time stamp of the current frames to be rendered within this
    // layout.
    int32_t start_timestamp;

    // Reusable buffer for storing rendered samples.
    std::vector<std::vector<int32_t>> rendered_samples;
  };

  // We need to store rendering metadata for each submix, layout, and audio
  // element. This metadata will then be used to render the audio frames at each
  // timestamp. Some metadata is common to all audio elements and all layouts
  // within a submix. We also want to optionally support writing to a wav file
  // and/or calculating loudness based on the rendered output.
  struct SubmixRenderingMetadata {
    uint32_t common_sample_rate;
    std::vector<SubMixAudioElement> audio_elements_in_sub_mix;
    // Mix gain applied to the entire submix.
    std::unique_ptr<MixGainParamDefinition> mix_gain;
    // This vector will contain one LayoutRenderingMetadata per layout in the
    // submix.
    std::vector<LayoutRenderingMetadata> layout_rendering_metadata;
  };

  /*!\brief Factory for a sample processor.
   *
   * Used to create a sample processor for use in post-processing the rendering.
   *
   * For example, if the user only wants a particular layout (e.g. stereo), or a
   * particular mix presentation to be saved to a wav file, then a factory could
   * select relevant layouts and mix presentations to create a `WavWriter` for.
   *
   * \param mix_presentation_id Mix presentation ID.
   * \param sub_mix_index Index of the sub mix within the mix presentation.
   * \param layout_index Index of the layout within the sub mix.
   * \param layout Associated layout.
   * \param prefix Prefix for the output file.
   * \param num_channels Number of channels.
   * \param sample_rate Sample rate of the input audio.
   * \param bit_depth Bit depth of the input audio.
   * \param num_samples_per_frame Number of samples per frame.
   * \return Unique pointer to a sample processor or `nullptr` if none is
   *         desired.
   */
  typedef absl::AnyInvocable<std::unique_ptr<SampleProcessorBase>(
      DecodedUleb128 mix_presentation_id, int sub_mix_index, int layout_index,
      const Layout& layout, int num_channels, int sample_rate, int bit_depth,
      size_t num_samples_per_frame) const>
      SampleProcessorFactory;

  /*!\brief Factory that never returns a sample processor.
   *
   * For convenience to use with `Create`.
   */
  static std::unique_ptr<SampleProcessorBase> ProduceNoSampleProcessors(
      DecodedUleb128 /*mix_presentation_id*/, int /*sub_mix_index*/,
      int /*layout_index*/, const Layout& /*layout*/, int /*num_channels*/,
      int /*sample_rate*/, int /*bit_depth*/,
      size_t /*num_samples_per_frame*/) {
    return nullptr;
  }

  /*!\brief Creates a rendering mix presentation finalizer.
   *
   * Rendering metadata is extracted from the mix presentation OBUs, which will
   * be used to render the mix presentations in PushTemporalUnit.
   *
   * \param renderer_factory Factory to create renderers, or `nullptr` to
   *        disable rendering.
   * \param loudness_calculator_factory Factory to create loudness calculators
   *        or `nullptr` to disable loudness calculation.
   * \param audio_elements Audio elements with data.
   * \param sample_processor_factory Factory to create sample processors for use
   *        after rendering.
   * \param mix_presentation_obus OBUs to render and measure the loudness of.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::StatusOr<RenderingMixPresentationFinalizer> Create(
      absl::Nullable<const RendererFactoryBase*> renderer_factory,
      absl::Nullable<const LoudnessCalculatorFactoryBase*>
          loudness_calculator_factory,
      const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
      const SampleProcessorFactory& sample_processor_factory,
      const std::list<MixPresentationObu>& mix_presentation_obus);

  /*!\brief Move constructor. */
  RenderingMixPresentationFinalizer(RenderingMixPresentationFinalizer&&) =
      default;
  /*!\brief Destructor. */
  ~RenderingMixPresentationFinalizer() = default;

  /*!\brief Renders and writes a single temporal unit.
   *
   * Renders a single temporal unit for all mix presentations. It also
   * accumulates the loudness of the rendered samples which will be finalized
   * once FinalizePushingTemporalUnits() is called. This function must not be
   * called after FinalizePushingTemporalUnits() has been called.
   *
   * \param id_to_labeled_frame Data structure of samples for a given timestamp,
   *        keyed by audio element ID and channel label.
   * \param start_timestamp Start timestamp of this temporal unit.
   * \param end_timestamp End timestamp of this temporal unit.
   * \param parameter_blocks Parameter Block OBUs associated with this temporal
   *        unit.
   * \param mix_presentation_obus Output list of OBUs to finalize with initial
   *        user-provided loudness information.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status PushTemporalUnit(
      const IdLabeledFrameMap& id_to_labeled_frame, int32_t start_timestamp,
      int32_t end_timestamp,
      const std::list<ParameterBlockWithData>& parameter_blocks);

  /*!\brief Signals that `PushTemporalUnit` will no longer be called.
   *
   * Since samples will no longer be pushed, this function flushes all of the
   * sample processors. E.g. when sample processors are `WavWriter`s, the file
   * will be updated with the final header and the underlying file will be
   * closed.
   *
   * \return `absl::OkStatus()` on success. `absl::FailedPreconditionError` if
   *         this function has already been called.
   */
  absl::Status FinalizePushingTemporalUnits();

  /*!\brief Retrieves the finalized mix presentation OBUs.
   *
   * Will return mix presentation OBUs with updated loudness information. Should
   * only be called after `FinalizePushingTemporalUnits` has been called.
   *
   * \param validate_loudness If true, validate the computed loudness matches
   *        the original user-provided provided loudness.
   * \return List of finalized OBUs with calculated loudness information. A
   *         specific status on failure.
   */
  absl::StatusOr<std::list<MixPresentationObu>> GetFinalizedMixPresentationObus(
      bool validate_loudness);

 private:
  enum State { kAcceptingTemporalUnits, kFinalizePushTemporalUnitCalled };

  /*!\brief  Metadata for all sub mixes within a single mix presentation. */
  struct MixPresentationRenderingMetadata {
    // Mix presentation OBU to render and to be updated with the final measured
    // loudness.
    MixPresentationObu mix_presentation_obu;

    // Metadata for rendering all sub mixes within the mix presentation. If
    // `nullopt`, rendering is disabled for this mix presentation.
    std::optional<std::vector<SubmixRenderingMetadata>>
        submix_rendering_metadata;
  };

  /*!\brief Private constructor.
   *
   * Used only by the factory method.
   *
   * \param rendering_metadata Mix presentation metadata.
   */
  RenderingMixPresentationFinalizer(
      std::list<MixPresentationRenderingMetadata>&& rendering_metadata)
      : rendering_metadata_(std::move(rendering_metadata)) {}

  State state_ = kAcceptingTemporalUnits;

  std::list<MixPresentationRenderingMetadata> rendering_metadata_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_
