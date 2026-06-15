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
#include "absl/strings/match.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"
#include "validation/opus_hoa/mapping_matrix.h"

namespace iamf_tools {
namespace opus_hoa {
namespace {

constexpr uint32_t kCodecConfigId = 101;
constexpr uint32_t kAudioElementId = 201;

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
  std::vector<DecodedUleb128> substreams(channel_count);
  std::iota(substreams.begin(), substreams.end(), 0);
  std::vector<uint8_t> mapping(channel_count);
  std::iota(mapping.begin(), mapping.end(), 0);

  return AudioElementObu::CreateForMonoAmbisonics(
             ObuHeader{.obu_type = kObuIaAudioElement}, audio_element_id,
             /*reserved=*/0, codec_config_id, substreams, mapping)
      .value();
}

AudioElementObu MakeAmbisonicsProjectionObu(uint32_t audio_element_id,
                                            uint32_t codec_config_id,
                                            int order) {
  int channel_count = (order + 1) * (order + 1);
  std::vector<DecodedUleb128> substreams(channel_count);
  std::iota(substreams.begin(), substreams.end(), 0);
  std::vector<int16_t> matrix(channel_count * channel_count, 0);

  return AudioElementObu::CreateForProjectionAmbisonics(
             ObuHeader{.obu_type = kObuIaAudioElement}, audio_element_id,
             /*reserved=*/0, codec_config_id, substreams,
             /*output_channel_count=*/channel_count,
             /*coupled_substream_count=*/0, matrix)
      .value();
}

AudioElementObu MakeCanonicalAmbisonicsProjectionObu(uint32_t audio_element_id,
                                                     uint32_t codec_config_id,
                                                     int order) {
  int channel_count = (order + 1) * (order + 1);
  std::vector<DecodedUleb128> substreams(channel_count, 0);

  absl::Span<const int16_t> matrix_span =
      (order == 3) ? absl::MakeConstSpan(kIamfDemixingMatrix3OA)
                   : absl::MakeConstSpan(kIamfDemixingMatrix4OA);

  std::vector<int16_t> matrix(matrix_span.begin(), matrix_span.end());

  return AudioElementObu::CreateForProjectionAmbisonics(
             ObuHeader{.obu_type = kObuIaAudioElement}, audio_element_id,
             /*reserved=*/0, codec_config_id, substreams,
             /*output_channel_count=*/channel_count,
             /*coupled_substream_count=*/0, matrix)
      .value();
}

std::vector<AudioElementVerificationResult> RunVerificationOnElement(
    const CodecConfigObu& codec_config, const AudioElementObu& audio_element) {
  IASequenceHeaderObu seq_header(ObuHeader{.obu_type = kObuIaSequenceHeader},
                                 ProfileVersion::kIamfBaseProfile,
                                 ProfileVersion::kIamfBaseProfile);
  AudioFrameObu audio_frame(ObuHeader{.obu_type = kObuIaAudioFrame},
                            /*substream_id=*/0, /*audio_frame=*/{0});

  std::vector<uint8_t> bitstream = SerializeObusExpectOk(
      {&seq_header, &codec_config, &audio_element, &audio_frame});
  std::string file_path = testing::TempDir() + "/test.iamf";
  WriteBytesToFile(file_path, bitstream);

  return *VerifyOpusAmbisonics(file_path);
}

TEST(HermeticIngestionTestSuite, IngestValidStandaloneIamf) {
  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  AudioElementObu audio_element =
      MakeAmbisonicsMonoObu(kAudioElementId, kCodecConfigId, 2);

  auto results = RunVerificationOnElement(codec_config, audio_element);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].status, VerificationStatus::kCanonical);
}

class CanonicalMonoModeTest : public testing::TestWithParam<int> {};

TEST_P(CanonicalMonoModeTest, Verify0OA_1OA_2OA) {
  int order = GetParam();

  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  AudioElementObu audio_element =
      MakeAmbisonicsMonoObu(kAudioElementId, kCodecConfigId, order);

  auto results = RunVerificationOnElement(codec_config, audio_element);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].status, VerificationStatus::kCanonical);
  EXPECT_EQ(results[0].order, order);
  EXPECT_EQ(results[0].ambisonics_mode, AmbisonicsConfig::kAmbisonicsModeMono);
}

