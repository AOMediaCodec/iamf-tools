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
#include "iamf/cli/channel_label.h"

#include <optional>
#include <vector>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Values;

using enum ChannelLabel::Label;
using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;

constexpr std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
    kNoExpandedLayout = std::nullopt;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ReconGainElement::ReconGainFlagBitmask;

TEST(GetDemixedLabel, SucceedsForDemixedStereo) {
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kR2), IsOkAndHolds(kDemixedR2));
}

TEST(GetDemixedLabel, SucceedsForDemixed3_1_2) {
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kL3), IsOkAndHolds(kDemixedL3));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kR3), IsOkAndHolds(kDemixedR3));
}

TEST(GetDemixedLabel, SucceedsForDemixed5_1_2) {
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kL5), IsOkAndHolds(kDemixedL5));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kR5), IsOkAndHolds(kDemixedR5));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kLs5), IsOkAndHolds(kDemixedLs5));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kRs5), IsOkAndHolds(kDemixedRs5));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kLtf2), IsOkAndHolds(kDemixedLtf2));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kRtf2), IsOkAndHolds(kDemixedRtf2));
}

TEST(GetDemixedLabel, SucceedsForDemixed7_1_4) {
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kL7), IsOkAndHolds(kDemixedL7));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kR7), IsOkAndHolds(kDemixedR7));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kLrs7), IsOkAndHolds(kDemixedLrs7));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kRrs7), IsOkAndHolds(kDemixedRrs7));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kLtb4), IsOkAndHolds(kDemixedLtb4));
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(kRtb4), IsOkAndHolds(kDemixedRtb4));
}

using GetDemixedLabelInvalidTest =
    ::testing::TestWithParam<ChannelLabel::Label>;

TEST_P(GetDemixedLabelInvalidTest, InvalidLabelsReturnError) {
  EXPECT_THAT(ChannelLabel::GetDemixedLabel(GetParam()), Not(IsOk()));
}

INSTANTIATE_TEST_SUITE_P(GetDemixedLabelInvaldForAmbisonicsChannels,
                         GetDemixedLabelInvalidTest,
                         Values(kA0, kA3, kA15, kA24));

INSTANTIATE_TEST_SUITE_P(GetDemixedLabelInvalidForCertainChannel,
                         GetDemixedLabelInvalidTest,
                         Values(kL2, kCentre, kLFE));

INSTANTIATE_TEST_SUITE_P(GetDemixedLabelInvalidFor9_1_6Family,
                         GetDemixedLabelInvalidTest,
                         Values(kFLc, kFRc, kSiL, kSiR));

INSTANTIATE_TEST_SUITE_P(GetDemixedLabelInvalidFor10_2_9_3Family,
                         GetDemixedLabelInvalidTest,
                         Values(kBC, kLFE2, kTpFC, kTpC, kTpBC, kBtFC, kBtFL,
                                kBtFR));

TEST(AmbisonicsChannelNumberToLabel, SucceedsForZerothOrderAmbisonics) {
  constexpr int kFirstZerothOrderAmbisonicsChannel = 0;
  EXPECT_THAT(ChannelLabel::AmbisonicsChannelNumberToLabel(
                  kFirstZerothOrderAmbisonicsChannel),
              IsOkAndHolds(kA0));
}

TEST(AmbisonicsChannelNumberToLabel, SucceedsForFourthOrderAmbisonics) {
  constexpr int kFirstFourthOrderAmbisonicsChannel = 16;
  constexpr int kLastFourthOrderAmbisonicsChannel = 24;
  EXPECT_THAT(ChannelLabel::AmbisonicsChannelNumberToLabel(
                  kFirstFourthOrderAmbisonicsChannel),
              IsOkAndHolds(kA16));
  EXPECT_THAT(ChannelLabel::AmbisonicsChannelNumberToLabel(
                  kLastFourthOrderAmbisonicsChannel),
              IsOkAndHolds(kA24));
}

TEST(AmbisonicsChannelNumberToLabel, InvalidForFifthOrderAmbisonics) {
  constexpr int kFirstFifthOrderAmbisonicsChannel = 25;
  constexpr int kLastFifthOrderAmbisonicsChannel = 35;
  EXPECT_FALSE(ChannelLabel::AmbisonicsChannelNumberToLabel(
                   kFirstFifthOrderAmbisonicsChannel)
                   .ok());
  EXPECT_FALSE(ChannelLabel::AmbisonicsChannelNumberToLabel(
                   kLastFifthOrderAmbisonicsChannel)
                   .ok());
}

