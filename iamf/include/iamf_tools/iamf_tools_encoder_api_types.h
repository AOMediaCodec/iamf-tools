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
#ifndef API_ENCODER_TYPES_H_
#define API_ENCODER_TYPES_H_

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/parameter_block.pb.h"

namespace iamf_tools {
namespace api {

// Audio channels for this audio element, mapped by the label.
using IamfAudioElementData =
    absl::flat_hash_map<iamf_tools_cli_proto::ChannelLabel,
                        absl::Span<const double>>;

struct IamfTemporalUnitData {
  // All parameter block metadata starting in this temporal unit.
  absl::flat_hash_map<uint32_t, iamf_tools_cli_proto::ParameterBlockObuMetadata>
      parameter_block_id_to_metadata;

  // All audio elements for this temporal unit.
  absl::flat_hash_map<uint32_t, IamfAudioElementData> audio_element_id_to_data;
};

}  // namespace api
}  // namespace iamf_tools

#endif  // API_ENCODER_TYPES_H_
