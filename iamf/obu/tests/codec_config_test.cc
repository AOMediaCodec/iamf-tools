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
#include "iamf/obu/codec_config.h"

#include <sys/stat.h>

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using testing::Not;

constexpr bool kOverrideAudioRollDistance = true;
constexpr bool kDontOverrideAudioRollDistance = false;
constexpr int16_t kInvalidAudioRollDistance = 123;
constexpr int16_t kLpcmAudioRollDistance = 0;
constexpr DecodedUleb128 kCodecConfigId = 123;
constexpr int16_t kArbitraryCodecDelay = 999;

// Despite being represented in 4-bits the AAC Sampling Frequency Index 64000 is
// serialized across a byte boundary.
constexpr uint8_t kUpperByteSerializedSamplingFrequencyIndex64000 =
    (static_cast<uint8_t>(AudioSpecificConfig::SampleFrequencyIndex::k64000) &
     0x0e) >>
    1;
constexpr uint8_t kLowerByteSerializedSamplingFrequencyIndex64000 =
    (static_cast<uint8_t>(AudioSpecificConfig::SampleFrequencyIndex::k64000) &
     0x01)
    << 7;

CodecConfig GetLpcmCodecConfig(uint32_t sample_rate) {
  return {.codec_id = CodecConfig::kCodecIdLpcm,
          .num_samples_per_frame = 64,
          .audio_roll_distance = 0,
          .decoder_config = LpcmDecoderConfig{
              .sample_format_flags_bitmask_ = LpcmDecoderConfig::kLpcmBigEndian,
              .sample_size_ = 16,
              .sample_rate_ = sample_rate}};
}

TEST(Create, SucceedsWithMaxPracticalFrameSize) {
  auto lpcm_codec_config = GetLpcmCodecConfig(48000);
  lpcm_codec_config.num_samples_per_frame =
      CodecConfigObu::kMaxPracticalFrameSize;

  EXPECT_THAT(
      CodecConfigObu::Create(ObuHeader(), kCodecConfigId, lpcm_codec_config),
      IsOk());
}

TEST(Create, FailsWithZeroSamplesPerFrame) {
  auto lpcm_codec_config = GetLpcmCodecConfig(48000);
  lpcm_codec_config.num_samples_per_frame = 0;

  EXPECT_THAT(
      CodecConfigObu::Create(ObuHeader(), kCodecConfigId, lpcm_codec_config),
      Not(IsOk()));
}

TEST(Create, FailsWithTooManySamplesPerFrame) {
  auto lpcm_codec_config = GetLpcmCodecConfig(48000);
  lpcm_codec_config.num_samples_per_frame =
      CodecConfigObu::kMaxPracticalFrameSize + 1;

  EXPECT_THAT(
      CodecConfigObu::Create(ObuHeader(), kCodecConfigId, lpcm_codec_config),
      Not(IsOk()));
}

class CodecConfigTestBase : public ObuTestBase {
 public:
  CodecConfigTestBase(CodecConfig::CodecId codec_id,
                      DecoderConfig decoder_config)
      : ObuTestBase(
            /*expected_header=*/{0, 14}, /*expected_payload=*/{}),
        codec_config_id_(kCodecConfigId),
        codec_config_({.codec_id = codec_id,
                       .num_samples_per_frame = 64,
                       .audio_roll_distance = 0,
                       .decoder_config = decoder_config}) {}

  ~CodecConfigTestBase() override = default;

 protected:
  void InitExpectOk() override {
    auto obu = CodecConfigObu::Create(header_, codec_config_id_, codec_config_,
                                      override_audio_roll_distance_);
    ASSERT_THAT(obu, IsOk());
    obu_ = std::make_unique<CodecConfigObu>(std::move(*obu));
  }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  std::unique_ptr<CodecConfigObu> obu_;

  DecodedUleb128 codec_config_id_;
  CodecConfig codec_config_;

  bool override_audio_roll_distance_ = kOverrideAudioRollDistance;
};

struct SampleRateTestCase {
  uint32_t sample_rate;
  bool expect_ok;
};

class CodecConfigLpcmTestForSampleRate
    : public testing::TestWithParam<SampleRateTestCase> {};
