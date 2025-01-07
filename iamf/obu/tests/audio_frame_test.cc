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
#include "iamf/obu/audio_frame.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/tests/obu_test_base.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

TEST(AudioFrameConstructor, SetsImplicitObuType0) {
  AudioFrameObu obu({}, /*audio_substream_id=*/0, {});
  EXPECT_EQ(obu.header_.obu_type, kObuIaAudioFrameId0);
}

TEST(AudioFrameConstructor, SetsImplicitObuType17) {
  AudioFrameObu obu({}, /*audio_substream_id=*/17, {});
  EXPECT_EQ(obu.header_.obu_type, kObuIaAudioFrameId17);
}

TEST(AudioFrameConstructor, SetsExplicitObuType) {
  AudioFrameObu obu({}, /*audio_substream_id=*/18, {});
  EXPECT_EQ(obu.header_.obu_type, kObuIaAudioFrame);
}

class AudioFrameObuTest : public ObuTestBase, public testing::Test {
 public:
  AudioFrameObuTest()
      : ObuTestBase(
            /*expected_header=*/{kObuIaAudioFrameId0 << 3, 1},
            /*expected_payload=*/{42}),
        audio_substream_id_(0),
        audio_frame_({42}) {}

  ~AudioFrameObuTest() override = default;

 protected:
  void InitExpectOk() override {
    obu_ = std::make_unique<AudioFrameObu>(header_, audio_substream_id_,
                                           audio_frame_);
  }

  void WriteObuExpectOk(WriteBitBuffer& wb) override {
    EXPECT_THAT(obu_->ValidateAndWriteObu(wb), IsOk());
  }

  DecodedUleb128 audio_substream_id_;
  std::vector<uint8_t> audio_frame_;

  std::unique_ptr<AudioFrameObu> obu_;
};

TEST_F(AudioFrameObuTest, DefaultImplicitMinSubstreamId) {
  InitAndTestWrite();
  EXPECT_EQ(obu_->GetSubstreamId(), 0);
}

TEST_F(AudioFrameObuTest, ImplicitSubstreamIdEdge) {
  audio_substream_id_ = 17;

  expected_header_ = {kObuIaAudioFrameId17 << 3, 1};
  InitAndTestWrite();
  EXPECT_EQ(obu_->GetSubstreamId(), 17);
}

TEST_F(AudioFrameObuTest, ExplicitSubstreamIdEdge) {
  audio_substream_id_ = 18;

  expected_header_ = {kObuIaAudioFrame << 3, 2},
  expected_payload_ = {// `explicit_audio_substream_id`
                       18,
                       // `audio_frame`.
                       42};
  InitAndTestWrite();
  EXPECT_EQ(obu_->GetSubstreamId(), 18);
}

TEST_F(AudioFrameObuTest, MaximumSubstreamId) {
  audio_substream_id_ = std::numeric_limits<DecodedUleb128>::max();

  expected_header_ = {kObuIaAudioFrame << 3, 6},
  expected_payload_ = {// `explicit_audio_substream_id`
                       0xff, 0xff, 0xff, 0xff, 0x0f,
                       // `audio_frame`.
                       42};
  InitAndTestWrite();
  EXPECT_EQ(obu_->GetSubstreamId(), std::numeric_limits<uint32_t>::max());
}

TEST_F(AudioFrameObuTest, NonMinimalLebGeneratorAffectsAllLeb128s) {
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 8);
  audio_substream_id_ = 128;

  expected_header_ = {kObuIaAudioFrame << 3,
                      0x80 | 9,
                      0x80,
                      0x80,
                      0x80,
                      0x80,
                      0x80,
                      0x80,
                      0x00},
  expected_payload_ = {// `explicit_audio_substream_id`
                       0x80, 0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00,
                       // `audio_frame`.
                       42};
  InitAndTestWrite();
  EXPECT_EQ(obu_->GetSubstreamId(), 128);
}

