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
#ifndef CLI_TESTS_CLI_TEST_UTILS_H_
#define CLI_TESTS_CLI_TEST_UTILS_H_

#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/renderer/audio_element_renderer_base.h"
#include "iamf/cli/wav_reader.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

// A specification for a decode request. Currently used in the context of
// extracting the relevant metadata from the UserMetadata proto associated
// with a given test vector.
struct DecodeSpecification {
  uint32_t mix_presentation_id;
  uint32_t sub_mix_index;
  LoudspeakersSsConventionLayout::SoundSystem sound_system;
  uint32_t layout_index;
};

/*!\brief Adds a configurable LPCM `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param sample_rate `sample_rate` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddLpcmCodecConfigWithIdAndSampleRate(
    uint32_t codec_config_id, uint32_t sample_rate,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable Opus `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddOpusCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable Flac `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddFlacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable AAC `CodecConfigObu` to the output argument.
 *
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param codec_config_obus Map to add the OBU to keyed by `codec_config_id`.
 */
void AddAacCodecConfigWithId(
    uint32_t codec_config_id,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus);

/*!\brief Adds a configurable ambisonics `AudioElementObu` to the output.
 *
 * \param audio_element_id `audio_element_id` of the OBU to create.
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param substream_ids `substream_ids` of the OBU to create.
 * \param codec_config_obus Codec Config OBUs containing the associated OBU.
 * \param audio_elements Map to add the OBU to keyed by `audio_element_id`.
 */
void AddAmbisonicsMonoAudioElementWithSubstreamIds(
    DecodedUleb128 audio_element_id, uint32_t codec_config_id,
    const std::vector<DecodedUleb128>& substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements);

/*!\brief Adds a configurable scalable `AudioElementObu` to the output argument.
 *
 * \param audio_element_id `audio_element_id` of the OBU to create.
 * \param codec_config_id `codec_config_id` of the OBU to create.
 * \param substream_ids `substream_ids` of the OBU to create.
 * \param codec_config_obus Codec Config OBUs containing the associated OBU.
 * \param audio_elements Map to add the OBU to keyed by `audio_element_id`.
 */
void AddScalableAudioElementWithSubstreamIds(
    DecodedUleb128 audio_element_id, uint32_t codec_config_id,
    const std::vector<DecodedUleb128>& substream_ids,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementWithData>& audio_elements);

/*!\brief Adds a configurable `MixPresentationObu` to the output argument.
 *
 * \param mix_presentation_id `mix_presentation_id` of the OBU to create.
 * \param audio_element_ids `audio_element_id`s of the OBU to create.
 * \param common_parameter_id `parameter_id` of all parameters within the
 *        created OBU.
 * \param common_parameter_rate `parameter_rate` of all parameters within the
 *        created OBU.
 * \param mix_presentations List to add OBU to.
 */
void AddMixPresentationObuWithAudioElementIds(
    DecodedUleb128 mix_presentation_id,
    const std::vector<DecodedUleb128>& audio_element_id,
    DecodedUleb128 common_parameter_id, DecodedUleb128 common_parameter_rate,
    std::list<MixPresentationObu>& mix_presentations);

/*!\brief Adds a configurable generic `ParamDefinition` to the output argument.
 *
 * \param parameter_id `parameter_id` of the `ParamDefinition` to create.
 * \param parameter_rate `parameter_rate` of the `ParamDefinition` to create.
 * \param duration `duration` and `constant_subblock_duration` of the
 *        `ParamDefinition` to create.
 * \param param_definitions Map to add the `ParamDefinition` to keyed by
 *        `parameter_id`.
 */
void AddParamDefinitionWithMode0AndOneSubblock(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration,
    absl::flat_hash_map<DecodedUleb128, std::unique_ptr<ParamDefinition>>&
        param_definitions);

/*!\brief Adds a demixing parameter definition to an Audio Element OBU.
 *
 * \param parameter_id `parameter_id` of the `ParamDefinition` to add.
 * \param parameter_rate `parameter_rate` of the `ParamDefinition` to add.
 * \param duration `duration` and `constant_subblock_duration` of the
 *        `ParamDefinition` to add.
 * \param audio_element_obu Audio Element OBU to add the `ParamDefinition` to.
 * \param param_definitions Output pointer to the map to add the
 *        `ParamDefinition*` to keyed by `parameter_id`.
 */
