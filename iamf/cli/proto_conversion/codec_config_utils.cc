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
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/types.h"
#include "include/opus_defines.h"

namespace iamf_tools {

namespace {

// Bitrates which Opus documents as a reasonable range.
constexpr int kMinOpusBitrate = 6000;
constexpr int kMaxOpusBitrate = 512000;

absl::StatusOr<int32_t> GetSanitizedBitrate(
    const iamf_tools_cli_proto::OpusEncoderMetadata& opus_encoder_metadata,
    int num_channels, DecodedUleb128 substream_id) {
  // IAMF elementary streams are only ever 1 or 2 channels.
  RETURN_IF_NOT_OK(ValidateInRange(num_channels, {1, 2}, "number of channels"));

  // Extract a base bitrate and a factor, so validation and checking sentinel
  // values is only done once.
  int32_t base_bitrate = 0;
  float factor = 1.0f;
  const auto override_it =
      opus_encoder_metadata.substream_id_to_bitrate_override().find(
          substream_id);
  if (override_it !=
      opus_encoder_metadata.substream_id_to_bitrate_override().end()) {
    base_bitrate = override_it->second;
  } else if (num_channels == 1) {
    base_bitrate = opus_encoder_metadata.target_bitrate_per_channel();
  } else {
    // Sanitize the coupling rate adjustment. Under the assumption that this is
    // two channels, it would be impractical to set this outside of the range
    // [0.5, 1.0].
    // At the lower bound, the effective bitrate for coupled channels would be
    // the same as a mono channel, for highly correlated signals.
    // At the upper bound, the effective bitrate for a coupled channel would be
    // two times the rate for a mono channel, for highly disparate signals.
    RETURN_IF_NOT_OK(
        ValidateInRange(opus_encoder_metadata.coupling_rate_adjustment(),
                        {0.5f, 1.0f}, "coupling rate adjustment"));
    // `OPUS_SET_BITRATE` treats this as the bit-rate for the entire substream.
    // By default, we want `libopus` to code coupled substreams and mono
    // substreams with the same effective bit-rate per channel, when the
    // coupling rate adjustment is 1.0.
    base_bitrate = opus_encoder_metadata.target_bitrate_per_channel();
    factor = opus_encoder_metadata.coupling_rate_adjustment() * num_channels;
  }

  // Directly forward some sentinel values from `libopus` to the caller.
  if (base_bitrate == OPUS_AUTO || base_bitrate == OPUS_BITRATE_MAX) {
    return base_bitrate;
  }

  // Sanitize the base bitrate. Ensuring that the following operations will not
  // fail with numerical errors.
  RETURN_IF_NOT_OK(ValidateInRange(
      base_bitrate, {kMinOpusBitrate, kMaxOpusBitrate}, "base bitrate"));

  return static_cast<int32_t>(base_bitrate * factor + 0.5f);
}

}  // namespace

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

  auto bitrate =
      GetSanitizedBitrate(opus_encoder_metadata, num_channels, substream_id);
  if (!bitrate.ok()) {
    return bitrate.status();
  }

  return OpusEncoder::Settings{
      .use_float_api = opus_encoder_metadata.use_float_api(),
      .libopus_application_mode = application,
      .target_substream_bitrate = *bitrate};
}

}  // namespace iamf_tools
