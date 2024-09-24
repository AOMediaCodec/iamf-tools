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
#include "iamf/obu/recon_gain_info_parameter_data.h"

#include <array>
#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {
namespace {

using absl_testing::IsOk;

constexpr uint32_t kAudioElementId = 0;

TEST(ReconGainInfoParameterDataReadTest, TwoLayerParamDefinition) {
  PerIdParameterMetadata per_id_metadata = {
      .recon_gain_is_present_flags = {false, true}};
  std::vector<uint8_t> source_data = {
      // Layer 0 is omitted due to `recon_gain_is_present_flags`.
      // `layer[1]`.
      ReconGainElement::kReconGainFlagR, 1};
  ReadBitBuffer buffer(1024, &source_data);

  ReconGainInfoParameterData recon_gain_info_parameter_data;
  EXPECT_THAT(
      recon_gain_info_parameter_data.ReadAndValidate(per_id_metadata, buffer),
      IsOk());
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements.size(), 1);
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0].recon_gain_flag,
      ReconGainElement::kReconGainFlagR);
  std::array<uint8_t, 12> expected_recon_gain = {0, 0, 1, 0, 0, 0,
                                                 0, 0, 0, 0, 0, 0};
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements[0].recon_gain,
            expected_recon_gain);
}

TEST(ReconGainInfoParameterDataReadTest, MaxLayer7_1_4) {
  PerIdParameterMetadata per_id_metadata = {
      .recon_gain_is_present_flags = {false, true, true, true, true, true}};
  std::vector<uint8_t> source_data = {
      // Layer 0 is omitted due to `recon_gain_is_present_flags`.
      // `layer[1]`.
      ReconGainElement::kReconGainFlagR, 1,
      // `layer[2]`.
      ReconGainElement::kReconGainFlagRss | ReconGainElement::kReconGainFlagLss,
      2, 3,
      // `layer[3]`.
      0x80,
      (ReconGainElement::kReconGainFlagLrs >> 7) |
          (ReconGainElement::kReconGainFlagRrs >> 7),
      4, 5,
      // `layer[4]`.
      ReconGainElement::kReconGainFlagLtf | ReconGainElement::kReconGainFlagRtf,
      6, 7,
      // `layer[5]`.
      0x80,
      (ReconGainElement::kReconGainFlagLtb >> 7) |
          (ReconGainElement::kReconGainFlagRtb >> 7),
      8, 9};
  ReadBitBuffer buffer(1024, &source_data);

  ReconGainInfoParameterData recon_gain_info_parameter_data;
  EXPECT_THAT(
      recon_gain_info_parameter_data.ReadAndValidate(per_id_metadata, buffer),
      IsOk());
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements.size(), 5);

  // Layer 0 is omitted due to `recon_gain_is_present_flags`.
  // `layer[1]`.
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[0].recon_gain_flag,
      ReconGainElement::kReconGainFlagR);
  std::array<uint8_t, 12> expected_recon_gain_layer_1 = {0, 0, 1, 0, 0, 0,
                                                         0, 0, 0, 0, 0, 0};
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements[0].recon_gain,
            expected_recon_gain_layer_1);

  // `layer[2]`.
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[1].recon_gain_flag,
      ReconGainElement::kReconGainFlagRss |
          ReconGainElement::kReconGainFlagLss);
  std::array<uint8_t, 12> expected_recon_gain_layer_2 = {0, 0, 0, 2, 3, 0,
                                                         0, 0, 0, 0, 0, 0};
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements[1].recon_gain,
            expected_recon_gain_layer_2);

  // `layer[3]`.
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[2].recon_gain_flag,
      ReconGainElement::kReconGainFlagLrs |
          ReconGainElement::kReconGainFlagRrs);
  std::array<uint8_t, 12> expected_recon_gain_layer_3 = {
      {0, 0, 0, 0, 0, 0, 0, 4, 5, 0, 0, 0}};
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements[2].recon_gain,
            expected_recon_gain_layer_3);

  // `layer[4]`.
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[3].recon_gain_flag,
      ReconGainElement::kReconGainFlagLtf |
          ReconGainElement::kReconGainFlagRtf);
  std::array<uint8_t, 12> expected_recon_gain_layer_4 = {0, 0, 0, 0, 0, 6,
                                                         7, 0, 0, 0, 0, 0};
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements[3].recon_gain,
            expected_recon_gain_layer_4);

  // `layer[5]`.
  EXPECT_EQ(
      recon_gain_info_parameter_data.recon_gain_elements[4].recon_gain_flag,
      ReconGainElement::kReconGainFlagLtb |
          ReconGainElement::kReconGainFlagRtb);
  std::array<uint8_t, 12> expected_recon_gain_layer_5 = {0, 0, 0, 0, 0, 0,
                                                         0, 0, 0, 8, 9, 0};
  EXPECT_EQ(recon_gain_info_parameter_data.recon_gain_elements[4].recon_gain,
            expected_recon_gain_layer_5);
}

}  // namespace
}  // namespace iamf_tools
