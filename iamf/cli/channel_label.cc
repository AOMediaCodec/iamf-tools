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
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "iamf/obu/audio_element.h"

namespace iamf_tools {

absl::StatusOr<ChannelLabel::Label>
ChannelLabel::AmbisonicsChannelNumberToLabel(int ambisonics_channel_number) {
  return ChannelLabel::StringToLabel(
      absl::StrCat("A", ambisonics_channel_number));
}

absl::StatusOr<ChannelLabel::Label> ChannelLabel::StringToLabel(
    absl::string_view label) {
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<
      absl::flat_hash_map<std::string, ChannelLabel::Label>>
      kStringToChannelLabel({
          {"Omitted", kOmitted},
          {"M", kMono},
          {"L2", kL2},
          {"R2", kR2},
          {"DemixedR2", kDemixedR2},
          {"C", kCentre},
          {"LFE", kLFE},
          {"L3", kL3},
          {"R3", kR3},
          {"Rtf3", kRtf3},
          {"Ltf3", kLtf3},
          {"DemixedL3", kDemixedL3},
          {"DemixedR3", kDemixedR3},
          {"L5", kL5},
          {"R5", kR5},
          {"Ls5", kLs5},
          {"Rs5", kRs5},
          {"Ltf2", kLtf2},
          {"Rtf2", kRtf2},
          {"DemixedL5", kDemixedL5},
          {"DemixedR5", kDemixedR5},
          {"DemixedLs5", kDemixedLs5},
          {"DemixedRs5", kDemixedRs5},
          {"DemixedRtf2", kDemixedRtf2},
          {"DemixedLtf2", kDemixedLtf2},
          {"L7", kL7},
          {"R7", kR7},
          {"Lss7", kLss7},
          {"Rss7", kRss7},
          {"Lrs7", kLrs7},
          {"Rrs7", kRrs7},
          {"Ltf4", kLtf4},
          {"Rtf4", kRtf4},
          {"Ltb4", kLtb4},
          {"Rtb4", kRtb4},
          {"DemixedL7", kDemixedL7},
          {"DemixedR7", kDemixedR7},
          {"DemixedLrs7", kDemixedLrs7},
          {"DemixedRrs7", kDemixedRrs7},
          {"DemixedLtb4", kDemixedLtb4},
          {"DemixedRtb4", kDemixedRtb4},
          {"FLc", kFLc},
          {"FC", kFC},
          {"FRc", kFRc},
          {"FL", kFL},
          {"FR", kFR},
          {"SiL", kSiL},
          {"SiR", kSiR},
          {"BL", kBL},
          {"BR", kBR},
          {"TpFL", kTpFL},
          {"TpFR", kTpFR},
          {"TpSiL", kTpSiL},
          {"TpSiR", kTpSiR},
          {"TpBL", kTpBL},
          {"TpBR", kTpBR},
          {"A0", kA0},
          {"A1", kA1},
          {"A2", kA2},
          {"A3", kA3},
          {"A4", kA4},
          {"A5", kA5},
          {"A6", kA6},
          {"A7", kA7},
          {"A8", kA8},
          {"A9", kA9},
          {"A10", kA10},
          {"A11", kA11},
          {"A12", kA12},
          {"A13", kA13},
          {"A14", kA14},
          {"A15", kA15},
          {"A16", kA16},
          {"A17", kA17},
          {"A18", kA18},
          {"A19", kA19},
          {"A20", kA20},
          {"A21", kA21},
          {"A22", kA22},
          {"A23", kA23},
          {"A24", kA24},
      });

  auto it = kStringToChannelLabel->find(label);
  if (it == kStringToChannelLabel->end()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown string-based label= ", label));
  }
  return it->second;
}

std::string ChannelLabel::LabelToString(Label label) {
  using enum ChannelLabel::Label;
  switch (label) {
    case kOmitted:
      return "Omitted";
    case kMono:
      return "M";
    case kL2:
      return "L2";
    case kR2:
      return "R2";
    case kDemixedR2:
      return "DemixedR2";
    case kCentre:
      return "C";
    case kLFE:
      return "LFE";
    case kL3:
      return "L3";
    case kR3:
      return "R3";
    case kRtf3:
      return "Rtf3";
    case kLtf3:
      return "Ltf3";
    case kDemixedL3:
      return "DemixedL3";
    case kDemixedR3:
      return "DemixedR3";
    case kL5:
      return "L5";
    case kR5:
      return "R5";
    case kLs5:
      return "Ls5";
    case kRs5:
      return "Rs5";
    case kLtf2:
      return "Ltf2";
    case kRtf2:
      return "Rtf2";
    case kDemixedL5:
      return "DemixedL5";
    case kDemixedR5:
      return "DemixedR5";
    case kDemixedLs5:
      return "DemixedLs5";
    case kDemixedRs5:
      return "DemixedRs5";
    case kDemixedRtf2:
      return "DemixedRtf2";
    case kDemixedLtf2:
      return "DemixedLtf2";
    case kL7:
      return "L7";
    case kR7:
      return "R7";
    case kLss7:
      return "Lss7";
    case kRss7:
      return "Rss7";
    case kLrs7:
      return "Lrs7";
    case kRrs7:
      return "Rrs7";
    case kLtf4:
      return "Ltf4";
    case kRtf4:
      return "Rtf4";
    case kLtb4:
      return "Ltb4";
    case kRtb4:
      return "Rtb4";
    case kDemixedL7:
      return "DemixedL7";
    case kDemixedR7:
      return "DemixedR7";
    case kDemixedLrs7:
      return "DemixedLrs7";
    case kDemixedRrs7:
      return "DemixedRrs7";
    case kDemixedLtb4:
      return "DemixedLtb4";
    case kDemixedRtb4:
      return "DemixedRtb4";
    case kFLc:
      return "FLc";
    case kFC:
      return "FC";
    case kFRc:
      return "FRc";
    case kFL:
      return "FL";
    case kFR:
      return "FR";
    case kSiL:
      return "SiL";
    case kSiR:
      return "SiR";
    case kBL:
      return "BL";
    case kBR:
      return "BR";
    case kTpFL:
      return "TpFL";
    case kTpFR:
      return "TpFR";
    case kTpSiL:
      return "TpSiL";
    case kTpSiR:
      return "TpSiR";
    case kTpBL:
      return "TpBL";
    case kTpBR:
      return "TpBR";
    case kA0:
      return "A0";
    case kA1:
      return "A1";
    case kA2:
      return "A2";
    case kA3:
      return "A3";
    case kA4:
      return "A4";
    case kA5:
      return "A5";
    case kA6:
      return "A6";
    case kA7:
      return "A7";
    case kA8:
      return "A8";
    case kA9:
      return "A9";
    case kA10:
      return "A10";
    case kA11:
      return "A11";
    case kA12:
      return "A12";
    case kA13:
      return "A13";
    case kA14:
      return "A14";
    case kA15:
      return "A15";
    case kA16:
      return "A16";
    case kA17:
      return "A17";
    case kA18:
      return "A18";
    case kA19:
      return "A19";
    case kA20:
      return "A20";
    case kA21:
      return "A21";
    case kA22:
      return "A22";
    case kA23:
      return "A23";
    case kA24:
      return "A24";
  }

  // The above switch statement is exhaustive.
  LOG(FATAL) << "Enum out of range.";
}

absl::StatusOr<ChannelLabel::Label> ChannelLabel::GetDemixedLabel(
    ChannelLabel::Label label) {
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<
      absl::flat_hash_map<ChannelLabel::Label, ChannelLabel::Label>>
      kChannelLabelToDemixedLabel({{kR2, kDemixedR2},
                                   {kL3, kDemixedL3},
                                   {kR3, kDemixedR3},
                                   {kL5, kDemixedL5},
                                   {kR5, kDemixedR5},
                                   {kLs5, kDemixedLs5},
                                   {kRs5, kDemixedRs5},
                                   {kLtf2, kDemixedLtf2},
                                   {kRtf2, kDemixedRtf2},
                                   {kL7, kDemixedL7},
                                   {kR7, kDemixedR7},
                                   {kLrs7, kDemixedLrs7},
                                   {kRrs7, kDemixedRrs7},
                                   {kLtb4, kDemixedLtb4},
                                   {kRtb4, kDemixedRtb4}});

  auto it = kChannelLabelToDemixedLabel->find(label);
  if (it == kChannelLabelToDemixedLabel->end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Demixed label is not known or allowed for label= ", label));
  }
  return it->second;
}

absl::StatusOr<std::vector<ChannelLabel::Label>>
ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
    const ChannelAudioLayerConfig::LoudspeakerLayout& loudspeaker_layout) {
  using enum ChannelAudioLayerConfig::LoudspeakerLayout;
  using enum ChannelLabel::Label;
  static const absl::NoDestructor<
      absl::flat_hash_map<ChannelAudioLayerConfig::LoudspeakerLayout,
                          std::vector<ChannelLabel::Label>>>
      kSoundSystemToLoudspeakerLayout({
          {kLayoutMono, {kMono}},
          {kLayoutStereo, {kL2, kR2}},
          {kLayout5_1_ch, {kL5, kR5, kCentre, kLFE, kLs5, kRs5}},
          {kLayout5_1_2_ch,
           {kL5, kR5, kCentre, kLFE, kLs5, kRs5, kLtf2, kRtf2}},
          {kLayout5_1_4_ch,
           {kL5, kR5, kCentre, kLFE, kLs5, kRs5, kLtf4, kRtf4, kLtb4, kRtb4}},
          {kLayout7_1_ch,
           {kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7}},
          {kLayout7_1_2_ch,
           {kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7, kLtf2, kRtf2}},
          {kLayout7_1_4_ch,
           {kL7, kR7, kCentre, kLFE, kLss7, kRss7, kLrs7, kRrs7, kLtf4, kRtf4,
            kLtb4, kRtb4}},
          {kLayout3_1_2_ch, {kL3, kR3, kCentre, kLFE, kLtf3, kRtf3}},
          {kLayoutBinaural, {kL2, kR2}},
      });

  auto it = kSoundSystemToLoudspeakerLayout->find(loudspeaker_layout);
  if (it == kSoundSystemToLoudspeakerLayout->end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Channel order not found for layout= ", loudspeaker_layout));
  }
  return it->second;
}

absl::StatusOr<absl::flat_hash_set<ChannelLabel::Label>>
ChannelLabel::LookupLabelsToReconstructFromScalableLoudspeakerLayout(
    const ChannelAudioLayerConfig::LoudspeakerLayout& loudspeaker_layout) {
  // Reconstruct the highest layer.
  const auto ordered_labels =
      ChannelLabel::LookupEarChannelOrderFromScalableLoudspeakerLayout(
          loudspeaker_layout);
  if (!ordered_labels.ok()) {
    return ordered_labels.status();
  } else {
    return absl::flat_hash_set<ChannelLabel::Label>(ordered_labels->begin(),
                                                    ordered_labels->end());
  }
}

}  // namespace iamf_tools