TEST_F(AudioFrameObuTest, AudioFrameEmpty) {
  audio_frame_ = {};

  expected_header_ = {kObuIaAudioFrameId0 << 3, 0};
  expected_payload_ = {};
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, VaryMostLegalFields) {
  header_ = ObuHeader{.obu_redundant_copy = false,
                      .obu_trimming_status_flag = true,
                      .obu_extension_flag = true,
                      .num_samples_to_trim_at_end = 128,
                      .num_samples_to_trim_at_start = 256,
                      .extension_header_size = 3,
                      .extension_header_bytes = {'a', 'b', 'c'}};
  leb_generator_ =
      LebGenerator::Create(LebGenerator::GenerationMode::kFixedSize, 5);
  audio_substream_id_ = 512;
  audio_frame_ = {255, 254, 253, 252, 251, 250};

  expected_header_ = {kObuIaAudioFrame << 3 | kObuTrimmingStatusFlagBitMask |
                          kObuExtensionFlagBitMask,
                      // `obu_size`.
                      0x80 | 29, 0x80, 0x80, 0x80, 0x00,
                      // `num_samples_to_trim_at_end`.
                      0x80, 0x81, 0x80, 0x80, 0x00,
                      // `num_samples_to_trim_at_start`.
                      0x80, 0x82, 0x80, 0x80, 0x00,
                      // `extension_header_size`.
                      0x80 | 3, 0x80, 0x80, 0x80, 0x00,
                      // `extension_header_bytes`.
                      'a', 'b', 'c'};
  expected_payload_ = {// `explicit_audio_substream_id`
                       0x80, 0x84, 0x80, 0x80, 0x00,
                       // `audio_frame`.
                       255, 254, 253, 252, 251, 250};

  InitAndTestWrite();

  EXPECT_EQ(obu_->GetSubstreamId(), 512);
}

TEST_F(AudioFrameObuTest, AudioFrameMultipleBytes) {
  audio_frame_ = {1, 2, 3, 4, 5};

  expected_header_ = {kObuIaAudioFrameId0 << 3, 5},
  expected_payload_ = {// `audio_frame`.
                       1, 2, 3, 4, 5};
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, AudioFrameLarge) {
  audio_frame_.resize(16385);
  std::iota(audio_frame_.begin(), audio_frame_.end(), 1);

  expected_header_ = {kObuIaAudioFrameId0 << 3, 0x81, 0x80, 0x01},
  expected_payload_ = audio_frame_;
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, ObuTrimmingStatusFlagAtEnd) {
  header_.obu_trimming_status_flag = 1;
  header_.num_samples_to_trim_at_end = 1;
  header_.num_samples_to_trim_at_start = 0;

  expected_header_ =
      {// `obu_type` (5), `obu_redundant_copy` (1), `obu_trimming_status_flag`
       // (1), `obu_extension_flag` (1)
       (kObuIaAudioFrameId0 << 3) | kObuTrimmingStatusFlagBitMask,
       // `obu_size`
       3,
       // `num_samples_to_trim_at_end`.
       1,
       // `num_samples_to_trim_at_start`.
       0},
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, ObuTrimmingStatusFlagNumSamplesMaximum) {
  header_.obu_trimming_status_flag = 1;
  header_.num_samples_to_trim_at_end = std::numeric_limits<uint32_t>::max();
  header_.num_samples_to_trim_at_start = std::numeric_limits<uint32_t>::max();

  expected_header_ =
      {// `obu_type` (5), `obu_redundant_copy` (1), `obu_trimming_status_flag`
       // (1), `obu_extension_flag` (1)
       (kObuIaAudioFrameId0 << 3) | kObuTrimmingStatusFlagBitMask,
       // `obu_size`
       11,
       // `num_samples_to_trim_at_end`.
       0xff, 0xff, 0xff, 0xff, 0x0f,
       // `num_samples_to_trim_at_start`.
       0xff, 0xff, 0xff, 0xff, 0x0f},
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, ObuTrimmingStatusFlagAtStart) {
  header_.obu_trimming_status_flag = 1;
  header_.num_samples_to_trim_at_end = 0;
  header_.num_samples_to_trim_at_start = 1;

  expected_header_ =
      {// `obu_type` (5), `obu_redundant_copy` (1), `obu_trimming_status_flag`
       // (1), `obu_extension_flag` (1)
       (kObuIaAudioFrameId0 << 3) | kObuTrimmingStatusFlagBitMask,
       // `obu_size`
       3,
       // `num_samples_to_trim_at_end`.
       0,
       // `num_samples_to_trim_at_start`.
       1},
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, ObuTrimmingStatusFlagBothStartAndEnd) {
  header_.obu_trimming_status_flag = 1;
  header_.num_samples_to_trim_at_end = 1;
  header_.num_samples_to_trim_at_start = 1;

  expected_header_ =
      {// `obu_type` (5), `obu_redundant_copy` (1), `obu_trimming_status_flag`
       // (1), `obu_extension_flag` (1)
       (kObuIaAudioFrameId0 << 3) | kObuTrimmingStatusFlagBitMask,
       // `obu_size`
       3,
       // `num_samples_to_trim_at_end`.
       1,
       // `num_samples_to_trim_at_start`.
       1},
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, ExtensionHeader) {
  header_.obu_extension_flag = 1;
  header_.extension_header_size = 5;
  header_.extension_header_bytes = {'e', 'x', 't', 'r', 'a'};

  expected_header_ = {kObuIaAudioFrameId0 << 3 | kObuExtensionFlagBitMask,
                      // `obu_size`.
                      7,
                      // `extension_header_size`.
                      5,
                      // `extension_header_bytes`.
                      'e', 'x', 't', 'r', 'a'};
  InitAndTestWrite();
}

