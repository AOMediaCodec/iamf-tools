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

#ifndef CLI_ADM_TO_USER_METADATA_ADM_XML_TO_ADM_H_
#define CLI_ADM_TO_USER_METADATA_ADM_XML_TO_ADM_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "iamf/cli/adm_to_user_metadata/adm/adm_elements.h"

namespace iamf_tools {
namespace adm_to_user_metadata {

/*!\brief Parses the input XML to `ADM`.
 *
 * \param xml_data XML data to parse.
 * \param importance_threshold Threshold for to determine which audio objects to
 *     ignore. Any `audioObject`s with a lower `importance` will be dropped from
 *     the output ADM.
 * \return Output ADM on success. A specific error code on failure.
 */
absl::StatusOr<ADM> ParseXmlToAdm(absl::string_view xml_data,
                                  int32_t importance_threshold);

}  // namespace adm_to_user_metadata
}  // namespace iamf_tools

#endif  // CLI_ADM_TO_USER_METADATA_ADM_XML_TO_ADM_H_
