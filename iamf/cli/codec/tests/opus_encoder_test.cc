/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/codec/opus_encoder.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/codec/tests/encoder_test_base.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"
#include "include/opus_defines.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr bool kOverrideAudioRollDistance = true;
constexpr bool kValidateCodecDelay = true;
constexpr bool kDontValidateCodecDelay = false;
constexpr uint16_t kIncorrectPreSkip = 999;
constexpr DecodedUleb128 kCodecConfigId = 57;
constexpr int kOneChannel = 1;

TEST(Initialize, SucceedsWithDefaultSettings) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  // `OpusEncoder::Settings` has default settings, which are guaranteed to be
  // acceptable.
  const OpusEncoder::Settings default_settings;
  OpusEncoder opus_encoder(default_settings,
                           codec_config_obus.at(kCodecConfigId), kOneChannel);

  EXPECT_THAT(opus_encoder.Initialize(kValidateCodecDelay), IsOk());
}

TEST(Initialize, FailsIfApplicationModeIsInvalid) {
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  OpusEncoder::Settings settings_with_invalid_application_mode = {
      .libopus_application_mode = 0,
  };
  OpusEncoder opus_encoder(settings_with_invalid_application_mode,
                           codec_config_obus.at(kCodecConfigId), kOneChannel);

  EXPECT_THAT(opus_encoder.Initialize(kValidateCodecDelay), Not(IsOk()));
}

struct BitrateTestCase {
  int32_t bitrate;
  int num_channels;
  bool is_valid;
};

using InitializeWithBitrateTest = testing::TestWithParam<BitrateTestCase>;

TEST_P(InitializeWithBitrateTest, ValidateBitrate) {
  const auto& test_case = GetParam();

  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, codec_config_obus);
  OpusEncoder::Settings settings = {
      .target_substream_bitrate = test_case.bitrate,
  };
  OpusEncoder opus_encoder(settings, codec_config_obus.at(kCodecConfigId),
                           test_case.num_channels);

  if (test_case.is_valid) {
    EXPECT_THAT(opus_encoder.Initialize(kValidateCodecDelay), IsOk());
  } else {
    EXPECT_THAT(opus_encoder.Initialize(kValidateCodecDelay), Not(IsOk()));
  }
}

INSTANTIATE_TEST_SUITE_P(ValidateBitrateOneChannel, InitializeWithBitrateTest,
                         testing::ValuesIn<BitrateTestCase>({
                             {3000, 1, true},
                             {256000, 1, true},
                         }));

INSTANTIATE_TEST_SUITE_P(ValidateBitrateTwoChannels, InitializeWithBitrateTest,
                         testing::ValuesIn<BitrateTestCase>({
                             {6000, 2, true},
                             {512000, 2, true},
                         }));

INSTANTIATE_TEST_SUITE_P(ValidateBitrateSentinelValues,
                         InitializeWithBitrateTest,
                         testing::ValuesIn<BitrateTestCase>({
                             {OPUS_AUTO, 1, true},
                             {OPUS_BITRATE_MAX, 1, true},
                             {OPUS_AUTO, 2, true},
                             {OPUS_BITRATE_MAX, 2, true},
                         }));

INSTANTIATE_TEST_SUITE_P(
    OutOfRangeNegativeValues, InitializeWithBitrateTest,
    testing::ValuesIn<BitrateTestCase>({
        // Note that some negative values (e.g. -1) are treated as
        // valid sentinel values by `libopus`.
        {-2, 1, false},
        {std::numeric_limits<int32_t>::min(), 1, false},
    }));

class OpusEncoderTest : public EncoderTestBase, public testing::Test {
 public:
  OpusEncoderTest() { num_samples_per_frame_ = 120; }

  ~OpusEncoderTest() = default;

 protected:
  void ConstructEncoder() override {
    // Construct a Codec Config OBU. The only fields that should affect the
    // output are `num_samples_per_frame` and `decoder_config`.
    const CodecConfig temp = {.codec_id = CodecConfig::kCodecIdOpus,
                              .num_samples_per_frame = num_samples_per_frame_,
                              .decoder_config = opus_decoder_config_};

    auto codec_config = CodecConfigObu::Create(ObuHeader(), 0, temp,
                                               kOverrideAudioRollDistance);
    ASSERT_THAT(codec_config, IsOk());

    encoder_ = std::make_unique<OpusEncoder>(opus_encoder_settings_,
                                             *codec_config, num_channels_);
  }

  OpusDecoderConfig opus_decoder_config_ = {
      .version_ = 1, .pre_skip_ = 312, .input_sample_rate_ = 48000};
  OpusEncoder::Settings opus_encoder_settings_ = {
      .use_float_api = true,
      .libopus_application_mode = OPUS_APPLICATION_AUDIO,
      .target_substream_bitrate = 48000};
};

TEST_F(OpusEncoderTest, FramesAreInOrder) {
  InitExpectOk();

  // Encode several frames and ensure the correct number of frames are output in
  // the same order as the input.
  const int kNumFrames = 100;
  for (int i = 0; i < kNumFrames; i++) {
    EncodeAudioFrame(std::vector<std::vector<int32_t>>(
        num_channels_, std::vector<int32_t>(num_samples_per_frame_, i)));
  }
  FinalizeAndValidateOrderOnly(kNumFrames);
}

TEST_F(OpusEncoderTest, EncodeAndFinalizes16BitFrameSucceeds) {
  InitExpectOk();

  EncodeAudioFrame(std::vector<std::vector<int32_t>>(
      num_channels_, std::vector<int32_t>(num_samples_per_frame_, 42 << 16)));

  FinalizeAndValidateOrderOnly(1);
}

TEST_F(OpusEncoderTest, EncodeAndFinalizes16BitFrameSucceedsWithoutFloatApi) {
  opus_encoder_settings_.use_float_api = false;
  InitExpectOk();

  EncodeAudioFrame(std::vector<std::vector<int32_t>>(
      num_channels_, std::vector<int32_t>(num_samples_per_frame_, 42 << 16)));

  FinalizeAndValidateOrderOnly(1);
}

TEST_F(OpusEncoderTest, EncodeAndFinalizes24BitFrameSucceeds) {
  InitExpectOk();

  EncodeAudioFrame(std::vector<std::vector<int32_t>>(
      num_channels_, std::vector<int32_t>(num_samples_per_frame_, 42 << 8)));

  FinalizeAndValidateOrderOnly(1);
}

TEST_F(OpusEncoderTest, EncodeAndFinalizes32BitFrameSucceeds) {
  InitExpectOk();

  EncodeAudioFrame(std::vector<std::vector<int32_t>>(
      num_channels_, std::vector<int32_t>(num_samples_per_frame_, 42)));

  FinalizeAndValidateOrderOnly(1);
}

TEST_F(OpusEncoderTest, IgnoresPreSkipWhenValidateCodecDelayIsFalse) {
  opus_decoder_config_.pre_skip_ = kIncorrectPreSkip;
  ConstructEncoder();

  EXPECT_THAT(encoder_->Initialize(kDontValidateCodecDelay), IsOk());
}

TEST_F(OpusEncoderTest, ChecksPreSkipWhenValidateCodecDelayIsTrue) {
  opus_decoder_config_.pre_skip_ = kIncorrectPreSkip;
  ConstructEncoder();

  EXPECT_FALSE(encoder_->Initialize(kValidateCodecDelay).ok());
}

}  // namespace
}  // namespace iamf_tools