INSTANTIATE_TEST_SUITE_P(Orders0_1_2, CanonicalMonoModeTest,
                         testing::Values(0, 1, 2));

class CustomProjectionModeTest : public testing::TestWithParam<int> {};

TEST_P(CustomProjectionModeTest, Verify0OA_1OA_2OA) {
  int order = GetParam();

  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  AudioElementObu audio_element =
      MakeAmbisonicsProjectionObu(kAudioElementId, kCodecConfigId, order);

  auto results = RunVerificationOnElement(codec_config, audio_element);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].status, VerificationStatus::kCustom);
  EXPECT_EQ(results[0].order, order);
}

INSTANTIATE_TEST_SUITE_P(Orders0_1_2, CustomProjectionModeTest,
                         testing::Values(0, 1, 2));

class CanonicalProjectionModeTest : public testing::TestWithParam<int> {};

TEST_P(CanonicalProjectionModeTest, Verify3OA_4OA) {
  int order = GetParam();

  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  AudioElementObu audio_element = MakeCanonicalAmbisonicsProjectionObu(
      kAudioElementId, kCodecConfigId, order);

  auto results = RunVerificationOnElement(codec_config, audio_element);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].status, VerificationStatus::kCanonical);
  EXPECT_EQ(results[0].order, order);
  EXPECT_EQ(results[0].ambisonics_mode,
            AmbisonicsConfig::kAmbisonicsModeProjection);
}

INSTANTIATE_TEST_SUITE_P(Orders3_4, CanonicalProjectionModeTest,
                         testing::Values(3, 4));

class CustomProjectionMatrixTest : public testing::TestWithParam<int> {};

TEST_P(CustomProjectionMatrixTest, Verify3OA_4OA) {
  int order = GetParam();
  uint32_t codec_config_id = 101;
  uint32_t audio_element_id = 201;

  CodecConfigObu codec_config = MakeOpusCodecConfigObu(codec_config_id);
  AudioElementObu audio_element =
      MakeAmbisonicsProjectionObu(audio_element_id, codec_config_id, order);

  auto results = RunVerificationOnElement(codec_config, audio_element);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].status, VerificationStatus::kCustom);
  EXPECT_EQ(results[0].order, order);
  EXPECT_EQ(results[0].custom_rationale,
            "Demixing matrix coefficients diverge from Opus Channel Mapping "
            "Family 3 reference.");
}

INSTANTIATE_TEST_SUITE_P(Orders3_4, CustomProjectionMatrixTest,
                         testing::Values(3, 4));

class CustomMonoModeTest : public testing::TestWithParam<int> {};

TEST_P(CustomMonoModeTest, Verify3OA_4OA) {
  int order = GetParam();

  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  AudioElementObu audio_element =
      MakeAmbisonicsMonoObu(kAudioElementId, kCodecConfigId, order);

  auto results = RunVerificationOnElement(codec_config, audio_element);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].status, VerificationStatus::kCustom);
  EXPECT_EQ(results[0].order, order);
}

INSTANTIATE_TEST_SUITE_P(Orders3_4, CustomMonoModeTest, testing::Values(3, 4));

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

TEST(VerificationTestSuite, SkipNonAmbisonicsAudioElements) {
  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  ScalableChannelLayoutConfig scalable_config;
  scalable_config.channel_audio_layer_configs.push_back(ChannelAudioLayerConfig{
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
      .output_gain_is_present_flag = 0,
      .recon_gain_is_present_flag = 0,
      .substream_count = 1,
      .coupled_substream_count = 0,
  });

  AudioElementObu channel_audio_element =
      AudioElementObu::CreateForScalableChannelLayout(
          ObuHeader{.obu_type = kObuIaAudioElement}, kAudioElementId,
          /*reserved=*/0, kCodecConfigId, /*audio_substream_ids=*/{0},
          scalable_config)
          .value();

  auto results = RunVerificationOnElement(codec_config, channel_audio_element);
  EXPECT_TRUE(results.empty());
}

