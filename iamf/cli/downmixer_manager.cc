/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/downmixer_manager.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <list>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer.h"
#include "iamf/cli/labeled_frame.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

using enum ChannelLabel::Label;

absl::Status FillRequiredDownmixingMetadata(
    const absl::flat_hash_set<ChannelLabel::Label>& labels_to_downmix,
    const SubstreamIdLabelsMap& substream_id_to_labels,
    const LabelGainMap& label_to_output_gain,
    DownmixerManager::DownmixerMetadataForAudioElementId& downmixer_metadata) {
  auto& down_mixers = downmixer_metadata.down_mixers;

  if (!down_mixers.empty()) {
    return absl::UnknownError(
        "`FillRequiredDownmixingMetadata()` should only be called once per "
        "Audio "
        "Element ID");
  }
  downmixer_metadata.substream_id_to_labels = substream_id_to_labels;
  downmixer_metadata.label_to_output_gain = label_to_output_gain;

  // Find the input surround number.
  int input_surround_number = 0;
  if (labels_to_downmix.contains(kL7)) {
    input_surround_number = 7;
  } else if (labels_to_downmix.contains(kL5)) {
    input_surround_number = 5;
  } else if (labels_to_downmix.contains(kL3)) {
    input_surround_number = 3;
  } else if (labels_to_downmix.contains(kL2)) {
    input_surround_number = 2;
  } else if (labels_to_downmix.contains(kMono)) {
    input_surround_number = 1;
  }

  // Find the lowest output surround number.
  int output_lowest_surround_number = INT_MAX;
  for (const auto& [substream_id, labels] :
       downmixer_metadata.substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), kL7) != labels.end() &&
        output_lowest_surround_number > 7) {
      output_lowest_surround_number = 7;
    } else if (std::find(labels.begin(), labels.end(), kL5) != labels.end() &&
               output_lowest_surround_number > 5) {
      output_lowest_surround_number = 5;
    } else if (std::find(labels.begin(), labels.end(), kL3) != labels.end() &&
               output_lowest_surround_number > 3) {
      output_lowest_surround_number = 3;
    } else if (std::find(labels.begin(), labels.end(), kL2) != labels.end() &&
               output_lowest_surround_number > 2) {
      output_lowest_surround_number = 2;
    } else if (std::find(labels.begin(), labels.end(), kMono) != labels.end() &&
               output_lowest_surround_number > 1) {
      output_lowest_surround_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }
  ABSL_VLOG(1) << "Surround down-mixers from S" << input_surround_number
               << " to S" << output_lowest_surround_number << " needed:";
  for (int surround_number = input_surround_number;
       surround_number > output_lowest_surround_number; surround_number--) {
    if (surround_number == 7) {
      down_mixers.push_back(S7ToS5DownMixer);
      ABSL_VLOG(1) << "  S7ToS5DownMixer added";
    } else if (surround_number == 5) {
      down_mixers.push_back(S5ToS3DownMixer);
      ABSL_VLOG(1) << "  S5ToS3DownMixer added";
    } else if (surround_number == 3) {
      down_mixers.push_back(S3ToS2DownMixer);
      ABSL_VLOG(1) << "  S3ToS2DownMixer added";
    } else if (surround_number == 2) {
      down_mixers.push_back(S2ToS1DownMixer);
      ABSL_VLOG(1) << "  S2ToS1DownMixer added";
    }
  }

  // Find the input height number. Artificially defining the height number of
  // "TF2" as 1.
  int input_height_number = 0;
  if (labels_to_downmix.contains(kLtf4)) {
    input_height_number = 4;
  } else if (labels_to_downmix.contains(kLtf2)) {
    input_height_number = 2;
  } else if (labels_to_downmix.contains(kLtf3)) {
    input_height_number = 1;
  }

  // Find the lowest output height number.
  int output_lowest_height_number = INT_MAX;
  for (const auto& [substream_id, labels] :
       downmixer_metadata.substream_id_to_labels) {
    if (std::find(labels.begin(), labels.end(), kLtf4) != labels.end() &&
        output_lowest_height_number > 4) {
      output_lowest_height_number = 4;
    } else if (std::find(labels.begin(), labels.end(), kLtf2) != labels.end() &&
               output_lowest_height_number > 2) {
      output_lowest_height_number = 2;
    } else if (std::find(labels.begin(), labels.end(), kLtf3) != labels.end() &&
               output_lowest_height_number > 1) {
      output_lowest_height_number = 1;
      // This is the lowest possible value, abort.
      break;
    }
  }

  ABSL_VLOG(1) << "Height down-mixers from T" << input_height_number << " to "
               << (output_lowest_height_number == 2 ? "T2" : "TF3")
               << " needed:";
  for (int height_number = input_height_number;
       height_number > output_lowest_height_number; height_number--) {
    if (height_number == 4) {
      down_mixers.push_back(T4ToT2DownMixer);
      ABSL_VLOG(1) << "  T4ToT2DownMixer added";
    } else if (height_number == 2) {
      down_mixers.push_back(T2ToTf2DownMixer);
      ABSL_VLOG(1) << "  T2ToTf2DownMixer added";
    }
  }

  return absl::OkStatus();
}

