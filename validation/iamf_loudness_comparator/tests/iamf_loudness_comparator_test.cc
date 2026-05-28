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
#include "validation/iamf_loudness_comparator/iamf_loudness_comparator.h"

#include <cstdint>
#include <list>
#include <vector>

#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions/mix_gain_param_definition.h"
#include "iamf/obu/tests/obu_test_utils.h"
#include "iamf/obu/types.h"

namespace iamf_loudness_comparator {

using iamf_tools::DecodedUleb128;
using iamf_tools::Layout;
using iamf_tools::LoudspeakersSsConventionLayout;
using iamf_tools::MixPresentationLayout;
using iamf_tools::MixPresentationObu;
using iamf_tools::MixPresentationSubMix;
using iamf_tools::ObuHeader;

namespace {

struct TestLayoutInfo {
  LoudspeakersSsConventionLayout::SoundSystem sound_system;
  double integrated_loudness;
  double digital_peak;
};

MixPresentationObu CreateMixPresentationWithLayouts(
    DecodedUleb128 id, const std::vector<TestLayoutInfo>& layouts_info) {
  std::vector<MixPresentationSubMix> sub_mixes;
  MixPresentationSubMix sub_mix;

  auto base_args = iamf_tools::MakeOneSubblockParamDefinitionBaseArgs(
      /*parameter_id=*/0, /*parameter_rate=*/48000, /*duration=*/8);
  sub_mix.output_mix_gain = iamf_tools::MixGainParamDefinition(base_args);

  sub_mix.audio_elements.push_back(iamf_tools::SubMixAudioElement{
      .audio_element_id = 1,
      .element_mix_gain = iamf_tools::MixGainParamDefinition(base_args)});

  for (const auto& info : layouts_info) {
    MixPresentationLayout layout{
        .loudness_layout = {.layout_type =
                                Layout::kLayoutTypeLoudspeakersSsConvention,
                            .specific_layout =
                                LoudspeakersSsConventionLayout{
                                    .sound_system = info.sound_system,
                                    .reserved = 0}},
        .loudness = {
            .info_type = 0,
            .integrated_loudness =
                iamf_tools::QFormatOrFloatingPoint::CreateFromFloatingPoint(
                    info.integrated_loudness)
                    .value()
                    .GetQ7_8(),
            .digital_peak =
                iamf_tools::QFormatOrFloatingPoint::CreateFromFloatingPoint(
                    info.digital_peak)
                    .value()
                    .GetQ7_8()}};
    sub_mix.layouts.push_back(layout);
  }
  sub_mixes.push_back(sub_mix);
  return MixPresentationObu(ObuHeader(), id, 0, {}, {}, sub_mixes);
}

// Constants representing the maximum difference in Q7.8 format that is still
// within the default tolerance of 0.1 LUFS, and the minimum difference that
// exceeds it.
// 25 / 256.0 = 0.09765625 (< 0.1)
// 26 / 256.0 = 0.1015625 (> 0.1)
constexpr double kQ7_8_ToleranceStepMatch = 25.0 / 256.0;
constexpr double kQ7_8_ToleranceStepFail = 26.0 / 256.0;

TEST(CompareLoudnessTest, EmptyObusMatch) {
  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({});
  EXPECT_TRUE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, MismatchMixPresentationCount) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({});

  EXPECT_FALSE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, IdenticalObusMatch) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_TRUE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, CustomTolerance) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.2, -1.2}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_TRUE(CompareLoudness(buffer1, buffer2, 0.3).all_match);
}

TEST(CompareLoudnessTest, MismatchMixPresentationId) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      2, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_FALSE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, IntegratedLoudnessCloseToToleranceMatch) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
           -24.0 + kQ7_8_ToleranceStepMatch, -1.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_TRUE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, IntegratedLoudnessCloseToToleranceFail) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0,
           -24.0 + kQ7_8_ToleranceStepFail, -1.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_FALSE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, PeakLoudnessCloseToToleranceMatch) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0,
           -1.0 + kQ7_8_ToleranceStepMatch}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_TRUE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, PeakLoudnessCloseToToleranceFail) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0,
           -1.0 + kQ7_8_ToleranceStepFail}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_FALSE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, OutOfOrderLayoutsMatch) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0},
          {LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0, -25.0, -2.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0, -25.0, -2.0},
          {LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_TRUE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, LayoutNotFoundFail) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0},
          {LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0, -25.0, -2.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0},
          {LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0, -25.0, -2.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_FALSE(CompareLoudness(buffer1, buffer2).all_match);
}

TEST(CompareLoudnessTest, LayoutCountMismatchFail) {
  auto obu1 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0}});
  auto obu2 = CreateMixPresentationWithLayouts(
      1, {{LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0, -24.0, -1.0},
          {LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0, -25.0, -2.0}});

  std::vector<uint8_t> buffer1 = iamf_tools::SerializeObusExpectOk({&obu1});
  std::vector<uint8_t> buffer2 = iamf_tools::SerializeObusExpectOk({&obu2});

  EXPECT_FALSE(CompareLoudness(buffer1, buffer2).all_match);
}

}  // namespace
}  // namespace iamf_loudness_comparator
