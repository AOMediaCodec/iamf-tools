#include "iamf/cli/codec/lpcm_decoder.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

CodecConfigObu CreateCodecConfigObu(LpcmDecoderConfig lpcm_decoder_config,
                                    uint32_t num_samples_per_frame = 1024) {
  const CodecConfig codec_config = {
      .codec_id = CodecConfig::kCodecIdLpcm,
      .num_samples_per_frame = num_samples_per_frame,
      .audio_roll_distance = 0,
      .decoder_config = lpcm_decoder_config};

  CodecConfigObu codec_config_obu(ObuHeader(), 0, codec_config);
  return codec_config_obu;
};

TEST(LpcmDecoderTest, Construct) {
  LpcmDecoderConfig lpcm_decoder_config;
  lpcm_decoder_config.sample_rate_ = 48000;
  lpcm_decoder_config.sample_size_ = 16;
  lpcm_decoder_config.sample_format_flags_bitmask_ =
      LpcmDecoderConfig::LpcmFormatFlagsBitmask::kLpcmLittleEndian;
  CodecConfigObu codec_config_obu = CreateCodecConfigObu(lpcm_decoder_config);
  ASSERT_TRUE(codec_config_obu.Initialize().ok());
  int number_of_channels = 11;  // Arbitrary.

  LpcmDecoder lpcm_decoder(codec_config_obu, number_of_channels);
}

TEST(LpcmDecoderTest, Initialize_InvalidConfigFails) {
  LpcmDecoderConfig lpcm_decoder_config;
  // The sample rate and bit depth are validated with CodecConfigObu::Initialize
  // so if we want to test the validation in LpcmDecoderConfig::Initialize we
  // will give an invalid sample_format_flags_bitmask_.
  lpcm_decoder_config.sample_rate_ = 48000;
  lpcm_decoder_config.sample_size_ = 16;
  lpcm_decoder_config.sample_format_flags_bitmask_ =
      LpcmDecoderConfig::LpcmFormatFlagsBitmask::kLpcmBeginReserved;
  CodecConfigObu codec_config_obu = CreateCodecConfigObu(lpcm_decoder_config);
  ASSERT_TRUE(codec_config_obu.Initialize().ok());
  int number_of_channels = 11;  // Arbitrary.

  LpcmDecoder lpcm_decoder(codec_config_obu, number_of_channels);
  auto status = lpcm_decoder.Initialize();

  EXPECT_FALSE(status.ok());
}

}  // namespace
}  // namespace iamf_tools
