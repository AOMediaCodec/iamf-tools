/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/mix_presentation_finalizer.h"

#include <filesystem>
#include <list>
#include <memory>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kMixPresentationId = 42;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kCommonParameterId = 999;
constexpr DecodedUleb128 kCommonParameterRate = 16000;

std::unique_ptr<WavWriter> ProduceNoWavWriters(DecodedUleb128, int, int,
                                               const Layout&,
                                               const std::filesystem::path&,
                                               int, int, int) {
  return nullptr;
}

class MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest
    : public ::testing::Test {
 public:
  MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest() {
    // Initialize the input OBUs which will have loudness finalized.
    AddMixPresentationObuWithAudioElementIds(
        kMixPresentationId, {kAudioElementId}, kCommonParameterId,
        kCommonParameterRate, obus_to_finalize_);
  }

  void FinalizeExpectOk() {
    MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizer finalizer;

    // `Finalize()` ignores most of the arguments.
    EXPECT_THAT(
        finalizer.Finalize({}, {}, {}, ProduceNoWavWriters, obus_to_finalize_),
        IsOk());
  }

 protected:
  std::list<MixPresentationObu> obus_to_finalize_;
};

TEST_F(MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest,
       NoMixPresentationObus) {
  obus_to_finalize_.clear();
  FinalizeExpectOk();
  EXPECT_TRUE(obus_to_finalize_.empty());
}

TEST_F(MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest,
       CopiesIntegratedLoudnessAndDigitalPeak) {
  const auto kLoudnessInfo = LoudnessInfo{
      .info_type = 0, .integrated_loudness = 99, .digital_peak = 100};
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness = kLoudnessInfo;
  FinalizeExpectOk();

  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kLoudnessInfo);
}

TEST_F(MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest,
       CopiesTruePeak) {
  const auto kLoudnessInfo = LoudnessInfo{.info_type = LoudnessInfo::kTruePeak,
                                          .integrated_loudness = 99,
                                          .digital_peak = 100,
                                          .true_peak = 101};
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness = kLoudnessInfo;
  FinalizeExpectOk();

  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kLoudnessInfo);
}

TEST_F(MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest,
       CopiesAnchoredLoudness) {
  const auto kLoudnessInfo =
      LoudnessInfo{.info_type = LoudnessInfo::kAnchoredLoudness,
                   .integrated_loudness = 99,
                   .digital_peak = 100,
                   .anchored_loudness{
                       .num_anchored_loudness = 2,
                       .anchor_elements = {
                           {.anchor_element =
                                AnchoredLoudnessElement::kAnchorElementDialogue,
                            .anchored_loudness = 1000},
                           {.anchor_element =
                                AnchoredLoudnessElement::kAnchorElementDialogue,
                            .anchored_loudness = 1001}}}};
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness = kLoudnessInfo;
  FinalizeExpectOk();

  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kLoudnessInfo);
}

TEST_F(MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest,
       CopiesExtensionLoudness) {
  const auto kLoudnessInfo = LoudnessInfo{
      .info_type = LoudnessInfo::kAnyLayoutExtension,
      .integrated_loudness = 99,
      .digital_peak = 100,
      .layout_extension = {.info_type_size = 1, .info_type_bytes = {'a'}}};
  obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness = kLoudnessInfo;
  FinalizeExpectOk();

  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kLoudnessInfo);
}

TEST_F(MeasureLoudnessOrFallbackToUserLoudnessMixPresentationFinalizerTest,
       CopiesMultipleObus) {
  obus_to_finalize_.clear();
  const auto kLoudnessInfo = LoudnessInfo{
      .info_type = LoudnessInfo::kAnyLayoutExtension,
      .integrated_loudness = 99,
      .digital_peak = 100,
      .layout_extension = {.info_type_size = 1, .info_type_bytes = {'a'}}};

  // Initialize two user OBUs and the corresponding OBUs.
  for (int i = 0; i < 2; i++) {
    AddMixPresentationObuWithAudioElementIds(
        kMixPresentationId, {kAudioElementId}, kCommonParameterId,
        kCommonParameterRate, obus_to_finalize_);
    obus_to_finalize_.back().sub_mixes_[0].layouts[0].loudness = kLoudnessInfo;
  }
  FinalizeExpectOk();

  EXPECT_EQ(obus_to_finalize_.size(), 2);
  EXPECT_EQ(obus_to_finalize_.front().sub_mixes_[0].layouts[0].loudness,
            kLoudnessInfo);
  EXPECT_EQ(obus_to_finalize_.back().sub_mixes_[0].layouts[0].loudness,
            kLoudnessInfo);
}

}  // namespace
}  // namespace iamf_tools
