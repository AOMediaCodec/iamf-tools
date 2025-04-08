#include "iamf/cli/codec/lpcm_decoder.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Not;

constexpr bool kOverrideAudioRollDistance = true;
constexpr uint32_t kNumSamplesPerFrame = 1024;
constexpr uint8_t kSampleSize16 = 16;
constexpr bool kLittleEndian = true;

CodecConfigObu CreateCodecConfigObu(LpcmDecoderConfig lpcm_decoder_config,
                                    uint32_t num_samples_per_frame) {
  const CodecConfig codec_config = {
      .codec_id = CodecConfig::kCodecIdLpcm,
      .num_samples_per_frame = num_samples_per_frame,
      .decoder_config = lpcm_decoder_config};

  CodecConfigObu codec_config_obu(ObuHeader(), 0, codec_config);
  return codec_config_obu;
};

TEST(Create, Succeed) {
  LpcmDecoderConfig lpcm_decoder_config;
  lpcm_decoder_config.sample_rate_ = 48000;
  lpcm_decoder_config.sample_size_ = 16;
  lpcm_decoder_config.sample_format_flags_bitmask_ =
      LpcmDecoderConfig::LpcmFormatFlagsBitmask::kLpcmLittleEndian;
  CodecConfigObu codec_config_obu =
      CreateCodecConfigObu(lpcm_decoder_config, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu.Initialize(kOverrideAudioRollDistance), IsOk());
  int number_of_channels = 11;  // Arbitrary.

  auto lpcm_decoder = LpcmDecoder::Create(codec_config_obu, number_of_channels);
  EXPECT_THAT(lpcm_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(Create, FailsWithInvalidConfig) {
  LpcmDecoderConfig lpcm_decoder_config;
  // The sample rate and bit depth are validated with CodecConfigObu::Initialize
  // so if we want to test the validation in LpcmDecoderConfig::Initialize we
  // will give an invalid sample_format_flags_bitmask_.
  lpcm_decoder_config.sample_rate_ = 48000;
  lpcm_decoder_config.sample_size_ = 16;
  lpcm_decoder_config.sample_format_flags_bitmask_ =
      LpcmDecoderConfig::LpcmFormatFlagsBitmask::kLpcmBeginReserved;
  CodecConfigObu codec_config_obu =
      CreateCodecConfigObu(lpcm_decoder_config, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu.Initialize(kOverrideAudioRollDistance), IsOk());
  int number_of_channels = 11;  // Arbitrary.

  auto lpcm_decoder = LpcmDecoder::Create(codec_config_obu, number_of_channels);
  EXPECT_THAT(lpcm_decoder, Not(IsOk()));
}

std::unique_ptr<DecoderBase> CreateDecoderForDecodingTest(
    uint8_t sample_size, bool little_endian, uint32_t num_samples_per_frame) {
  LpcmDecoderConfig lpcm_decoder_config;
  // The sample rate and bit depth are validated with CodecConfigObu::Initialize
  // so if we want to test the validation in LpcmDecoderConfig::Initialize we
  // will give an invalid sample_format_flags_bitmask_.
  lpcm_decoder_config.sample_rate_ = 48000;
  lpcm_decoder_config.sample_size_ = sample_size;
  using enum LpcmDecoderConfig::LpcmFormatFlagsBitmask;
  lpcm_decoder_config.sample_format_flags_bitmask_ =
      little_endian ? kLpcmLittleEndian : kLpcmBigEndian;
  CodecConfigObu codec_config_obu =
      CreateCodecConfigObu(lpcm_decoder_config, num_samples_per_frame);
  if (!codec_config_obu.Initialize(kOverrideAudioRollDistance).ok()) {
    LOG(ERROR) << "Failed to initialize codec config OBU";
  }
  constexpr int kTwoChannels = 2;  // Keep the amount of test data reasonable.

  auto lpcm_decoder = LpcmDecoder::Create(codec_config_obu, kTwoChannels);
  EXPECT_THAT(lpcm_decoder, IsOkAndHolds(Not(IsNull())));
  return std::move(*lpcm_decoder);
}

TEST(LpcmDecoderTest, DecodeAudioFrame_FailsWhenFrameIsLargerThanExpected) {
  constexpr uint32_t kShortNumberOfSamplesPerFrame = 1;
  auto lpcm_decoder = CreateDecoderForDecodingTest(
      kSampleSize16, kLittleEndian, kShortNumberOfSamplesPerFrame);
  const std::vector<uint8_t>& kEncodedFrameWithOneSamplesPerFrame = {
      0x00, 0x00,  // 0
      0x01, 0x00,  // 1
  };
  // The decoder is configured correctly. One sample per frame decodes fine.
  ASSERT_THAT(
      lpcm_decoder->DecodeAudioFrame(kEncodedFrameWithOneSamplesPerFrame),
      IsOk());

  // But decoding two samples per frame fails, since the decoder was configured
  // for at most one sample per frame.
  const std::vector<uint8_t>& kEncodedFrameWithTwoSamplesPerFrame = {
      0x00, 0x00,  // 0
      0x01, 0x00,  // 1
      0x00, 0x01,  // 256
      0x80, 0xff,  // -128
  };
  EXPECT_FALSE(
      lpcm_decoder->DecodeAudioFrame(kEncodedFrameWithTwoSamplesPerFrame).ok());
}

TEST(LpcmDecoderTest, DecodeAudioFrame_LittleEndian16BitSamples) {
  uint8_t sample_size = 16;
  bool little_endian = true;
  auto lpcm_decoder = CreateDecoderForDecodingTest(sample_size, little_endian,
                                                   kNumSamplesPerFrame);
  const std::vector<uint8_t>& encoded_frame = {
      0x00, 0x00,  // 0
      0x01, 0x00,  // 1
      0x00, 0x01,  // 256
      0x80, 0xff,  // -128
  };

  EXPECT_THAT(lpcm_decoder->DecodeAudioFrame(encoded_frame), IsOk());
  const auto& decoded_samples = lpcm_decoder->ValidDecodedSamples();

  // We have two channels and four samples, so we expect two time ticks of two
  // samples each.
  EXPECT_EQ(decoded_samples.size(), 2);
  EXPECT_EQ(decoded_samples[0].size(), 2);
  EXPECT_EQ(decoded_samples[0][0], 0);
  EXPECT_EQ(decoded_samples[0][1], 0x00010000);
  EXPECT_EQ(decoded_samples[1].size(), 2);
  EXPECT_EQ(decoded_samples[1][0], 0x01000000);
  EXPECT_EQ(decoded_samples[1][1], 0xff800000);
}

TEST(LpcmDecoderTest, DecodeAudioFrame_BigEndian24BitSamples) {
  uint8_t sample_size = 24;
  bool little_endian = false;
  auto lpcm_decoder = CreateDecoderForDecodingTest(sample_size, little_endian,
                                                   kNumSamplesPerFrame);
  const std::vector<uint8_t>& encoded_frame = {
      0x00, 0x00, 0x00,  // 0
      0x00, 0x00, 0x01,  // 1
      0x00, 0x00, 0x03,  // 3
      0x00, 0x00, 0x04,  // 4
      0x7f, 0xff, 0xff,  // 8388607
      0x80, 0x00, 0x00,  // -8388608
  };

  EXPECT_THAT(lpcm_decoder->DecodeAudioFrame(encoded_frame), IsOk());
  const auto& decoded_samples = lpcm_decoder->ValidDecodedSamples();

  // We have two channels and six samples, so we expect three time ticks of two
  // samples each.
  EXPECT_EQ(decoded_samples.size(), 3);
  EXPECT_EQ(decoded_samples[0].size(), 2);
  EXPECT_EQ(decoded_samples[0][0], 0);
  EXPECT_EQ(decoded_samples[0][1], 0x00000100);
  EXPECT_EQ(decoded_samples[1].size(), 2);
  EXPECT_EQ(decoded_samples[1][0], 0x00000300);
  EXPECT_EQ(decoded_samples[1][1], 0x00000400);
  EXPECT_EQ(decoded_samples[2].size(), 2);
  EXPECT_EQ(decoded_samples[2][0], 0x7fffff00);
  EXPECT_EQ(decoded_samples[2][1], 0x80000000);
}

TEST(LpcmDecoderTest, DecodeAudioFrame_WillNotDecodeWrongSize) {
  uint8_t sample_size = 16;
  bool little_endian = true;
  auto lpcm_decoder = CreateDecoderForDecodingTest(sample_size, little_endian,
                                                   kNumSamplesPerFrame);
  // If we have 6 bytes, 16-bit samples, and two channels, we only have 3
  // samples which doesn't divide evenly into the number of channels.
  const std::vector<uint8_t>& encoded_frame = {0x00, 0x00, 0x00,
                                               0x00, 0x00, 0x00};

  EXPECT_THAT(lpcm_decoder->DecodeAudioFrame(encoded_frame), Not(IsOk()));
  EXPECT_THAT(lpcm_decoder->ValidDecodedSamples(), IsEmpty());
}

TEST(LpcmDecoderTest, DecodeAudioFrame_OverwritesExistingSamples) {
  uint8_t sample_size = 16;
  bool little_endian = true;
  auto lpcm_decoder = CreateDecoderForDecodingTest(sample_size, little_endian,
                                                   kNumSamplesPerFrame);
  const std::vector<uint8_t>& encoded_frame = {0x00, 0x00, 0x01, 0x00};

  EXPECT_THAT(lpcm_decoder->DecodeAudioFrame(encoded_frame), IsOk());
  EXPECT_EQ(lpcm_decoder->ValidDecodedSamples().size(), 1);

  EXPECT_THAT(lpcm_decoder->DecodeAudioFrame(encoded_frame), IsOk());
  EXPECT_EQ(lpcm_decoder->ValidDecodedSamples().size(), 1);

  EXPECT_THAT(lpcm_decoder->DecodeAudioFrame(encoded_frame), IsOk());
  EXPECT_EQ(lpcm_decoder->ValidDecodedSamples().size(), 1);
}

}  // namespace
}  // namespace iamf_tools
