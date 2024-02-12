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

#ifndef CLI_IAMF_COMPONENTS_H_
#define CLI_IAMF_COMPONENTS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "iamf/cli/mix_presentation_finalizer.h"
#include "iamf/cli/obu_sequencer.h"
#include "iamf/cli/proto/mix_presentation.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

/*\!brief Creates an instance of `MixPresentationFinalizerBase`.
 *
 * This is useful for binding different kinds of finalizers in an IAMF Encoder.
 *
 * \param mix_presentation_metadata Input mix presentation metadata.
 * \param file_name_prefix Prefix of output file name.
 * \param output_wav_file_bit_depth_override Override for the bit-depth of
 *     the rendered wav file.
 * \return Unique pointer to the created Mix Presentation finalizer.
 */
std::unique_ptr<MixPresentationFinalizerBase> CreateMixPresentationFinalizer(
    const ::google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::MixPresentationObuMetadata>&
        mix_presentation_metadata,
    const std::string& file_name_prefix,
    std::optional<uint8_t> output_wav_file_bit_depth_override);

/*\!brief Creates instances of `ObuSequencerBase`.
 *
 * This is useful for binding different kinds of sequencers in an IAMF Encoder.
 *
 * \param user_metadata Input user metadata.
 * \param output_iamf_directory Directory to output IAMF files to.
 * \param include_temporal_delimiters Whether the serialized data should
 *     include temporal delimiters.
 * \return Vector of unique pointers to the created OBU sequencers.
 */
std::vector<std::unique_ptr<ObuSequencerBase>> CreateObuSequencers(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const std::string& output_iamf_directory, bool include_temporal_delimiters);

}  // namespace iamf_tools

#endif  // CLI_IAMF_COMPONENTS_H_