TEST_F(AudioFrameObuTest, ValidateAndWriteObuFailsWithIllegalRedundantCopy) {
  header_.obu_redundant_copy = 1;

  InitExpectOk();
  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(obu_->ValidateAndWriteObu(unused_wb).ok());
}

// --- Begin CreateFromBuffer tests ---
TEST(CreateFromBuffer, ValidAudioFrameWithExplicitId) {
  std::vector<uint8_t> source = {// `explicit_audio_substream_id`
                                 18,
                                 // `audio_frame`.
                                 8, 6, 24, 55, 11};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  ObuHeader header = {.obu_type = kObuIaAudioFrame};
  const int64_t obu_payload_size = 6;
  auto obu = AudioFrameObu::CreateFromBuffer(header, obu_payload_size, *buffer);
  EXPECT_THAT(obu, IsOk());
  EXPECT_EQ(obu->GetSubstreamId(), 18);
  EXPECT_EQ(obu->audio_frame_, std::vector<uint8_t>({8, 6, 24, 55, 11}));
}

TEST(CreateFromBuffer, ValidAudioFrameWithImplicitId) {
  std::vector<uint8_t> source = {// `audio_frame`.
                                 8, 6, 24, 55, 11};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  ObuHeader header = {.obu_type = kObuIaAudioFrameId0};
  int64_t obu_payload_size = 5;
  auto obu = AudioFrameObu::CreateFromBuffer(header, obu_payload_size, *buffer);
  EXPECT_THAT(obu, IsOk());
  // audio_substream_id is set implicitly to the value of `obu_type`
  AudioFrameObu expected_obu =
      AudioFrameObu(header, /*audio_substream_id=*/0,
                    /*audio_frame=*/{8, 6, 24, 55, 11});
  EXPECT_EQ(obu->GetSubstreamId(), 0);
  EXPECT_EQ(obu->audio_frame_, std::vector<uint8_t>({8, 6, 24, 55, 11}));
}

TEST(CreateFromBuffer, FailsWithPayloadSizeTooLarge) {
  std::vector<uint8_t> source = {// `explicit_audio_substream_id`
                                 18,
                                 // `audio_frame`.
                                 8, 6, 24, 55, 11};
  auto buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      1024, absl::MakeConstSpan(source));
  ObuHeader header = {.obu_type = kObuIaAudioFrame};
  int64_t obu_payload_size = 7;
  auto obu = AudioFrameObu::CreateFromBuffer(header, obu_payload_size, *buffer);
  EXPECT_FALSE(obu.ok());
}

}  // namespace
}  // namespace iamf_tools
