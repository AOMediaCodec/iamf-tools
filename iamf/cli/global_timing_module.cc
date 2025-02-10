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
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::Status GlobalTimingModule::GetTimestampsForId(
    const DecodedUleb128 id, const uint32_t duration,
    absl::flat_hash_map<DecodedUleb128, TimingData>& id_to_timing_data,
    InternalTimestamp& start_timestamp, InternalTimestamp& end_timestamp) {
  auto timing_data_iter = id_to_timing_data.find(id);
  if (timing_data_iter == id_to_timing_data.end()) {
    // This allows generating timing information when
    // `IGNORE_ERRORS_USE_ONLY_FOR_IAMF_TEST_SUITE` is defined.
    // TODO(b/278865608): Find better solutions to generate negative test
    //                    vectors.
    start_timestamp = 0;
    end_timestamp = duration;
    return absl::InvalidArgumentError(
        absl::StrCat("Timestamps for ID: ", id, " not found"));
  }

  auto& timing_data = timing_data_iter->second;
  start_timestamp = timing_data.timestamp;
  end_timestamp = start_timestamp + duration;
  timing_data.timestamp += duration;
  return absl::OkStatus();
}

absl::Status GlobalTimingModule::Initialize(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  // TODO(b/283281856): Handle cases where `parameter_rate` and `sample_rate`
  //                    differ.
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
          {audio_substream_id, {.rate = sample_rate, .timestamp = 0}});

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

    const auto [unused_iter, inserted] = parameter_block_timing_data_.insert(
        {parameter_id, {.rate = parameter_rate, .timestamp = 0}});
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
    InternalTimestamp& start_timestamp, InternalTimestamp& end_timestamp) {
  return GetTimestampsForId(audio_substream_id, duration,
                            audio_frame_timing_data_, start_timestamp,
                            end_timestamp);
}

absl::Status GlobalTimingModule::GetNextParameterBlockTimestamps(
    const uint32_t parameter_id, const InternalTimestamp input_start_timestamp,
    const uint32_t duration, InternalTimestamp& start_timestamp,
    InternalTimestamp& end_timestamp) {
  RETURN_IF_NOT_OK(GetTimestampsForId(parameter_id, duration,
                                      parameter_block_timing_data_,
                                      start_timestamp, end_timestamp));
  return CompareTimestamps(
      input_start_timestamp, start_timestamp,
      absl::StrCat("In GetNextParameterBlockTimestamps() for param ID= ",
                   parameter_id, ": "));
}

absl::Status GlobalTimingModule::GetGlobalAudioFrameTimestamp(
    std::optional<InternalTimestamp>& global_timestamp) const {
  if (audio_frame_timing_data_.empty()) {
    return absl::InvalidArgumentError("No audio frames to get timestamps for");
  }

  const InternalTimestamp common_timestamp =
      audio_frame_timing_data_.begin()->second.timestamp;
  for (const auto& [unused_id, timing_data] : audio_frame_timing_data_) {
    if (common_timestamp != timing_data.timestamp) {
      // Some audio frames have not advance their timestamps yet, return OK
      // but let `global_timestamp` hold no value.
      global_timestamp = std::nullopt;
      return absl::OkStatus();
    }
  }

  global_timestamp = common_timestamp;
  return absl::OkStatus();
}

}  // namespace iamf_tools
