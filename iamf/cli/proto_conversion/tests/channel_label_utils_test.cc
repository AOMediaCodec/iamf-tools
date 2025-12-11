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

#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status_matchers.h"
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
        {// Object channels.
         CHANNEL_LABEL_OBJECT_CHANNEL0, CHANNEL_LABEL_OBJECT_CHANNEL1,
         CHANNEL_LABEL_MONO,
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

TEST(ConvertAndFillLabels, OutputContainerHasSameOrderAsInputContainer) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>(
          {CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2, CHANNEL_LABEL_CENTRE,
           CHANNEL_LABEL_LFE}),
      std::vector<ChannelLabel::Label>({kL2, kR2, kCentre, kLFE}));
}

TEST(ConvertAndFillLabels, AppendsToOutputContainer) {
  const std::vector<iamf_tools_cli_proto::ChannelLabel> kInputLabels = {
      CHANNEL_LABEL_R_2, CHANNEL_LABEL_CENTRE, CHANNEL_LABEL_LFE};
  const std::vector<ChannelLabel::Label> kExpectedOutputVector = {
      kL2, kR2, kCentre, kLFE};
  std::vector<ChannelLabel::Label> output_vector = {kL2};
  EXPECT_THAT(
      ChannelLabelUtils::ConvertAndFillLabels(kInputLabels, output_vector),
      IsOk());

  EXPECT_EQ(output_vector, kExpectedOutputVector);
}

TEST(ConvertAndFillLabels, ValidWithUnorderedOutputContainers) {
  const std::vector<iamf_tools_cli_proto::ChannelLabel> kInputLabels = {
      CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2, CHANNEL_LABEL_CENTRE,
      CHANNEL_LABEL_LFE};
  const absl::flat_hash_set<ChannelLabel::Label> kExpectedOutputSet = {
      kL2, kR2, kCentre, kLFE};
  absl::flat_hash_set<ChannelLabel::Label> output_set;
  EXPECT_THAT(ChannelLabelUtils::ConvertAndFillLabels(kInputLabels, output_set),
              IsOk());

  EXPECT_EQ(output_set, kExpectedOutputSet);
}

TEST(ConvertAndFillLabels, SucceedsForMonoProtoLabels) {
  ExpectConvertAndFillLabelsHasExpectedOutput(
      std::vector<iamf_tools_cli_proto::ChannelLabel>({CHANNEL_LABEL_MONO}),
      std::vector<ChannelLabel::Label>({kMono}));
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

TEST(ConvertAndFillLabels, InvalidWhenThereAreDuplicateLabelsWithOutputVector) {
  const std::vector<iamf_tools_cli_proto::ChannelLabel> kInputWithDuplicates = {
      CHANNEL_LABEL_R_2, CHANNEL_LABEL_CENTRE, CHANNEL_LABEL_L_2};
  std::vector<ChannelLabel::Label> output_vector = {kL2};

  EXPECT_FALSE(ChannelLabelUtils::ConvertAndFillLabels(kInputWithDuplicates,
                                                       output_vector)
                   .ok());
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreDuplicateLabelsWithOutputSet) {
  const std::vector<iamf_tools_cli_proto::ChannelLabel> kInputWithDuplicates = {
      CHANNEL_LABEL_R_2, CHANNEL_LABEL_CENTRE, CHANNEL_LABEL_L_2};
  absl::flat_hash_set<ChannelLabel::Label> output_set = {kL2};

  EXPECT_FALSE(
      ChannelLabelUtils::ConvertAndFillLabels(kInputWithDuplicates, output_set)
          .ok());
}

TEST(ConvertAndFillLabels, InvalidWhenThereAreUnknownLabels) {
  const std::vector<iamf_tools_cli_proto::ChannelLabel> kInputWithDuplicates = {
      CHANNEL_LABEL_L_2, CHANNEL_LABEL_R_2, CHANNEL_LABEL_CENTRE,
      CHANNEL_LABEL_INVALID};
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

}  // namespace
}  // namespace iamf_tools
