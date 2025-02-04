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
#include "iamf/cli/proto_to_obu/arbitrary_obu_generator.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/lookup_tables.h"
#include "iamf/cli/proto/arbitrary_obu.pb.h"
#include "iamf/cli/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {

namespace {
absl::Status CopyArbitraryObuType(
    iamf_tools_cli_proto::ArbitraryObuType arbitrary_obu_type,
    ObuType& output_obu_type) {
  static const auto kProtoArbitraryObuTypeToObuType = BuildStaticMapFromPairs(
      LookupTables::kProtoArbitraryObuTypeAndInternalObuTypes);

  return CopyFromMap(*kProtoArbitraryObuTypeToObuType, arbitrary_obu_type,
                     "Internal version of proto `ArbitraryObuType`",
                     output_obu_type);
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
    std::optional<int64_t> insertion_tick = std::nullopt;
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
      case INSERTION_HOOK_AFTER_CODEC_CONFIGS:
        insertion_hook = kInsertionHookAfterCodecConfigs;
        break;
      case INSERTION_HOOK_AFTER_AUDIO_ELEMENTS:
        insertion_hook = kInsertionHookAfterAudioElements;
        break;
      case INSERTION_HOOK_AFTER_MIX_PRESENTATIONS:
        insertion_hook = kInsertionHookAfterMixPresentations;
        break;
      case INSERTION_HOOK_BEFORE_PARAMETER_BLOCKS_AT_TICK:
        insertion_hook = kInsertionHookBeforeParameterBlocksAtTick;
        insertion_tick = arbitrary_obu_metadata.insertion_tick();
        break;
      case INSERTION_HOOK_AFTER_PARAMETER_BLOCKS_AT_TICK:
        insertion_hook = kInsertionHookAfterParameterBlocksAtTick;
        insertion_tick = arbitrary_obu_metadata.insertion_tick();
        break;
      case INSERTION_HOOK_AFTER_AUDIO_FRAMES_AT_TICK:
        insertion_hook = kInsertionHookAfterAudioFramesAtTick;
        insertion_tick = arbitrary_obu_metadata.insertion_tick();
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
        payload, insertion_hook, insertion_tick,
        arbitrary_obu_metadata.invalidates_bitstream());
  }

  // Examine arbitrary OBUs.
  for (const auto& arbitrary_obu : arbitrary_obus) {
    arbitrary_obu.PrintObu();
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
