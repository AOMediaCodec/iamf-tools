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

#include "iamf/cli/ambisonics_mixer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::DoubleNear;
using ::testing::Pointwise;
using ::testing::VariantWith;

using enum CodecConfig::CodecId;
using enum AmbisonicsConfig::AmbisonicsMode;

constexpr size_t kSamplesPerFrame = 1024;
constexpr InternalSampleType kTolerance = 1e-6;

MATCHER_P2(PointwiseDoubleNear2D, expected, tolerance, "") {
  if (!testing::ExplainMatchResult(testing::Eq(expected.size()), arg.size(),
                                   result_listener)) {
    return false;
  }
  for (size_t c = 0; c < arg.size(); ++c) {
    if (!testing::ExplainMatchResult(
            Pointwise(DoubleNear(tolerance), expected[c]), arg[c],
            result_listener)) {
      return false;
    }
  }
  return true;
}

void ApplyDemixingMatrix(
    absl::Span<const int16_t> demixing_matrix,
    absl::Span<const absl::Span<const InternalSampleType>> mixed_samples,
    std::vector<std::vector<InternalSampleType>>& reconstructed_samples) {
  const size_t num_channels = mixed_samples.size();
  const auto num_ticks = mixed_samples[0].size();
  reconstructed_samples.assign(num_channels,
                               std::vector<InternalSampleType>(num_ticks, 0.0));

  for (size_t out_channel = 0; out_channel < num_channels; ++out_channel) {
    for (size_t in_channel = 0; in_channel < num_channels; ++in_channel) {
      const double demixing_value =
          static_cast<double>(
              demixing_matrix[in_channel * num_channels + out_channel]) /
          32768.0;
      for (size_t t = 0; t < num_ticks; ++t) {
        reconstructed_samples[out_channel][t] +=
            demixing_value * mixed_samples[in_channel][t];
      }
    }
  }
}

struct MakeFromPresetTestCase {
  CodecConfig::CodecId codec_id;
  AmbisonicsMixer::Preset preset;

  AmbisonicsConfig::AmbisonicsMode expected_mode;
};

class MakeFromPresetParameterizedTest
    : public testing::TestWithParam<MakeFromPresetTestCase> {};

TEST_P(MakeFromPresetParameterizedTest, ConfigHasExpectedMode) {
  const auto& test_case = GetParam();
  auto mixer = AmbisonicsMixer::MakeFromPreset(
      test_case.codec_id, test_case.preset, kSamplesPerFrame);

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
      AmbisonicsMixer::MakeFromAmbisonicsConfig(config, kSamplesPerFrame);

  EXPECT_THAT(mixer.GetAmbisonicsConfig().ambisonics_config,
              VariantWith<AmbisonicsMonoConfig>(*mixed_order_mono_config));
}

TEST(PushFrame, IsTransparentForNonProjection) {
  constexpr size_t kNumChannels = 4;
  constexpr size_t kFourSamplesPerFrame = 4;
  auto mixer = AmbisonicsMixer::MakeFromPreset(
      CodecConfig::kCodecIdOpus,
      AmbisonicsMixer::Preset::kBestPracticeForOrder1, kFourSamplesPerFrame);
  const std::vector<std::vector<InternalSampleType>> input_samples(
      kNumChannels, {0.0, 0.25, 0.5, 0.75});

  EXPECT_THAT(mixer.PushFrame(MakeSpanOfConstSpans(input_samples)), IsOk());

  EXPECT_THAT(mixer.GetOutputSamplesAsSpan(),
              PointwiseDoubleNear2D(input_samples, kTolerance));
}

TEST(PushFrame, ThirdOrderProjectionIsReversibleWithDemixingMatrix) {
  // Due to quantization errors, the reconstructed samples will not be exactly
  // the same as the input ones.
  constexpr double kEquivalenceTolerance = 5e-4;
  auto mixer = AmbisonicsMixer::MakeFromPreset(
      CodecConfig::kCodecIdOpus,
      AmbisonicsMixer::Preset::kBestPracticeForOrder3, kSamplesPerFrame);
  const size_t kNumChannels = 16;
  std::vector<std::vector<InternalSampleType>> input_samples(kNumChannels);
  // Fill input with sine waves of different frequencies.
  for (size_t c = 0; c < kNumChannels; ++c) {
    double frequency_hz = 1000.0 + c * 100.0;
    input_samples[c] = GenerateSineWav(/*start_tick=*/0, kSamplesPerFrame,
                                       /*sample_rate_hz=*/48000, frequency_hz,
                                       /*amplitude=*/1.0);
  }

  // Mix from HOA to substreams signals.
  EXPECT_THAT(mixer.PushFrame(MakeSpanOfConstSpans(input_samples)), IsOk());

  // Reconstruct from substreams signals to ACN using the demixing matrix.
  auto mixed_samples = mixer.GetOutputSamplesAsSpan();
  auto config = mixer.GetAmbisonicsConfig();
  auto demixing_matrix = config.GetDemixingMatrix();
  ASSERT_TRUE(demixing_matrix.has_value());
  std::vector<std::vector<InternalSampleType>> reconstructed_samples;
  ApplyDemixingMatrix(*demixing_matrix, mixed_samples, reconstructed_samples);

  // We expect the reconstructed samples to be close to the original ones.
  EXPECT_THAT(reconstructed_samples,
              PointwiseDoubleNear2D(input_samples, kEquivalenceTolerance));
}

}  // namespace
}  // namespace iamf_tools
