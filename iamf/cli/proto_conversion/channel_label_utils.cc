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

#include <array>
#include <utility>

#include "absl/status/statusor.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/common/utils/map_utils.h"

namespace iamf_tools {
namespace {

using enum iamf_tools_cli_proto::ChannelLabel;
using enum ChannelLabel::Label;

constexpr auto kProtoAndInternalLabel = std::to_array<
    std::pair<iamf_tools_cli_proto::ChannelLabel, ChannelLabel::Label>>({
    {CHANNEL_LABEL_MONO, kMono},     {CHANNEL_LABEL_L_2, kL2},
    {CHANNEL_LABEL_R_2, kR2},        {CHANNEL_LABEL_CENTRE, kCentre},
    {CHANNEL_LABEL_LFE, kLFE},       {CHANNEL_LABEL_L_3, kL3},
    {CHANNEL_LABEL_R_3, kR3},        {CHANNEL_LABEL_LTF_3, kLtf3},
    {CHANNEL_LABEL_RTF_3, kRtf3},    {CHANNEL_LABEL_L_5, kL5},
    {CHANNEL_LABEL_R_5, kR5},        {CHANNEL_LABEL_LS_5, kLs5},
    {CHANNEL_LABEL_RS_5, kRs5},      {CHANNEL_LABEL_LTF_2, kLtf2},
    {CHANNEL_LABEL_RTF_2, kRtf2},    {CHANNEL_LABEL_LTF_4, kLtf4},
    {CHANNEL_LABEL_RTF_4, kRtf4},    {CHANNEL_LABEL_LTB_4, kLtb4},
    {CHANNEL_LABEL_RTB_4, kRtb4},    {CHANNEL_LABEL_L_7, kL7},
    {CHANNEL_LABEL_R_7, kR7},        {CHANNEL_LABEL_LSS_7, kLss7},
    {CHANNEL_LABEL_RSS_7, kRss7},    {CHANNEL_LABEL_LRS_7, kLrs7},
    {CHANNEL_LABEL_RRS_7, kRrs7},    {CHANNEL_LABEL_FLC, kFLc},
    {CHANNEL_LABEL_FC, kFC},         {CHANNEL_LABEL_FRC, kFRc},
    {CHANNEL_LABEL_FL, kFL},         {CHANNEL_LABEL_FR, kFR},
    {CHANNEL_LABEL_SI_L, kSiL},      {CHANNEL_LABEL_SI_R, kSiR},
    {CHANNEL_LABEL_BL, kBL},         {CHANNEL_LABEL_BR, kBR},
    {CHANNEL_LABEL_TP_FL, kTpFL},    {CHANNEL_LABEL_TP_FR, kTpFR},
    {CHANNEL_LABEL_TP_SI_L, kTpSiL}, {CHANNEL_LABEL_TP_SI_R, kTpSiR},
    {CHANNEL_LABEL_TP_BL, kTpBL},    {CHANNEL_LABEL_TP_BR, kTpBR},
    {CHANNEL_LABEL_BC, kBC},         {CHANNEL_LABEL_LFE2, kLFE2},
    {CHANNEL_LABEL_TP_FC, kTpFC},    {CHANNEL_LABEL_TP_C, kTpC},
    {CHANNEL_LABEL_TP_BC, kTpBC},    {CHANNEL_LABEL_BT_FC, kBtFC},
    {CHANNEL_LABEL_BT_FL, kBtFL},    {CHANNEL_LABEL_BT_FR, kBtFR},
    {CHANNEL_LABEL_A_0, kA0},        {CHANNEL_LABEL_A_1, kA1},
    {CHANNEL_LABEL_A_2, kA2},        {CHANNEL_LABEL_A_3, kA3},
    {CHANNEL_LABEL_A_4, kA4},        {CHANNEL_LABEL_A_5, kA5},
    {CHANNEL_LABEL_A_6, kA6},        {CHANNEL_LABEL_A_7, kA7},
    {CHANNEL_LABEL_A_8, kA8},        {CHANNEL_LABEL_A_9, kA9},
    {CHANNEL_LABEL_A_10, kA10},      {CHANNEL_LABEL_A_11, kA11},
    {CHANNEL_LABEL_A_12, kA12},      {CHANNEL_LABEL_A_13, kA13},
    {CHANNEL_LABEL_A_14, kA14},      {CHANNEL_LABEL_A_15, kA15},
    {CHANNEL_LABEL_A_16, kA16},      {CHANNEL_LABEL_A_17, kA17},
    {CHANNEL_LABEL_A_18, kA18},      {CHANNEL_LABEL_A_19, kA19},
    {CHANNEL_LABEL_A_20, kA20},      {CHANNEL_LABEL_A_21, kA21},
    {CHANNEL_LABEL_A_22, kA22},      {CHANNEL_LABEL_A_23, kA23},
    {CHANNEL_LABEL_A_24, kA24},
});
}  // namespace

absl::StatusOr<ChannelLabel::Label> ChannelLabelUtils::ProtoToLabel(
    iamf_tools_cli_proto::ChannelLabel proto_label) {
  static const auto kProtoToLabel =
      BuildStaticMapFromPairs(kProtoAndInternalLabel);

  return LookupInMap(*kProtoToLabel, proto_label,
                     "Internal version of proto `ChannelLabel`");
}

absl::StatusOr<iamf_tools_cli_proto::ChannelLabel>
ChannelLabelUtils::LabelToProto(ChannelLabel::Label label) {
  static const auto kLabelToProto =
      BuildStaticMapFromInvertedPairs(kProtoAndInternalLabel);

  return LookupInMap(*kLabelToProto, label,
                     "Proto version of internal `ChannelLabel`");
}

}  // namespace iamf_tools
