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

#include "iamf/cli/proto_conversion/channel_label_utils.h"

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using enum iamf_tools_cli_proto::ChannelLabel;
using enum ChannelLabel::Label;

TEST(ProtoToLabel, SucceedsForMonoInput) {
  EXPECT_THAT(ChannelLabelUtils::ProtoToLabel(CHANNEL_LABEL_MONO),
              IsOkAndHolds(kMono));
}

TEST(ProtoToLabel, FailsForInvalidInput) {
  EXPECT_FALSE(ChannelLabelUtils::ProtoToLabel(CHANNEL_LABEL_INVALID).ok());
}

using LabelTestCase = ::testing::TestWithParam<ChannelLabel::Label>;
TEST_P(LabelTestCase,
       ConvertAndFillLabelsAndLabelToStringForDebuggingAreSymmetric) {
  const ChannelLabel::Label label = GetParam();
  const std::vector<std::string> label_string_for_debugging{
      ChannelLabel::LabelToStringForDebugging(label)};

  std::vector<ChannelLabel::Label> round_trip_labels;
  ASSERT_THAT(ChannelLabelUtils::ConvertAndFillLabels(
                  label_string_for_debugging, round_trip_labels),
              IsOk());
  ASSERT_EQ(round_trip_labels, std::vector<ChannelLabel::Label>{label});
}

INSTANTIATE_TEST_SUITE_P(
    StringToLabelAndLabelToStringAreSymmetric, LabelTestCase,
    ::testing::ValuesIn<ChannelLabel::Label>(
        {kOmitted,
         // Mono channels.
         kMono,
         // Stereo or binaural channels.
         kL2, kR2, kDemixedR2,
         // Centre channel common to several layouts
         // (e.g. 3.1.2, 5.x.y, 7.x.y, 9.1.6).
         kCentre,
         // LFE channel common to several layouts
         // (e.g. 3.1.2, 5.1.y, 7.1.y, 9.1.6).
         kLFE,
         // 3.1.2 surround channels.
         kL3, kR3, kLtf3, kRtf3, kDemixedL3, kDemixedR3,
         // 5.x.y surround channels.
         kL5, kR5, kLs5, kRs5, kDemixedL5, kDemixedR5, kDemixedLs5, kDemixedRs5,
         // Common channels between 5.1.2 and 7.1.2.
         kLtf2, kRtf2, kDemixedLtf2, kDemixedRtf2,
         // Common channels between 5.1.4 and 7.1.4.
         kLtf4, kRtf4, kLtb4, kRtb4, kDemixedLtb4, kDemixedRtb4,
         // 7.x.y surround channels.
         kL7, kR7, kLss7, kRss7, kLrs7, kRrs7, kDemixedL7, kDemixedR7,
         kDemixedLrs7, kDemixedRrs7,
         // 9.1.6 surround channels.
         kFLc, kFC, kFRc, kFL, kFR, kSiL, kSiR, kBL, kBR, kTpFL, kTpFR, kTpSiL,
         kTpSiR, kTpBL, kTpBR,
         // Ambisonics channels.
         kA0, kA1, kA2, kA3, kA4, kA5, kA6, kA7, kA8, kA9, kA10, kA11, kA12,
         kA13, kA14, kA15, kA16, kA17, kA18, kA19, kA20, kA21, kA22, kA23,
         kA24}));

using ProtoLabelTestCase =
    ::testing::TestWithParam<iamf_tools_cli_proto::ChannelLabel>;
TEST_P(ProtoLabelTestCase, ProtoToLabelAndLabelToProtoAreSymmetric) {
  const iamf_tools_cli_proto::ChannelLabel proto_label = GetParam();
  const auto channel_label = ChannelLabelUtils::ProtoToLabel(proto_label);
  EXPECT_THAT(channel_label, IsOk());

  EXPECT_THAT(ChannelLabelUtils::LabelToProto(*channel_label),
              IsOkAndHolds(proto_label));
}