// Instantiate an LPCM `CodecConfigOBU` with the specified parameters. Verify
// the validation function returns the expected status.
TEST_P(CodecConfigLpcmTestForSampleRate, TestCodecConfigLpcm) {
  auto obu = CodecConfigObu::Create(ObuHeader(), kCodecConfigId,
                                    GetLpcmCodecConfig(GetParam().sample_rate));
  const bool expect_ok = GetParam().expect_ok;
  if (expect_ok) {
    ASSERT_THAT(obu, IsOk());

    WriteBitBuffer unused_wb(0);
    EXPECT_EQ(obu->ValidateAndWriteObu(unused_wb).ok(), expect_ok);

    // Validate the functions to get the sample rate return the expected
    // value.
    EXPECT_EQ(obu->GetOutputSampleRate(), GetParam().sample_rate);
    // The input sample rate function for LPCM should the output sample rate
    // function.
    EXPECT_EQ(obu->GetInputSampleRate(), GetParam().sample_rate);
  } else {
    EXPECT_THAT(obu, Not(IsOk()));
  }
}

INSTANTIATE_TEST_SUITE_P(LegalSampleRates, CodecConfigLpcmTestForSampleRate,
                         testing::ValuesIn<SampleRateTestCase>({{48000, true},
                                                                {16000, true},
                                                                {32000, true},
                                                                {44100, true},
                                                                {96000,
                                                                 true}}));

INSTANTIATE_TEST_SUITE_P(
    IllegalSampleRates, CodecConfigLpcmTestForSampleRate,
    ::testing::ValuesIn<SampleRateTestCase>({{0, false},
                                             {8000, false},
                                             {22050, false},
                                             {23000, false},
                                             {196000, false}}));

class CodecConfigLpcmTest : public CodecConfigTestBase, public testing::Test {
 public:
  CodecConfigLpcmTest()
      : CodecConfigTestBase(
            CodecConfig::kCodecIdLpcm,
            LpcmDecoderConfig{.sample_format_flags_bitmask_ =
                                  LpcmDecoderConfig::kLpcmBigEndian,
                              .sample_size_ = 16,
                              .sample_rate_ = 48000}) {
    expected_payload_ = {// `codec_config_id`.
                         kCodecConfigId,
                         // `codec_id`.
                         'i', 'p', 'c', 'm',
                         // `num_samples_per_frame`.
                         64,
                         // `audio_roll_distance`.
                         0, 0,
                         // `sample_format_flags`.
                         0,
                         // `sample_size`.
                         16,
                         // `sample_rate`.
                         0, 0, 0xbb, 0x80};
  }
};

TEST_F(CodecConfigLpcmTest, IsAlwaysLossless) {
  InitExpectOk();

  EXPECT_TRUE(obu_->IsLossless());
}

TEST_F(CodecConfigLpcmTest, SetCodecDelayIsNoOp) {
  InitExpectOk();

  EXPECT_THAT(obu_->SetCodecDelay(kArbitraryCodecDelay), IsOk());
}

TEST_F(CodecConfigLpcmTest, CreateSetsObuTyoe) {
  InitExpectOk();

  EXPECT_EQ(obu_->header_.obu_type, kObuIaCodecConfig);
}

TEST_F(CodecConfigLpcmTest, CreateSetsAudioRollDistance) {
  codec_config_.audio_roll_distance = kInvalidAudioRollDistance;
  override_audio_roll_distance_ = kOverrideAudioRollDistance;
  InitExpectOk();

  EXPECT_EQ(obu_->GetCodecConfig().audio_roll_distance, kLpcmAudioRollDistance);
}

TEST_F(CodecConfigLpcmTest, CreateObeysInvalidAudioRollDistance) {
  codec_config_.audio_roll_distance = kInvalidAudioRollDistance;
  override_audio_roll_distance_ = kDontOverrideAudioRollDistance;
  InitExpectOk();

  EXPECT_EQ(obu_->GetCodecConfig().audio_roll_distance,
            kInvalidAudioRollDistance);
}

TEST_F(CodecConfigLpcmTest, CreateMayOverrideAudioRollDistance) {
  codec_config_.audio_roll_distance = kInvalidAudioRollDistance;
  override_audio_roll_distance_ = kOverrideAudioRollDistance;
  InitExpectOk();

  EXPECT_EQ(obu_->GetCodecConfig().audio_roll_distance, kLpcmAudioRollDistance);
}