absl::Status GetDownmixerMetadata(
    const DecodedUleb128 audio_element_id,
    const absl::flat_hash_map<
        DecodedUleb128, DownmixerManager::DownmixerMetadataForAudioElementId>&
        audio_element_id_to_downmixer_metadata,
    const DownmixerManager::DownmixerMetadataForAudioElementId*&
        downmixer_metadata) {
  const auto iter =
      audio_element_id_to_downmixer_metadata.find(audio_element_id);
  if (iter == audio_element_id_to_downmixer_metadata.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Downmixing metadata for Audio Element ID= ", audio_element_id,
        " not found"));
  }
  downmixer_metadata = &iter->second;
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<DownmixerManager> DownmixerManager::Create(
    const absl::flat_hash_map<DecodedUleb128, DownmixingConfig>&
        id_to_config_map) {
  absl::flat_hash_map<DecodedUleb128, DownmixerMetadataForAudioElementId>
      audio_element_id_to_downmixer_metadata;

  for (const auto& [audio_element_id, config] : id_to_config_map) {
    RETURN_IF_NOT_OK(FillRequiredDownmixingMetadata(
        config.user_labels, config.substream_id_to_labels,
        config.label_to_output_gain,
        audio_element_id_to_downmixer_metadata[audio_element_id]));
  }

  return DownmixerManager(std::move(audio_element_id_to_downmixer_metadata));
}

absl::StatusOr<DownmixerManager> DownmixerManager::CreateForPassthrough(
    const DescriptorObus::AudioElementsById& audio_elements) {
  absl::flat_hash_map<DecodedUleb128, DownmixerMetadataForAudioElementId>
      audio_element_id_to_downmixer_metadata;

  // Configure a minimal config (without down-mixers) to trigger passthrough
  // mode.
  for (const auto& [audio_element_id, audio_element_with_data] :
       audio_elements) {
    audio_element_id_to_downmixer_metadata.emplace(
        audio_element_id,
        DownmixerMetadataForAudioElementId{
            .down_mixers = {},
            .substream_id_to_labels =
                audio_element_with_data.substream_id_to_labels,
            .label_to_output_gain =
                audio_element_with_data.label_to_output_gain,
        });
  }

  return DownmixerManager(std::move(audio_element_id_to_downmixer_metadata));
}

absl::Status DownmixerManager::DownMixSamplesToSubstreams(
    DecodedUleb128 audio_element_id, const DownMixingParams& down_mixing_params,
    LabelSamplesMap& input_label_to_samples,
    absl::flat_hash_map<uint32_t, SubstreamData>&
        substream_id_to_substream_data) const {
  const DownmixerMetadataForAudioElementId* downmixer_metadata = nullptr;
  RETURN_IF_NOT_OK(GetDownmixerMetadata(audio_element_id,
                                        audio_element_id_to_downmixer_metadata_,
                                        downmixer_metadata));

  // First perform all the down mixing.
  for (const auto& down_mixer : downmixer_metadata->down_mixers) {
    RETURN_IF_NOT_OK(down_mixer(down_mixing_params, input_label_to_samples));
  }

  for (const auto& [substream_id, output_channel_labels] :
       downmixer_metadata->substream_id_to_labels) {
    // Find the `SubstreamData` with this `substream_id`.
    auto substream_data_iter =
        substream_id_to_substream_data.find(substream_id);
    if (substream_data_iter == substream_id_to_substream_data.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find substream data for substream ID= ", substream_id));
    }
    auto& substream_data = substream_data_iter->second;

    int channel_index = 0;
    for (const auto& output_channel_label : output_channel_labels) {
      // Compute and store the linear output gains for this channel.
      const auto gain_iter =
          downmixer_metadata->label_to_output_gain.find(output_channel_label);
      const double output_gain_linear =
          (gain_iter == downmixer_metadata->label_to_output_gain.end())
              ? 1.0
              : std::pow(10.0, gain_iter->second / 20.0);
      auto samples_iter = input_label_to_samples.find(output_channel_label);
      if (samples_iter == input_label_to_samples.end()) {
        return absl::InvalidArgumentError(absl::StrCat(
            "Samples do not exist for channel: ", output_channel_label));
      }
      const auto& input_samples = samples_iter->second;

      // Add all down mixed samples to both substream frames.
      for (const auto input_sample : input_samples) {
        substream_data.frames_in_obu.PushSample(channel_index, input_sample);

        // Apply output gains to the samples going to the encoder and also
        // convert the samples to 32-bit integers.
        int32_t attenuated_sample_int32 = 0;
        RETURN_IF_NOT_OK(NormalizedFloatingPointToInt32(
            input_sample / output_gain_linear, attenuated_sample_int32));
        substream_data.frames_to_encode.PushSample(channel_index,
                                                   attenuated_sample_int32);
      }
      channel_index++;
    }
  }

  return absl::OkStatus();
}

absl::StatusOr<const std::list<DownMixer>* absl_nonnull>
DownmixerManager::GetDownMixers(DecodedUleb128 audio_element_id) const {
  const DownmixerMetadataForAudioElementId* downmixer_metadata = nullptr;
  RETURN_IF_NOT_OK(GetDownmixerMetadata(audio_element_id,
                                        audio_element_id_to_downmixer_metadata_,
                                        downmixer_metadata));
  return &downmixer_metadata->down_mixers;
}

}  // namespace iamf_tools
