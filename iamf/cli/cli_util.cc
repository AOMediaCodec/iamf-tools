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
#include "iamf/cli/cli_util.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <list>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/param_definitions.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"

namespace iamf_tools {

absl::Status CopyParamDefinition(
    const iamf_tools_cli_proto::ParamDefinition& input_param_definition,
    ParamDefinition* param_definition) {
  param_definition->parameter_id_ = input_param_definition.parameter_id();
  param_definition->parameter_rate_ = input_param_definition.parameter_rate();

  param_definition->param_definition_mode_ =
      input_param_definition.param_definition_mode();
  RETURN_IF_NOT_OK(Uint32ToUint8(input_param_definition.reserved(),
                                 param_definition->reserved_));
  param_definition->duration_ = input_param_definition.duration();
  param_definition->constant_subblock_duration_ =
      input_param_definition.constant_subblock_duration();

  if (input_param_definition.constant_subblock_duration() != 0) {
    // Nothing else to be done. Return.
    return absl::OkStatus();
  }

  if (input_param_definition.num_subblocks() <
      input_param_definition.subblock_durations_size()) {
    LOG(ERROR) << "Expected at least " << input_param_definition.num_subblocks()
               << "subblock durations for parameter id: "
               << input_param_definition.parameter_id();
    return absl::InvalidArgumentError("");
  }

  param_definition->InitializeSubblockDurations(
      static_cast<DecodedUleb128>(input_param_definition.num_subblocks()));
  for (int i = 0; i < input_param_definition.num_subblocks(); ++i) {
    RETURN_IF_NOT_OK(param_definition->SetSubblockDuration(
        i, input_param_definition.subblock_durations(i)));
  }

  return absl::OkStatus();
}

ObuHeader GetHeaderFromMetadata(
    const iamf_tools_cli_proto::ObuHeaderMetadata& input_obu_header) {
  std::vector<uint8_t> extension_header_bytes(
      input_obu_header.extension_header_bytes().size());
  std::transform(input_obu_header.extension_header_bytes().begin(),
                 input_obu_header.extension_header_bytes().end(),
                 extension_header_bytes.begin(),
                 [](char c) { return static_cast<uint8_t>(c); });

  return ObuHeader{
      .obu_redundant_copy = input_obu_header.obu_redundant_copy(),
      .obu_trimming_status_flag = input_obu_header.obu_trimming_status_flag(),
      .obu_extension_flag = input_obu_header.obu_extension_flag(),
      .num_samples_to_trim_at_end =
          input_obu_header.num_samples_to_trim_at_end(),
      .num_samples_to_trim_at_start =
          input_obu_header.num_samples_to_trim_at_start(),
      .extension_header_size = input_obu_header.extension_header_size(),
      .extension_header_bytes = extension_header_bytes};
}

absl::Status CopyDemixingInfoParameterData(
    const iamf_tools_cli_proto::DemixingInfoParameterData&
        input_demixing_info_parameter_data,
    DemixingInfoParameterData& obu_demixing_param_data) {
  auto& dmixp_mode = obu_demixing_param_data.dmixp_mode;
  switch (input_demixing_info_parameter_data.dmixp_mode()) {
    using enum iamf_tools_cli_proto::DMixPMode;
    using enum DemixingInfoParameterData::DMixPMode;
    case DMIXP_MODE_1:
      dmixp_mode = kDMixPMode1;
      break;
    case DMIXP_MODE_2:
      dmixp_mode = kDMixPMode2;
      break;
    case DMIXP_MODE_3:
      dmixp_mode = kDMixPMode3;
      break;
    case DMIXP_MODE_RESERVED_A:
      dmixp_mode = kDMixPModeReserved1;
      break;
    case DMIXP_MODE_1_N:
      dmixp_mode = kDMixPMode1_n;
      break;
    case DMIXP_MODE_2_N:
      dmixp_mode = kDMixPMode2_n;
      break;
    case DMIXP_MODE_3_N:
      dmixp_mode = kDMixPMode3_n;
      break;
    case DMIXP_MODE_RESERVED_B:
      dmixp_mode = kDMixPModeReserved2;
      break;
    default:
      return absl::InvalidArgumentError("");
  }
  RETURN_IF_NOT_OK(Uint32ToUint8(input_demixing_info_parameter_data.reserved(),
                                 obu_demixing_param_data.reserved));

  return absl::OkStatus();
}

// Returns `true` if the profile fully supports temporal delimiter OBUs.
// Although profile that do not support them can safely ignore them.
bool ProfileSupportsTemporalDelimiterObus(ProfileVersion profile) {
  switch (profile) {
    case ProfileVersion::kIamfSimpleProfile:
    case ProfileVersion::kIamfBaseProfile:
      return true;
    default:
      return false;
  }
}

absl::Status GetIncludeTemporalDelimiterObus(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const IASequenceHeaderObu& ia_sequence_header_obu,
    bool& include_temporal_delimiter) {
  const bool input_include_temporal_delimiter =
      user_metadata.temporal_delimiter_metadata().enable_temporal_delimiters();

  // Allow Temporal Delimiter OBUs as long as at least one of the profiles
  // supports them. If one of the profiles (e.g. simple profile) does not
  // "support" them they can be safely ignored.
  if (input_include_temporal_delimiter &&
      (!ProfileSupportsTemporalDelimiterObus(
           ia_sequence_header_obu.primary_profile_) &&
       !ProfileSupportsTemporalDelimiterObus(
           ia_sequence_header_obu.additional_profile_))) {
    LOG(ERROR) << "Temporal Delimiter OBUs need either `primary_profile` or "
                  "`additional_profile` to support them.";
    return absl::InvalidArgumentError("");
  }
  include_temporal_delimiter = input_include_temporal_delimiter;
  return absl::OkStatus();
}

absl::Status CollectAndValidateParamDefinitions(
    const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>&
        audio_elements,
    const std::list<MixPresentationObu>& mix_presentation_obus,
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>&
        param_definitions) {
  auto insert_and_check_equivalence =
      [&](const ParamDefinition* param_definition) -> absl::Status {
    const auto parameter_id = param_definition->parameter_id_;
    const auto [iter, inserted] =
        param_definitions.insert({parameter_id, param_definition});
    if (!inserted && *iter->second != *param_definition) {
      LOG(ERROR) << "Inequivalent `param_definition_mode` for id= "
                 << parameter_id;
      return absl::InvalidArgumentError("");
    }

    return absl::OkStatus();
  };

  // Collect all `param_definition`s in Audio Element and Mix Presentation
  // OBUs.
  for (const auto& [unused_id, audio_element] : audio_elements) {
    for (const auto& audio_element_param :
         audio_element.obu.audio_element_params_) {
      const auto param_definition_type =
          audio_element_param.param_definition_type;
      if (param_definition_type !=
              ParamDefinition::kParameterDefinitionDemixing &&
          param_definition_type !=
              ParamDefinition::kParameterDefinitionReconGain) {
        LOG(ERROR) << "Param definition type: " << param_definition_type
                   << " not allowed in an audio element";
        return absl::InvalidArgumentError("");
      }
      RETURN_IF_NOT_OK(insert_and_check_equivalence(
          audio_element_param.param_definition.get()));
    }
  }

  for (const auto& mix_presentation_obu : mix_presentation_obus) {
    for (const auto& sub_mix : mix_presentation_obu.sub_mixes_) {
      for (const auto& audio_element : sub_mix.audio_elements) {
        RETURN_IF_NOT_OK(insert_and_check_equivalence(
            &audio_element.element_mix_config.mix_gain));
      }
      RETURN_IF_NOT_OK(insert_and_check_equivalence(
          &sub_mix.output_mix_config.output_mix_gain));
    }
  }

  return absl::OkStatus();
}

absl::Status CompareTimestamps(int32_t expected_timestamp,
                               int32_t actual_timestamp) {
  if (expected_timestamp != actual_timestamp) {
    LOG(ERROR) << "Expected timestamp != actual timestamp: ("
               << expected_timestamp << " vs " << actual_timestamp << ").";
    return absl::InvalidArgumentError("");
  }
  return absl::OkStatus();
}

absl::Status WritePcmFrameToBuffer(
    const std::vector<std::vector<int32_t>>& frame,
    uint32_t samples_to_trim_at_start, uint32_t samples_to_trim_at_end,
    uint8_t bit_depth, bool big_endian, size_t buffer_size,
    uint8_t* const buffer) {
  if (bit_depth % 8 != 0) {
    LOG(ERROR) << "This function only supports an integer number of bytes.";
    return absl::InvalidArgumentError("");
  }
  const size_t num_samples =
      (frame.size() - samples_to_trim_at_start - samples_to_trim_at_end) *
      frame[0].size();

  if (buffer_size < num_samples * (bit_depth / 8)) {
    LOG(ERROR) << "Invalid buffer size";
    return absl::InvalidArgumentError("");
  }

  // The input frame is arranged in (time, channel) axes. Interlace these in the
  // output PCM and skip over any trimmed samples.
  int write_position = 0;
  for (int t = samples_to_trim_at_start;
       t < frame.size() - samples_to_trim_at_end; t++) {
    for (int c = 0; c < frame[0].size(); ++c) {
      const uint32_t sample = static_cast<uint32_t>(frame[t][c]);
      RETURN_IF_NOT_OK(WritePcmSample(sample, bit_depth, big_endian, buffer,
                                      write_position));
    }
  }

  return absl::OkStatus();
}

absl::Status GetCommonSampleRateAndBitDepth(
    const absl::flat_hash_set<uint32_t>& sample_rates,
    const absl::flat_hash_set<uint8_t>& bit_depths,
    uint32_t& common_sample_rate, uint8_t& common_bit_depth,
    bool& requires_resampling) {
  requires_resampling = false;
  if (sample_rates.empty() || bit_depths.empty()) {
    return absl::InvalidArgumentError("");
  }

  if (sample_rates.size() == 1) {
    common_sample_rate = *sample_rates.begin();
  } else {
    // No common sample rate. The spec recommends the rendering output to be
    // resampled to 48000 Hz.
    common_sample_rate = 48000;
    requires_resampling = true;
  }

  if (bit_depths.size() == 1) {
    common_bit_depth = *bit_depths.begin();
  } else {
    // No common bit-depth. The spec recommends the rendering output to be
    // resampled to 16-bits.
    common_bit_depth = 16;
    requires_resampling = true;
  }

  return absl::OkStatus();
}

absl::Status GetCommonSamplesPerFrame(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    uint32_t& common_samples_per_frame) {
  bool first = true;
  for (const auto& [unused_id, codec_config_obu] : codec_config_obus) {
    if (first) {
      common_samples_per_frame = codec_config_obu.GetNumSamplesPerFrame();
      first = false;
      continue;
    }

    if (common_samples_per_frame != codec_config_obu.GetNumSamplesPerFrame()) {
      LOG(ERROR)
          << "The encoder does not support Codec Config OBUs with a different "
             "number of samples per frame yet.";
      return absl::UnknownError("");
    }
  }

  return absl::OkStatus();
}

void LogChannelNumbers(const std::string& name,
                       const ChannelNumbers& channel_numbers) {
  LOG(INFO) << name << ": [" << channel_numbers.surround << "."
            << channel_numbers.lfe << "." << channel_numbers.height << "]";
}

}  // namespace iamf_tools
