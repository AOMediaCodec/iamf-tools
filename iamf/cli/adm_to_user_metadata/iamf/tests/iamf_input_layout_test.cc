/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/adm_to_user_metadata/iamf/iamf_input_layout.h"

#include <string>

#include "gtest/gtest.h"

namespace iamf_tools {
namespace adm_to_user_metadata {
namespace {

TEST(LookupInputLayoutFromAudioPackFormatId, UnknownAudioPackFormatId) {
  EXPECT_FALSE(LookupInputLayoutFromAudioPackFormatId("").ok());
  EXPECT_FALSE(LookupInputLayoutFromAudioPackFormatId("AP_00020001").ok());
  EXPECT_FALSE(LookupInputLayoutFromAudioPackFormatId("00010002").ok());
  EXPECT_FALSE(LookupInputLayoutFromAudioPackFormatId("Stereo").ok());
}

struct SupportedAudioPackFormatIdTestCase {
  std::string test_audio_pack_format_id;
  IamfInputLayout expected_layout;
};

using SupportedAudioPackFormatId =
    ::testing::TestWithParam<SupportedAudioPackFormatIdTestCase>;

TEST_P(SupportedAudioPackFormatId, TestLookupInputLayoutFromAudioPackFormatId) {
  const SupportedAudioPackFormatIdTestCase& test_case = GetParam();

  const auto layout = LookupInputLayoutFromAudioPackFormatId(
      test_case.test_audio_pack_format_id);
  ASSERT_TRUE(layout.ok());
  EXPECT_EQ(*layout, test_case.expected_layout);
}

INSTANTIATE_TEST_SUITE_P(ChannelBased, SupportedAudioPackFormatId,
                         testing::ValuesIn<SupportedAudioPackFormatIdTestCase>({
                             {"AP_00010001", IamfInputLayout::kMono},
                             {"AP_00010002", IamfInputLayout::kStereo},
                             {"AP_00010017", IamfInputLayout::k7_1_4},
                         }));

INSTANTIATE_TEST_SUITE_P(Binaraul, SupportedAudioPackFormatId,
                         testing::ValuesIn<SupportedAudioPackFormatIdTestCase>(
                             {{"AP_00050001", IamfInputLayout::kBinaural}}));

INSTANTIATE_TEST_SUITE_P(
    Ambisonics, SupportedAudioPackFormatId,
    testing::ValuesIn<SupportedAudioPackFormatIdTestCase>({
        {"AP_00040001", IamfInputLayout::kAmbisonicsOrder1},
        {"AP_00040002", IamfInputLayout::kAmbisonicsOrder2},
        {"AP_00040003", IamfInputLayout::kAmbisonicsOrder3},
    }));

}  // namespace
}  // namespace adm_to_user_metadata
}  // namespace iamf_tools