TEST(AmbisonicsChannelNumberToLabel, InvalidForFourteenthOrderAmbisonics) {
  constexpr int kFirstFourteenthOrderAmbisonicsChannel = 196;
  constexpr int kLastFourteenthOrderAmbisonicsChannel = 224;
  EXPECT_FALSE(ChannelLabel::AmbisonicsChannelNumberToLabel(
                   kFirstFourteenthOrderAmbisonicsChannel)
                   .ok());
  EXPECT_FALSE(ChannelLabel::AmbisonicsChannelNumberToLabel(
                   kLastFourteenthOrderAmbisonicsChannel)
                   .ok());
}

TEST(LookupEarChannelOrderFromScalableLoudspeakerLayout,
     SucceedsForChannelBasedLayout) {
  EXPECT_THAT(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                  ChannelAudioLayerConfig::kLayoutMono, kNoExpandedLayout),
              IsOk());
}

TEST(LookupEarChannelOrderFromScalableLoudspeakerLayout,
     FailsForReservedLayouts10Through14) {
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved10, kNoExpandedLayout)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved11, kNoExpandedLayout)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved12, kNoExpandedLayout)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved13, kNoExpandedLayout)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved14, kNoExpandedLayout)
                   .ok());
}

TEST(LookupEarChannelOrderFromScalableLoudspeakerLayout,
     InvalidWhenExpandedLayoutIsInconsistent) {
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   ChannelAudioLayerConfig::kLayoutExpanded, kNoExpandedLayout)
                   .ok());
}

struct ExpandedLayoutAndChannelOrderTestCase {
  ChannelAudioLayerConfig::ExpandedLoudspeakerLayout expanded_layout;
  std::vector<ChannelLabel::Label> ordered_labels;
};

using LookupEarChannelOrderFromScalableLoudspeakerLayoutTest =
    ::testing::TestWithParam<ExpandedLayoutAndChannelOrderTestCase>;
TEST_P(LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
       HoldsExpectedValue) {
  EXPECT_THAT(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                  kLayoutExpanded, GetParam().expanded_layout),
              IsOkAndHolds(GetParam().ordered_labels));
}

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutLFE,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutLFE,
                              {kOmitted, kOmitted, kOmitted, kLFE, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted}}));

INSTANTIATE_TEST_SUITE_P(kExpandedLayoutStereoS,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoS,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kLs5,
                               kRs5, kOmitted, kOmitted, kOmitted, kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoSS,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoSS,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kLss7,
                               kRss7, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoRS,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoRS,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kLrs7, kRrs7, kOmitted, kOmitted,
                               kOmitted, kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoTF,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoTF,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kLtf4, kRtf4,
                               kOmitted, kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoTB,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoTB,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kLtb4, kRtb4}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoTop4Ch,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutTop4Ch,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kLtf4, kRtf4,
                               kLtb4, kRtb4}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereo9_1_6Ch,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayout9_1_6_ch,
                              {kFL, kFR, kFC, kLFE, kBL, kBR, kFLc, kFRc, kSiL,
                               kSiR, kTpFL, kTpFR, kTpBL, kTpBR, kTpSiL,
                               kTpSiR}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoF,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoF,
                              {kFL, kFR, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoSi,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoSi,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kSiL, kSiR,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutStereoTpSi,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutStereoTpSi,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kTpSiL,
                               kTpSiR}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutTp6ch,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutTop6Ch,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kTpFL, kTpFR, kTpBL, kTpBR, kTpSiL, kTpSiR}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayout10_2_9_3,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayout10_2_9_3,
                              {kFL,    kFR,    kFC,   kLFE,  kBL,   kBR,
                               kFLc,   kFRc,   kBC,   kLFE2, kSiL,  kSiR,
                               kTpFL,  kTpFR,  kTpFC, kTpC,  kTpBL, kTpBR,
                               kTpSiL, kTpSiR, kTpBC, kBtFC, kBtFL, kBtFR}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutLfePair,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutLfePair,
                              {kOmitted, kOmitted, kOmitted, kLFE,     kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kLFE2,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted}}));

INSTANTIATE_TEST_SUITE_P(ExpandedLayoutBottom3Ch,
                         LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
                         Values<ExpandedLayoutAndChannelOrderTestCase>(
                             {kExpandedLayoutBottom3Ch,
                              {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
                               kOmitted, kBtFC,    kBtFL,    kBtFR}}));

TEST(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
     SucceedsForChannelBasedLayout) {
  EXPECT_THAT(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutMono, kNoExpandedLayout),
      IsOk());
}

