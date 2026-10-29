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

#ifndef CLI_RENDERER_OAR_WRAPPER_H_
#define CLI_RENDERER_OAR_WRAPPER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions/demixing_param_definition.h"
#include "iamf/obu/rendering_config.h"

// Forward declaration of the C structure that this wrapper interfaces.
struct Oar;

namespace iamf_tools {

/*!\brief Wrapper class around the `liboar` C library.
 *
 * This class provides a simpler API to render audio samples decoded by
 * the upstream pipeline in `iamf-tools`. The use pattern of this class is:

 * // Create an instance and add audio elements to be rendered.
 * auto oar = OarWrapper::Create(...);
 * for (all audio elements) {
 *   oar->AddAudioElement(...);
 * }
 *
 * for (all frames) {
 *   // Decode audio frames and parameter blocks from upstream components.
 *   // Add input audio data and update element/output mix gains.
 *   for (all audio elements) {
 *     oar->AddInput(...);
 *     for (all parameter subblocks of the element mix gain) {
 *       oar->UpdateElementMixGain(...);
 *     }
 *     oar->UpdateDownmixMode(...);
 *     if (this audio element is object-based) {
 *       oar->UpdateObjectPositions(...);
 *     }
 *   }
 *   for (all parameter subblocks of the output mix gain) {
 *     oar->UpdateOutputMixGain(...);
 *   }
 *
 *   if (rendering binaurally) {
 *     oar->SetHeadRotation(...);
 *   }
 *   // Render.
 *   oar->Render(...);
 *   // Further processing of the rendered audio.
 * }
 *
 * NOTE: All input/output samples are arranged in a planar (channel-time)
 * layout flattened to a 1D array and accessed via `absl::Span`.
 */
class OarWrapper {
 public:
  /*!\brief Type of audio samples used by the `liboar` library. */
  using OarSampleType = float;

  // TODO(b/516667116)): Support more than just constant positions.
  /*!\brief Struct describing an object's position. */
  struct OarPositionData {
    float azimuth;
    float elevation;
    float distance;
  };

  static const size_t kMaxNumObjects;

  // TODO(b/576563728): Update or remove these limitations once clarified.
  // Currently if the output layout is binaural, these frame size limitations
  // apply.
  static constexpr size_t kBinauralMinFrameSize = 32;
  static constexpr size_t kBinauralMaxFrameSize = 16384;

  /*!\brief Factory method to create `OarWrapper`.
   *
   * \param output_layout Target output layout to render to.
   * \param sample_rate Sample rate of rendered audio.
   * \param num_output_channels Number of output channels.
   * \param num_samples_per_frame Number of samples per frame.
   * \param output_mix_gain_id Parameter ID of output mix gain.
   * \return Unique pointer to created `OarWrapper` instance on success, or
   *         `nullptr` on failure.
   */
  static std::unique_ptr<OarWrapper> Create(const Layout& output_layout,
                                            uint32_t sample_rate,
                                            uint32_t num_output_channels,
                                            uint32_t num_samples_per_frame,
                                            uint32_t output_mix_gain_id);
  /*!\brief Destroyer of an `OarWrapper` instance. */
  ~OarWrapper();

  /*!\brief Adds audio element to be rendered.
   *
   * \param audio_element_id ID of the audio element.
   * \param audio_element_type Type of the audio element.
   * \param audio_element_config Configuration of the audio element.
   * \param demixing_param_definition Optional demixing parameter definition.
   * \param rendering_config Rendering config for the audio element.
   * \param element_mix_gain_id Parameter ID of element mix gain.
   * \return `absl::OkStatus()` on success, or specific status on failure.
   */
  absl::Status AddAudioElement(
      uint32_t audio_element_id,
      AudioElementObu::AudioElementType audio_element_type,
      const AudioElementObu::AudioElementConfig& audio_element_config,
      const std::optional<DemixingParamDefinition>& demixing_param_definition,
      const RenderingConfig& rendering_config, uint32_t element_mix_gain_id);

