/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#ifndef API_CONVERSION_MIX_PRESENTATION_METADATA_H_
#define API_CONVERSION_MIX_PRESENTATION_METADATA_H_

#include "iamf/api/types.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

/*!\brief Converts the API-requested OutputLayout to an internal IAMF Layout. */
Layout ApiToInternalType(api::OutputLayout api_output_layout);

}  // namespace iamf_tools

#endif  // API_CONVERSION_MIX_PRESENTATION_METADATA_H_
