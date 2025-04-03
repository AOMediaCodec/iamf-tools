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
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::ElementsAreArray;
using ::testing::Not;
using ::testing::Test;

// Derived from iamf/cli/testdata/stereo_8_samples_48khz_s16le.wav @ 16 samples
// per frame.
constexpr std::array<uint8_t, 22> kFlacEncodedFrame = {
    0xff, 0xf8, 0x6a, 0xa8, 0x00, 0x0f, 0x42, 0x00, 0x00, 0x00, 0x13,
    0x80, 0x00, 0x80, 0x04, 0x92, 0x49, 0x00, 0x01, 0xfe, 0x81, 0xee};

constexpr uint32_t kNumSamplesPerFrame = 16;
constexpr int kNumChannels = 2;

TEST(Initialize, Succeeds) {
  FlacDecoder flac_decoder(kNumChannels, kNumSamplesPerFrame);

  EXPECT_THAT(flac_decoder.Initialize(), IsOk());
}

TEST(DecodeAudioFrame, Succeeds) {
  FlacDecoder flac_decoder(kNumChannels, kNumSamplesPerFrame);

  EXPECT_THAT(flac_decoder.Initialize(), IsOk());
  auto status = flac_decoder.DecodeAudioFrame(
      std::vector(kFlacEncodedFrame.begin(), kFlacEncodedFrame.end()));
  EXPECT_THAT(status, IsOk());

  const std::vector<std::vector<int32_t>> kExpectedDecodedSamples = {
      {0x00010000, static_cast<int32_t>(0xffff0000)},
      {0x00020000, static_cast<int32_t>(0xfffe0000)},
      {0x00030000, static_cast<int32_t>(0xfffd0000)},
      {0x00040000, static_cast<int32_t>(0xfffc0000)},
      {0x00050000, static_cast<int32_t>(0xfffb0000)},
      {0x00060000, static_cast<int32_t>(0xfffa0000)},
      {0x00070000, static_cast<int32_t>(0xfff90000)},
      {0x00080000, static_cast<int32_t>(0xfff80000)},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000},
      {0x00000000, 0x00000000}};
  EXPECT_EQ(flac_decoder.ValidDecodedSamples(), kExpectedDecodedSamples);

  // Decode again.
  status = flac_decoder.DecodeAudioFrame(
      std::vector(kFlacEncodedFrame.begin(), kFlacEncodedFrame.end()));
  EXPECT_THAT(status, IsOk());
  EXPECT_EQ(flac_decoder.ValidDecodedSamples(), kExpectedDecodedSamples);
}

TEST(DecodeAudioFrame, DoesNotHangOnInvalidFrame) {
  FlacDecoder flac_decoder(kNumChannels, kNumSamplesPerFrame);
  EXPECT_THAT(flac_decoder.Initialize(), IsOk());

  const std::vector<uint8_t> kInvalidFrame = {0x00};
  const auto status = flac_decoder.DecodeAudioFrame(
      std::vector(kInvalidFrame.begin(), kInvalidFrame.end()));

  // The frame is not valid, but we expect to not hang and get an error status.
  EXPECT_THAT(status, Not(IsOk()));
}

TEST(DecodeAudioFrame, FailsOnMismatchedBlocksizeTooLarge) {
  constexpr uint32_t kNumSamplesPerFrame = 15;
  // num_samples_per_channel = 15, but the encoded frame has 16 samples per
  // channel.
  FlacDecoder flac_decoder(kNumChannels, kNumSamplesPerFrame);
  EXPECT_THAT(flac_decoder.Initialize(), IsOk());

  auto status = flac_decoder.DecodeAudioFrame(
      std::vector(kFlacEncodedFrame.begin(), kFlacEncodedFrame.end()));

  EXPECT_FALSE(status.ok());
}

TEST(DecodeAudioFrame, FillsExtraSamplesWithZeros) {
  constexpr uint32_t kNumSamplesPerFrame = 17;
  // num_samples_per_channel = 17, but the actual encoded frame has 16 samples
  // per channel.
  FlacDecoder flac_decoder(kNumChannels, kNumSamplesPerFrame);
  EXPECT_THAT(flac_decoder.Initialize(), IsOk());

  EXPECT_THAT(flac_decoder.DecodeAudioFrame(std::vector(
                  kFlacEncodedFrame.begin(), kFlacEncodedFrame.end())),
              IsOk());
  const auto decoded_samples = flac_decoder.ValidDecodedSamples();

  // Ok, we still expect 17 samples per frame, the last one is filled with
  // zeros, and typically would be trimmed.
  EXPECT_EQ(decoded_samples.size(), kNumSamplesPerFrame);
  EXPECT_EQ(decoded_samples.back(), std::vector<int32_t>(kNumChannels, 0));
}

}  // namespace

}  // namespace iamf_tools
