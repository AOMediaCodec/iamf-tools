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

#ifndef CLI_CLI_UTIL_H_
#define CLI_CLI_UTIL_H_

#include <cstdint>
#include <list>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Gets indices for the target Layout in the mix presentation.
 *
 * This function grabs the submix index and layout index of the desired layout
 * in the mix presentation.
 *
 * \param mix_presentation_sub_mixes List of mix presentation submixes where we
 *        search for the target_layout.
 * \param target_layout Layout to get the indices for.
 * \param output_submix_index Index of the submix to use.
 * \param output_layout_index Index of the layout to use.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status GetIndicesForLayout(
    const std::vector<MixPresentationSubMix>& mix_presentation_sub_mixes,
    const Layout& target_layout, int& output_submix_index,
    int& output_layout_index);

/*!\brief Collects and validates the parameter definitions against the spec.
 *
 * When `param_definition_mode = 0`, `duration`, `num_subblocks`,
 * `constant_subblock_duration` and `subblock_duration` shall be same in all
 * parameter definitions, respectively.
 *
 * \param audio_elements List of Audio Element OBUs with data.
 * \param mix_presentation_obus List of Mix Presentation OBUs.
 * \param param_definition_variants Output map from parameter IDs to parameter
 *        definitions.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status CollectAndValidateParamDefinitions(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants);

/*!\brief Validates that two timestamps are equal.
 *
 * \param expected_timestamp Expected timestamp.
 * \param actual_timestamp Actual timestamp.
 * \param prompt Prompt message to be included in the error status when the
 *        timestamps do not match. Defaulted to be empty.
 * \return `absl::OkStatus()` if the timestamps are equal.
 *         `absl::InvalidArgumentError()` with a custom message otherwise.
 */
absl::Status CompareTimestamps(InternalTimestamp expected_timestamp,
                               InternalTimestamp actual_timestamp,
                               absl::string_view prompt = "");

/*!\brief Writes interlaced PCM samples into the output buffer.
 *
 * \param frame Input frames arranged in (channel, time) axes.
 * \param bit_depth Sample size in bits.
 * \param big_endian Whether the sample should be written as big or little
 *        endian.
 * \param buffer Buffer to resize and write to.
 * \return `absl::OkStatus()` on success. A specific status on failure.
 */
absl::Status WritePcmFrameToBuffer(
    const std::vector<std::vector<int32_t>>& frame, uint8_t bit_depth,
    bool big_endian, std::vector<uint8_t>& buffer);

/*!\brief Gets the common output sample rate and bit-deph of the input sets.
 *
 * \param sample_rates Sample rates to find the common output sample rate of.
 * \param bit_depths Bit-depths to find the common output bit-depth rate of.
 * \param common_sample_rate The output sample rate of the Codec Config OBUs or
 *        48000 if no common output sample rate is found.
 * \param common_bit_depth The output bit-depth rate of the Codec Config OBUs or
 *        16 if no common output bit depth is found.
 * \param requires_resampling False if all output sample rates and bit-depths
 *        were the same. True otherwise.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 *         either of the input hash sets are empty.
 */
absl::Status GetCommonSampleRateAndBitDepth(
    const absl::flat_hash_set<uint32_t>& sample_rates,
    const absl::flat_hash_set<uint8_t>& bit_depths,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling);

/*!\brief Gets the common samples per frame from all Codec Config OBUs
 *
 * \param codec_config_obus Codec Config OBUs to get the common frame size of.
 * \param common_samples_per_frame The frame size of all Codec Config OBUs.
 * \return `absl::OkStatus()` on success. `absl::UnknownError()` if there is no
 *         common frame size.
 */
absl::Status GetCommonSamplesPerFrame(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    uint32_t& common_samples_per_frame);

/*!\brief Logs the channel numbers.
 *
 * \param name Name of the channel to log.
 * \param channel_numbers `ChannelNumbers` of the channel to log.
 */
void LogChannelNumbers(const std::string& name,
                       const ChannelNumbers& channel_numbers);

}  // namespace iamf_tools

#endif  // CLI_CLI_UTIL_H_
