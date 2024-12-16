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

#ifndef CLI_ADM_TO_USER_METADATA_ADM_WAV_FILE_SPLICER_H_
#define CLI_ADM_TO_USER_METADATA_ADM_WAV_FILE_SPLICER_H_

#include <filesystem>
#include <istream>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/bw64_reader.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Splices a wav file based on the input ADM file type.
 *
 * \param output_file_path Path to output wav files to.
 * \param file_prefix File prefix to use when naming output wav files.
 * \param profile_version IAMF output spec version to use for panning the audio
 *        objects.
 * \param reader Bw64Reader associated with the input stream.
 * \param input_stream Input stream to process.
 * \param lfe_count Number of LFE channels, updated if the being converted from
 *        an extended ADM file.
 * \return `absl::OkStatus()` on success. A specific error code on failure.
 */
absl::Status SpliceWavFilesFromAdm(
    const std::filesystem::path& output_file_path,
    absl::string_view file_prefix, ProfileVersion profile_version,
    const Bw64Reader& reader, std::istream& input_stream, int& lfe_count);

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_ADM_WAV_FILE_SPLICER_H_
