#include "iamf/cli/codec/lpcm_decoder.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

CodecConfigObu CreateCodecConfigObu(uint32_t num_samples_per_frame) {
  LpcmDecoderConfig lpcm_decoder_config;
  lpcm_decoder_config.sample_size_ = 16;
  lpcm_decoder_config.sample_rate_ = 48000;

  const CodecConfig codec_config = {
      .codec_id = CodecConfig::kCodecIdLpcm,
      .num_samples_per_frame = num_samples_per_frame,
      .audio_roll_distance = 0,
      .decoder_config = lpcm_decoder_config};

  CodecConfigObu codec_config_obu(ObuHeader(), 0, codec_config);
  return codec_config_obu;
};

TEST(LpcmDecoderTest, Construct) {
  uint32_t num_samples_per_frame = 1024;
  CodecConfigObu codec_config_obu = CreateCodecConfigObu(num_samples_per_frame);
  ASSERT_TRUE(codec_config_obu.Initialize().ok());

  int number_of_channels = 11;

  LpcmDecoder lpcm_decoder(codec_config_obu, number_of_channels);
}

}  // namespace
}  // namespace iamf_tools
