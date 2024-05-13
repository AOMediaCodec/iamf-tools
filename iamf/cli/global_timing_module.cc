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
#include "iamf/cli/global_timing_module.h"

#include <cstdint>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

absl::Status GlobalTimingModule::GetTimestampsForId(
    const DecodedUleb128 id, const uint32_t duration,
    absl::flat_hash_map<DecodedUleb128, TimingData>& id_to_timing_data,
    int32_t& start_timestamp, int32_t& end_timestamp) {
  if (id_to_timing_data.contains(id)) {
    auto& timing_data = id_to_timing_data.at(id);
    start_timestamp = timing_data.timestamp;
    end_timestamp = start_timestamp + duration;
    timing_data.timestamp += duration;
    return absl::OkStatus();
  }

  LOG(ERROR) << "Timestamps for ID: " << id << " not found";

  // This allows generating timing information when `NO_CHECK_ERROR` is defined.
  // TODO(b/278865608): Find better solutions to generate negative test
  //                    vectors.
  start_timestamp = 0;
  end_timestamp = duration;
  return absl::InvalidArgumentError("");
}

absl::Status GlobalTimingModule::Initialize(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  // TODO(b/277899855): Handle different rates.
  for (const auto& [unused_id, audio_element] : audio_elements) {
    // Initialize all substream IDs to start at 0 even if the substreams do not
    // actually appear in the bitstream.
    for (const auto& audio_substream_id :
         audio_element.obu.audio_substream_ids_) {
      const uint32_t sample_rate =
          audio_element.codec_config->GetOutputSampleRate();
      RETURN_IF_NOT_OK(
          ValidateNotEqual(sample_rate, uint32_t{0}, "sample rate"));

      const auto [unused_iter, inserted] = audio_frame_timing_data_.insert(
          {audio_substream_id,
           {.rate = sample_rate, .global_start_timestamp = 0, .timestamp = 0}});

      if (!inserted) {
        return absl::InvalidArgumentError(
            absl::StrCat("Audio substream ID: ", audio_substream_id,
                         " already exists in the Global Timing Module"));
      }
    }
  }

  // Initialize all parameter IDs to start with a timestamp 0.
  for (const auto& [parameter_id, param_definition] : param_definitions) {
    const DecodedUleb128 parameter_rate = param_definition->parameter_rate_;
    RETURN_IF_NOT_OK(
        ValidateNotEqual(parameter_rate, DecodedUleb128(0), "parameter rate"));

    const auto [unused_iter, inserted] =
        parameter_block_timing_data_.insert({parameter_id,
                                             {.rate = parameter_rate,
                                              .global_start_timestamp = 0,
                                              .timestamp = 0}});
    if (!inserted) {
      return absl::InvalidArgumentError(
          absl::StrCat("Parameter ID: ", parameter_id,
                       " already exists in the Global Timing Module"));
    }
  }

  return absl::OkStatus();
}

absl::Status GlobalTimingModule::GetNextAudioFrameTimestamps(
    const DecodedUleb128 audio_substream_id, const uint32_t duration,
    int32_t& start_timestamp, int32_t& end_timestamp) {
  return GetTimestampsForId(audio_substream_id, duration,
                            audio_frame_timing_data_, start_timestamp,
                            end_timestamp);
}

absl::Status GlobalTimingModule::GetNextParameterBlockTimestamps(
    const uint32_t parameter_id, const int32_t input_start_timestamp,
    const uint32_t duration, int32_t& start_timestamp, int32_t& end_timestamp) {
  RETURN_IF_NOT_OK(GetTimestampsForId(parameter_id, duration,
                                      parameter_block_timing_data_,
                                      start_timestamp, end_timestamp));
  return CompareTimestamps(input_start_timestamp, start_timestamp);
}

absl::Status GlobalTimingModule::ValidateParameterBlockCoversAudioFrame(
    const uint32_t parameter_id, const int32_t parameter_block_start,
    const int32_t parameter_block_end,
    const uint32_t audio_substream_id) const {
  if (!audio_frame_timing_data_.contains(audio_substream_id)) {
    LOG(ERROR) << "Audio substream ID: " << audio_substream_id
               << " has no global starting timestamp.";
    return absl::InvalidArgumentError("");
  }

  const auto& timing_data = audio_frame_timing_data_.at(audio_substream_id);

  // Check that this parameter block's starting/ending timestamp is at
  // least as early/late as the global starting/ending timestamp of its
  // associated audio substreams.
  if (parameter_block_start > timing_data.global_start_timestamp) {
    LOG(ERROR) << "Parameter stream with ID: " << parameter_id << " starts at "
               << parameter_block_start
               << " , which is later than its associated audio"
               << " substream with ID: " << audio_substream_id
               << " starting at " << timing_data.global_start_timestamp;
    return absl::InvalidArgumentError("");
  }

  if (parameter_block_end < timing_data.timestamp) {
    LOG(ERROR) << "Parameter stream with ID: " << parameter_id << " ends at "
               << parameter_block_end
               << " , which is earlier than its associated audio"
               << " substream with ID: " << audio_substream_id << " ending at "
               << timing_data.timestamp;
    return absl::InvalidArgumentError("");
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
