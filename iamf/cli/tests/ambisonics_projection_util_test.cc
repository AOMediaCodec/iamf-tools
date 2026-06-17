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

#include "iamf/cli/ambisonics_projection_util.h"

#include <string>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/codec_config.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::VariantWith;

using enum CodecConfig::CodecId;
using enum AmbisonicsConfig::AmbisonicsMode;

struct MakeFromPresetTestCase {
  CodecConfig::CodecId codec_id;
  AmbisonicsMixer::Preset preset;

  AmbisonicsConfig::AmbisonicsMode expected_mode;
};

class MakeFromPresetParameterizedTest
    : public testing::TestWithParam<MakeFromPresetTestCase> {};

TEST_P(MakeFromPresetParameterizedTest, ConfigHasExpectedMode) {
  const auto& test_case = GetParam();
  auto mixer =
      AmbisonicsMixer::MakeFromPreset(test_case.codec_id, test_case.preset);

  EXPECT_EQ(mixer.GetAmbisonicsConfig().GetAmbisonicsMode(),
            test_case.expected_mode);
}

INSTANTIATE_TEST_SUITE_P(
    BestPracticeForOrder0IsAlwaysMono, MakeFromPresetParameterizedTest,
    testing::Values(
        MakeFromPresetTestCase{
            .codec_id = kCodecIdLpcm,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder0,
            .expected_mode = kAmbisonicsModeMono,
        },
        MakeFromPresetTestCase{
            .codec_id = kCodecIdOpus,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder0,
            .expected_mode = kAmbisonicsModeMono,
        }));

INSTANTIATE_TEST_SUITE_P(
    BestPracticeForOrder1IsAlwaysMono, MakeFromPresetParameterizedTest,
    testing::Values(
        MakeFromPresetTestCase{
            .codec_id = kCodecIdLpcm,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder1,
            .expected_mode = kAmbisonicsModeMono,
        },
        MakeFromPresetTestCase{
            .codec_id = kCodecIdOpus,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder1,
            .expected_mode = kAmbisonicsModeMono,
        }));

INSTANTIATE_TEST_SUITE_P(
    BestPracticeForOrder2IsAlwaysMono, MakeFromPresetParameterizedTest,
    testing::Values(
        MakeFromPresetTestCase{
            .codec_id = kCodecIdLpcm,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder2,
            .expected_mode = kAmbisonicsModeMono,
        },
        MakeFromPresetTestCase{
            .codec_id = kCodecIdOpus,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder2,
            .expected_mode = kAmbisonicsModeMono,
        }));

INSTANTIATE_TEST_SUITE_P(
    BestPracticeForOrder3IsProjectionForOpus, MakeFromPresetParameterizedTest,
    testing::Values(MakeFromPresetTestCase{
        .codec_id = kCodecIdOpus,
        .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder3,
        .expected_mode = kAmbisonicsModeProjection,
    }));

INSTANTIATE_TEST_SUITE_P(
    BestPracticeForOrder3IsMonoForNonOpus, MakeFromPresetParameterizedTest,
    testing::Values(
        MakeFromPresetTestCase{
            .codec_id = kCodecIdLpcm,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder3,
            .expected_mode = kAmbisonicsModeMono,
        },
        MakeFromPresetTestCase{
            .codec_id = kCodecIdFlac,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder3,
            .expected_mode = kAmbisonicsModeMono,
        },
        MakeFromPresetTestCase{
            .codec_id = kCodecIdAacLc,
            .preset = AmbisonicsMixer::Preset::kBestPracticeForOrder3,
            .expected_mode = kAmbisonicsModeMono,
        }));

TEST(MakeFromAmbisonicsConfigTest, WrapsInputConfig) {
  // Create an arbitrary config.
  auto mixed_order_mono_config = AmbisonicsMonoConfig::Create(
      3, {0, 1, 2, AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber});
  ASSERT_THAT(mixed_order_mono_config, IsOk());
  const AmbisonicsConfig config = {.ambisonics_config =
                                       *mixed_order_mono_config};

  const AmbisonicsMixer mixer =
      AmbisonicsMixer::MakeFromAmbisonicsConfig(config);

  EXPECT_THAT(mixer.GetAmbisonicsConfig().ambisonics_config,
              VariantWith<AmbisonicsMonoConfig>(*mixed_order_mono_config));
}

}  // namespace
}  // namespace iamf_tools
