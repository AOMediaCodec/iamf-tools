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
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/codec/opus_encoder.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/types.h"
#include "include/opus_defines.h"

namespace iamf_tools {

absl::StatusOr<OpusEncoder::Settings> CreateOpusEncoderSettings(
    const iamf_tools_cli_proto::OpusEncoderMetadata& opus_encoder_metadata,
    int num_channels, DecodedUleb128 substream_id) {
  int application;
  switch (opus_encoder_metadata.application()) {
    using enum iamf_tools_cli_proto::OpusApplicationFlag;
    case APPLICATION_VOIP:
      application = OPUS_APPLICATION_VOIP;
      break;
    case APPLICATION_AUDIO:
      application = OPUS_APPLICATION_AUDIO;
      break;
    case APPLICATION_RESTRICTED_LOWDELAY:
      application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "Unrecognized application.", opus_encoder_metadata.application()));
  }

  // `OPUS_SET_BITRATE` treats this as the bit-rate for the entire substream.
  // Configure `libopus` so coupled substreams and mono substreams have equally
  // effective bit-rate per channel.
  float opus_rate;
  if (opus_encoder_metadata.substream_id_to_bitrate_override().contains(
          substream_id)) {
    opus_rate = opus_encoder_metadata.substream_id_to_bitrate_override().at(
        substream_id);
  } else if (num_channels > 1) {
    opus_rate = opus_encoder_metadata.target_bitrate_per_channel() *
                num_channels * opus_encoder_metadata.coupling_rate_adjustment();
  } else {
    opus_rate = opus_encoder_metadata.target_bitrate_per_channel();
  }

  return OpusEncoder::Settings{
      .use_float_api = opus_encoder_metadata.use_float_api(),
      .libopus_application_mode = application,
      .target_substream_bitrate = static_cast<int32_t>(opus_rate + 0.5f)};
}

}  // namespace iamf_tools
