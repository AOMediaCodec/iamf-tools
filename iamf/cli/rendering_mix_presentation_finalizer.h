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

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/loudness_calculator_factory_base.h"
#include "iamf/cli/mix_presentation_finalizer.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/renderer_factory.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

class RenderingMixPresentationFinalizer : public MixPresentationFinalizerBase {
 public:
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
      : MixPresentationFinalizerBase(),
        file_path_prefix_(file_path_prefix),
        output_wav_file_bit_depth_override_(output_wav_file_bit_depth_override),
        validate_loudness_(validate_loudness),
        renderer_factory_(std::move(renderer_factory)),
        loudness_calculator_factory_(std::move(loudness_calculator_factory)) {}

  /*!\brief Destructor.
   */
  ~RenderingMixPresentationFinalizer() override = default;

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
      std::list<MixPresentationObu>& mix_presentation_obus) override;

 private:
  const std::filesystem::path file_path_prefix_;
  const std::optional<uint8_t> output_wav_file_bit_depth_override_;
  const bool validate_loudness_;

  const std::unique_ptr<RendererFactoryBase> renderer_factory_;
  const std::unique_ptr<LoudnessCalculatorFactoryBase>
      loudness_calculator_factory_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERING_MIX_PRESENTATION_FINALIZER_H_
