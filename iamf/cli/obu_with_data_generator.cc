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

#include "iamf/cli/obu_with_data_generator.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto_to_obu/audio_element_generator.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/recon_gain_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::StatusOr<absl::flat_hash_map<DecodedUleb128, AudioElementWithData>>
ObuWithDataGenerator::GenerateAudioElementsWithData(
    const absl::flat_hash_map<DecodedUleb128, CodecConfigObu>&
        codec_config_obus,
    absl::flat_hash_map<DecodedUleb128, AudioElementObu>& audio_element_obus) {
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      audio_element_with_data;
  for (auto& [audio_element_id, audio_element_obu] : audio_element_obus) {
    SubstreamIdLabelsMap substream_id_to_labels;
    LabelGainMap label_to_output_gain;
    std::vector<ChannelNumbers> channel_numbers_for_layers;
    if (audio_element_obu.GetAudioElementType() ==
        AudioElementObu::AudioElementType::kAudioElementChannelBased) {
      if (!std::holds_alternative<ScalableChannelLayoutConfig>(
              audio_element_obu.config_)) {
        return absl::InvalidArgumentError(
            "Audio Element OBU signals it holds a scalable channel layout "
            "config, but one is not present.");
      }

      RETURN_IF_NOT_OK(
          AudioElementGenerator::FinalizeScalableChannelLayoutConfig(
              audio_element_obu.audio_substream_ids_,
              std::get<ScalableChannelLayoutConfig>(audio_element_obu.config_),
              substream_id_to_labels, label_to_output_gain,
              channel_numbers_for_layers));
    }
    if (audio_element_obu.GetAudioElementType() ==
        AudioElementObu::AudioElementType::kAudioElementSceneBased) {
      RETURN_IF_NOT_OK(AudioElementGenerator::FinalizeAmbisonicsConfig(
          audio_element_obu, substream_id_to_labels));
    }
    auto iter = codec_config_obus.find(audio_element_obu.GetCodecConfigId());
    if (iter == codec_config_obus.end()) {
      return absl::InvalidArgumentError(
          "codec_config_obus does not contain codec_config_id");
    }
    audio_element_with_data.insert(
        {audio_element_id,
         AudioElementWithData(std::move(audio_element_obu), &iter->second,
                              substream_id_to_labels, label_to_output_gain,
                              channel_numbers_for_layers)});
  }
  audio_element_obus.clear();
  return audio_element_with_data;
}

absl::StatusOr<AudioFrameWithData>
ObuWithDataGenerator::GenerateAudioFrameWithData(
    const AudioElementWithData& audio_element_with_data,
    const AudioFrameObu& audio_frame_obu,
    GlobalTimingModule& global_timing_module,
    ParametersManager& parameters_manager) {
  const auto audio_substream_id = audio_frame_obu.GetSubstreamId();
  const auto audio_element_id = audio_element_with_data.obu.GetAudioElementId();

  // Make sure we have the correct audio element.
  if (!audio_element_with_data.substream_id_to_labels.contains(
          audio_substream_id)) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Audio element with ID= ", audio_element_id,
        " does not contain a substream with ID= ", audio_substream_id));
  }

  const uint32_t duration =
      audio_element_with_data.codec_config->GetNumSamplesPerFrame();

  // Get the timestamps and demixing and recon-gain parameters to fill in
  // `AudioFrameWithData`.
  int32_t start_timestamp;
  int32_t end_timestamp;
  RETURN_IF_NOT_OK(global_timing_module.GetNextAudioFrameTimestamps(
      audio_substream_id, duration, start_timestamp, end_timestamp));
  DownMixingParams down_mixing_params;
  RETURN_IF_NOT_OK(parameters_manager.GetDownMixingParameters(
      audio_element_id, down_mixing_params));
  ReconGainInfoParameterData recon_gain_info_parameter_data;
  RETURN_IF_NOT_OK(parameters_manager.GetReconGainInfoParameterData(
      audio_element_id,
      audio_element_with_data.channel_numbers_for_layers.size(),
      recon_gain_info_parameter_data));

  return AudioFrameWithData{
      .obu = std::move(audio_frame_obu),
      .start_timestamp = start_timestamp,
      .end_timestamp = end_timestamp,
      .pcm_samples = std::nullopt,  // The PCM samples cannot be derived from
                                    // the bitstream.
      .down_mixing_params = down_mixing_params,
      .recon_gain_info_parameter_data = recon_gain_info_parameter_data,
      .audio_element_with_data = &audio_element_with_data};
}

absl::StatusOr<ParameterBlockWithData>
ObuWithDataGenerator::GenerateParameterBlockWithData(
    int32_t input_start_timestamp, GlobalTimingModule& global_timing_module,
    std::unique_ptr<ParameterBlockObu> parameter_block_obu) {
  int32_t start_timestamp;
  int32_t end_timestamp;
  RETURN_IF_NOT_OK(global_timing_module.GetNextParameterBlockTimestamps(
      parameter_block_obu->parameter_id_, input_start_timestamp,
      parameter_block_obu->GetDuration(), start_timestamp, end_timestamp));
  return ParameterBlockWithData{.obu = std::move(parameter_block_obu),
                                .start_timestamp = start_timestamp,
                                .end_timestamp = end_timestamp};
}

}  // namespace iamf_tools