TEST_F(CodecConfigLpcmTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 2);
  codec_config_id_ = 0;
  codec_config_.num_samples_per_frame = 1;

  expected_header_ = {0, 0x80 | 16, 0};
  expected_payload_ = {// `codec_config_id`.
                       0x80, 0x00,
                       // `codec_id`.
                       'i', 'p', 'c', 'm',
                       // `num_samples_per_frame`.
                       0x81, 0x00,
                       // `audio_roll_distance`.
                       0, 0,
                       // `sample_format_flags`.
                       0,
                       // `sample_size`.
                       16,
                       // `sample_rate`.
                       0, 0, 0xbb, 0x80};

  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, CreateFailsWithIllegalCodecId) {
  codec_config_.codec_id = static_cast<CodecConfig::CodecId>(0);

  EXPECT_THAT(CodecConfigObu::Create(header_, codec_config_id_, codec_config_),
              Not(IsOk()));
}

TEST_F(CodecConfigLpcmTest, CreateFailsWithWriteIllegalSampleSize) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_size_ = 33;

  EXPECT_THAT(CodecConfigObu::Create(header_, codec_config_id_, codec_config_),
              Not(IsOk()));
}

TEST_F(CodecConfigLpcmTest, CreateFailsWithGetIllegalSampleSize) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_size_ = 33;

  EXPECT_THAT(CodecConfigObu::Create(header_, codec_config_id_, codec_config_),
              Not(IsOk()));
}

TEST_F(CodecConfigLpcmTest, Default) { InitAndTestWrite(); }

TEST_F(CodecConfigLpcmTest, ExtensionHeader) {
  header_.obu_extension_flag = true;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaCodecConfig << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      20,
                      // `extension_header_size`.
                      5,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};
  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, ConfigId) {
  codec_config_id_ = 100;
  expected_payload_ = {// `codec_config_id`.
                       100,
                       // `codec_id`.
                       'i', 'p', 'c', 'm',
                       // `num_samples_per_frame`.
                       64,
                       // `audio_roll_distance`.
                       0, 0,
                       // `sample_format_flags`.
                       0,
                       // `sample_size`.
                       16,
                       // `sample_rate`.
                       0, 0, 0xbb, 0x80};
  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, NumSamplesPerFrame) {
  codec_config_.num_samples_per_frame = 128;
  expected_header_ = {0, 15};
  expected_payload_ = {// `codec_config_id`.
                       kCodecConfigId,
                       // `codec_id`.
                       'i', 'p', 'c', 'm',
                       // `num_samples_per_frame`.
                       0x80, 0x01,
                       // `audio_roll_distance`.
                       0, 0,
                       // `sample_format_flags`.
                       0,
                       // `sample_size`.
                       16,
                       // `sample_rate`.
                       0, 0, 0xbb, 0x80};

  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, SampleFormatFlags) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config)
      .sample_format_flags_bitmask_ = LpcmDecoderConfig::kLpcmLittleEndian;
  expected_payload_ = {// `codec_config_id`.
                       kCodecConfigId,
                       // `codec_id`.
                       'i', 'p', 'c', 'm',
                       // `num_samples_per_frame`.
                       64,
                       // `audio_roll_distance`.
                       0, 0,
                       // `sample_format_flags`.
                       1,
                       // `sample_size`.
                       16,
                       // `sample_rate`.
                       0, 0, 0xbb, 0x80};

  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, WriteSampleSize) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_size_ = 24;
  expected_payload_ = {// `codec_config_id`.
                       kCodecConfigId,
                       // `codec_id`.
                       'i', 'p', 'c', 'm',
                       // `num_samples_per_frame`.
                       64,
                       // `audio_roll_distance`.
                       0, 0,
                       // `sample_format_flags`.
                       0,
                       // `sample_size`.
                       24,
                       // `sample_rate`.
                       0, 0, 0xbb, 0x80};

  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, GetSampleSize) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_size_ = 24;
  InitExpectOk();

  EXPECT_EQ(obu_->GetBitDepthToMeasureLoudness(), 24);
}

TEST_F(CodecConfigLpcmTest, WriteSampleRate) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_rate_ =
      16000;
  expected_payload_ = {// `codec_config_id`.
                       kCodecConfigId,
                       // `codec_id`.
                       'i', 'p', 'c', 'm',
                       // `num_samples_per_frame`.
                       64,
                       // `audio_roll_distance`.
                       0, 0,
                       // `sample_format_flags`.
                       0,
                       // `sample_size`.
                       16,
                       // `sample_rate`.
                       0, 0, 0x3e, 0x80};

  InitAndTestWrite();
}

