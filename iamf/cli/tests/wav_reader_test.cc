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
#include <filesystem>
#include <string>
#include <vector>

// Placeholder for get runfiles header.
#include "gtest/gtest.h"

namespace iamf_tools {
namespace {

WavReader InitAndValidate(const std::filesystem::path& filename,
                          const size_t num_samples_per_frame,
                          const int expected_num_channels,
                          const int expected_sample_rate_hz,
                          const int expected_bit_depth) {
  const auto input_wav_file = std::filesystem::current_path() /
                              std::string("iamf/cli/testdata/") / filename;
  WavReader wav_reader(input_wav_file.c_str(), num_samples_per_frame);

  // Validate `wav_reader` sees the expected properties from the wav header.
  EXPECT_EQ(wav_reader.num_samples_per_frame_, num_samples_per_frame);
  EXPECT_EQ(wav_reader.num_channels(), expected_num_channels);
  EXPECT_EQ(wav_reader.sample_rate_hz(), expected_sample_rate_hz);
  EXPECT_EQ(wav_reader.bit_depth(), expected_bit_depth);
  return wav_reader;
}

TEST(WavReader, OneFrame16BitLittleEndian) {
  auto wav_reader = InitAndValidate(
      "stereo_8_samples_48khz_s16le.wav",
      /*num_samples_per_frame=*/8, /*expected_num_channels=*/2,
      /*expected_sample_rate_hz=*/48000, /*expected_bit_depth=*/16);

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
  auto wav_reader = InitAndValidate(
      "stereo_8_samples_48khz_s16le.wav",
      /*num_samples_per_frame=*/4, /*expected_num_channels=*/2,
      /*expected_sample_rate_hz=*/48000, /*expected_bit_depth=*/16);

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
  auto wav_reader = InitAndValidate("stereo_8_samples_48khz_s24le.wav",
                                    /*num_samples_per_frame=*/2,
                                    /*expected_num_channels=*/2,
                                    /*expected_sample_rate_hz=*/48000,
                                    /*expected_bit_depth=*/24);
  EXPECT_EQ(wav_reader.ReadFrame(), 4);
  std::vector<std::vector<int32_t>> expected_frame = {
      {0x00000100, static_cast<int32_t>(0xffffff00)},
      {0x00000200, static_cast<int32_t>(0xfffffe00)},
  };

  EXPECT_EQ(wav_reader.buffers_, expected_frame);
}

TEST(WavReader, OneFrame32BitLittleEndian) {
  auto wav_reader = InitAndValidate(
      "sine_1000_16khz_512ms_s32le.wav",
      /*num_samples_per_frame=*/8, /*expected_num_channels=*/1,
      /*expected_sample_rate_hz=*/16000, /*expected_bit_depth=*/32);

  EXPECT_EQ(wav_reader.ReadFrame(), 8);
  std::vector<std::vector<int32_t>> expected_frame = {
      {0},         {82180641},  {151850024}, {198401618},
      {214748364}, {198401618}, {151850024}, {82180641},
  };
  EXPECT_EQ(wav_reader.buffers_, expected_frame);
}

}  // namespace
}  // namespace iamf_tools
