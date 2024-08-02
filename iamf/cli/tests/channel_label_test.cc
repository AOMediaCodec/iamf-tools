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

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/audio_element.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;

using enum ChannelLabel::Label;

TEST(StringToLabel, SucceedsForMonoInput) {
  EXPECT_THAT(ChannelLabel::StringToLabel("M"), IsOkAndHolds(kMono));
}

TEST(StringToLabel, SucceedsForStereoInput) {
  EXPECT_THAT(ChannelLabel::StringToLabel("L2"), IsOkAndHolds(kL2));
  EXPECT_THAT(ChannelLabel::StringToLabel("R2"), IsOkAndHolds(kR2));
}

TEST(StringToLabel, SucceedsFor3_1_2Input) {
  EXPECT_THAT(ChannelLabel::StringToLabel("L3"), IsOkAndHolds(kL3));
  EXPECT_THAT(ChannelLabel::StringToLabel("R3"), IsOkAndHolds(kR3));
  EXPECT_THAT(ChannelLabel::StringToLabel("Ltf3"), IsOkAndHolds(kLtf3));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rtf3"), IsOkAndHolds(kRtf3));
  EXPECT_THAT(ChannelLabel::StringToLabel("C"), IsOkAndHolds(kCentre));
  EXPECT_THAT(ChannelLabel::StringToLabel("LFE"), IsOkAndHolds(kLFE));
}

TEST(StringToLabel, SucceedsFor5_1_2Input) {
  EXPECT_THAT(ChannelLabel::StringToLabel("L5"), IsOkAndHolds(kL5));
  EXPECT_THAT(ChannelLabel::StringToLabel("R5"), IsOkAndHolds(kR5));
  EXPECT_THAT(ChannelLabel::StringToLabel("Ls5"), IsOkAndHolds(kLs5));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rs5"), IsOkAndHolds(kRs5));
  EXPECT_THAT(ChannelLabel::StringToLabel("Ltf2"), IsOkAndHolds(kLtf2));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rtf2"), IsOkAndHolds(kRtf2));
  EXPECT_THAT(ChannelLabel::StringToLabel("C"), IsOkAndHolds(kCentre));
  EXPECT_THAT(ChannelLabel::StringToLabel("LFE"), IsOkAndHolds(kLFE));
}

TEST(StringToLabel, SucceedsFor7_1_4Input) {
  EXPECT_THAT(ChannelLabel::StringToLabel("L7"), IsOkAndHolds(kL7));
  EXPECT_THAT(ChannelLabel::StringToLabel("R7"), IsOkAndHolds(kR7));
  EXPECT_THAT(ChannelLabel::StringToLabel("Lss7"), IsOkAndHolds(kLss7));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rss7"), IsOkAndHolds(kRss7));
  EXPECT_THAT(ChannelLabel::StringToLabel("Lrs7"), IsOkAndHolds(kLrs7));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rrs7"), IsOkAndHolds(kRrs7));
  EXPECT_THAT(ChannelLabel::StringToLabel("Ltf4"), IsOkAndHolds(kLtf4));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rtf4"), IsOkAndHolds(kRtf4));
  EXPECT_THAT(ChannelLabel::StringToLabel("Ltb4"), IsOkAndHolds(kLtb4));
  EXPECT_THAT(ChannelLabel::StringToLabel("Rtb4"), IsOkAndHolds(kRtb4));
  EXPECT_THAT(ChannelLabel::StringToLabel("C"), IsOkAndHolds(kCentre));
  EXPECT_THAT(ChannelLabel::StringToLabel("LFE"), IsOkAndHolds(kLFE));
}

TEST(StringToLabel, SucceedsForFOAInput) {
  EXPECT_THAT(ChannelLabel::StringToLabel("A0"), IsOkAndHolds(kA0));
  EXPECT_THAT(ChannelLabel::StringToLabel("A1"), IsOkAndHolds(kA1));
  EXPECT_THAT(ChannelLabel::StringToLabel("A2"), IsOkAndHolds(kA2));
  EXPECT_THAT(ChannelLabel::StringToLabel("A3"), IsOkAndHolds(kA3));
}

TEST(StringToLabel, SucceedsForFourthOrderAmbisonicsInput) {
  EXPECT_THAT(ChannelLabel::StringToLabel("A16"), IsOkAndHolds(kA16));
  EXPECT_THAT(ChannelLabel::StringToLabel("A24"), IsOkAndHolds(kA24));
}

TEST(StringToLabel, InvalidForFifthOrderAmbisonicsInput) {
  EXPECT_FALSE(ChannelLabel::StringToLabel("A25").ok());
  EXPECT_FALSE(ChannelLabel::StringToLabel("A35").ok());
}

TEST(StringToLabel, InvalidForFourteenthOrderAmbisonicsInput) {
  EXPECT_FALSE(ChannelLabel::StringToLabel("A196").ok());
  EXPECT_FALSE(ChannelLabel::StringToLabel("A224").ok());
}

