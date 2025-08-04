/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef CLI_OBU_PROCESSOR_H_
#define CLI_OBU_PROCESSOR_H_

#include <cstdint>
#include <list>
#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_decoder.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/temporal_delimiter.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

class ObuProcessor {
 public:
  // TODO(b/330732117): Remove this function and use the non-static version.
  /*!\brief Processes one Temporal Unit OBU of an IA Sequence.
   *
   * This function should only be called after successfully calling
   * ProcessDescriptorObus. Output audio frames and parameter blocks are
   * ordered by timestamps first and then by IDs.
   *
   * \param audio_elements_with_data Map containing the audio elements that
   *        were present in the descriptor OBUs, keyed by audio element ID.
   * \param codec_config_obus Map containing the codec configs that were
   *        present in the descriptor OBUs, keyed by codec config ID.
   * \param substream_id_to_audio_element Mapping from substream IDs to the
   *        audio elements that they belong to.
   * \param param_definition_variants Map containing the param definitions that
   *        were present in the descriptor OBUs, keyed by parameter ID.
   * \param parameters_manager Manager of parameters.
   * \param read_bit_buffer Buffer reader that reads the IAMF bitstream.
   * \param global_timing_module Module to keep track of the timing of audio
   *        frames and parameters.
   * \param output_audio_frame_with_data Output Audio Frame with the requisite
   *        data.
   * \param output_parameter_block_with_data Output parameter Block with the
   *        requisite data.
   * \param output_temporal_delimiter Output temporal deilimiter OBU.
   * \param continue_processing Whether the processing should be continued.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  [[deprecated(
      "Remove when all tests are ported. Use the non-static version instead.")]]
  static absl::Status ProcessTemporalUnitObu(
      const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          audio_elements_with_data,
      const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
          codec_config_obus,
      const absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>&
          substream_id_to_audio_element,
      const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
          param_definition_variants,
      ParametersManager& parameters_manager, ReadBitBuffer& read_bit_buffer,
      GlobalTimingModule& global_timing_module,
      std::optional<AudioFrameWithData>& output_audio_frame_with_data,
      std::optional<ParameterBlockWithData>& output_parameter_block_with_data,
      std::optional<TemporalDelimiterObu>& output_temporal_delimiter,
      bool& continue_processing);

  /*!\brief Creates the OBU processor.
   *
   * Creation succeeds only if the descriptor OBUs are successfully processed.
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream.
   * \param output_insufficient_data True iff the bitstream provided is
   *        insufficient to process all descriptor OBUs and there is no other
   *        error.
   * \return std::unique_ptr<ObuProcessor> on success. `nullptr` on failure.
   */
  static std::unique_ptr<ObuProcessor> Create(bool is_exhaustive_and_exact,
                                              ReadBitBuffer* read_bit_buffer,
                                              bool& output_insufficient_data);

  /*!\brief Move constructor. */
  ObuProcessor(ObuProcessor&& obu_processor) = delete;

  /*!\brief Creates the OBU processor for rendering.
   *
   * Creation succeeds only if the descriptor OBUs are successfully processed
   * and all rendering modules are successfully initialized.
   *
   * \param desired_profile_versions Profiles that are permitted to be used
   *        selecting the mix presentation.
   * \param desired_layout Specifies the desired layout that will be used to
   *        render the audio, if available in the mix presentations. If not
   *        available, the first layout in the first mix presentation will be
   *        used.
   * \param sample_processor_factory Factory to create post processors.
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream.
   * \param output_layout The layout that will be used to render the audio. This
   *        is the same as `desired_layout` if it is available in the mix
   *        presentations, otherwise a default layout is used.
   * \param output_insufficient_data True iff the bitstream provided is
   *        insufficient to process all descriptor OBUs and there is no other
   *        error.
   * \return Pointer to an ObuProcessor on success. `nullptr` on failure.
   */
  static std::unique_ptr<ObuProcessor> CreateForRendering(
      const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
      const Layout& desired_layout,
      const RenderingMixPresentationFinalizer::SampleProcessorFactory&
          sample_processor_factory,
      bool is_exhaustive_and_exact, ReadBitBuffer* read_bit_buffer,
      Layout& output_layout, bool& output_insufficient_data);

  /*!\brief Gets the sample rate of the output audio.
   *
   * \return Sample rate of the output audio, or a specific error code on
   *         failure.
   */
  absl::StatusOr<uint32_t> GetOutputSampleRate() const;

