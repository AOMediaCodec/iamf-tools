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

#include <cstddef>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"
#include "iamf/cli/wav_writer.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

constexpr int kOutputWavChannels = 16;

/*!\brief Invokes panner to convert audio objects to 3rd order ambisonics.
 *
 * \param input_filename Input wav file to panner.
 * \param input_adm Input ADM struct to panner.
 * \param block_indices Vector holding the index of audioBlockFormat(s)
 *        which contain the positional metadata for each channel.
 * \param wav_writer WavWriter used for writing into the output file.
 * \return `absl::OkStatus()` on success. A specific error code on failure.
 */
absl::Status PanObjectsToAmbisonics(const std::string& input_filename,
                                    const ADM& input_adm,
                                    const std::vector<size_t>& block_indices,
                                    WavWriter& wav_writer);

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
