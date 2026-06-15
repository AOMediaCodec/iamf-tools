/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "validation/opus_hoa/opus_hoa.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <numeric>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace opus_hoa {
namespace {

void WriteBytesToFile(const std::string& path,
                      const std::vector<uint8_t>& data) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(data.data()), data.size());
}

CodecConfigObu MakeOpusCodecConfigObu(uint32_t codec_config_id) {
  OpusDecoderConfig decoder_config;
  decoder_config.version_ = 1;
  decoder_config.pre_skip_ = 312;
  decoder_config.input_sample_rate_ = 48000;

  return CodecConfigObu::Create(ObuHeader{.obu_type = kObuIaCodecConfig},
                                codec_config_id,
                                CodecConfig{
                                    .codec_id = CodecConfig::kCodecIdOpus,
                                    .num_samples_per_frame = 960,
                                    .audio_roll_distance = -4,
                                    .decoder_config = decoder_config,
                                })
      .value();
}

AudioElementObu MakeAmbisonicsMonoObu(uint32_t audio_element_id,
                                      uint32_t codec_config_id, int order) {
  int channel_count = (order + 1) * (order + 1);
  std::vector<DecodedUleb128> substreams(channel_count, 0);
  std::vector<uint8_t> mapping(channel_count);
  std::iota(mapping.begin(), mapping.end(), 0);

  return AudioElementObu::CreateForMonoAmbisonics(
             ObuHeader{.obu_type = kObuIaAudioElement}, audio_element_id,
             /*reserved=*/0, codec_config_id, substreams, mapping)
      .value();
}

TEST(HermeticIngestionTestSuite, IngestValidStandaloneIamf) {
  IASequenceHeaderObu seq_header(ObuHeader{.obu_type = kObuIaSequenceHeader},
                                 ProfileVersion::kIamfBaseProfile,
                                 ProfileVersion::kIamfBaseProfile);
  CodecConfigObu codec_config = MakeOpusCodecConfigObu(101);
  AudioElementObu audio_element = MakeAmbisonicsMonoObu(201, 101, 2);
  AudioFrameObu audio_frame(ObuHeader{.obu_type = kObuIaAudioFrame},
                            /*substream_id=*/0, /*audio_frame=*/{0});

  std::vector<uint8_t> bitstream = SerializeObusExpectOk(
      {&seq_header, &codec_config, &audio_element, &audio_frame});
  std::string file_path = testing::TempDir() + "/valid_standalone.iamf";
  WriteBytesToFile(file_path, bitstream);

  auto results = VerifyOpusAmbisonics(file_path);
  ASSERT_TRUE(results.ok()) << results.status();
}

TEST(HermeticVerificationTestSuite, RejectMalformedDescriptors) {
  // Construct a bitstream with a truncated IA Sequence Header OBU payload.
  //   - `0x69, 0x61, 0x6d, 0x66`: IAMF magic bytes ('i', 'a', 'm', 'f').
  //   - `0xff`                  : `primary_profile` byte.
  //   - Missing                 : `additional_profile` is abruptly cut off.
  std::vector<uint8_t> corrupt_bitstream = {0x69, 0x61, 0x6d, 0x66, 0xff};
  std::string file_path = testing::TempDir() + "/corrupt.iamf";
  WriteBytesToFile(file_path, corrupt_bitstream);

  auto result = VerifyOpusAmbisonics(file_path);
  EXPECT_FALSE(result.ok());
}

}  // namespace
}  // namespace opus_hoa
}  // namespace iamf_tools