  /*!\brief Removes an audio element.
   * \param id ID of the audio element to remove.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status RemoveAudioElement(uint32_t id);

  /*!\brief Adds input audio samples to be rendered.
   *
   * \param audio_element_id ID of the audio element that the audio samples
   *        belong to.
   * \param input_buffer Span of input samples. The size must be a full frame,
   *        i.e., `num_channels` x `num_samples_per_frame_`. The user is
   *        responsible for padding the input with zeros to make a full frame.
   * \param num_channels Number of input channels.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status AddInput(uint32_t audio_element_id,
                        absl::Span<const OarSampleType> input_buffer,
                        uint32_t num_channels);

  /*!\brief Updates the element mix gain of an audio element.
   *
   * Oar supports sub-frame parameter specifications ("parameter subblocks" in
   * the Iamf Spec). So it is possible to call this method many times within a
   * frame, as long as the total duration equals to one frame.
   *
   * \param audio_element_id ID of the audio element to be updated.
   * \param duration Duration of this parameter subblock.
   * \param default_gain_value Default gain value to be applied when the
   *        parameter ID does not correspond to any parameter stream, and the
   *        detailed `mix_gain_parameter_data` is missing.
   * \param mix_gain_parameter_data Optional mix gain parameter data defining
   *        the details of the mix gain. When it is missing, the
   *        `default_mix_gain` will be used.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateElementMixGain(
      uint32_t audio_element_id, uint32_t duration, float default_gain_value,
      const std::optional<MixGainParameterData>& mix_gain_parameter_data);

  /*!\brief Updates the output mix gain of the rendered result.
   *
   * Similar to `UpdateElementMixGain()`, except that this gain is applied as
   * to the final output.
   *
   * \param duration Duration of this parameter subblock.
   * \param default_gain_value Default gain value to be applied when the
   *        parameter ID does not correspond to any parameter stream, and the
   *        detailed `mix_gain_parameter_data` is missing.
   * \param mix_gain_parameter_data Optional mix gain parameter data defining
   *        the details of the mix gain. When it is missing, the
   *        `default_mix_gain` will be used.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status UpdateOutputMixGain(
      uint32_t duration, float default_gain_value,
      const std::optional<MixGainParameterData>& mix_gain_parameter_data);

  /*!\brief Sets the global head rotation using a quaternion representation.
   *
   * \param w W-component of the quaternion.
   * \param x X-component of the quaternion.
   * \param y Y-component of the quaternion.
   * \param z Z-component of the quaternion.
   * \param duration Duration in samples for which this metadata is valid.
   * \return absl::OkStatus() on success, or a specific status on failure.
   */
  absl::Status SetHeadRotation(float w, float x, float y, float z,
                               uint32_t duration);

  /*!\brief Updates the object positions of an audio element.
   *
   * \param audio_element_id ID of the audio element.
   * \param duration Duration in samples for which this metadata is valid.
   * \param positions Position data for each object.
   * \return absl::OkStatus() on success, or a specific status on failure.
   */
  absl::Status UpdateObjectPositions(
      uint32_t audio_element_id, uint32_t duration,
      const std::vector<OarPositionData>& positions);

  /*!\brief Updates the IAMF down-mix mode of an audio element.
   *
   * Note this only works if the input and output layout combintion requires
   * a down-mixer (e.g. from 5.1 to stereo), and that the audio element has a
   * demixing info param definition.
   *
   * \param audio_element_id ID of the audio element.
   * \param duration Duration in samples for which this metadata is valid.
   * \param mode Demixing mode.
   * \return absl::OkStatus() on success, or a specific status on failure.
   */
  absl::Status UpdateDownMixMode(uint32_t audio_element_id, uint32_t duration,
                                 DemixingInfoParameterData::DMixPMode mode);

