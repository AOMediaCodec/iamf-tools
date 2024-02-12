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
#include "iamf/cli/arbitrary_obu_generator.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/arbitrary_obu.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"

namespace iamf_tools {

namespace {
absl::Status CopyArbitraryObuType(
    const iamf_tools_cli_proto::ArbitraryObuType arbitrary_obu_type,
    ObuType& output_obu_type) {
  using enum iamf_tools_cli_proto::ArbitraryObuType;
  static const auto* kArbitraryObuTypeToObuType =
      new absl::flat_hash_map<iamf_tools_cli_proto::ArbitraryObuType, ObuType>({
          {OBU_IA_CODEC_CONFIG, kObuIaCodecConfig},
          {OBU_IA_AUDIO_ELEMENT, kObuIaAudioElement},
          {OBU_IA_MIX_PRESENTATION, kObuIaMixPresentation},
          {OBU_IA_PARAMETER_BLOCK, kObuIaParameterBlock},
          {OBU_IA_TEMPORAL_DELIMITER, kObuIaTemporalDelimiter},
          {OBU_IA_AUDIO_FRAME, kObuIaAudioFrame},
          {OBU_IA_AUDIO_FRAME_ID_0, kObuIaAudioFrameId0},
          {OBU_IA_AUDIO_FRAME_ID_1, kObuIaAudioFrameId1},
          {OBU_IA_AUDIO_FRAME_ID_2, kObuIaAudioFrameId2},
          {OBU_IA_AUDIO_FRAME_ID_3, kObuIaAudioFrameId3},
          {OBU_IA_AUDIO_FRAME_ID_4, kObuIaAudioFrameId4},
          {OBU_IA_AUDIO_FRAME_ID_5, kObuIaAudioFrameId5},
          {OBU_IA_AUDIO_FRAME_ID_6, kObuIaAudioFrameId6},
          {OBU_IA_AUDIO_FRAME_ID_7, kObuIaAudioFrameId7},
          {OBU_IA_AUDIO_FRAME_ID_8, kObuIaAudioFrameId8},
          {OBU_IA_AUDIO_FRAME_ID_9, kObuIaAudioFrameId9},
          {OBU_IA_AUDIO_FRAME_ID_10, kObuIaAudioFrameId10},
          {OBU_IA_AUDIO_FRAME_ID_11, kObuIaAudioFrameId11},
          {OBU_IA_AUDIO_FRAME_ID_12, kObuIaAudioFrameId12},
          {OBU_IA_AUDIO_FRAME_ID_13, kObuIaAudioFrameId13},
          {OBU_IA_AUDIO_FRAME_ID_14, kObuIaAudioFrameId14},
          {OBU_IA_AUDIO_FRAME_ID_15, kObuIaAudioFrameId15},
          {OBU_IA_AUDIO_FRAME_ID_16, kObuIaAudioFrameId16},
          {OBU_IA_AUDIO_FRAME_ID_17, kObuIaAudioFrameId17},
          {OBU_IA_RESERVED_24, kObuIaReserved24},
          {OBU_IA_RESERVED_25, kObuIaReserved25},
          {OBU_IA_RESERVED_26, kObuIaReserved26},
          {OBU_IA_RESERVED_27, kObuIaReserved27},
          {OBU_IA_RESERVED_28, kObuIaReserved28},
          {OBU_IA_RESERVED_29, kObuIaReserved29},
          {OBU_IA_RESERVED_30, kObuIaReserved30},
          {OBU_IA_SEQUENCE_HEADER, kObuIaSequenceHeader},
      });

  if (!LookupInMap(*kArbitraryObuTypeToObuType, arbitrary_obu_type,
                   output_obu_type)
           .ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown arbitrary_obu_type= ", arbitrary_obu_type));
  }
  return absl::OkStatus();
}
}  // namespace

absl::Status ArbitraryObuGenerator::Generate(
    std::list<ArbitraryObu>& arbitrary_obus) {
  // Arbitrary OBU-related parameters.
  for (const auto& arbitrary_obu_metadata : arbitrary_obu_metadata_) {
    ObuType obu_type;
    RETURN_IF_NOT_OK(
        CopyArbitraryObuType(arbitrary_obu_metadata.obu_type(), obu_type));

    ArbitraryObu::InsertionHook insertion_hook;
    switch (arbitrary_obu_metadata.insertion_hook()) {
      using enum iamf_tools_cli_proto::InsertionHook;
      using enum ArbitraryObu::InsertionHook;
      case INSERTION_HOOK_BEFORE_DESCRIPTORS:
        insertion_hook = kInsertionHookBeforeDescriptors;
        break;
      case INSERTION_HOOK_AFTER_DESCRIPTORS:
        insertion_hook = kInsertionHookAfterDescriptors;
        break;
      case INSERTION_HOOK_AFTER_IA_SEQUENCE_HEADER:
        insertion_hook = kInsertionHookAfterIaSequenceHeader;
        break;
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("Unknown insertion hook= ",
                         arbitrary_obu_metadata.insertion_hook()));
    }

    std::vector<uint8_t> payload(arbitrary_obu_metadata.payload().size());
    std::transform(arbitrary_obu_metadata.payload().begin(),
                   arbitrary_obu_metadata.payload().end(), payload.begin(),
                   [](char c) { return static_cast<uint8_t>(c); });

    arbitrary_obus.emplace_back(
        obu_type, GetHeaderFromMetadata(arbitrary_obu_metadata.obu_header()),
        payload, insertion_hook);
  }

  // Examine arbitrary OBUs.
  for (const auto& arbitrary_obu : arbitrary_obus) {
    arbitrary_obu.PrintObu();
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