TEST_F(CodecConfigLpcmTest, GetOutputSampleRate) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_rate_ =
      16000;
  InitExpectOk();

  EXPECT_EQ(obu_->GetOutputSampleRate(), 16000);
}

TEST_F(CodecConfigLpcmTest, GetInputSampleRate) {
  std::get<LpcmDecoderConfig>(codec_config_.decoder_config).sample_rate_ =
      16000;
  InitExpectOk();

  EXPECT_EQ(obu_->GetInputSampleRate(), 16000);
}

TEST_F(CodecConfigLpcmTest, RedundantCopy) {
  header_.obu_redundant_copy = true;

  expected_header_ = {kObuIaCodecConfig << 3 | kObuRedundantCopyBitMask, 14};
  InitAndTestWrite();
}

class CodecConfigOpusTest : public CodecConfigTestBase, public testing::Test {
 public:
  CodecConfigOpusTest()
      : CodecConfigTestBase(
            CodecConfig::kCodecIdOpus,
            OpusDecoderConfig{
                .version_ = 1, .pre_skip_ = 0, .input_sample_rate_ = 0}) {
    // Overwrite some default values to be more reasonable for Opus.
    codec_config_.num_samples_per_frame = 960;
    codec_config_.audio_roll_distance = -4;
    expected_header_ = {0, 20};
    expected_payload_ = {kCodecConfigId, 'O', 'p', 'u', 's', 0xc0, 0x07, 0xff,
                         0xfc,
                         // Start `DecoderConfig`.
                         1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  }
};

TEST_F(CodecConfigOpusTest, IsNeverLossless) {
  InitExpectOk();

  EXPECT_FALSE(obu_->IsLossless());
}

TEST_F(CodecConfigOpusTest, ManyLargeValues) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);
  codec_config_id_ = std::numeric_limits<DecodedUleb128>::max();
  codec_config_.num_samples_per_frame = CodecConfigObu::kMaxPracticalFrameSize;
  codec_config_.audio_roll_distance = -1;
  std::get<OpusDecoderConfig>(codec_config_.decoder_config).pre_skip_ = 0xffff;
  std::get<OpusDecoderConfig>(codec_config_.decoder_config).input_sample_rate_ =
      0xffffffff;

  expected_header_ = {0, 0x80 | 33, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
  expected_payload_ = {// `codec_config_id`.
                       0xff, 0xff, 0xff, 0xff, 0x8f, 0x80, 0x80, 0x00,
                       // `codec_id`.
                       'O', 'p', 'u', 's',
                       // `num_samples_per_frame`.
                       0x80, 0xee, 0x85, 0x80, 0x80, 0x80, 0x80, 0x00,
                       // `audio_roll_distance`.
                       0xff, 0xff,
                       // Start `DecoderConfig`.
                       // `version`.
                       1,
                       // `output_channel_count`.
                       OpusDecoderConfig::kOutputChannelCount,
                       // `pre_skip`
                       0xff, 0xff,
                       //
                       // `input_sample_rate`.
                       0xff, 0xff, 0xff, 0xff,
                       // `output_gain`.
                       0, 0,
                       // `mapping_family`.
                       OpusDecoderConfig::kMappingFamily};

  InitAndTestWrite();
}

TEST_F(CodecConfigOpusTest, InitializeFailsWithIllegalCodecId) {
  codec_config_.codec_id = static_cast<CodecConfig::CodecId>(0);

  EXPECT_THAT(CodecConfigObu::Create(header_, codec_config_id_, codec_config_),
              Not(IsOk()));
}

TEST_F(CodecConfigOpusTest, CreateFailsWhenOverridingAudioRollDistanceFails) {
  constexpr uint32_t kNumSamplesPerFrameCausesDivideByZero = 0;
  codec_config_.num_samples_per_frame = kNumSamplesPerFrameCausesDivideByZero;
  override_audio_roll_distance_ = kOverrideAudioRollDistance;

  // Underlying Opus roll distance calculation would fail.
  EXPECT_THAT(CodecConfigObu::Create(header_, codec_config_id_, codec_config_,
                                     override_audio_roll_distance_),
              Not(IsOk()));
}

