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
#include "iamf/cli/codec/opus_decoder.h"

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

using ::testing::IsNull;
using ::testing::Not;

constexpr uint32_t kCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 960;
constexpr uint32_t kSampleRate = 48000;
constexpr int kOneChannel = 1;
constexpr int kTwoChannels = 2;

TEST(Create, SucceedsForOneChannel) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfig(kCodecConfigId, kNumSamplesPerFrame, kSampleRate,
                     codec_config_obus);

  auto opus_decoder =
      OpusDecoder::Create(codec_config_obus.at(kCodecConfigId), kOneChannel);

  EXPECT_THAT(opus_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(Create, SucceedsForTwoChannels) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfig(kCodecConfigId, kNumSamplesPerFrame, kSampleRate,
                     codec_config_obus);

  auto opus_decoder =
      OpusDecoder::Create(codec_config_obus.at(kCodecConfigId), kTwoChannels);

  EXPECT_THAT(opus_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(Create, SucceedsForAlternativeSampleRate) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  constexpr uint32_t kSampleRate16000 = 16000;
  AddOpusCodecConfig(kCodecConfigId, kNumSamplesPerFrame, kSampleRate16000,
                     codec_config_obus);

  auto opus_decoder =
      OpusDecoder::Create(codec_config_obus.at(kCodecConfigId), kTwoChannels);

  EXPECT_THAT(opus_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(DecodeAudioFrame, SucceedsForEmptyFrame) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfig(kCodecConfigId, kNumSamplesPerFrame, kSampleRate,
                     codec_config_obus);
  auto opus_decoder =
      OpusDecoder::Create(codec_config_obus.at(kCodecConfigId), kTwoChannels);
  ASSERT_THAT(opus_decoder, IsOkAndHolds(Not(IsNull())));

  constexpr absl::Span<const uint8_t> kEmptyFrame;
  EXPECT_THAT((*opus_decoder)->DecodeAudioFrame(kEmptyFrame), IsOk());
}

}  // namespace
}  // namespace iamf_tools