TEST(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
     FailsForReservedLayouts10Through14) {
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved10, kNoExpandedLayout)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved11, kNoExpandedLayout)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved12, kNoExpandedLayout)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved13, kNoExpandedLayout)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved14, kNoExpandedLayout)
          .ok());
}

TEST(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
     InvalidWhenExpandedLayoutIsInconsistent) {
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          ChannelAudioLayerConfig::kLayoutExpanded, kNoExpandedLayout)
          .ok());
}

using LookupLabelsToReconstructFromScalableLoudspeakerLayout =
    ::testing::TestWithParam<
        ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>;
TEST_P(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
       ReturnsEmptySet) {
  EXPECT_THAT(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutExpanded, GetParam()),
      IsOkAndHolds(IsEmpty()));
}

INSTANTIATE_TEST_SUITE_P(
    BaseEnhancedProfileExpandedLayoutsReturnEmptySet,
    LookupLabelsToReconstructFromScalableLoudspeakerLayout,
    Values(kExpandedLayoutLFE, kExpandedLayoutStereoS, kExpandedLayoutStereoSS,
           kExpandedLayoutStereoRS, kExpandedLayoutStereoTF,
           kExpandedLayoutStereoTB, kExpandedLayoutTop4Ch,
           kExpandedLayout3_0_ch, kExpandedLayout9_1_6_ch,
           kExpandedLayoutStereoF, kExpandedLayoutStereoSi,
           kExpandedLayoutStereoTpSi, kExpandedLayoutTop6Ch));

TEST(GetDemixedChannelLabelForReconGain, SucceedsForL3) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayout3_1_2_ch,
                                                               kReconGainFlagL),
              IsOkAndHolds(kDemixedL3));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForL5) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayout5_1_ch,
                                                               kReconGainFlagL),
              IsOkAndHolds(kDemixedL5));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForL7) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayout7_1_2_ch,
                                                               kReconGainFlagL),
              IsOkAndHolds(kDemixedL7));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForR2) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayoutStereo,
                                                               kReconGainFlagR),
              IsOkAndHolds(kDemixedR2));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForR3) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayout3_1_2_ch,
                                                               kReconGainFlagR),
              IsOkAndHolds(kDemixedR3));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForR5) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayout5_1_ch,
                                                               kReconGainFlagR),
              IsOkAndHolds(kDemixedR5));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForR7) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayout7_1_2_ch,
                                                               kReconGainFlagR),
              IsOkAndHolds(kDemixedR7));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForLs5) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout5_1_ch, kReconGainFlagLss),
              IsOkAndHolds(kDemixedLs5));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForRs5) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout5_1_ch, kReconGainFlagRss),
              IsOkAndHolds(kDemixedRs5));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForLtf2) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout5_1_ch, kReconGainFlagLtf),
              IsOkAndHolds(kDemixedLtf2));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForRtf2) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout5_1_ch, kReconGainFlagRtf),
              IsOkAndHolds(kDemixedRtf2));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForLrs7) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout7_1_2_ch, kReconGainFlagLrs),
              IsOkAndHolds(kDemixedLrs7));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForRrs7) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout7_1_2_ch, kReconGainFlagRrs),
              IsOkAndHolds(kDemixedRrs7));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForLtb4) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout5_1_4_ch, kReconGainFlagLtb),
              IsOkAndHolds(kDemixedLtb4));
}

TEST(GetDemixedChannelLabelForReconGain, SucceedsForRtb4) {
  EXPECT_THAT(ChannelLabel::GetDemixedChannelLabelForReconGain(
                  kLayout5_1_4_ch, kReconGainFlagRtb),
              IsOkAndHolds(kDemixedRtb4));
}

TEST(GetDemixedChannelLabelForReconGain, FailsForReconGainFlagC) {
  EXPECT_FALSE(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayoutStereo,
                                                                kReconGainFlagC)
                   .ok());
}

TEST(GetDemixedChannelLabelForReconGain, FailsForReconGainFlagLfe) {
  EXPECT_FALSE(ChannelLabel::GetDemixedChannelLabelForReconGain(
                   kLayout5_1_ch, kReconGainFlagLfe)
                   .ok());
}

TEST(GetDemixedChannelLabelForReconGain,
     FailsForReconGainFlagLWithoutAppropriateLayout) {
  EXPECT_FALSE(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayoutStereo,
                                                                kReconGainFlagL)
                   .ok());
}

TEST(GetDemixedChannelLabelForReconGain,
     FailsForReconGainFlagRWithoutAppropriateLayout) {
  EXPECT_FALSE(ChannelLabel::GetDemixedChannelLabelForReconGain(kLayoutMono,
                                                                kReconGainFlagR)
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
