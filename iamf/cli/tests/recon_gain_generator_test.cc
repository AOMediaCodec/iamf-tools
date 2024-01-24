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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/demixing_module.h"
#include "iamf/cli/proto/user_metadata.pb.h"

namespace iamf_tools {
namespace {

void TestOneChannelLrs7(const std::vector<int32_t>& original_channel,
                        const std::vector<int32_t>& mixed_channel,
                        const std::vector<int32_t>& demixed_channel,
                        const double expected_recon_gain) {
  const uint32_t audio_element_id = 37;
  const int32_t start_timestamp = 0;
  const IdTimeLabeledFrameMap id_to_time_to_labeled_frame{
      {audio_element_id,
       {{start_timestamp,
         {.label_to_samples = {{"D_Lrs7", original_channel},
                               {"Ls5", mixed_channel}}}}}}};
  const IdTimeLabeledFrameMap id_to_time_to_labeled_decoded_frame{
      {audio_element_id,
       {{start_timestamp,
         {.label_to_samples = {{"D_Lrs7", demixed_channel}}}}}}};
  ReconGainGenerator generator(id_to_time_to_labeled_frame,
                               id_to_time_to_labeled_decoded_frame);

  double recon_gain;
  EXPECT_TRUE(generator
                  .ComputeReconGain("D_Lrs7", audio_element_id, start_timestamp,
                                    recon_gain)
                  .ok());
  EXPECT_THAT(recon_gain, testing::DoubleNear(expected_recon_gain, 0.0001));
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

TEST(ReconGainGenerator, AccessAdditionalLogging) {
  ReconGainGenerator generator({}, {});
  EXPECT_TRUE(generator.additional_logging());
  generator.set_additional_logging(false);
  EXPECT_FALSE(generator.additional_logging());
  generator.set_additional_logging(true);
  EXPECT_TRUE(generator.additional_logging());
}

}  // namespace
}  // namespace iamf_tools
