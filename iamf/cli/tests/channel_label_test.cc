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
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::testing::IsEmpty;

using enum iamf_tools_cli_proto::ChannelLabel;
using enum ChannelLabel::Label;
using enum ChannelAudioLayerConfig::ExpandedLoudspeakerLayout;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;

constexpr std::optional<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>
    kNoExpandedLayout = std::nullopt;
using enum ChannelAudioLayerConfig::LoudspeakerLayout;
using enum ReconGainElement::ReconGainFlagBitmask;

TEST(StringToLabel, SucceedsForMonoInput) {
  EXPECT_THAT(ChannelLabel::StringToLabel("M"), IsOkAndHolds(kMono));
}

TEST(ProtoToLabel, SucceedsForMonoInput) {
  EXPECT_THAT(ChannelLabel::ProtoToLabel(CHANNEL_LABEL_MONO),
              IsOkAndHolds(kMono));
}

TEST(ProtoToLabel, FailsForInvalidInput) {
  EXPECT_FALSE(ChannelLabel::ProtoToLabel(CHANNEL_LABEL_INVALID).ok());
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

using LabelTestCase = ::testing::TestWithParam<ChannelLabel::Label>;
TEST_P(LabelTestCase, StringToLabelAndLabelToStringForDebuggingAreSymmetric) {
  const ChannelLabel::Label label = GetParam();
  const std::string label_string_for_debugging =
      ChannelLabel::LabelToStringForDebugging(label);

  EXPECT_THAT(ChannelLabel::StringToLabel(label_string_for_debugging),
              IsOkAndHolds(label));
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
  const auto channel_label = ChannelLabel::ProtoToLabel(proto_label);
  EXPECT_THAT(channel_label, IsOk());

  EXPECT_THAT(ChannelLabel::LabelToProto(*channel_label),
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
      ChannelLabel::ConvertAndFillLabels(input_labels, converted_labels),
      IsOk());
  EXPECT_EQ(converted_labels, expected_output);
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
  EXPECT_THAT(ChannelLabel::ConvertAndFillLabels(kInputLabels, output_vector),
              IsOk());

  EXPECT_EQ(output_vector, kExpectedOutputVector);
}

TEST(ConvertAndFillLabels, ValidWithUnorderedOutputContainers) {
  const std::vector<std::string> kInputLabels = {"L2", "R2", "C", "LFE"};
  const absl::flat_hash_set<ChannelLabel::Label> kExpectedOutputSet = {
      kL2, kR2, kCentre, kLFE};
  absl::flat_hash_set<ChannelLabel::Label> output_set;
  EXPECT_THAT(ChannelLabel::ConvertAndFillLabels(kInputLabels, output_set),
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
  EXPECT_THAT(ChannelLabel::ConvertAndFillLabels(k7_1_4InputLabels, output),
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
  EXPECT_THAT(ChannelLabel::ConvertAndFillLabels(k9_1_6InputLabels, output),
              IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreDuplicateLabelsWithOutputVector) {
  const std::vector<std::string> kInputWithDuplicates = {"R2", "C", "L2"};
  std::vector<ChannelLabel::Label> output_vector = {kL2};

  EXPECT_FALSE(
      ChannelLabel::ConvertAndFillLabels(kInputWithDuplicates, output_vector)
          .ok());
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreDuplicateLabelsWithOutputSet) {
  const std::vector<std::string> kInputWithDuplicates = {"R2", "C", "L2"};
  absl::flat_hash_set<ChannelLabel::Label> output_set = {kL2};

  EXPECT_FALSE(
      ChannelLabel::ConvertAndFillLabels(kInputWithDuplicates, output_set)
          .ok());
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreUnknownLabels) {
  const std::vector<std::string> kInputWithDuplicates = {"L2", "R2", "C",
                                                         "InvalidLabel"};
  std::vector<ChannelLabel::Label> output;

  EXPECT_FALSE(
      ChannelLabel::ConvertAndFillLabels(kInputWithDuplicates, output).ok());
}

TEST(ConvertAndFillLabels, ValidWithChannelMetadatas) {
  using iamf_tools_cli_proto::ChannelMetadata;
  std::vector<ChannelMetadata> channel_metadatas;
  channel_metadatas.emplace_back().set_channel_label(CHANNEL_LABEL_L_2);
  channel_metadatas.emplace_back().set_channel_label(CHANNEL_LABEL_R_2);
  const std::vector<ChannelLabel::Label> kExpectedOutput = {kL2, kR2};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(ChannelLabel::ConvertAndFillLabels(channel_metadatas, output),
              IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(SelectConvertAndFillLabels, FillsBasedOnDeprecatedChannelLabels) {
  iamf_tools_cli_proto::AudioFrameObuMetadata audio_frame_metadata;
  audio_frame_metadata.add_channel_labels("L2");
  audio_frame_metadata.add_channel_labels("R2");
  const std::vector<ChannelLabel::Label> kExpectedOutput = {kL2, kR2};
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(
      ChannelLabel::SelectConvertAndFillLabels(audio_frame_metadata, output),
      IsOk());

  EXPECT_EQ(output, kExpectedOutput);
}

TEST(SelectConvertAndFillLabels, SucceedsWithEmptyLabels) {
  const iamf_tools_cli_proto::AudioFrameObuMetadata kEmptyAudioFrameMetadata;
  std::vector<ChannelLabel::Label> output;
  EXPECT_THAT(ChannelLabel::SelectConvertAndFillLabels(kEmptyAudioFrameMetadata,
                                                       output),
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
  EXPECT_THAT(
      ChannelLabel::SelectConvertAndFillLabels(audio_frame_metadata, output),
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
  EXPECT_FALSE(
      ChannelLabel::SelectConvertAndFillLabels(audio_frame_metadata, output)
          .ok());
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

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutLFE, LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutLFE,
          {kOmitted, kOmitted, kOmitted, kLFE, kOmitted, kOmitted, kOmitted,
           kOmitted, kOmitted, kOmitted, kOmitted, kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    kExpandedLayoutStereoS,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoS,
          {kOmitted, kOmitted, kOmitted, kOmitted, kLs5, kRs5, kOmitted,
           kOmitted, kOmitted, kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoSS,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoSS,
          {kOmitted, kOmitted, kOmitted, kOmitted, kLss7, kRss7, kOmitted,
           kOmitted, kOmitted, kOmitted, kOmitted, kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoRS,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoRS,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kLrs7,
           kRrs7, kOmitted, kOmitted, kOmitted, kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoTF,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoTF,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kLtf4, kRtf4, kOmitted, kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoTB,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoTB,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kOmitted, kOmitted, kLtb4, kRtb4}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoTop4Ch,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutTop4Ch,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kLtf4, kRtf4, kLtb4, kRtb4}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereo9_1_6Ch,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayout9_1_6_ch,
          {kFL, kFR, kFC, kLFE, kBL, kBR, kFLc, kFRc, kSiL, kSiR, kTpFL, kTpFR,
           kTpBL, kTpBR, kTpSiL, kTpSiR}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoF,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoF,
          {kFL, kFR, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoSi,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoSi,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kSiL, kSiR, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kOmitted}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutStereoTpSi,
    LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutStereoTpSi,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kTpSiL, kTpSiR}}}));

INSTANTIATE_TEST_SUITE_P(
    ExpandedLayoutTp6ch, LookupEarChannelOrderFromScalableLoudspeakerLayoutTest,
    ::testing::ValuesIn<ExpandedLayoutAndChannelOrderTestCase>(
        {{kExpandedLayoutTop6Ch,
          {kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted, kOmitted,
           kOmitted, kOmitted, kOmitted, kTpFL, kTpFR, kTpBL, kTpBR, kTpSiL,
           kTpSiR}}}));

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
    ::testing::ValuesIn<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>(
        {kExpandedLayoutLFE, kExpandedLayoutStereoS, kExpandedLayoutStereoSS,
         kExpandedLayoutStereoRS, kExpandedLayoutStereoTF,
         kExpandedLayoutStereoTB, kExpandedLayoutTop4Ch, kExpandedLayout3_0_ch,
         kExpandedLayout9_1_6_ch, kExpandedLayoutStereoF,
         kExpandedLayoutStereoSi, kExpandedLayoutStereoTpSi,
         kExpandedLayoutTop6Ch}));

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
