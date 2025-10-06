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
#include "iamf/cli/proto_conversion/codec_config_utils.h"

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/types.h"
#include "include/opus_defines.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

using iamf_tools_cli_proto::OpusEncoderMetadata;

constexpr int kOneChannel = 1;
constexpr int kTwoChannels = 2;
constexpr DecodedUleb128 kSubstreamId = 99;

OpusEncoderMetadata CreateOpusEncoderMetadata() {
  OpusEncoderMetadata opus_encoder_metadata;
  opus_encoder_metadata.set_use_float_api(true);
  opus_encoder_metadata.set_application(iamf_tools_cli_proto::APPLICATION_VOIP);
  opus_encoder_metadata.set_target_bitrate_per_channel(48000);
  return opus_encoder_metadata;
}

TEST(CreateOpusEncoderSettings, SetsUseFloatApi) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_use_float_api(false);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_FALSE(settings->use_float_api);
}

TEST(CreateOpusEncoderSettings, SetsApplicationMode) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_application(
      iamf_tools_cli_proto::APPLICATION_RESTRICTED_LOWDELAY);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->libopus_application_mode,
            OPUS_APPLICATION_RESTRICTED_LOWDELAY);
}

TEST(CreateOpusEncoderSettings, ReturnsErrorForInvalidApplicationMode) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_application(
      iamf_tools_cli_proto::APPLICATION_INVALID);

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                        kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings, SetsTargetBitratePerChannelForOneChannel) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, 96000);
}

TEST(CreateOpusEncoderSettings, SetsSentinelBitrateForOneChannel) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(OPUS_AUTO);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, OPUS_AUTO);
}

TEST(CreateOpusEncoderSettings,
     MultipliesTargetBitratePerChannelForTwoChannels) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  // By default coupled channels will be assigned double the bitrate.
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, 192000);
}

TEST(CreateOpusEncoderSettings,
     MultipliesNumberOfChannelsByCouplingRateAdjustment) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);
  // When the input audio data is highly correlated, the user may prefer to have
  // a factor applied to coupled channels.
  opus_encoder_metadata.set_coupling_rate_adjustment(0.75);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, 144000);
}

TEST(CreateOpusEncoderSettings, SetsSentinelBitrateForTwoChannels) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(OPUS_BITRATE_MAX);

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, OPUS_BITRATE_MAX);
}

TEST(CreateOpusEncoderSettings, CouplingRateAdjustmentIsIgnoredForOneChannel) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);
  opus_encoder_metadata.set_coupling_rate_adjustment(0.75);

  // Often, an IAMF will have a combination of elementary and coupled and
  // singular streams. The coupling rate adjustment is ignored for singular
  // streams.
  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, 96000);
}

TEST(CreateOpusEncoderSettings, MayOverrideBitrateForOneChannelSubstreamId) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);
  // Bitrate may be overridden per ID. For example, to reduce the bitrate of the
  // least important channels.
  opus_encoder_metadata.mutable_substream_id_to_bitrate_override()->insert(
      {kSubstreamId, 24000});

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, 24000);
}

TEST(CreateOpusEncoderSettings, SetsOverrideToSentinelBitrate) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  // Bitrate may be overridden per ID, even to sentinel values.
  opus_encoder_metadata.mutable_substream_id_to_bitrate_override()->insert(
      {kSubstreamId, OPUS_AUTO});

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, OPUS_AUTO);
}

TEST(CreateOpusEncoderSettings, MayOverrideBitrateForTwoChannelSubstream) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);
  // When the user overrides the bitrate, we obey it exactly. We do not apply
  // and changes based on whether the substream is coupled or not.
  opus_encoder_metadata.mutable_substream_id_to_bitrate_override()->insert(
      {kSubstreamId, 24000});

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  EXPECT_EQ(settings->target_substream_bitrate, 24000);
}

TEST(CreateOpusEncoderSettings, IgnoresBitrateOverrideForDifferentSubstreamId) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(96000);
  constexpr DecodedUleb128 kOtherSubstreamId = kSubstreamId + 1;
  opus_encoder_metadata.mutable_substream_id_to_bitrate_override()->insert(
      {kOtherSubstreamId, 24000});

  auto settings = CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                            kSubstreamId);

  ASSERT_THAT(settings, IsOk());
  // The override for the other substream should be ignored.
  EXPECT_EQ(settings->target_substream_bitrate, 96000);
}

TEST(CreateOpusEncoderSettings, ReturnsErrorForUnsanitizedNumChannels) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  const int kInvalidNumChannels = std::numeric_limits<int32_t>::max();

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata,
                                        kInvalidNumChannels, kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings,
     ReturnsErrorForUnsanitizedLargeBitrateOneChannel) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_target_bitrate_per_channel(
      std::numeric_limits<int32_t>::max());

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                        kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings,
     ReturnsErrorForUnsanitizedLargeBitrateTwoChannels) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  // Set a value which will overflow when multiplied by two.
  opus_encoder_metadata.set_target_bitrate_per_channel(
      std::numeric_limits<int32_t>::max());

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                        kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings,
     ReturnsErrorForUnsanitizedLargeBitrateOverride) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.mutable_substream_id_to_bitrate_override()->insert(
      {kSubstreamId, std::numeric_limits<int32_t>::max()});

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kOneChannel,
                                        kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings,
     ReturnsErrorForUnsanitizedCouplingRateAdjustmentInfinity) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_coupling_rate_adjustment(
      std::numeric_limits<float>::infinity());

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                        kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings,
     ReturnsErrorForUnsanitizedCouplingRateAdjustmentNan) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_coupling_rate_adjustment(std::nan(""));

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                        kSubstreamId),
              Not(IsOk()));
}

TEST(CreateOpusEncoderSettings,
     ReturnsErrorForUnsanitizedLargeCouplingRateAdjustment) {
  auto opus_encoder_metadata = CreateOpusEncoderMetadata();
  opus_encoder_metadata.set_coupling_rate_adjustment(
      std::numeric_limits<int32_t>::max());

  EXPECT_THAT(CreateOpusEncoderSettings(opus_encoder_metadata, kTwoChannels,
                                        kSubstreamId),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