INSTANTIATE_TEST_SUITE_P(
    ProtoToLabelAndLabelToProtoAreSymmetric, ProtoLabelTestCase,
    ::testing::ValuesIn<iamf_tools_cli_proto::ChannelLabel>(
        {CHANNEL_LABEL_MONO,
         // Stereo or binaural channels.
         CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2,
         // Centre channel common to several layouts (e.g. 3.1.2, 5.x.y, 7.x.y).
         CHANNEL_LABEL_CENTRE,
         // LFE channel common to several layouts
         // (e.g. 3.1.2, 5.1.y, 7.1.y, 9.1.6).
         CHANNEL_LABEL_LFE,
         // 3.1.2 surround channels.
         CHANNEL_LABEL_L_3, CHANNEL_LABEL_R_3, CHANNEL_LABEL_LTF_3,
         CHANNEL_LABEL_RTF_3,
         // 5.x.y surround channels.
         CHANNEL_LABEL_L_5, CHANNEL_LABEL_R_5, CHANNEL_LABEL_LS_5,
         CHANNEL_LABEL_RS_5,
         // Common channels between 5.1.2 and 7.1.2.
         CHANNEL_LABEL_LTF_2, CHANNEL_LABEL_RTF_2,
         // Common channels between 5.1.4 and 7.1.4.
         CHANNEL_LABEL_LTF_4, CHANNEL_LABEL_RTF_4, CHANNEL_LABEL_LTB_4,
         CHANNEL_LABEL_RTB_4,
         // 7.x.y surround channels.
         CHANNEL_LABEL_L_7, CHANNEL_LABEL_R_7, CHANNEL_LABEL_LSS_7,
         CHANNEL_LABEL_RSS_7, CHANNEL_LABEL_LRS_7, CHANNEL_LABEL_RRS_7,
         // 9.1.6 surround channels.
         CHANNEL_LABEL_FLC, CHANNEL_LABEL_FC, CHANNEL_LABEL_FRC,
         CHANNEL_LABEL_FL, CHANNEL_LABEL_FR, CHANNEL_LABEL_SI_L,
         CHANNEL_LABEL_SI_R, CHANNEL_LABEL_BL, CHANNEL_LABEL_BR,
         CHANNEL_LABEL_TP_FL, CHANNEL_LABEL_TP_FR, CHANNEL_LABEL_TP_SI_L,
         CHANNEL_LABEL_TP_SI_R, CHANNEL_LABEL_TP_BL, CHANNEL_LABEL_TP_BR,
         CHANNEL_LABEL_A_0, CHANNEL_LABEL_A_1, CHANNEL_LABEL_A_2,
         CHANNEL_LABEL_A_3, CHANNEL_LABEL_A_4, CHANNEL_LABEL_A_5,
         CHANNEL_LABEL_A_6, CHANNEL_LABEL_A_7, CHANNEL_LABEL_A_8,
         CHANNEL_LABEL_A_9, CHANNEL_LABEL_A_10, CHANNEL_LABEL_A_11,
         CHANNEL_LABEL_A_12, CHANNEL_LABEL_A_13, CHANNEL_LABEL_A_14,
         CHANNEL_LABEL_A_15, CHANNEL_LABEL_A_16, CHANNEL_LABEL_A_17,
         CHANNEL_LABEL_A_18, CHANNEL_LABEL_A_19, CHANNEL_LABEL_A_20,
         CHANNEL_LABEL_A_21, CHANNEL_LABEL_A_22, CHANNEL_LABEL_A_23,
         CHANNEL_LABEL_A_24}));

template <class InputContainer, class OutputContainer>
void ExpectConvertAndFillLabelsHasExpectedOutput(
    const InputContainer& input_labels,
    const OutputContainer& expected_output) {
  OutputContainer converted_labels;
  ASSERT_THAT(
      ChannelLabelUtils::ConvertAndFillLabels(input_labels, converted_labels),
      IsOk());
  EXPECT_EQ(converted_labels, expected_output);
}

TEST(ConvertAndFillLabels, SucceedsForStringBasedMonoInput) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"M"}),
      std::vector<ChannelLabel::Label>({kMono}));
}

TEST(ConvertAndFillLabels, SucceedsForStringBasedStereoInput) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"L2", "R2"}),
      std::vector<ChannelLabel::Label>({kL2, kR2}));
}

TEST(ConvertAndFillLabels, SucceedsForStringBased3_1_2Input) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"L3", "R3", "Ltf3", "Rtf3", "C", "LFE"}),
      std::vector<ChannelLabel::Label>(
          {kL3, kR3, kLtf3, kRtf3, kCentre, kLFE}));
}