  /*!\brief Renders the audio.
   *
   * \param output_buffer Span of the rendered output. The size must be a full
   *        frame, i.e., `num_output_channels_` x `num_samples_per_frame_`.
   *        The user is responsible for trimming the output if a partial frame
   *        is desired.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Render(absl::Span<OarSampleType> output_buffer);

  /*!\brief Enables or disables the loudness processor.
   *
   * \param enable `true` to enable, `false` to disable.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status EnableLoudnessProcessor(bool enable);

  /*!\brief Sets loudness parameters for the audio group.
   *
   * \param loudness Current loudness in dB.
   * \param target_loudness Target loudness in dB.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status SetLoudness(float loudness, float target_loudness);

  /*!\brief Enables or disables the limiter.
   *
   * \param enable `true` to enable, `false` to disable.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status EnableLimiter(bool enable);

  /*!\brief Enables or disables head tracking for all audio elements.
   *
   * When enabled, OAR will take the head-orientation data (as quaternions)
   * to compensate for listener head rotation. Counter-rotation is applied to
   * the Ambisonic bed using spherical harmonic rotation matrices, ensuring
   * the spatial audio scene remains stable in world space.
   * \param enable `true` to enable, `false` to disable.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status EnableHeadTracking(bool enable);

  /*!\brief Gets the number of samples per frame.
   *
   * \return Number of samples per frame, or 0 on error.
   */
  uint32_t GetNumSamplesPerFrame() const;

  /*!\brief Gets the sampling rate.
   *
   * \return Sampling rate, or 0 on error.
   */
  uint32_t GetSamplingRate() const;

  /*!\brief Gets the number of channels for a specific audio element.
   *
   * \param id ID of the audio element.
   * \return Number of channels, or 0 on error.
   */
  uint32_t GetNumAudioElementChannels(uint32_t id) const;

  /*!\brief Gets the number of output channels.
   *
   * \return Number of output channels, or 0 on error.
   */
  uint32_t GetNumOutputChannels() const;

  /*!\brief Gets the total number of audio elements.
   *
   * \return Total number of audio elements, or 0 on error.
   */
  uint32_t GetNumAudioElements() const;

 private:
  /*!\brief Deleter of the underlying `Oar` instance.
   *
   * This is required to declare a unique pointer to a structure of unknown
   * size (hidden in the implementation .cc file).
   */
  struct OarDeleter {
    void operator()(Oar*) const;
  };

  /*!\brief Private constructor of an `OarWrapper` instance.
   *
   * Called only by the factory method, `Create()`.
   *
   * \param oar Unique pointer to underlying `Oar` instance.
   * \param num_samples_per_frame Number of samples per frame.
   * \param group_id ID of audio group, which is only used internally
   *        by `liboar` and is not part of public API.
   * \param output_mix_gain_id Parameter ID of output mix gain.
   */
  OarWrapper(std::unique_ptr<Oar, OarDeleter> oar,
             uint32_t num_samples_per_frame, uint32_t group_id,
             uint32_t output_mix_gain_id);

  // Unique pointer to the underlying `Oar` instance.
  std::unique_ptr<Oar, OarDeleter> oar_;

  // Number of output channels.
  const uint32_t num_output_channels_;

  // Number of samples per frame.
  const uint32_t num_samples_per_frame_;

  // Audio group ID.
  const uint32_t group_id_;

  // Output mix gain parameter ID.
  const uint32_t output_mix_gain_id_;

  // Mapping from each audio element ID to its corresponding mix gain parameter
  // ID.
  absl::flat_hash_map<uint32_t, uint32_t> id_to_element_mix_gain_ids_;

  // Mapping from each audio element ID to its corresponding demixing parameter
  // ID, if available.
  absl::flat_hash_map<uint32_t, uint32_t> id_to_demixing_info_ids_;
};

}  // namespace iamf_tools

#endif  // CLI_RENDERER_OAR_WRAPPER_H_
