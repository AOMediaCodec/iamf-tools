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
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator_factory.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_generator.h"
#include "iamf/cli/channel_label.h"
#include "iamf/cli/codec/encoder_base.h"
#include "iamf/cli/codec/lpcm_encoder.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/downmixer_manager.h"
#include "iamf/cli/global_timing_module.h"
#include "iamf/cli/parameters_manager.h"
#include "iamf/cli/proto/audio_frame.pb.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto_conversion/channel_label_utils.h"
#include "iamf/cli/proto_conversion/codec_config_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/substream_channel_count.h"
#include "iamf/obu/types.h"
#include "src/google/protobuf/repeated_ptr_field.h"

// These defines are not part of an official API and are likely to change or be
// removed. Please do not depend on them.
#ifndef IAMF_TOOLS_DISABLE_AAC_ENCODER
#include "iamf/cli/codec/aac_encoder.h"
#endif

#ifndef IAMF_TOOLS_DISABLE_FLAC_ENCODER
#include "iamf/cli/codec/flac_encoder.h"
#endif

#ifndef IAMF_TOOLS_DISABLE_OPUS_ENCODER
#include "iamf/cli/codec/opus_encoder.h"
#endif

namespace iamf_tools {
namespace {

constexpr bool kValidateCodecDelay = true;

absl::Status InitializeEncoder(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    const CodecConfigObu& codec_config, SubstreamChannelCount channel_count,
    std::unique_ptr<EncoderBase>& encoder, bool validate_codec_delay,
    int substream_id = 0) {
  switch (codec_config.GetCodecConfig().GetCodecId()) {
    using enum CodecConfig::CodecId;
    case kCodecIdLpcm:
      encoder = std::make_unique<LpcmEncoder>(codec_config, channel_count);
      break;
#ifndef IAMF_TOOLS_DISABLE_OPUS_ENCODER
    case kCodecIdOpus: {
      auto opus_encoder_settings = CreateOpusEncoderSettings(
          codec_config_metadata.decoder_config_opus().opus_encoder_metadata(),
          channel_count, substream_id);
      if (!opus_encoder_settings.ok()) {
        return opus_encoder_settings.status();
      }
      encoder = std::make_unique<OpusEncoder>(*opus_encoder_settings,
                                              codec_config, channel_count);
      break;
    }
#endif
#ifndef IAMF_TOOLS_DISABLE_AAC_ENCODER
    case kCodecIdAacLc:
      encoder = std::make_unique<AacEncoder>(
          codec_config_metadata.decoder_config_aac().aac_encoder_metadata(),
          codec_config, channel_count);
      break;
#endif
#ifndef IAMF_TOOLS_DISABLE_FLAC_ENCODER
    case kCodecIdFlac:
      encoder = std::make_unique<FlacEncoder>(
          codec_config_metadata.decoder_config_flac().flac_encoder_metadata(),
          codec_config, channel_count);
      break;
#endif
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown or disabled codec_id= ",
                       codec_config.GetCodecConfig().GetCodecId()));
  }
  RETURN_IF_NOT_OK(encoder->Initialize(validate_codec_delay));
  return absl::OkStatus();
}

// Gets data relevant to encoding (Codec Config OBU and AudioElementWithData)
// and initializes encoders.
absl::Status GetEncodingDataAndInitializeEncoders(
    const absl::flat_hash_map<DecodedUleb128,
                              iamf_tools_cli_proto::CodecConfig>&
        codec_config_metadata,
    const AudioElementWithData& audio_element_with_data,
    absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>&
        substream_id_to_encoder) {
  if (audio_element_with_data.codec_config == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Audio Element with ID= ",
                     audio_element_with_data.obu.GetAudioElementId(),
                     " has no codec_config"));
  }
  for (const auto& [substream_id, labels] :
       audio_element_with_data.substream_id_to_labels) {
    const auto channel_count = SubstreamChannelCount::Create(labels.size());
    if (!channel_count.ok()) {
      return channel_count.status();
    }
    const CodecConfigObu& codec_config_obu =
        *audio_element_with_data.codec_config;
    auto codec_config_metadata_iter =
        codec_config_metadata.find(codec_config_obu.GetCodecConfigId());
    if (codec_config_metadata_iter == codec_config_metadata.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to find codec config metadata for codec_config_id= ",
          codec_config_obu.GetCodecConfigId()));
    }

    RETURN_IF_NOT_OK(InitializeEncoder(codec_config_metadata_iter->second,
                                       codec_config_obu, *channel_count,
                                       substream_id_to_encoder[substream_id],
                                       kValidateCodecDelay, substream_id));
  }

  return absl::OkStatus();
}