TEST(ConvertAndFillLabels, SucceedsForStringBased5_1_2Input) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>(
          {"L5", "R5", "Ls5", "Rs5", "Ltf2", "Rtf2", "C", "LFE"}),
      std::vector<ChannelLabel::Label>(
          {kL5, kR5, kLs5, kRs5, kLtf2, kRtf2, kCentre, kLFE}));
}

TEST(ConvertAndFillLabels, SucceedsForStringBased7_1_4Input) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"L7", "R7", "Lss7", "Rss7", "Lrs7",
                                      "Rrs7", "Ltf4", "Rtf4", "Ltb4", "Rtb4",
                                      "C", "LFE"}),
      std::vector<ChannelLabel::Label>({kL7, kR7, kLss7, kRss7, kLrs7, kRrs7,
                                        kLtf4, kRtf4, kLtb4, kRtb4, kCentre,
                                        kLFE}));
}

TEST(ConvertAndFillLabels, SucceedsForStringBasedFirstOrderAmbisonicsInput) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"A0", "A1", "A2", "A3"}),
      std::vector<ChannelLabel::Label>({kA0, kA1, kA2, kA3}));
}

TEST(ConvertAndFillLabels, SucceedsForStringBaseFourthOrderAmbisonicsInput) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"A16", "A24"}),
      std::vector<ChannelLabel::Label>({kA16, kA24}));
}

TEST(ConvertAndFillLabels, InvalidForFifthOrderAmbisonicsInput) {
  const std::vector<absl::string_view> kInvalidFifthOrderAmbisonicsLabels = {
      "A25", "A35"};

  std::vector<ChannelLabel::Label> output;
  EXPECT_FALSE(ChannelLabelUtils::ConvertAndFillLabels(
                   kInvalidFifthOrderAmbisonicsLabels, output)
                   .ok());
  EXPECT_TRUE(output.empty());
}

TEST(ConvertAndFillLabels, InvalidForFourteenthOrderAmbisonicsInput) {
  const std::vector<absl::string_view> kInvalidFourteenthOrderAmbisonicsLabels =
      {"A196", "A224"};

  std::vector<ChannelLabel::Label> output;
  EXPECT_FALSE(ChannelLabelUtils::ConvertAndFillLabels(
                   kInvalidFourteenthOrderAmbisonicsLabels, output)
                   .ok());
  EXPECT_TRUE(output.empty());
}

TEST(ConvertAndFillLabels, OutputContainerHasSameOrderAsInputContainer) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<absl::string_view>({"L2", "R2", "C", "LFE"}),
      std::vector<ChannelLabel::Label>({kL2, kR2, kCentre, kLFE}));
}

TEST(ConvertAndFillLabels, AppendsToOutputContainer) {
  const std::vector<std::string> kInputLabels = {"R2", "C", "LFE"};
  const std::vector<ChannelLabel::Label> kExpectedOutputVector = {
      kL2, kR2, kCentre, kLFE};
  std::vector<ChannelLabel::Label> output_vector = {kL2};
  EXPECT_THAT(
      ChannelLabelUtils::ConvertAndFillLabels(kInputLabels, output_vector),
      IsOk());

  EXPECT_EQ(output_vector, kExpectedOutputVector);
}

TEST(ConvertAndFillLabels, ValidWithUnorderedOutputContainers) {
  const std::vector<std::string> kInputLabels = {"L2", "R2", "C", "LFE"};
  const absl::flat_hash_set<ChannelLabel::Label> kExpectedOutputSet = {
      kL2, kR2, kCentre, kLFE};
  absl::flat_hash_set<ChannelLabel::Label> output_set;
  EXPECT_THAT(ChannelLabelUtils::ConvertAndFillLabels(kInputLabels, output_set),
              IsOk());

  EXPECT_EQ(output_set, kExpectedOutputSet);
}

TEST(ConvertAndFillLabels, ValidWithStereoProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2}),
      std::vector<ChannelLabel::Label>({kL2, kR2}));
}

TEST(ConvertAndFillLabels, ValidWith3_1_2ProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_L_3, CHANNEL_LABEL_R_3, CHANNEL_LABEL_CENTRE,
           CHANNEL_LABEL_LFE, CHANNEL_LABEL_LTF_3, CHANNEL_LABEL_RTF_3}),
      std::vector<ChannelLabel::Label>(
          {kL3, kR3, kCentre, kLFE, kLtf3, kRtf3}));
}