TEST_F(CodecConfigOpusTest, Default) { InitAndTestWrite(); }

TEST_F(CodecConfigOpusTest, SetCodecDelaySetsPreSkip) {
  InitExpectOk();
  EXPECT_THAT(obu_->SetCodecDelay(kArbitraryCodecDelay), IsOk());

  const auto decoder_config = obu_->GetCodecConfig().decoder_config;
  ASSERT_TRUE(std::holds_alternative<OpusDecoderConfig>(decoder_config));
  EXPECT_EQ(std::get<OpusDecoderConfig>(decoder_config).pre_skip_,
            kArbitraryCodecDelay);
}

TEST_F(CodecConfigOpusTest, VarySeveralFields) {
  codec_config_id_ = 99;
  std::get<OpusDecoderConfig>(codec_config_.decoder_config).version_ = 15;
  std::get<OpusDecoderConfig>(codec_config_.decoder_config).pre_skip_ = 3;
  std::get<OpusDecoderConfig>(codec_config_.decoder_config).input_sample_rate_ =
      4;
  expected_payload_ = {99, 'O', 'p', 'u', 's', 0xc0, 0x07, 0xff, 0xfc,
                       // Start `DecoderConfig`.
                       // `version`.
                       15,
                       // `output_channel_count`.
                       OpusDecoderConfig::kOutputChannelCount,
                       // `pre_skip`
                       0, 3,
                       //
                       // `input_sample_rate`.
                       0, 0, 0, 4,
                       // `output_gain`.
                       0, 0,
                       // `mapping_family`.
                       OpusDecoderConfig::kMappingFamily};
  InitAndTestWrite();
}

TEST_F(CodecConfigOpusTest, RedundantCopy) {
  header_.obu_redundant_copy = true;
  expected_header_ = {4, 20};
  InitAndTestWrite();
}

