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
#include <vector>

#include "gtest/gtest.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {
namespace {

void TestOneChannelLrs7(const std::vector<int32_t>& original_channel,
                        const std::vector<int32_t>& mixed_channel,
                        const std::vector<int32_t>& demixed_channel,
                        const double expected_recon_gain) {
  const LabelSamplesMap label_to_samples{{"D_Lrs7", original_channel},
                                         {"Ls5", mixed_channel}};
  const LabelSamplesMap label_to_decoded_samples{{"D_Lrs7", demixed_channel}};

  double recon_gain;
  EXPECT_TRUE(ReconGainGenerator::ComputeReconGain(
                  "D_Lrs7", label_to_samples, label_to_decoded_samples,
                  /*additional_logging=*/true, recon_gain)
                  .ok());
  EXPECT_NEAR(recon_gain, expected_recon_gain, 0.0001);
}

TEST(ReconGainGenerator, LessThanFirstThreshold) {
  // 10 * log_10(Ok / 32767^2) ~= -80.30 dB. Since this is < -80 dB the
  // recon gain must be set to 0.0.
  TestOneChannelLrs7({10}, {10}, {10}, 0.0);
}

TEST(ReconGainGenerator, GreaterThanSecondThreshold) {
  // 10 * log_10(Ok/Mk) ~= -4.77 dB. Since this is >= -6 dB the recon gain
  // must be set to 1.0.
  TestOneChannelLrs7({20 << 16}, {60 << 16}, {60 << 16}, 1.0);
}

TEST(ReconGainGenerator, LessThanSecondThreshold) {
  // 10 * log_10(Ok/Mk) ~= -6.99 dB. Since this is < -6 dB the recon gain is
  // set to the value which makes Ok = (Recon_Gain(k,1))^2 * Dk.
  TestOneChannelLrs7({12 << 16}, {60 << 16}, {60 << 16}, 0.4472);
}

}  // namespace
}  // namespace iamf_tools