TEST(ConvertAndFillLabels, ValidWith5_1_2ProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_L_5, CHANNEL_LABEL_R_5, CHANNEL_LABEL_CENTRE,
           CHANNEL_LABEL_LFE, CHANNEL_LABEL_LS_5, CHANNEL_LABEL_RS_5,
           CHANNEL_LABEL_LTF_2, CHANNEL_LABEL_RTF_2}),
      std::vector<ChannelLabel::Label>(
          {kL5, kR5, kCentre, kLFE, kLs5, kRs5, kLtf2, kRtf2}));
}

TEST(ConvertAndFillLabels, ValidWith7_1_4ProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_L_7, CHANNEL_LABEL_R_7, CHANNEL_LABEL_CENTRE,
           CHANNEL_LABEL_LFE, CHANNEL_LABEL_LSS_7, CHANNEL_LABEL_RSS_7,
           CHANNEL_LABEL_LRS_7, CHANNEL_LABEL_RRS_7, CHANNEL_LABEL_LTF_4,
           CHANNEL_LABEL_RTF_4, CHANNEL_LABEL_LTB_4, CHANNEL_LABEL_RTB_4}),
      std::vector<ChannelLabel::Label>({kL7, kR7, kCentre, kLFE, kLss7, kRss7,
                                        kLrs7, kRrs7, kLtf4, kRtf4, kLtb4,
                                        kRtb4}));
}

TEST(ConvertAndFillLabels, ValidWith9_1_6ProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_FLC, CHANNEL_LABEL_FC, CHANNEL_LABEL_FRC,
           CHANNEL_LABEL_FL, CHANNEL_LABEL_FR, CHANNEL_LABEL_SI_L,
           CHANNEL_LABEL_SI_R, CHANNEL_LABEL_BL, CHANNEL_LABEL_BR,
           CHANNEL_LABEL_TP_FL, CHANNEL_LABEL_TP_FR, CHANNEL_LABEL_TP_SI_L,
           CHANNEL_LABEL_TP_SI_R, CHANNEL_LABEL_TP_BL, CHANNEL_LABEL_TP_BR}),
      std::vector<ChannelLabel::Label>({kFLc, kFC, kFRc, kFL, kFR, kSiL, kSiR,
                                        kBL, kBR, kTpFL, kTpFR, kTpSiL, kTpSiR,
                                        kTpBL, kTpBR}));
}

TEST(ConvertAndFillLabels, ValidWithZerothOrderAmbisonicsProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>({CHANNEL_LABEL_A_0}),
      std::vector<ChannelLabel::Label>({kA0}));
}

TEST(ConvertAndFillLabels, ValidWithFirstOrderAmbisonicsProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_A_1, CHANNEL_LABEL_A_2, CHANNEL_LABEL_A_3}),
      std::vector<ChannelLabel::Label>({kA1, kA2, kA3}));
}

TEST(ConvertAndFillLabels, ValidWithThirdOrderAmbisonicsProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_A_9, CHANNEL_LABEL_A_15}),
      std::vector<ChannelLabel::Label>({kA9, kA15}));
}

TEST(ConvertAndFillLabels, ValidWithFourthOrderAmbisonicsProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_A_16, CHANNEL_LABEL_A_24}),
      std::vector<ChannelLabel::Label>({kA16, kA24}));
}