// Validates that all substreams share the same trimming information.
absl::Status ValidateSubstreamsShareTrimming(
    const iamf_tools_cli_proto::AudioFrameObuMetadata& audio_frame_metadata,
    const AudioFrameGenerator::TrimmingConfig& common_trimming_config) {
  if (audio_frame_metadata.samples_to_trim_at_end() !=
          common_trimming_config.samples_to_trim_at_end ||
      audio_frame_metadata.samples_to_trim_at_start() !=
          common_trimming_config.samples_to_trim_at_start ||
      audio_frame_metadata.samples_to_trim_at_end_includes_padding() !=
          common_trimming_config.samples_to_trim_at_end_includes_padding ||
      audio_frame_metadata.samples_to_trim_at_start_includes_codec_delay() !=
          common_trimming_config
              .samples_to_trim_at_start_includes_codec_delay) {
    return absl::InvalidArgumentError(
        "Expected all substreams to have the same trimming information");
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<AudioFrameGenerator> absl_nonnull>
CreateAudioFrameGeneratorFromMetadata(
    const google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::AudioFrameObuMetadata>& audio_frame_metadatas,
    const google::protobuf::RepeatedPtrField<
        iamf_tools_cli_proto::CodecConfigObuMetadata>& codec_config_metadatas,
    const DescriptorObus::AudioElementsById& audio_elements,
    std::unique_ptr<DownmixerManager> absl_nonnull downmixer_manager,
    ParametersManager& parameters_manager,
    GlobalTimingModule& global_timing_module) {
  ABSL_CHECK_NE(downmixer_manager, nullptr);
  if (audio_frame_metadatas.empty()) {
    // Ok, nothing will be generated. This state helps clients handle trivial IA
    // Sequences.
    return AudioFrameGenerator::Create(
        audio_elements, /*substream_id_to_encoder=*/{},
        std::move(downmixer_manager), parameters_manager, global_timing_module,
        /*audio_element_id_to_labels=*/{},
        AudioFrameGenerator::TrimmingConfig{});
  }

  // Mapping from Codec Config ID to additional codec config metadata used
  // to configure encoders.
  absl::flat_hash_map<DecodedUleb128, iamf_tools_cli_proto::CodecConfig>
      codec_config_metadata;
  for (const auto& codec_config_obu_metadata : codec_config_metadatas) {
    codec_config_metadata[codec_config_obu_metadata.codec_config_id()] =
        codec_config_obu_metadata.codec_config();
  }

  const auto& first_audio_frame_metadata = audio_frame_metadatas[0];
  const AudioFrameGenerator::TrimmingConfig common_trimming_config = {
      .samples_to_trim_at_start =
          first_audio_frame_metadata.samples_to_trim_at_start(),
      .samples_to_trim_at_end =
          first_audio_frame_metadata.samples_to_trim_at_end(),
      .samples_to_trim_at_start_includes_codec_delay =
          first_audio_frame_metadata
              .samples_to_trim_at_start_includes_codec_delay(),
      .samples_to_trim_at_end_includes_padding =
          first_audio_frame_metadata.samples_to_trim_at_end_includes_padding(),
  };

  absl::flat_hash_map<uint32_t, std::unique_ptr<EncoderBase>>
      substream_id_to_encoder;
  absl::flat_hash_map<DecodedUleb128, absl::flat_hash_set<ChannelLabel::Label>>
      audio_element_id_to_labels;
  for (const auto& audio_frame_metadata : audio_frame_metadatas) {
    RETURN_IF_NOT_OK(ValidateSubstreamsShareTrimming(audio_frame_metadata,
                                                     common_trimming_config));

    const DecodedUleb128 audio_element_id =
        audio_frame_metadata.audio_element_id();
    const auto audio_elements_iter = audio_elements.find(audio_element_id);
    if (audio_elements_iter == audio_elements.end()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Audio Element with ID= ", audio_element_id, " not found"));
    }

    // Create an encoder for each substream.
    RETURN_IF_NOT_OK(GetEncodingDataAndInitializeEncoders(
        codec_config_metadata, audio_elements_iter->second,
        substream_id_to_encoder));

    // Precompute the `ChannelLabel::Label` for each channel label proto.
    RETURN_IF_NOT_OK(ChannelLabelUtils::ConvertAndFillLabels(
        audio_frame_metadata.channel_metadatas(),
        audio_element_id_to_labels[audio_element_id]));
  }

  return AudioFrameGenerator::Create(
      audio_elements, std::move(substream_id_to_encoder),
      std::move(downmixer_manager), parameters_manager, global_timing_module,
      std::move(audio_element_id_to_labels), common_trimming_config);
}

absl::StatusOr<uint32_t> GetNumberOfSamplesToDelayAtStart(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    const CodecConfigObu& codec_config) {
  // This function is useful when querying what the codec delay should be. We
  // don't want it to fail if the user-provided codec delay is wrong.
  constexpr bool kDontValidateCodecDelay = false;

  std::unique_ptr<EncoderBase> encoder;
  RETURN_IF_NOT_OK(
      InitializeEncoder(codec_config_metadata, codec_config,
                        /*channel_count=*/SubstreamChannelCount::MakeSingular(),
                        encoder, kDontValidateCodecDelay));
  if (encoder == nullptr) {
    return absl::InvalidArgumentError("Failed to initialize encoder");
  }
  return encoder->GetNumberOfSamplesToDelayAtStart();
}

}  // namespace iamf_tools
