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
#include "iamf/cli/lpcm_encoder.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/encoder_test_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

class LpcmEncoderTest : public EncoderTestBase, public testing::Test {
 public:
  LpcmEncoderTest() { input_sample_size_ = 32; }
  ~LpcmEncoderTest() = default;

 protected:
  void ConstructEncoder() override {
    // Construct a Codec Config OBU. The only fields that should affect the
    // output are `num_samples_per_frame` and `decoder_config`.
    const CodecConfig temp = {.codec_id = CodecConfig::kCodecIdLpcm,
                              .num_samples_per_frame = num_samples_per_frame_,
                              .audio_roll_distance = 0,
                              .decoder_config = lpcm_decoder_config_};
    CodecConfigObu codec_config(ObuHeader(), 0, temp);
    EXPECT_EQ(codec_config.Initialize().code(), expected_init_status_code_);

    encoder_ = std::make_unique<LpcmEncoder>(codec_config, num_channels_);
  }

  LpcmDecoderConfig lpcm_decoder_config_ = {
      .sample_format_flags_ = LpcmDecoderConfig::kLpcmLittleEndian,
      .sample_size_ = 32,
      .sample_rate_ = 48000};
};  // namespace iamf_tools

TEST_F(LpcmEncoderTest, LittleEndian32bit) {
  Init();

  EncodeAudioFrame({{0x01234567}});
  expected_audio_frames_.push_back({0x67, 0x45, 0x23, 0x01});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, BigEndian32bit) {
  lpcm_decoder_config_.sample_format_flags_ = LpcmDecoderConfig::kLpcmBigEndian;
  Init();

  EncodeAudioFrame({{0x01234567}});
  expected_audio_frames_.push_back({0x01, 0x23, 0x45, 0x67});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, MultipleFrames) {
  Init();

  EncodeAudioFrame({{0x01234567}});
  expected_audio_frames_.push_back({0x67, 0x45, 0x23, 0x01});
  EncodeAudioFrame({{0x77665544}});
  expected_audio_frames_.push_back({0x44, 0x55, 0x66, 0x77});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, LittleEndian16bit) {
  lpcm_decoder_config_.sample_size_ = 16;
  input_sample_size_ = 16;
  Init();

  EncodeAudioFrame({{0x12340000}});
  expected_audio_frames_.push_back({0x34, 0x12});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, BigEndian16bit) {
  lpcm_decoder_config_.sample_size_ = 16;
  lpcm_decoder_config_.sample_format_flags_ = LpcmDecoderConfig::kLpcmBigEndian;

  input_sample_size_ = 16;
  Init();

  EncodeAudioFrame({{0x12340000}});
  expected_audio_frames_.push_back({0x12, 0x34});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, LittleEndian24bit) {
  lpcm_decoder_config_.sample_size_ = 24;
  input_sample_size_ = 24;
  Init();

  EncodeAudioFrame({{0x12345600}});
  expected_audio_frames_.push_back({0x56, 0x34, 0x12});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, BigEndian24bit) {
  lpcm_decoder_config_.sample_size_ = 24;
  lpcm_decoder_config_.sample_format_flags_ = LpcmDecoderConfig::kLpcmBigEndian;
  input_sample_size_ = 24;
  Init();

  EncodeAudioFrame({{0x12345600}});
  expected_audio_frames_.push_back({0x12, 0x34, 0x56});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, MultipleSamplesPerFrame) {
  num_samples_per_frame_ = 3;
  Init();

  EncodeAudioFrame({{0x11111111}, {0x22222222}, {0x33333333}});
  expected_audio_frames_.push_back(
      {0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22, 0x33, 0x33, 0x33, 0x33});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, InvalidEmptySamples) {
  Init();

  expected_encode_frame_status_code_ = absl::StatusCode::kInvalidArgument;
  EncodeAudioFrame({});
}

TEST_F(LpcmEncoderTest, SupportsPartialFrames) {
  num_samples_per_frame_ = 3;
  Init();

  EncodeAudioFrame({{0x11111111}, {0x22222222}});
  expected_audio_frames_.push_back(
      {0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, TwoChannels) {
  num_channels_ = 2;
  Init();

  EncodeAudioFrame({{0x11111111, 0x22222222}});
  expected_audio_frames_.push_back(
      {0x11, 0x11, 0x11, 0x11, 0x22, 0x22, 0x22, 0x22});
  FinalizeAndValidate();
}

TEST_F(LpcmEncoderTest, InconsistentNumberOfChannels) {
  num_channels_ = 1;
  Init();

  expected_encode_frame_status_code_ = absl::StatusCode::kInvalidArgument;
  EncodeAudioFrame({{0x11111111, 0x22222222}});
}

TEST_F(LpcmEncoderTest, FramesAreInOrder) {
  Init();

  // Encode several frames and ensure the correct number of frames are output in
  // the same order as the input.
  const int kNumFrames = 100;
  for (int i = 0; i < kNumFrames; i++) {
    EncodeAudioFrame(std::vector<std::vector<int32_t>>(
        num_samples_per_frame_, std::vector<int32_t>(num_channels_, i)));
  }
  FinalizeAndValidateOrderOnly(kNumFrames);
}

}  // namespace
}  // namespace iamf_tools