TEST(CreateFromBuffer, OpusDecoderConfig) {
  constexpr DecodedUleb128 kExpectedNumSamplesPerFrame = 960;
  constexpr int16_t kExpectedAudioRollDistance = -4;
  constexpr uint8_t kVersion = 15;
  constexpr int16_t kExpectedPreSkip = 3;
  constexpr int16_t kExpectedInputSampleRate = 4;
  std::vector<uint8_t> source_data = {kCodecConfigId, 'O', 'p', 'u', 's',
                                      // num_samples_per_frame
                                      0xc0, 0x07,
                                      // audio_roll_distance
                                      0xff, 0xfc,
                                      // Start `DecoderConfig`.
                                      // `version`.
                                      kVersion,
                                      // `output_channel_count`.
                                      OpusDecoderConfig::kOutputChannelCount,
                                      // `pre_skip`
                                      0, 3,
                                      //
                                      // `input_sample_rate`.
                                      0, 0, 0, 4,
                                      // `output_gain`.
                                      0, 0,
                                      // `mapping_family`.
                                      OpusDecoderConfig::kMappingFamily};
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  ObuHeader header;

  absl::StatusOr<CodecConfigObu> obu =
      CodecConfigObu::CreateFromBuffer(header, payload_size, *buffer);
  EXPECT_THAT(obu, IsOk());

  EXPECT_EQ(obu->GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(obu->GetCodecConfig().codec_id, CodecConfig::kCodecIdOpus);
  EXPECT_EQ(obu->GetNumSamplesPerFrame(), kExpectedNumSamplesPerFrame);
  EXPECT_EQ(obu->GetCodecConfig().audio_roll_distance,
            kExpectedAudioRollDistance);
  ASSERT_TRUE(std::holds_alternative<OpusDecoderConfig>(
      obu->GetCodecConfig().decoder_config));
  const auto& opus_decoder_config =
      std::get<OpusDecoderConfig>(obu->GetCodecConfig().decoder_config);
  EXPECT_EQ(opus_decoder_config.version_, kVersion);
  EXPECT_EQ(opus_decoder_config.output_channel_count_,
            OpusDecoderConfig::kOutputChannelCount);
  EXPECT_EQ(opus_decoder_config.pre_skip_, kExpectedPreSkip);
  EXPECT_EQ(opus_decoder_config.input_sample_rate_, kExpectedInputSampleRate);
  EXPECT_EQ(opus_decoder_config.output_gain_, OpusDecoderConfig::kOutputGain);
  EXPECT_EQ(opus_decoder_config.mapping_family_,
            OpusDecoderConfig::kMappingFamily);
  EXPECT_FALSE(obu->IsLossless());
};

class CodecConfigAacTest : public CodecConfigTestBase, public testing::Test {
 public:
  CodecConfigAacTest()
      : CodecConfigTestBase(
            CodecConfig::kCodecIdAacLc,
            AacDecoderConfig{
                .buffer_size_db_ = 0,
                .max_bitrate_ = 0,
                .average_bit_rate_ = 0,
                .decoder_specific_info_ =
                    {.audio_specific_config =
                         {.sample_frequency_index_ = AudioSpecificConfig::
                              SampleFrequencyIndex::k64000}},
            }) {
    // Overwrite some default values to be more reasonable for AAC.
    codec_config_.num_samples_per_frame = 1024;
    codec_config_.audio_roll_distance = -1;
  }
};

TEST_F(CodecConfigAacTest, IsNeverLossless) {
  InitExpectOk();

  EXPECT_FALSE(obu_->IsLossless());
}

TEST_F(CodecConfigAacTest, SetCodecDelayIsNoOp) {
  InitExpectOk();

  EXPECT_THAT(obu_->SetCodecDelay(kArbitraryCodecDelay), IsOk());
}

TEST(CreateFromBuffer, AacLcDecoderConfig) {
  // A 7-bit mask representing `channel_configuration`, and all three fields in
  // the GA specific config.
  constexpr uint8_t kChannelConfigurationAndGaSpecificConfigMask =
      AudioSpecificConfig::kChannelConfiguration << 3 |               // 4 bits.
      AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag << 2 |  // 1 bit.
      AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder
          << 1 |                                                   // 1 bit.
      AudioSpecificConfig::GaSpecificConfig::kExtensionFlag << 0;  // 1 bit.
  constexpr DecodedUleb128 kExpectedNumSamplesPerFrame = 1024;
  constexpr int16_t kExpectedAudioRollDistance = -1;
  std::vector<uint8_t> source_data = {
      kCodecConfigId, 'm', 'p', '4', 'a',
      // num_samples_per_frame
      0x80, 0x08,
      // audio_roll_distance
      0xff, 0xff,
      // Start `DecoderConfig`.
      // `decoder_config_descriptor_tag`
      AacDecoderConfig::kDecoderConfigDescriptorTag,
      // ISO 14496:1 expandable size field.
      17,
      // `object_type_indication`.
      AacDecoderConfig::kObjectTypeIndication,
      // `stream_type`, `upstream`, `reserved`.
      AacDecoderConfig::kStreamType << 2 | AacDecoderConfig::kUpstream << 1 |
          AacDecoderConfig::kReserved << 0,
      // `buffer_size_db`.
      0, 0, 0,
      // `max_bitrate`.
      0, 0, 0, 0,
      // `average_bit_rate`.
      0, 0, 0, 0,
      // `decoder_specific_info_tag`
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
      // ISO 14496:1 expandable size field.
      2,
      // `audio_object_type`, upper 3 bits of `sample_frequency_index`.
      AudioSpecificConfig::kAudioObjectType << 3 |
          kUpperByteSerializedSamplingFrequencyIndex64000,
      // lower bit of `sample_frequency_index`,
      // `channel_configuration`, `frame_length_flag`,
      // `depends_on_core_coder`, `extension_flag`.
      kLowerByteSerializedSamplingFrequencyIndex64000 |
          kChannelConfigurationAndGaSpecificConfigMask};
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  ObuHeader header;

  absl::StatusOr<CodecConfigObu> obu =
      CodecConfigObu::CreateFromBuffer(header, payload_size, *buffer);
  EXPECT_THAT(obu, IsOk());

  EXPECT_EQ(obu->GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(obu->GetCodecConfig().codec_id, CodecConfig::kCodecIdAacLc);
  EXPECT_EQ(obu->GetNumSamplesPerFrame(), kExpectedNumSamplesPerFrame);
  EXPECT_EQ(obu->GetCodecConfig().audio_roll_distance,
            kExpectedAudioRollDistance);
  ASSERT_TRUE(std::holds_alternative<AacDecoderConfig>(
      obu->GetCodecConfig().decoder_config));
  const auto& aac_decoder_config =
      std::get<AacDecoderConfig>(obu->GetCodecConfig().decoder_config);
  EXPECT_EQ(aac_decoder_config.decoder_config_descriptor_tag_,
            AacDecoderConfig::kDecoderConfigDescriptorTag);
  EXPECT_EQ(aac_decoder_config.object_type_indication_,
            AacDecoderConfig::kObjectTypeIndication);
  EXPECT_EQ(aac_decoder_config.stream_type_, AacDecoderConfig::kStreamType);
  EXPECT_EQ(aac_decoder_config.upstream_, AacDecoderConfig::kUpstream);
  EXPECT_EQ(aac_decoder_config.reserved_, AacDecoderConfig::kReserved);
  EXPECT_EQ(aac_decoder_config.buffer_size_db_, 0);
  EXPECT_EQ(aac_decoder_config.max_bitrate_, 0);
  EXPECT_EQ(aac_decoder_config.average_bit_rate_, 0);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.decoder_specific_info_tag,
            AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.audio_specific_config
                .audio_object_type_,
            AudioSpecificConfig::kAudioObjectType);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.audio_specific_config
                .sample_frequency_index_,
            AudioSpecificConfig::SampleFrequencyIndex::k64000);
  EXPECT_EQ(obu->GetOutputSampleRate(), 64000);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.audio_specific_config
                .channel_configuration_,
            AudioSpecificConfig::kChannelConfiguration);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.audio_specific_config
                .ga_specific_config_.frame_length_flag,
            AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.audio_specific_config
                .ga_specific_config_.depends_on_core_coder,
            AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder);
  EXPECT_EQ(aac_decoder_config.decoder_specific_info_.audio_specific_config
                .ga_specific_config_.extension_flag,
            AudioSpecificConfig::GaSpecificConfig::kExtensionFlag);
  EXPECT_FALSE(obu->IsLossless());
};

