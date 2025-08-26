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
#include "iamf/cli/itu_1770_4/loudness_calculator_factory_itu_1770_4.h"

#include <cstdint>

#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

constexpr uint32_t kNumSamplesPerFrame = 1024;
constexpr int32_t kRenderedSampleRate = 48000;

const MixPresentationLayout kStereoMixPresentationLayout = {
    .loudness_layout =
        {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
         .specific_layout =
             LoudspeakersSsConventionLayout{
                 .sound_system =
                     LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}},
    .loudness = {}};

const MixPresentationLayout kUnknownExtensionLayout = {
    .loudness_layout = {.layout_type = Layout::kLayoutTypeReserved0,
                        .specific_layout =
                            LoudspeakersReservedOrBinauralLayout{}},
    .loudness = {}};

TEST(CreateLoudnessCalculator, ReturnsNonNullWhenLayoutIsKnown) {
  const LoudnessCalculatorFactoryItu1770_4 factory;

  EXPECT_NE(factory.CreateLoudnessCalculator(kStereoMixPresentationLayout,
                                             kNumSamplesPerFrame,
                                             kRenderedSampleRate),
            nullptr);
}

TEST(CreateLoudnessCalculator, ReturnsNullWhenLayoutIsUnknown) {
  const LoudnessCalculatorFactoryItu1770_4 factory;

  EXPECT_EQ(
      factory.CreateLoudnessCalculator(
          kUnknownExtensionLayout, kNumSamplesPerFrame, kRenderedSampleRate),
      nullptr);
}

}  // namespace
}  // namespace iamf_tools
