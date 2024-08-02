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

#include "iamf/cli/recon_gain_generator.h"

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using enum ChannelLabel::Label;

const int32_t kArbitrarySample = std::numeric_limits<int32_t>::max();

void TestComputeReconGainForOneChannelLrs7(
    const std::vector<int32_t>& original_channel,
    const std::vector<int32_t>& mixed_channel,
    const std::vector<int32_t>& demixed_channel,
    const double expected_recon_gain) {
  const LabelSamplesMap label_to_samples{{kDemixedLrs7, original_channel},
                                         {kLs5, mixed_channel}};
  const LabelSamplesMap label_to_decoded_samples{
      {kDemixedLrs7, demixed_channel}};

  double recon_gain;
  EXPECT_THAT(ReconGainGenerator::ComputeReconGain(
                  kDemixedLrs7, label_to_samples, label_to_decoded_samples,
                  /*additional_logging=*/true, recon_gain),
              IsOk());
  EXPECT_NEAR(recon_gain, expected_recon_gain, 0.0001);
}

TEST(ComputeReconGain, LessThanFirstThreshold) {
  // 10 * log_10(Ok / 32767^2) ~= -80.30 dB. Since this is < -80 dB the
  // recon gain must be set to 0.0.
  TestComputeReconGainForOneChannelLrs7({10}, {10}, {10}, 0.0);
}

TEST(ComputeReconGain, GreaterThanSecondThreshold) {
  // 10 * log_10(Ok/Mk) ~= -4.77 dB. Since this is >= -6 dB the recon gain
  // must be set to 1.0.
  TestComputeReconGainForOneChannelLrs7({20 << 16}, {60 << 16}, {60 << 16},
                                        1.0);
}

TEST(ComputeReconGain, LessThanSecondThreshold) {
  // 10 * log_10(Ok/Mk) ~= -6.99 dB. Since this is < -6 dB the recon gain is
  // set to the value which makes Ok = (Recon_Gain(k,1))^2 * Dk.
  TestComputeReconGainForOneChannelLrs7({12 << 16}, {60 << 16}, {60 << 16},
                                        0.4472);
}

TEST(ComputeReconGain, SucceedsForTwoLayerStereo) {
  const std::vector<int32_t> kOriginalChannel{kArbitrarySample};
  const std::vector<int32_t> kMixedChannel{kArbitrarySample};
  const std::vector<int32_t> kDemixedChannel{kArbitrarySample};
  const LabelSamplesMap label_to_samples{{kDemixedR2, kOriginalChannel},
                                         {kMono, kMixedChannel}};
  const LabelSamplesMap label_to_decoded_samples{{kDemixedR2, kDemixedChannel}};

  double recon_gain;
  EXPECT_THAT(ReconGainGenerator::ComputeReconGain(
                  kDemixedR2, label_to_samples, label_to_decoded_samples,
                  /*additional_logging=*/true, recon_gain),
              IsOk());
}

TEST(ComputeReconGain, InvalidWhenRelevantMixedSampleCannotBeFound) {
  const std::vector<int32_t> kOriginalChannel{kArbitrarySample};
  const std::vector<int32_t> kDemixedChannel{kArbitrarySample};
  const LabelSamplesMap label_to_samples{{kDemixedR2, kOriginalChannel}};
  const LabelSamplesMap label_to_decoded_samples{{kDemixedR2, kDemixedChannel}};

  double recon_gain;
  EXPECT_FALSE(ReconGainGenerator::ComputeReconGain(
                   kDemixedR2, label_to_samples, label_to_decoded_samples,
                   /*additional_logging=*/true, recon_gain)
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