TEST(VerificationTestSuite, ReportNonOpusCodecConfig) {
  IASequenceHeaderObu seq_header(ObuHeader{.obu_type = kObuIaSequenceHeader},
                                 ProfileVersion::kIamfBaseProfile,
                                 ProfileVersion::kIamfBaseProfile);

  LpcmDecoderConfig lpcm_config;
  lpcm_config.sample_format_flags_bitmask_ =
      LpcmDecoderConfig::kLpcmLittleEndian;
  lpcm_config.sample_size_ = 16;
  lpcm_config.sample_rate_ = 48000;

  CodecConfigObu lpcm_codec_config =
      CodecConfigObu::Create(ObuHeader{.obu_type = kObuIaCodecConfig},
                             kCodecConfigId,
                             CodecConfig{
                                 .codec_id = CodecConfig::kCodecIdLpcm,
                                 .num_samples_per_frame = 240,
                                 .audio_roll_distance = 0,
                                 .decoder_config = lpcm_config,
                             })
          .value();

  AudioElementObu audio_element =
      MakeAmbisonicsMonoObu(kAudioElementId, kCodecConfigId, 1);
  AudioFrameObu audio_frame(ObuHeader{.obu_type = kObuIaAudioFrame},
                            /*substream_id=*/0, /*audio_frame=*/{0});

  std::vector<uint8_t> bitstream = SerializeObusExpectOk(
      {&seq_header, &lpcm_codec_config, &audio_element, &audio_frame});
  std::string file_path = testing::TempDir() + "/non_opus_codec.iamf";
  WriteBytesToFile(file_path, bitstream);

  auto results = VerifyOpusAmbisonics(file_path);
  ASSERT_TRUE(results.ok()) << results.status();
  ASSERT_EQ(results->size(), 1);
  EXPECT_EQ((*results)[0].status, VerificationStatus::kInvalidOrNonOpus);
  EXPECT_TRUE(absl::StrContains((*results)[0].custom_rationale,
                                "Not an Opus Codec Config"));
}

TEST(VerificationReportTestSuite, IngestFileWithNoAudioElements) {
  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  IASequenceHeaderObu seq_header(ObuHeader{.obu_type = kObuIaSequenceHeader},
                                 ProfileVersion::kIamfBaseProfile,
                                 ProfileVersion::kIamfBaseProfile);
  AudioFrameObu audio_frame(ObuHeader{.obu_type = kObuIaAudioFrame},
                            /*substream_id=*/0, /*audio_frame=*/{0});

  std::vector<uint8_t> bitstream =
      SerializeObusExpectOk({&seq_header, &codec_config, &audio_frame});
  std::string file_path = testing::TempDir() + "/no_audio_elements.iamf";
  WriteBytesToFile(file_path, bitstream);

  auto results = VerifyOpusAmbisonics(file_path);
  ASSERT_TRUE(results.ok()) << results.status();
  EXPECT_TRUE(results->empty());

  VerificationReport report = GenerateVerificationReport(*results);
  EXPECT_TRUE(report.all_canonical);
  EXPECT_EQ(report.overall_summary,
            "Result: No Opus Ambisonics Audio Elements found.");
  EXPECT_EQ(report.report,
            "\nResult: No Opus Ambisonics Audio Elements found.\n");
}

TEST(VerificationReportTestSuite, GenerateReportForNonAmbisonicsAudioElement) {
  CodecConfigObu codec_config = MakeOpusCodecConfigObu(kCodecConfigId);
  ScalableChannelLayoutConfig scalable_config;
  scalable_config.channel_audio_layer_configs.push_back(ChannelAudioLayerConfig{
      .loudspeaker_layout = ChannelAudioLayerConfig::kLayoutMono,
      .output_gain_is_present_flag = 0,
      .recon_gain_is_present_flag = 0,
      .substream_count = 1,
      .coupled_substream_count = 0,
  });

  AudioElementObu channel_audio_element =
      AudioElementObu::CreateForScalableChannelLayout(
          ObuHeader{.obu_type = kObuIaAudioElement}, kAudioElementId,
          /*reserved=*/0, kCodecConfigId, /*audio_substream_ids=*/{0},
          scalable_config)
          .value();

  auto results = RunVerificationOnElement(codec_config, channel_audio_element);
  EXPECT_TRUE(results.empty());

  VerificationReport report = GenerateVerificationReport(results);
  EXPECT_TRUE(report.all_canonical);
  EXPECT_EQ(report.overall_summary,
            "Result: No Opus Ambisonics Audio Elements found.");
  EXPECT_EQ(report.report,
            "\nResult: No Opus Ambisonics Audio Elements found.\n");
}

}  // namespace
}  // namespace opus_hoa
}  // namespace iamf_tools
