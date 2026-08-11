/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include "iamf/cli/proto_conversion/downmixing_reconstruction_util.h"

#include <list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/ambisonics_mixer.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer.h"
#include "iamf/cli/downmixer_factory.h"
#include "iamf/cli/downmixer_manager.h"
#include "iamf/cli/proto/audio_element.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::StatusOr<std::list<DownMixer>> CreateScalableChannelDownmixers(
    const iamf_tools_cli_proto::AudioFrameObuMetadata&
        user_audio_frame_metadata,
    const SubstreamIdLabelsMap& substream_id_to_labels) {
  absl::flat_hash_set<ChannelLabel::Label> user_channel_labels;
  RETURN_IF_NOT_OK(ChannelLabelUtils::ConvertAndFillLabels(
      user_audio_frame_metadata.channel_metadatas(), user_channel_labels));
  return DownmixerFactory::CreateScalableChannelDownmixers(
      user_channel_labels, substream_id_to_labels);
}

std::optional<AmbisonicsMixer::Preset>
MaybeGetAmbisonicsPresetForAudioElementId(
    DecodedUleb128 audio_element_id,
    const iamf_tools_cli_proto::UserMetadata& user_metadata) {
  for (const auto& metadata : user_metadata.audio_element_metadata()) {
    if (metadata.audio_element_id() == audio_element_id &&
        metadata.has_ambisonics_config() &&
        metadata.ambisonics_config().has_ambisonics_preset_config()) {
      const auto preset =
          ProtoToAmbisonicsPreset(metadata.ambisonics_config()
                                      .ambisonics_preset_config()
                                      .ambisonics_preset());
      return preset.ok() ? std::optional(*preset) : std::nullopt;
    }
  }
  return std::nullopt;
}

absl::StatusOr<std::list<DownMixer>> CreateAmbisonicsDownmixers(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const AudioElementWithData& audio_element_with_data) {
  const std::optional<AmbisonicsMixer::Preset> ambisonics_preset =
      MaybeGetAmbisonicsPresetForAudioElementId(
          audio_element_with_data.obu.GetAudioElementId(), user_metadata);
  if (!ambisonics_preset.has_value()) {
    // Ok. Don't set up down-mixers.
    return std::list<DownMixer>{};
  }

  // Extract the down-mixer depending on the preset configuration. The
  // appropriate down-mixers actually depend on the Codec Config OBU.
  RETURN_IF_NOT_OK(ValidateNotNull(
      audio_element_with_data.codec_config,
      absl::StrCat("Codec config for Audio Element ID= ",
                   audio_element_with_data.obu.GetAudioElementId())));
  auto mixer =
      std::make_unique<AmbisonicsMixer>(AmbisonicsMixer::MakeFromPreset(
          audio_element_with_data.codec_config->GetCodecConfig().codec_id,
          *ambisonics_preset,
          audio_element_with_data.codec_config->GetNumSamplesPerFrame()));
  const std::vector<ChannelLabel::Label> input_labels = mixer->GetInputLabels();
  auto down_mixer = DownmixerFactory::SampleProcessorToDownMixer(
      input_labels, std::move(mixer));
  if (!down_mixer.ok()) {
    return down_mixer.status();
  }
  // Wrap in a list to agree with scalable channel down-mixers.
  std::list<DownMixer> down_mixers;
  down_mixers.push_back(*std::move(down_mixer));
  return down_mixers;
}

}  // namespace

absl::StatusOr<
    absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>>
CreateAudioElementIdToDownmixingConfig(
    const iamf_tools_cli_proto::UserMetadata& user_metadata,
    const DescriptorObus::AudioElementsById& audio_elements) {
  absl::flat_hash_map<DecodedUleb128, DownmixerManager::DownmixingConfig>
      result;
  // For each AudioFrameObuMetadata, we pull out the audio element ID, find
  // the matching AudioElementWithData, and convert the proto labels to internal
  // labels, and pair up the converted labels with `substream_id_to_labels` and
  // `label_to_output_gain` from the AudioElementWithData.
  for (const iamf_tools_cli_proto::AudioFrameObuMetadata&
           user_audio_frame_metadata : user_metadata.audio_frame_metadata()) {
    const auto audio_element_id = user_audio_frame_metadata.audio_element_id();
    DescriptorObus::AudioElementsById::const_iterator audio_element =
        audio_elements.find(audio_element_id);
    if (audio_element == audio_elements.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Audio Element ID= ", audio_element_id, " not found"));
    }
    const auto& audio_element_with_data = audio_element->second;

    absl::StatusOr<std::list<DownMixer>> down_mixers;
    switch (audio_element_with_data.obu.GetAudioElementType()) {
      case AudioElementObu::kAudioElementChannelBased:
        down_mixers = CreateScalableChannelDownmixers(
            user_audio_frame_metadata,
            audio_element_with_data.substream_id_to_labels);
        break;
      case AudioElementObu::kAudioElementSceneBased:
        down_mixers =
            CreateAmbisonicsDownmixers(user_metadata, audio_element_with_data);
        break;
      default:
        down_mixers = std::list<DownMixer>{};
        break;
    }
    if (!down_mixers.ok()) {
      return down_mixers.status();
    }

    result[audio_element_id] = {*std::move(down_mixers),
                                audio_element_with_data.substream_id_to_labels,
                                audio_element_with_data.label_to_output_gain};
  }

  return result;
}

}  // namespace iamf_tools
