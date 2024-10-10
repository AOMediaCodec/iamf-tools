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
#include "iamf/cli/codec/flac_encoder.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/codec/tests/encoder_test_base.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr bool kOverrideAudioRollDistance = true;
constexpr bool kIgnoredValidateCodecDelay = true;

class FlacEncoderTest : public EncoderTestBase, public testing::Test {
 public:
  FlacEncoderTest() {
    flac_encoder_metadata_.set_compression_level(0);
    num_samples_per_frame_ = 16;
    input_sample_size_ = 32;
  }

  ~FlacEncoderTest() = default;

 protected:
  void ConstructEncoder() override {
    // Construct a Codec Config OBU. The only fields that should affect the
    // output are `num_samples_per_frame` and `decoder_config`.
    const CodecConfig temp = {.codec_id = CodecConfig::kCodecIdFlac,
                              .num_samples_per_frame = num_samples_per_frame_,
                              .decoder_config = flac_decoder_config_};

    CodecConfigObu codec_config(ObuHeader(), 0, temp);
    ASSERT_THAT(codec_config.Initialize(kOverrideAudioRollDistance), IsOk());

    encoder_ = std::make_unique<FlacEncoder>(flac_encoder_metadata_,
                                             codec_config, num_channels_);
  }

  FlacDecoderConfig flac_decoder_config_ = {
      {{.header = {.last_metadata_block_flag = true,
                   .block_type = FlacMetaBlockHeader::kFlacStreamInfo,
                   .metadata_data_block_length = 34},
        .payload = FlacMetaBlockStreamInfo{.minimum_block_size = 16,
                                           .maximum_block_size = 16,
                                           .sample_rate = 48000,
                                           .bits_per_sample = 31,
                                           .total_samples_in_stream = 16}}}};
  iamf_tools_cli_proto::FlacEncoderMetadata flac_encoder_metadata_ = {};
};  // namespace iamf_tools

TEST_F(FlacEncoderTest, FramesAreInOrder) {
  InitExpectOk();

  // Encode several frames and ensure the correct number of frames are output in
  // the same order as the input.
  const int kNumFrames = 100;
  for (int i = 0; i < kNumFrames; i++) {
    EncodeAudioFrame(std::vector<std::vector<int32_t>>(
        num_samples_per_frame_, std::vector<int32_t>(num_channels_, i)));
  }
  FinalizeAndValidateOrderOnly(kNumFrames);
}

TEST_F(FlacEncoderTest,
       InitializeFailsWhenNumSamplesPerFrameIsLessThanSixteen) {
  num_samples_per_frame_ = 15;

  ConstructEncoder();

  EXPECT_FALSE(encoder_->Initialize(kIgnoredValidateCodecDelay).ok());
}

TEST_F(FlacEncoderTest, EncodeAudioFrameSucceeds) {
  // Typically the user of the encoder should pad partial frames of input data
  // before passing it into the encoder.
  const std::vector<std::vector<int32_t>> kAudioFrameWithCorrectNumSamples(
      num_samples_per_frame_, std::vector<int32_t>(num_channels_, 0));
  InitExpectOk();

  EncodeAudioFrame(kAudioFrameWithCorrectNumSamples);
}

TEST_F(FlacEncoderTest,
       EncodeAudioFrameFailsWhenAudioFrameIsSmallerThanNumSamplesPerFrame) {
  // Typically the user of the encoder should pad partial frames of input data
  // before passing it into the encoder.
  const std::vector<std::vector<int32_t>> kAudioFrameWithMissingSample(
      num_samples_per_frame_ - 1, std::vector<int32_t>(num_channels_, 0));
  InitExpectOk();

  EncodeAudioFrame(kAudioFrameWithMissingSample,
                   /*expected_encode_frame_is_ok=*/false);
}

TEST_F(FlacEncoderTest,
       EncodeAudioFrameFailsWhenAudioFrameIsLargerThanNumSamplesPerFrame) {
  const std::vector<std::vector<int32_t>> kAudioFrameWithExtraSample(
      num_samples_per_frame_ + 1, std::vector<int32_t>(num_channels_, 0));
  InitExpectOk();

  EncodeAudioFrame(kAudioFrameWithExtraSample,
                   /*expected_encode_frame_is_ok=*/false);
}

}  // namespace
}  // namespace iamf_tools