void AddDemixingParamDefinition(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration, AudioElementObu& audio_element_obu,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>*
        param_definitions);

/*!\brief Adds a recon gain parameter definition to an Audio Element OBU.
 *
 * \param parameter_id `parameter_id` of the `ParamDefinition` to add.
 * \param parameter_rate `parameter_rate` of the `ParamDefinition` to add.
 * \param duration `duration` and `constant_subblock_duration` of the
 *        `ParamDefinition` to add.
 * \param audio_element_obu Audio Element OBU to add the `ParamDefinition` to.
 * \param param_definitions Output pointer to the map to add the
 *        `ParamDefinition*` to keyed by `parameter_id`.
 */
void AddReconGainParamDefinition(
    DecodedUleb128 parameter_id, DecodedUleb128 parameter_rate,
    DecodedUleb128 duration, AudioElementObu& audio_element_obu,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>*
        param_definitions);

/*!\brief Calls `CreateWavReader` and unwraps the `StatusOr`.
 *
 * \param filename Filename to forward to `CreateWavReader`.
 * \param num_samples_per_frame Number of samples per frame to forward to
 *        `CreateWavReader`.
 * \return Unwrapped `WavReader` created by `CreateWavReader`.
 */
WavReader CreateWavReaderExpectOk(const std::string& filename,
                                  int num_samples_per_frame = 1);

/*!\brief Renders the `LabeledFrame` flushes to the output vector.
 *
 * \param labeled_frame Labeled frame to render.
 * \param renderer Renderer to use.
 * \param output_samples Vector to flush to.
 */
void RenderAndFlushExpectOk(const LabeledFrame& labeled_frame,
                            AudioElementRendererBase* renderer,
                            std::vector<InternalSampleType>& output_samples);

/*!\brief Gets and cleans up unique file name based on the specified suffix.
 *
 * Useful when testing components that write to a single file.
 *
 * \param suffix Suffix to append to the file path.
 * \return Unique file path based on the current unit test info.
 */
std::string GetAndCleanupOutputFileName(absl::string_view suffix);

/*!\brief Gets and creates a unique directory based on the specified suffix.
 *
 * Useful when testing components that write several files to a single
 * directory.
 *
 * \param suffix Suffix to append to the directory.
 * \return Unique file path based on the current unit test info.
 */
std::string GetAndCreateOutputDirectory(absl::string_view suffix);

/*!\brief Parses a textproto file into a `UserMetadata` proto.
 *
 * This function also asserts that the file exists and is readable.
 *
 * \param textproto_filename File to parse.
 * \param user_metadata Proto to populate.
 */
void ParseUserMetadataAssertSuccess(
    const std::string& textproto_filename,
    iamf_tools_cli_proto::UserMetadata& user_metadata);

/*!\brief Computes the log-spectral distance (LSD) between two spectra.
 *
 * The log-spectral distance (LSD) is a distance measure (expressed in dB)
 * between two spectra.
 *
 * \param first_log_spectrum First log-spectrum to compare.
 * \param second_log_spectrum Second log-spectrum to compare.
 * \param threshold_db LSD Threshold to compare against, in db.
 * \return True if the log-spectral distance
 *         between the two spectra is below the specified threshold, false
 *         otherwise.
 */
bool IsLogSpectralDistanceBelowThreshold(
    const absl::Span<const InternalSampleType>& first_log_spectrum,
    const absl::Span<const InternalSampleType>& second_log_spectrum,
    double threshold_db);

/*!\brief Extracts the relevant metadata for a given test case.
 *
 * This is used to properly associate gold-standard wav files with the output of
 * the decoder.
 *
 * \param user_metadata Proto associated with a given test vector.
 * \return `DecodeSpecification`(s) for the given test case.
 */
std::vector<DecodeSpecification> GetDecodeSpecifications(
    const iamf_tools_cli_proto::UserMetadata& user_metadata);

}  // namespace iamf_tools

#endif  // CLI_TESTS_CLI_TEST_UTILS_H_
