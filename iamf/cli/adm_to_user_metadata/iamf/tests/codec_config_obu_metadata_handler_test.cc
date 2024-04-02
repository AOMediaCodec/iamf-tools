/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/adm_to_user_metadata/iamf/codec_config_obu_metadata_handler.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "iamf/cli/adm_to_user_metadata/adm/format_info_chunk.h"
#include "iamf/cli/proto/codec_config.pb.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

// The following constants are fixed for every call to
// GenerateLpcmCodecConfigObuMetadata.
const uint32_t kCodecConfigId = 0;
const auto kLpcmCodecId = iamf_tools_cli_proto::CODEC_ID_LPCM;
const uint32_t kAudioRollDistance = 0;
const auto kSampleFormatFlags = iamf_tools_cli_proto::LPCM_LITTLE_ENDIAN;

TEST(CodecConfigObuMetadataHandlerTest, PopulatesCodecConfigObuMetadata) {
  // Configure some constants that affect the output.
  const FormatInfoChunk format_info{
      .samples_per_sec = 48000,
      .bits_per_sample = 16,
  };
  int64_t kNumSamplesPerFrame = 1024;
  iamf_tools_cli_proto::CodecConfigObuMetadata codec_config_obu_metadata;

  GenerateLpcmCodecConfigObuMetadata(format_info, kNumSamplesPerFrame,
                                     codec_config_obu_metadata);

  EXPECT_EQ(codec_config_obu_metadata.codec_config_id(), kCodecConfigId);
  const auto& codec_config = codec_config_obu_metadata.codec_config();
  EXPECT_EQ(codec_config.codec_id(), kLpcmCodecId);
  EXPECT_EQ(codec_config.num_samples_per_frame(), kNumSamplesPerFrame);
  EXPECT_EQ(codec_config.audio_roll_distance(), kAudioRollDistance);
  const auto& decoder_config_lpcm = codec_config.decoder_config_lpcm();
  EXPECT_EQ(decoder_config_lpcm.sample_format_flags(), kSampleFormatFlags);
  EXPECT_EQ(decoder_config_lpcm.sample_size(), format_info.bits_per_sample);
  EXPECT_EQ(decoder_config_lpcm.sample_rate(), format_info.samples_per_sec);
}

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
