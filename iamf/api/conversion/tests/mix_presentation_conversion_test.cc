/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/api/conversion/mix_presentation_conversion.h"

#include <optional>
#include <utility>
#include <variant>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::TestWithParam;

TEST(ApiToInternalType_OutputLayout, NulloptIsNullopt) {
  std::optional<Layout> resulting_layout = ApiToInternalType(std::nullopt);
  EXPECT_EQ(resulting_layout, std::nullopt);
}

using LayoutPair =
    std::pair<api::OutputLayout, LoudspeakersSsConventionLayout::SoundSystem>;
using ApiToInternalType_OutputLayout = TestWithParam<LayoutPair>;

auto kApiOutputToInternalSoundSystemPairs = ::testing::Values(
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemA_0_2_0,
               LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemB_0_5_0,
               LoudspeakersSsConventionLayout::kSoundSystemB_0_5_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemC_2_5_0,
               LoudspeakersSsConventionLayout::kSoundSystemC_2_5_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemD_4_5_0,
               LoudspeakersSsConventionLayout::kSoundSystemD_4_5_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemE_4_5_1,
               LoudspeakersSsConventionLayout::kSoundSystemE_4_5_1),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemF_3_7_0,
               LoudspeakersSsConventionLayout::kSoundSystemF_3_7_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemG_4_9_0,
               LoudspeakersSsConventionLayout::kSoundSystemG_4_9_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemH_9_10_3,
               LoudspeakersSsConventionLayout::kSoundSystemH_9_10_3),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemI_0_7_0,
               LoudspeakersSsConventionLayout::kSoundSystemI_0_7_0),
    LayoutPair(api::OutputLayout::kItu2051_SoundSystemJ_4_7_0,
               LoudspeakersSsConventionLayout::kSoundSystemJ_4_7_0),
    LayoutPair(api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0,
               LoudspeakersSsConventionLayout::kSoundSystem10_2_7_0),
    LayoutPair(api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0,
               LoudspeakersSsConventionLayout::kSoundSystem11_2_3_0),
    LayoutPair(api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0,
               LoudspeakersSsConventionLayout::kSoundSystem12_0_1_0),
    LayoutPair(api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0,
               LoudspeakersSsConventionLayout::kSoundSystem13_6_9_0));

TEST_P(ApiToInternalType_OutputLayout, ConvertsOutputStereoToInternalLayout) {
  const auto& [api_output_layout, expected_specific_layout] = GetParam();

  std::optional<Layout> resulting_layout = ApiToInternalType(api_output_layout);

  ASSERT_TRUE(resulting_layout.has_value());
  EXPECT_EQ(resulting_layout->layout_type,
            Layout::kLayoutTypeLoudspeakersSsConvention);
  EXPECT_TRUE(std::holds_alternative<LoudspeakersSsConventionLayout>(
      resulting_layout->specific_layout));
  EXPECT_EQ(std::get<LoudspeakersSsConventionLayout>(
                resulting_layout->specific_layout)
                .sound_system,
            expected_specific_layout);
}

INSTANTIATE_TEST_SUITE_P(ApiToInternalType_OutputLayout_Instantiation,
                         ApiToInternalType_OutputLayout,
                         kApiOutputToInternalSoundSystemPairs);

Layout MakeLayout(LoudspeakersSsConventionLayout::SoundSystem sound_system) {
  return {.layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
          .specific_layout =
              LoudspeakersSsConventionLayout{.sound_system = sound_system}};
};

using InternalTypeToApi_OutputLayout = TestWithParam<LayoutPair>;

TEST_P(InternalTypeToApi_OutputLayout, ConvertsInternalStereoToOutputLayout) {
  const auto& [expected_api_output_layout, internal_sound_system] = GetParam();
  Layout internal_layout = MakeLayout(internal_sound_system);

  auto api_output_layout = InternalToApiType(internal_layout);

  EXPECT_THAT(api_output_layout, IsOk());
  EXPECT_EQ(*api_output_layout, expected_api_output_layout);
}

INSTANTIATE_TEST_SUITE_P(InternalTypeToApi_OutputLayout_Instantiation,
                         InternalTypeToApi_OutputLayout,
                         kApiOutputToInternalSoundSystemPairs);
}  // namespace
}  // namespace iamf_tools