class CodecConfigFlacTest : public CodecConfigTestBase, public testing::Test {
 public:
  CodecConfigFlacTest()
      : CodecConfigTestBase(
            CodecConfig::kCodecIdFlac,
            FlacDecoderConfig{
                {{.header = {.block_type =
                                 FlacMetaBlockHeader::kFlacStreamInfo},
                  .payload = FlacMetaBlockStreamInfo{
                      .minimum_block_size = 16,
                      .maximum_block_size = 16,
                      .sample_rate = 48000,
                      .bits_per_sample = 15,
                      .total_samples_in_stream = 0}}}}) {}
};

TEST_F(CodecConfigFlacTest, IsAlwaysLossless) {
  InitExpectOk();

  EXPECT_TRUE(obu_->IsLossless());
}

TEST_F(CodecConfigFlacTest, SetCodecDelayIsNoOp) {
  InitExpectOk();

  EXPECT_THAT(obu_->SetCodecDelay(kArbitraryCodecDelay), IsOk());
}

TEST(CreateFromBuffer, ValidLpcmDecoderConfig) {
  constexpr DecodedUleb128 kNumSamplesPerFrame = 64;
  constexpr int16_t kExpectedAudioRollDistance = 0;
  constexpr uint8_t kSampleFormatFlagsAsUint8 = 0x00;
  constexpr uint8_t kSampleSize = 16;
  constexpr uint32_t kExpectedSampleRate = 48000;

  std::vector<uint8_t> source_data = {// `codec_config_id`.
                                      kCodecConfigId,
                                      // `codec_id`.
                                      'i', 'p', 'c', 'm',
                                      // `num_samples_per_frame`.
                                      kNumSamplesPerFrame,
                                      // `audio_roll_distance`.
                                      0, 0,
                                      // `sample_format_flags`.
                                      kSampleFormatFlagsAsUint8,
                                      // `sample_size`.
                                      kSampleSize,
                                      // `sample_rate`.
                                      0, 0, 0xbb, 0x80};
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  ObuHeader header;

  absl::StatusOr<CodecConfigObu> obu =
      CodecConfigObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(obu->GetCodecConfig().codec_id, CodecConfig::kCodecIdLpcm);
  EXPECT_EQ(obu->GetNumSamplesPerFrame(), kNumSamplesPerFrame);
  EXPECT_EQ(obu->GetCodecConfig().audio_roll_distance,
            kExpectedAudioRollDistance);
  ASSERT_TRUE(std::holds_alternative<LpcmDecoderConfig>(
      obu->GetCodecConfig().decoder_config));
  const auto& lpcm_decoder_config =
      std::get<LpcmDecoderConfig>(obu->GetCodecConfig().decoder_config);
  EXPECT_EQ(
      static_cast<uint8_t>(lpcm_decoder_config.sample_format_flags_bitmask_),
      kSampleFormatFlagsAsUint8);
  EXPECT_EQ(lpcm_decoder_config.sample_size_, kSampleSize);
  EXPECT_EQ(lpcm_decoder_config.sample_rate_, kExpectedSampleRate);
  EXPECT_TRUE(obu->IsLossless());
}