  /*!\brief Gets the frame size of the output audio.
   *
   * Useful to determine the maximum number of samples per
   * `RenderTemporalUnitAndMeasureLoudness` call.
   *
   * \return Number of samples in per frame of the output audio, or a specific
   *         specific error code on failure.
   */
  absl::StatusOr<uint32_t> GetOutputFrameSize() const;

  struct OutputTemporalUnit {
    std::list<AudioFrameWithData> output_audio_frames;
    std::list<ParameterBlockWithData> output_parameter_blocks;
    InternalTimestamp output_timestamp;
  };

  // TODO(b/379819959): Also handle Temporal Delimiter OBUs.
  /*!\brief Processes all OBUs from a Temporal Unit from the stored IA Sequence.
   *
   * \param eos_is_end_of_sequence Whether reaching the end of the stream
   *        should be considered as the end of the sequence, and therefore the
   *        end of the temporal unit.
   * \param output_temporal_unit Contains the data from the temporal unit that
   *        is processed.
   * \param continue_processing Whether the processing should be continued.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  absl::Status ProcessTemporalUnit(
      bool eos_is_end_of_sequence,
      std::optional<OutputTemporalUnit>& output_temporal_unit,
      bool& continue_processing);

  /*!\brief Renders a temporal unit and measures loudness.
   *
   * `InitializeForRendering()` must be called before calling this.
   *
   * \param timestamp Timestamp of this temporal unit. Used to verify that
   *        the input OBUs actually belong to the same temporal unit.
   * \param parameter_blocks_with_data Parameter Blocks with the requisite data.
   * \param audio_frames_with_data Audio Frames to decode in place.
   * \param output_rendered_samples Output rendered samples. These
   *        should be used immediately after this function is called; they will
   *        be invalidated after the next call to
   *        `RenderTemporalUnitAndMeasureLoudness()`, as well as after the
   *        `ObuProcessor` is destroyed.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  absl::Status RenderTemporalUnitAndMeasureLoudness(
      InternalTimestamp timestamp,
      const std::list<ParameterBlockWithData>& parameter_blocks,
      std::list<AudioFrameWithData>& audio_frames,
      absl::Span<const absl::Span<const InternalSampleType>>&
          output_rendered_samples);

  IASequenceHeaderObu ia_sequence_header_;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus_ = {};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_ =
      {};
  std::list<MixPresentationObu> mix_presentations_ = {};

 private:
  /*!\brief Private constructor used only by Create() and CreateForRendering().
   *
   * \param read_bit_buffer Pointer to the read bit buffer that reads the IAMF
   *        bitstream.
   * \return ObuProcessor instance.
   */
  explicit ObuProcessor(ReadBitBuffer* /* absl_nonnull */ buffer)
      : read_bit_buffer_(buffer) {}

  /*!\brief Processes the Descriptor OBUs of an IA Sequence.
   *
   * If insufficient data to process all descriptor OBUs is provided, a failing
   * status will be returned. `insufficient_data` will be set to true, the
   * read_bit_buffer will not be consumed, and the output parameters will not be
   * populated. A user should call this function again after providing more
   * data within the read_bit_buffer.
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
   * \param read_bit_buffer Buffer containing a portion of an iamf bitstream
   *        containing a sequence of OBUs. The buffer will be consumed up to the
   *        end of the descriptor OBUs if processing is successful.
   * \param output_sequence_header IA sequence header processed from the
   *        bitstream.
   * \param output_codec_config_obus Map of Codec Config OBUs processed from the
   *        bitstream.
   * \param output_audio_elements_with_data Map of Audio Elements and metadata
   *        processed from the bitstream.
   * \param output_mix_presentation_obus List of Mix Presentation OBUs processed
   *        from the bitstream.
   * \param insufficient_data Whether the bitstream provided is insufficient  to
   *        process all descriptor OBUs.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  static absl::Status ProcessDescriptorObus(
      bool is_exhaustive_and_exact, ReadBitBuffer& read_bit_buffer,
      IASequenceHeaderObu& output_sequence_header,
      absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
          output_codec_config_obus,
      absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
          output_audio_elements_with_data,
      std::list<MixPresentationObu>& output_mix_presentation_obus,
      bool& insufficient_data);

  /*!\brief Performs internal initialization of the OBU processor.
   *
   * Only used by Create() and CreateForRendering().
   *
   * \param is_exhaustive_and_exact Whether the bitstream provided is meant to
   *        include all descriptor OBUs and no other data. This should only be
   *        set to true if the user knows the exact boundaries of their set of
   *        descriptor OBUs.
    \param output_insufficient_data True iff the bitstream provided is
   *        insufficient to process all descriptor OBUs and there is no other
   *        error.
   * \return `absl::OkStatus()` if initialization is successful. A specific
   *        status on failure.
   */
  absl::Status InitializeInternal(bool is_exhaustive_and_exact,
                                  bool& output_insufficient_data);

