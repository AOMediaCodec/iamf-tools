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
#include "iamf/cli/codec/aac_decoder.h"

#include <cstdint>
#include <memory>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

using ::testing::IsNull;
using ::testing::Not;

constexpr uint32_t kNumSamplesPerFrame = 1024;
constexpr uint32_t kSampleRate = 48000;
constexpr int kOneChannel = 1;
constexpr int kTwoChannels = 2;

AacDecoderConfig CreateAacDecoderConfig(uint32_t sample_rate) {
  return AacDecoderConfig{
      .buffer_size_db_ = 0,
      .max_bitrate_ = 0,
      .average_bit_rate_ = 0,
      .decoder_specific_info_ =
          {.audio_specific_config =
               {.sample_frequency_index_ =
                    AudioSpecificConfig::SampleFrequencyIndex::kEscapeValue,
                .sampling_frequency_ = sample_rate}},
  };
}

TEST(Create, SucceedsForOneChannel) {
  const AacDecoderConfig aac_decoder_config =
      CreateAacDecoderConfig(kSampleRate);

  auto aac_decoder =
      AacDecoder::Create(aac_decoder_config, kOneChannel, kNumSamplesPerFrame);

  EXPECT_THAT(aac_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(Create, SucceedsForTwoChannels) {
  const AacDecoderConfig aac_decoder_config =
      CreateAacDecoderConfig(kSampleRate);

  auto aac_decoder =
      AacDecoder::Create(aac_decoder_config, kTwoChannels, kNumSamplesPerFrame);

  EXPECT_THAT(aac_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(Create, SucceedsForAlternativeSampleRate) {
  constexpr uint32_t kSampleRate16000 = 16000;
  const AacDecoderConfig aac_decoder_config =
      CreateAacDecoderConfig(kSampleRate16000);

  auto aac_decoder =
      AacDecoder::Create(aac_decoder_config, kTwoChannels, kNumSamplesPerFrame);

  EXPECT_THAT(aac_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(DecodeAudioFrame, FailsForEmptyFrame) {
  const AacDecoderConfig aac_decoder_config =
      CreateAacDecoderConfig(kSampleRate);
  auto aac_decoder =
      AacDecoder::Create(aac_decoder_config, kTwoChannels, kNumSamplesPerFrame);
  ASSERT_THAT(aac_decoder, IsOkAndHolds(Not(IsNull())));

  constexpr absl::Span<const uint8_t> kEmptyFrame;
  EXPECT_THAT((*aac_decoder)->DecodeAudioFrame(kEmptyFrame), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