TEST(ConvertAndFillLabels, ValidWith7_1_4StringLabels) {
  const std::vector<std::string> k7_1_4InputLabels = {
      "L7",   "R7",   "C",    "LFE",  "Lss7", "Rss7",
      "Lrs7", "Rrs7", "Ltf4", "Rtf4", "Ltb4", "Rtb4"};
  const std::vector<ChannelLabel::Label> kExpectedOutput = {
      kL7,   kR7,   kCentre, kLFE,  kLss7, kRss7,
      kLrs7, kRrs7, kLtf4,   kRtf4, kLtb4, kRtb4};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(
      ChannelLabelUtils::ConvertAndFillLabels(k7_1_4InputLabels, output),
      IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(ConvertAndFillLabels, ValidWith9_1_6StringLabels) {
  const std::vector<std::string> k9_1_6InputLabels = {
      "FLc", "FC",   "FRc",  "FL",    "FR",    "SiL",  "SiR",  "BL",
      "BR",  "TpFL", "TpFR", "TpSiL", "TpSiR", "TpBL", "TpBR", "LFE"};
  const std::vector<ChannelLabel::Label> kExpectedOutput = {
      kFLc, kFC,   kFRc,  kFL,    kFR,    kSiL,  kSiR,  kBL,
      kBR,  kTpFL, kTpFR, kTpSiL, kTpSiR, kTpBL, kTpBR, kLFE};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(
      ChannelLabelUtils::ConvertAndFillLabels(k9_1_6InputLabels, output),
      IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreDuplicateLabelsWithOutputVector) {
  const std::vector<std::string> kInputWithDuplicates = {"R2", "C", "L2"};
  std::vector<ChannelLabel::Label> output_vector = {kL2};

  EXPECT_FALSE(ChannelLabelUtils::ConvertAndFillLabels(kInputWithDuplicates,
                                                       output_vector)
                   .ok());
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreDuplicateLabelsWithOutputSet) {
  const std::vector<std::string> kInputWithDuplicates = {"R2", "C", "L2"};
  absl::flat_hash_set<ChannelLabel::Label> output_set = {kL2};

  EXPECT_FALSE(
      ChannelLabelUtils::ConvertAndFillLabels(kInputWithDuplicates, output_set)
          .ok());
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreUnknownLabels) {
  const std::vector<std::string> kInputWithDuplicates = {"L2", "R2", "C",
                                                         "InvalidLabel"};
  std::vector<ChannelLabel::Label> output;

  EXPECT_FALSE(
      ChannelLabelUtils::ConvertAndFillLabels(kInputWithDuplicates, output)
          .ok());
}

TEST(ConvertAndFillLabels, ValidWithChannelMetadatas) {
  using iamf_tools_cli_proto::ChannelMetadata;
  std::vector<ChannelMetadata> channel_metadatas;
  channel_metadatas.emplace_back().set_channel_label(CHANNEL_LABEL_L_2);
  channel_metadatas.emplace_back().set_channel_label(CHANNEL_LABEL_R_2);
  const std::vector<ChannelLabel::Label> kExpectedOutput = {kL2, kR2};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(
      ChannelLabelUtils::ConvertAndFillLabels(channel_metadatas, output),
      IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(SelectConvertAndFillLabels, FillsBasedOnDeprecatedChannelLabels) {
  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata;
  audio_frame_metadata.add_channel_labels("L2");
  audio_frame_metadata.add_channel_labels("R2");
  const std::vector<ChannelLabel::Label> kExpectedOutput = {kL2, kR2};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(ChannelLabelUtils::SelectConvertAndFillLabels(
                  audio_frame_metadata, output),
              IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(SelectConvertAndFillLabels, SucceedsWithEmptyLabels) {
  const iamf_tools_cli_proto::AudioFrameObuMetadata kEmptyAudioFrameMetadata;
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(ChannelLabelUtils::SelectConvertAndFillLabels(
                  kEmptyAudioFrameMetadata, output),
              IsOk());

  EXPECT_TRUE(output.empty());
}

TEST(SelectConvertAndFillLabels, FillsBasedOnChannelMetadatas) {
  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata;
  auto* channel_metadata = audio_frame_metadata.add_channel_metadatas();
  channel_metadata->set_channel_label(CHANNEL_LABEL_L_2);
  channel_metadata = audio_frame_metadata.add_channel_metadatas();
  channel_metadata->set_channel_label(CHANNEL_LABEL_R_2);
  const std::vector<ChannelLabel::Label> kExpectedOutput = {kL2, kR2};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(ChannelLabelUtils::SelectConvertAndFillLabels(
                  audio_frame_metadata, output),
              IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(SelectConvertAndFillLabels,
     FailsWhenMixingChannelLabelsAndChannelMetadatas) {
  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata;
  auto* channel_metadata = audio_frame_metadata.add_channel_metadatas();
  channel_metadata->set_channel_label(CHANNEL_LABEL_L_2);
  audio_frame_metadata.add_channel_labels("R2");
  std::vector<ChannelLabel::Label> output;

  // Require upgrading all labels in the same `AudioFrameObuMetadata` proto,
  // once one is upgraded.
  EXPECT_FALSE(ChannelLabelUtils::SelectConvertAndFillLabels(
                   audio_frame_metadata, output)
                   .ok());
}

}  // namespace
}  // namespace iamf_tools
