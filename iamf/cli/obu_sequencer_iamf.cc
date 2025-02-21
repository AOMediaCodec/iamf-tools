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
#include "iamf/cli/obu_sequencer_iamf.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <list>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/obu_sequencer_base.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"

namespace iamf_tools {

namespace {

constexpr int64_t kBufferStartSize = 65536;

// The caller of this function should pre-seed the write buffer with Descriptor
// OBUs.
absl::Status WriteIaSequenceToFile(const std::string& iamf_filename,
                                   bool include_temporal_delimiters,
                                   const TemporalUnitMap& temporal_unit_map,
                                   WriteBitBuffer& wb) {
  std::optional<std::fstream> output_iamf;
  if (!iamf_filename.empty()) {
    output_iamf.emplace(iamf_filename, std::fstream::out | std::ios::binary);
  }

  // Write all Audio Frame and Parameter Block OBUs ordered by temporal unit.
  int num_samples = 0;
  for (const auto& temporal_unit : temporal_unit_map) {
    // The temporal units will typically be the largest part of an IAMF
    // sequence. Occasionally flush to buffer to avoid keeping it all in memory.
    RETURN_IF_NOT_OK(wb.MaybeFlushIfCloseToCapacity(output_iamf));

    RETURN_IF_NOT_OK(ObuSequencerBase::WriteTemporalUnit(
        include_temporal_delimiters, temporal_unit.second, wb, num_samples));
  }
  LOG(INFO) << "Wrote " << temporal_unit_map.size()
            << " temporal units with a total of " << num_samples
            << " samples excluding padding.";

  // Flush any unwritten bytes before exiting.
  RETURN_IF_NOT_OK(wb.FlushAndWriteToFile(output_iamf));
  return absl::OkStatus();
}

void MaybeRemoveFile(const std::string& filename) {
  if (filename.empty()) {
    return;
  }
  std::error_code error_code;
  std::filesystem::remove(filename, error_code);
  if (!error_code) {
    // File clean up failed somehow. Just log the error and move on.
    LOG(ERROR).WithPerror() << "Failed to remove " << filename;
  }
}

}  // namespace

absl::Status ObuSequencerIamf::PickAndPlace(
    const IASequenceHeaderObu& ia_sequence_header_obu,
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    const std::list<AudioFrameWithData>& audio_frames,
    const std::list<ParameterBlockWithData>& parameter_blocks,
    const std::list<ArbitraryObu>& arbitrary_obus) {
  // Seed with a reasonable starting size. It is arbitrary because
  // `WriteBitBuffer`s automatically resize as needed.
  WriteBitBuffer wb(kBufferStartSize, leb_generator_);

  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookBeforeDescriptors, arbitrary_obus, wb));
  // Write out the descriptor OBUs.
  RETURN_IF_NOT_OK(ObuSequencerBase::WriteDescriptorObus(
      ia_sequence_header_obu, codec_config_obus, audio_elements,
      mix_presentation_obus, arbitrary_obus, wb));
  RETURN_IF_NOT_OK(ArbitraryObu::WriteObusWithHook(
      ArbitraryObu::kInsertionHookAfterDescriptors, arbitrary_obus, wb));

  // Map of temporal unit start time -> OBUs that overlap this temporal unit.
  // Using absl::btree_map for convenience as this allows iterating by
  // timestamp (which is the key).
  TemporalUnitMap temporal_unit_map;
  RETURN_IF_NOT_OK(ObuSequencerBase::GenerateTemporalUnitMap(
      audio_frames, parameter_blocks, arbitrary_obus, temporal_unit_map));

  const auto write_status = WriteIaSequenceToFile(
      iamf_filename_, include_temporal_delimiters_, temporal_unit_map, wb);
  if (!write_status.ok()) {
    MaybeRemoveFile(iamf_filename_);
  }
  return write_status;
}

}  // namespace iamf_tools