TEST(CreateFromBuffer, ValidFlacDecoderConfig) {
  constexpr DecodedUleb128 kNumSamplesPerFrame = 64;
  constexpr int16_t kExpectedAudioRollDistance = 0;

  std::vector<uint8_t> source_data = {
      // `codec_config_id`.
      kCodecConfigId,
      // `codec_id`.
      'f', 'L', 'a', 'C',
      // `num_samples_per_frame`.
      kNumSamplesPerFrame,
      // `audio_roll_distance`.
      0, 0,
      // begin `FlacDecoderConfig`.
      // `last_metadata_block_flag` and `block_type` fields.
      1 << 7 | FlacMetaBlockHeader::kFlacStreamInfo,
      // `metadata_data_block_length`.
      0, 0, 34,
      // `minimum_block_size`.
      0, 64,
      // `maximum_block_size`.
      0, 64,
      // `minimum_frame_size`.
      0, 0, 0,
      // `maximum_frame_size`.
      0, 0, 0,
      // `sample_rate` (20 bits)
      0x0b, 0xb8,
      (0 << 4) |
          // `number_of_channels` (3 bits) and `bits_per_sample` (5 bits).
          FlacStreamInfoStrictConstraints::kNumberOfChannels << 1,
      7 << 4 |
          // `total_samples_in_stream` (36 bits).
          0,
      0x00, 0x00, 0x00, 100,
      // MD5 sum.
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00};
  const int64_t payload_size = source_data.size();
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(source_data));
  ObuHeader header;

  absl::StatusOr<CodecConfigObu> obu =
      CodecConfigObu::CreateFromBuffer(header, payload_size, *buffer);

  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(obu->GetCodecConfig().codec_id, CodecConfig::kCodecIdFlac);
  EXPECT_EQ(obu->GetNumSamplesPerFrame(), kNumSamplesPerFrame);
  EXPECT_EQ(obu->GetCodecConfig().audio_roll_distance,
            kExpectedAudioRollDistance);

  ASSERT_TRUE(std::holds_alternative<FlacDecoderConfig>(
      obu->GetCodecConfig().decoder_config));
  const auto& flac_decoder_config =
      std::get<FlacDecoderConfig>(obu->GetCodecConfig().decoder_config);
  EXPECT_EQ(flac_decoder_config.metadata_blocks_.size(), 1);
  FlacMetaBlockHeader flac_meta_block_header =
      flac_decoder_config.metadata_blocks_[0].header;
  EXPECT_EQ(flac_meta_block_header.block_type,
            FlacMetaBlockHeader::kFlacStreamInfo);
  FlacMetaBlockStreamInfo stream_info = std::get<FlacMetaBlockStreamInfo>(
      flac_decoder_config.metadata_blocks_[0].payload);
  EXPECT_EQ(stream_info.minimum_block_size, 64);
  EXPECT_EQ(stream_info.maximum_block_size, 64);
  EXPECT_EQ(stream_info.minimum_frame_size, 0);
  EXPECT_EQ(stream_info.maximum_frame_size, 0);
  EXPECT_EQ(stream_info.sample_rate, 48000);
  EXPECT_EQ(stream_info.number_of_channels,
            FlacStreamInfoStrictConstraints::kNumberOfChannels);
  EXPECT_EQ(stream_info.bits_per_sample, 7);
  EXPECT_EQ(stream_info.total_samples_in_stream, 100);
  EXPECT_EQ(stream_info.md5_signature,
            FlacStreamInfoLooseConstraints::kMd5Signature);
  EXPECT_TRUE(obu->IsLossless());
}

}  // namespace
}  // namespace iamf_tools
