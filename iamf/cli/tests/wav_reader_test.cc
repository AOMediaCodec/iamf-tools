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
#include "iamf/cli/wav_reader.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

// Placeholder for get runfiles header.
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

WavReader InitAndValidate(const std::filesystem::path& filename,
                          const size_t num_samples_per_frame) {
  const auto input_wav_file = std::filesystem::current_path() /
                              std::string("iamf/cli/testdata/") / filename;
  WavReader wav_reader(input_wav_file.c_str(), num_samples_per_frame);

  // Validate `wav_reader` sees the expected properties from the wav header.
  EXPECT_EQ(wav_reader.num_samples_per_frame_, num_samples_per_frame);
  return wav_reader;
}

TEST(WavReader, GetNumChannelsMatchesWavFile) {
  const size_t kNumSamplesPerFrame = 8;

  EXPECT_EQ(
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame)
          .num_channels(),
      2);
  EXPECT_EQ(
      InitAndValidate("stereo_8_samples_48khz_s24le.wav", kNumSamplesPerFrame)
          .num_channels(),
      2);
  EXPECT_EQ(
      InitAndValidate("sine_1000_16khz_512ms_s32le.wav", kNumSamplesPerFrame)
          .num_channels(),
      1);
}

TEST(WavReader, GetSampleRateHzMatchesWavFile) {
  const size_t kNumSamplesPerFrame = 8;

  EXPECT_EQ(
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame)
          .sample_rate_hz(),
      48000);
  EXPECT_EQ(
      InitAndValidate("stereo_8_samples_48khz_s24le.wav", kNumSamplesPerFrame)
          .sample_rate_hz(),
      48000);
  EXPECT_EQ(
      InitAndValidate("sine_1000_16khz_512ms_s32le.wav", kNumSamplesPerFrame)
          .sample_rate_hz(),
      16000);
}

TEST(WavReader, GetBitDepthMatchesWavFile) {
  const size_t kNumSamplesPerFrame = 8;

  EXPECT_EQ(
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame)
          .bit_depth(),
      16);
  EXPECT_EQ(
      InitAndValidate("stereo_8_samples_48khz_s24le.wav", kNumSamplesPerFrame)
          .bit_depth(),
      24);
  EXPECT_EQ(
      InitAndValidate("sine_1000_16khz_512ms_s32le.wav", kNumSamplesPerFrame)
          .bit_depth(),
      32);
}

TEST(WavReader, GetNumRemainingSamplesUpdatesWithRead) {
  // Read four samples x two channels per frame.
  const size_t kNumSamplesPerFrame = 4;
  auto wav_reader =
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame);
  EXPECT_EQ(wav_reader.remaining_samples(), 16);
  wav_reader.ReadFrame();
  EXPECT_EQ(wav_reader.remaining_samples(), 8);
  wav_reader.ReadFrame();
  EXPECT_EQ(wav_reader.remaining_samples(), 0);
}

TEST(WavReader, OneFrame16BitLittleEndian) {
  const size_t kNumSamplesPerFrame = 8;
  auto wav_reader =
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame);

  // Read one frame. The result of n-bit samples are stored in the upper `n`
  // bits.
  EXPECT_EQ(wav_reader.ReadFrame(), 16);
  std::vector<std::vector<int32_t>> expected_frame = {
      {0x00010000, static_cast<int32_t>(0xffff0000)},
      {0x00020000, static_cast<int32_t>(0xfffe0000)},
      {0x00030000, static_cast<int32_t>(0xfffd0000)},
      {0x00040000, static_cast<int32_t>(0xfffc0000)},
      {0x00050000, static_cast<int32_t>(0xfffb0000)},
      {0x00060000, static_cast<int32_t>(0xfffa0000)},
      {0x00070000, static_cast<int32_t>(0xfff90000)},
      {0x00080000, static_cast<int32_t>(0xfff80000)},
  };
  EXPECT_EQ(wav_reader.buffers_, expected_frame);
}

TEST(WavReader, TwoFrames16BitLittleEndian) {
  const size_t kNumSamplesPerFrame = 4;
  auto wav_reader =
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.ReadFrame(), 8);
  std::vector<std::vector<int32_t>> expected_frame = {
      {0x00010000, static_cast<int32_t>(0xffff0000)},
      {0x00020000, static_cast<int32_t>(0xfffe0000)},
      {0x00030000, static_cast<int32_t>(0xfffd0000)},
      {0x00040000, static_cast<int32_t>(0xfffc0000)},
  };
  EXPECT_EQ(wav_reader.buffers_, expected_frame);

  expected_frame = {{0x00050000, static_cast<int32_t>(0xfffb0000)},
                    {0x00060000, static_cast<int32_t>(0xfffa0000)},
                    {0x00070000, static_cast<int32_t>(0xfff90000)},
                    {0x00080000, static_cast<int32_t>(0xfff80000)}};
  wav_reader.ReadFrame();
  EXPECT_EQ(wav_reader.buffers_, expected_frame);
}

TEST(WavReader, OneFrame24BitLittleEndian) {
  const size_t kNumSamplesPerFrame = 2;
  auto wav_reader =
      InitAndValidate("stereo_8_samples_48khz_s24le.wav", kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.ReadFrame(), 4);
  std::vector<std::vector<int32_t>> expected_frame = {
      {0x00000100, static_cast<int32_t>(0xffffff00)},
      {0x00000200, static_cast<int32_t>(0xfffffe00)},
  };

  EXPECT_EQ(wav_reader.buffers_, expected_frame);
}

TEST(WavReader, OneFrame32BitLittleEndian) {
  const size_t kNumSamplesPerFrame = 8;

  auto wav_reader =
      InitAndValidate("sine_1000_16khz_512ms_s32le.wav", kNumSamplesPerFrame);

  EXPECT_EQ(wav_reader.ReadFrame(), 8);
  std::vector<std::vector<int32_t>> expected_frame = {
      {0},         {82180641},  {151850024}, {198401618},
      {214748364}, {198401618}, {151850024}, {82180641},
  };
  EXPECT_EQ(wav_reader.buffers_, expected_frame);
}

TEST(WavReader, IsSafeToCallReadFrameAfterMove) {
  const size_t kNumSamplesPerFrame = 1;
  auto wav_reader =
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame);
  auto wav_reader_moved = std::move(wav_reader);

  EXPECT_EQ(wav_reader_moved.ReadFrame(), 2);
  const std::vector<std::vector<int32_t>> kExpectedFrame = {
      {0x00010000, static_cast<int32_t>(0xffff0000)}};
  EXPECT_EQ(wav_reader_moved.buffers_, kExpectedFrame);
}

template <typename T>
std::vector<uint8_t> GetRawBytes(const T& object) {
  std::vector<uint8_t> raw_object(sizeof(object));
  std::memcpy(raw_object.data(), static_cast<const void*>(&object),
              raw_object.size());
  return raw_object;
}

TEST(WavReader, IsByteEquivalentAfterMoving) {
  const size_t kNumSamplesPerFrame = 1;
  auto wav_reader =
      InitAndValidate("stereo_8_samples_48khz_s16le.wav", kNumSamplesPerFrame);
  const auto raw_wav_reader_before_move = GetRawBytes(wav_reader);

  const auto wav_reader_moved = std::move(wav_reader);
  const auto raw_wav_reader_after_move = GetRawBytes(wav_reader_moved);

  EXPECT_EQ(raw_wav_reader_before_move, raw_wav_reader_after_move);
}

}  // namespace
}  // namespace iamf_tools