  /*!\brief Initializes the OBU processor for rendering.
   *
   * Must be called after `Initialize()` is called.
   *
   * \param desired_profile_versions Profiles that are permitted to be used
   *        selecting the mix presentation.
   * \param desired_layout Specifies the layout that will be used to render the
   *        audio, if available.
   * \param sample_processor_factory Factory to create post processors.
   * \param output_layout The layout that will be used to render the audio. This
   *        is the same as `desired_layout` if it is available, otherwise a
   *        default layout is used.
   * \return `absl::OkStatus()` if the process is successful. A specific status
   *         on failure.
   */
  absl::Status InitializeForRendering(
      const absl::flat_hash_set<ProfileVersion>& desired_profile_versions,
      const Layout& desired_layout,
      const RenderingMixPresentationFinalizer::SampleProcessorFactory&
          sample_processor_factory,
      Layout& output_layout);

  struct DecodingLayoutInfo {
    DecodedUleb128 mix_presentation_id;
    int sub_mix_index;
    int layout_index;
  };

  struct TemporalUnitData {
    std::list<ParameterBlockWithData> parameter_blocks;
    std::list<AudioFrameWithData> audio_frames;

    std::optional<TemporalDelimiterObu> temporal_delimiter;
    std::optional<InternalTimestamp> timestamp;

    bool Empty() const {
      return parameter_blocks.empty() && audio_frames.empty();
    }

    void Clear() {
      audio_frames.clear();
      parameter_blocks.clear();
      temporal_delimiter.reset();
      timestamp.reset();
    }

    template <class T>
    static void AddDataToCorrectTemporalUnit(
        TemporalUnitData& current_temporal_unit,
        TemporalUnitData& next_temporal_unit, T&& obu_with_data) {
      const InternalTimestamp new_timestamp = obu_with_data.start_timestamp;
      if (!current_temporal_unit.timestamp.has_value()) {
        current_temporal_unit.timestamp = new_timestamp;
      }
      if (*current_temporal_unit.timestamp == new_timestamp) {
        current_temporal_unit.GetList<T>().push_back(
            std::forward<T>(obu_with_data));
      } else {
        next_temporal_unit.GetList<T>().push_back(
            std::forward<T>(obu_with_data));
        next_temporal_unit.timestamp = new_timestamp;
      }
    }

   private:
    template <class T>
    std::list<T>& GetList() {
      if constexpr (std::is_same_v<T, ParameterBlockWithData>) {
        return parameter_blocks;
      } else if constexpr (std::is_same_v<T, AudioFrameWithData>) {
        return audio_frames;
      }
    };
  };

  std::optional<uint32_t> output_sample_rate_;
  std::optional<uint32_t> output_frame_size_;

  absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>
      param_definition_variants_;
  absl::flat_hash_map<DecodedUleb128, const AudioElementWithData*>
      substream_id_to_audio_element_;
  std::unique_ptr<GlobalTimingModule> global_timing_module_;
  std::optional<ParametersManager> parameters_manager_;
  ReadBitBuffer* /* absl_nonnull */ read_bit_buffer_;

  // Contains target layout information for rendering.
  DecodingLayoutInfo decoding_layout_info_;

  // Cached data when processing temporal units.
  TemporalUnitData current_temporal_unit_;
  TemporalUnitData next_temporal_unit_;

  // Modules used for rendering.
  std::optional<AudioFrameDecoder> audio_frame_decoder_;
  std::optional<DemixingModule> demixing_module_;
  std::optional<RenderingMixPresentationFinalizer> mix_presentation_finalizer_;
};
}  // namespace iamf_tools
#endif  // CLI_OBU_PROCESSOR_H_
