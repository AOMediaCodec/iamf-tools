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

#include "iamf/cli/codec/flac_decoder.h"

#include <array>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/codec/decoder_base.h"
#include "iamf/cli/tests/cli_test_utils.h"

namespace iamf_tools {
namespace {

using absl::MakeConstSpan;
using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::Test;

// Derived from iamf/cli/testdata/stereo_8_samples_48khz_s16le.wav @ 16 samples
// per frame.
constexpr std::array<uint8_t, 22> kFlacEncodedFrame = {
    0xff, 0xf8, 0x6a, 0xa8, 0x00, 0x0f, 0x42, 0x00, 0x00, 0x00, 0x13,
    0x80, 0x00, 0x80, 0x04, 0x92, 0x49, 0x00, 0x01, 0xfe, 0x81, 0xee};

constexpr uint32_t kNumSamplesPerFrame = 16;
constexpr int kNumChannels = 2;

std::unique_ptr<DecoderBase> CreateFlacDecoderExpectNonNull(
    int num_channels, uint32_t num_samples_per_frame) {
  auto flac_decoder = FlacDecoder::Create(num_channels, num_samples_per_frame);
  EXPECT_THAT(flac_decoder, IsOkAndHolds(Not(IsNull())));
  return std::move(*flac_decoder);
}

TEST(Create, Succeeds) {
  auto flac_decoder = FlacDecoder::Create(kNumChannels, kNumSamplesPerFrame);
  EXPECT_THAT(flac_decoder, IsOkAndHolds(Not(IsNull())));
}

TEST(DecodeAudioFrame, SubsequentCallsSucceed) {
  auto flac_decoder =
      CreateFlacDecoderExpectNonNull(kNumChannels, kNumSamplesPerFrame);

  EXPECT_THAT(flac_decoder->DecodeAudioFrame(MakeConstSpan(kFlacEncodedFrame)),
              IsOk());
  const std::vector<std::vector<int32_t>> kExpectedDecodedSamplesInt32 = {
      {0x00010000, 0x00020000, 0x00030000, 0x00040000, 0x00050000, 0x00060000,
       0x00070000, 0x00080000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
       0x00000000, 0x00000000, 0x00000000, 0x00000000},
      {static_cast<int32_t>(0xffff0000), static_cast<int32_t>(0xfffe0000),
       static_cast<int32_t>(0xfffd0000), static_cast<int32_t>(0xfffc0000),
       static_cast<int32_t>(0xfffb0000), static_cast<int32_t>(0xfffa0000),
       static_cast<int32_t>(0xfff90000), static_cast<int32_t>(0xfff80000),
       0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
       0x00000000, 0x00000000}};
  const auto kExpectedDecodedSamples =
      Int32ToInternalSampleType2D(kExpectedDecodedSamplesInt32);

  EXPECT_EQ(flac_decoder->ValidDecodedSamples(), kExpectedDecodedSamples);

  // Decode again.
  EXPECT_THAT(flac_decoder->DecodeAudioFrame(std::vector(
                  kFlacEncodedFrame.begin(), kFlacEncodedFrame.end())),
              IsOk());
  EXPECT_EQ(flac_decoder->ValidDecodedSamples(), kExpectedDecodedSamples);
}

TEST(DecodeAudioFrame, DoesNotHangOnInvalidFrame) {
  auto flac_decoder =
      CreateFlacDecoderExpectNonNull(kNumChannels, kNumSamplesPerFrame);

  const std::vector<uint8_t> kInvalidFrame = {0x00};
  const auto status =
      flac_decoder->DecodeAudioFrame(MakeConstSpan(kInvalidFrame));

  // The frame is not valid, but we expect to not hang and get an error status.
  EXPECT_THAT(status, Not(IsOk()));
}

TEST(DecodeAudioFrame, FailsOnMismatchedBlocksizeTooLarge) {
  constexpr uint32_t kNumSamplesPerFrame = 15;
  // num_samples_per_channel = 15, but the encoded frame has 16 samples per
  // channel.
  auto flac_decoder =
      CreateFlacDecoderExpectNonNull(kNumChannels, kNumSamplesPerFrame);

  EXPECT_THAT(flac_decoder->DecodeAudioFrame(MakeConstSpan(kFlacEncodedFrame)),
              Not(IsOk()));
}

TEST(DecodeAudioFrame, FillsExtraSamplesWithZeros) {
  constexpr uint32_t kNumSamplesPerFrame = 17;
  // num_samples_per_channel = 17, but the actual encoded frame has 16 samples
  // per channel.
  auto flac_decoder =
      CreateFlacDecoderExpectNonNull(kNumChannels, kNumSamplesPerFrame);

  EXPECT_THAT(flac_decoder->DecodeAudioFrame(MakeConstSpan(kFlacEncodedFrame)),
              IsOk());
  const auto decoded_samples = flac_decoder->ValidDecodedSamples();

  // Ok, we still expect 2 channels with 17 samples per channel. The last sample
  // of each channel is filled with a zero, and typically would be trimmed.
  EXPECT_EQ(decoded_samples.size(), kNumChannels);
  for (int c = 0; c < kNumChannels; c++) {
    EXPECT_EQ(decoded_samples[c].back(), 0);
  }
}

}  // namespace

}  // namespace iamf_tools
