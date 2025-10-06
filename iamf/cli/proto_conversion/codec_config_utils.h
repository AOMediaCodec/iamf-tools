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

#ifndef CLI_PROTO_CONVERSION_CODEC_CONFIG_UTILS_H_
#define CLI_PROTO_CONVERSION_CODEC_CONFIG_UTILS_H_

#include "absl/status/statusor.h"
#include "iamf/cli/codec/opus_encoder.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Creates `OpusEncoder::Settings` from the input protocol buffer.
 *
 * \param opus_encoder_metadata  Input protocol buffer.
 * \param num_channels Number of channels.
 * \param substream_id Substream ID.
 * \return `OpusEncoder::Settings` on success. A specific status on failure.
 */
absl::StatusOr<OpusEncoder::Settings> CreateOpusEncoderSettings(
    const iamf_tools_cli_proto::OpusEncoderMetadata& opus_encoder_metadata,
    int num_channels, DecodedUleb128 substream_id);

}  // namespace iamf_tools

#endif  // CLI_PROTO_CONVERSION_PROTO_UTILS_H_
