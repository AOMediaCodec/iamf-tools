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
#ifndef CLI_IAMF_LITE_ENCODER_FACTORY_UTILS_H_
#define CLI_IAMF_LITE_ENCODER_FACTORY_UTILS_H_

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Creates an `IASequenceHeaderObu` for a single-element stream.
 *
 * Selects the minimum IAMF profile version that supports `audio_element_type`.
 *
 * \param audio_element_type Scene-based or channel-based layout type.
 * \return Configured `IASequenceHeaderObu`.
 */
IASequenceHeaderObu MakeIaSequenceHeaderObu(
    api::AudioElementType audio_element_type);

/*!\brief Creates a 48 kHz Opus `CodecConfigObu`.
 *
 * \param codec_config_id ID to assign to the created `CodecConfigObu`.
 * \param num_samples_per_frame Frame length in samples.
 * \return `CodecConfigObu` on success, or a specific status on failure.
 */
absl::StatusOr<CodecConfigObu> CreateOpusCodecConfigObu(
    DecodedUleb128 codec_config_id, DecodedUleb128 num_samples_per_frame);

/*!\brief Creates a 16-bit little-endian linear PCM `CodecConfigObu` for
 *        `sample_rate`.
 *
 * \param sample_rate Input and output audio sample rate.
 * \param codec_config_id ID to assign to the created `CodecConfigObu`.
 * \param num_samples_per_frame Frame length in samples.
 * \return `CodecConfigObu` on success, or a specific status on failure.
 */
absl::StatusOr<CodecConfigObu> CreatePcmCodecConfigObu(
    api::PcmSampleRate sample_rate, DecodedUleb128 codec_config_id,
    DecodedUleb128 num_samples_per_frame);

/*!\brief Creates an `AudioElementWithData` for `audio_element_type`.
 *
 * \param audio_element_type Scene-based or channel-based layout type.
 * \param audio_element_id ID to assign to the created `AudioElementObu`.
 * \param codec_config_obu Associated `CodecConfigObu`. Must outlive the
 *        returned `AudioElementWithData`.
 * \return `AudioElementWithData` on success, or a specific status on failure.
 */
absl::StatusOr<AudioElementWithData> CreateAudioElementWithData(
    api::AudioElementType audio_element_type, DecodedUleb128 audio_element_id,
    const CodecConfigObu& codec_config_obu);

/*!\brief Creates and initializes encoders for all substreams in
 *        `audio_element`.
 *
 * \param audio_element Audio element describing the substream channel layouts.
 * \param codec_config_obu Codec configuration for the substream encoders.
 * \return Map of substream IDs to initialized encoders on success, or a
 *         specific status on failure.
 */
absl::StatusOr<absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>>
CreateSubstreamEncoders(const AudioElementWithData& audio_element,
                        const CodecConfigObu& codec_config_obu);

/*!\brief Creates a default `MixPresentationObu` for a single audio element.
 *
 * \param mix_presentation_id ID to assign to the created `MixPresentationObu`.
 * \param audio_element_id ID of the single audio element included in the mix.
 * \param codec_config_obu Codec configuration providing frame length and
 *        parameter rate.
 * \return `MixPresentationObu` on success, or a specific status on failure.
 */
absl::StatusOr<MixPresentationObu> CreateMixPresentationObu(
    DecodedUleb128 mix_presentation_id, DecodedUleb128 audio_element_id,
    const CodecConfigObu& codec_config_obu);

}  // namespace iamf_tools

#endif  // CLI_IAMF_LITE_ENCODER_FACTORY_UTILS_H_