TEST(FillLabelsFromStrings, OutputContainerHasSameOrderAsInputContainer) {
  std::vector<std::string> input_labels = {"L2", "R2", "C", "LFE"};
  const std::vector<ChannelLabel::Label> kExpectedOrderedOutput = {
      kL2, kR2, kCentre, kLFE};
  std::vector<ChannelLabel::Label> ordered_output;
  EXPECT_THAT(ChannelLabel::FillLabelsFromStrings(input_labels, ordered_output),
              IsOk());

  EXPECT_EQ(ordered_output, kExpectedOrderedOutput);
}

TEST(FillLabelsFromStrings, AppendsToOutputContainer) {
  std::vector<std::string> input_labels = {"R2", "C", "LFE"};
  const std::vector<ChannelLabel::Label> kExpectedOutputVector = {
      kL2, kR2, kCentre, kLFE};
  std::vector<ChannelLabel::Label> output_vector = {kL2};
  EXPECT_THAT(ChannelLabel::FillLabelsFromStrings(input_labels, output_vector),
              IsOk());

  EXPECT_EQ(output_vector, kExpectedOutputVector);
}

TEST(FillLabelsFromStrings, ValidWithUnorderedOutputContainers) {
  std::vector<std::string> input_labels = {"L2", "R2", "C", "LFE"};
  const absl::flat_hash_set<ChannelLabel::Label> kExpectedOutputSet = {
      kL2, kR2, kCentre, kLFE};
  absl::flat_hash_set<ChannelLabel::Label> output_set;
  EXPECT_THAT(ChannelLabel::FillLabelsFromStrings(input_labels, output_set),
              IsOk());

  EXPECT_EQ(output_set, kExpectedOutputSet);
}

TEST(FillLabelsFromStrings,
     InvalidWhenThereAreDuplicateLabelsWithOutputVector) {
  const std::vector<std::string> kInputWithDuplicates = {"R2", "C", "L2"};
  std::vector<ChannelLabel::Label> output_vector = {kL2};

  EXPECT_FALSE(
      ChannelLabel::FillLabelsFromStrings(kInputWithDuplicates, output_vector)
          .ok());
}

TEST(FillLabelsFromStrings, InvalidWhenThereAreDuplicateLabelsWithOutputSet) {
  const std::vector<std::string> kInputWithDuplicates = {"R2", "C", "L2"};
  absl::flat_hash_set<ChannelLabel::Label> output_set = {kL2};

  EXPECT_FALSE(
      ChannelLabel::FillLabelsFromStrings(kInputWithDuplicates, output_set)
          .ok());
}

TEST(FillLabelsFromStrings, InvalidWhenThereAreUnknownLabels) {
  const std::vector<std::string> kInputWithDuplicates = {"L2", "R2", "C",
                                                         "InvalidLabel"};
  std::vector<ChannelLabel::Label> output;

  EXPECT_FALSE(
      ChannelLabel::FillLabelsFromStrings(kInputWithDuplicates, output).ok());
}

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

TEST(GetDemixedLabel, InvalidForL2) {
  EXPECT_FALSE(ChannelLabel::GetDemixedLabel(kL2).ok());
}

TEST(GetDemixedLabel, InvalidForCentre) {
  EXPECT_FALSE(ChannelLabel::GetDemixedLabel(kCentre).ok());
}

TEST(GetDemixedLabel, InvalidForLFE) {
  EXPECT_FALSE(ChannelLabel::GetDemixedLabel(kCentre).ok());
}

TEST(GetDemixedLabel, InvalidForAmbisonics) {
  EXPECT_FALSE(ChannelLabel::GetDemixedLabel(kA0).ok());
}

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
                  ChannelAudioLayerConfig::kLayoutMono),
              IsOk());
}

TEST(LookupEarChannelOrderFromScalableLoudspeakerLayout,
     FailsForReservedLayouts10Through14) {
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved10)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved11)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved12)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved13)
                   .ok());
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   kLayoutReserved14)
                   .ok());
}

TEST(LookupEarChannelOrderFromScalableLoudspeakerLayout,
     FailsForReservedLayout15) {
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   ChannelAudioLayerConfig::kLayoutReserved15)
                   .ok());
}

TEST(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
     SucceedsForChannelBasedLayout) {
  EXPECT_THAT(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          ChannelAudioLayerConfig::kLayoutMono),
      IsOk());
}

TEST(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
     FailsForReservedLayouts10Through14) {
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;

  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved10)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved11)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved12)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved13)
          .ok());
  EXPECT_FALSE(
      ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
          kLayoutReserved14)
          .ok());
}

TEST(LookupLabelsToReconstructFromScalableLoudspeakerLayout,
     FailsForReservedLayouts15) {
  EXPECT_FALSE(ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
                   ChannelAudioLayerConfig::kLayoutReserved15)
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
