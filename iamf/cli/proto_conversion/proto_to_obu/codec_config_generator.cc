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
#include "iamf/cli/proto_conversion/proto_to_obu/codec_config_generator.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/cli/proto_conversion/lookup_tables.h"
#include "iamf/cli/proto_conversion/proto_to_obu/audio_frame_generator.h"
#include "iamf/cli/proto_conversion/proto_utils.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/map_utils.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

// Copies the `CodecId` based on the input data.
absl::Status CopyCodecId(
    const iamf_tools_cli_proto::CodecConfig& input_codec_config,
    CodecConfig::CodecId& output_codec_id) {
  if (input_codec_config.has_deprecated_codec_id()) {
    return absl::InvalidArgumentError(
        "Please upgrade the `deprecated_codec_id` field to the new `codec_id` "
        "field.\n"
        "Suggested upgrades:\n"
        "- `deprecated_codec_id: 0x6d703461` -> `codec_id: CODEC_ID_AAC_LC`\n"
        "- `deprecated_codec_id: 0x664c6143` -> `codec_id: CODEC_ID_FLAC`\n"
        "- `deprecated_codec_id: 0x6970636d` -> `codec_id: CODEC_ID_LPCM`\n"
        "- `deprecated_codec_id: 0x4f707573` -> `codec_id: CODEC_ID_OPUS`\n");
  }
  if (!input_codec_config.has_codec_id()) {
    return absl::InvalidArgumentError("Missing `codec_id` field.");
  }

  static const auto kProtoToInternalCodecId =
      BuildStaticMapFromPairs(LookupTables::kProtoAndInternalCodecIds);

  return CopyFromMap(*kProtoToInternalCodecId, input_codec_config.codec_id(),
                     "Internal version of proto `CodecId`= ", output_codec_id);
}

absl::Status CopyFlacBlockType(
    iamf_tools_cli_proto::FlacBlockType input_flac_block_type,
    FlacMetaBlockHeader::FlacBlockType& output_flac_block_type) {
  static const auto kProtoToInternalFlacBlockType =
      BuildStaticMapFromPairs(LookupTables::kProtoAndInternalFlacBlockTypes);

  return CopyFromMap(*kProtoToInternalFlacBlockType, input_flac_block_type,
                     "Internal version of proto `FlacBlockType`",
                     output_flac_block_type);
}

absl::Status CopySampleFrequencyIndex(
    iamf_tools_cli_proto::SampleFrequencyIndex input_sample_frequency_index,
    AudioSpecificConfig::SampleFrequencyIndex& output_sample_frequency_index) {
  static const auto kProtoToInternalSampleFrequencyIndex =
      BuildStaticMapFromPairs(
          LookupTables::kProtoAndInternalSampleFrequencyIndices);

  return CopyFromMap(*kProtoToInternalSampleFrequencyIndex,
                     input_sample_frequency_index,
                     "Internal version of proto `SampleFrequencyIndex`",
                     output_sample_frequency_index);
}

absl::Status GenerateLpcmDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    LpcmDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_lpcm()) {
    return absl::InvalidArgumentError("Missing LPCM decoder config.");
  }
  const auto& lpcm_metadata = user_codec_config.decoder_config_lpcm();
  switch (lpcm_metadata.sample_format_flags()) {
    using enum iamf_tools_cli_proto::LpcmFormatFlags;
    using enum LpcmDecoderConfig::LpcmFormatFlagsBitmask;
    case LPCM_BIG_ENDIAN:
      obu_decoder_config.sample_format_flags_bitmask_ = kLpcmBigEndian;
      break;
    case LPCM_LITTLE_ENDIAN:
      obu_decoder_config.sample_format_flags_bitmask_ = kLpcmLittleEndian;
      break;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("Unknown sample_format_flags= ",
                       lpcm_metadata.sample_format_flags()));
  }

  obu_decoder_config.sample_rate_ = lpcm_metadata.sample_rate();
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "LpcmDecoderConfig.sample_size", lpcm_metadata.sample_size(),
      obu_decoder_config.sample_size_));

  return absl::OkStatus();
}

absl::Status GenerateOpusDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    OpusDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_opus()) {
    return absl::InvalidArgumentError("Missing Opus decoder config.");
  }
  const auto& opus_metadata = user_codec_config.decoder_config_opus();

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "OpusDecoderConfig.version", opus_metadata.version(),
      obu_decoder_config.version_));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "OpusDecoderConfig.output_channel_count",
      opus_metadata.output_channel_count(),
      obu_decoder_config.output_channel_count_));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint16_t>(
      "OpusDecoderConfig.pre_skip", opus_metadata.pre_skip(),
      obu_decoder_config.pre_skip_));
  obu_decoder_config.input_sample_rate_ = opus_metadata.input_sample_rate();
  RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
      "OpusDecoderConfig.output_gain", opus_metadata.output_gain(),
      obu_decoder_config.output_gain_));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "OpusDecoderConfig.mapping_family", opus_metadata.mapping_family(),
      obu_decoder_config.mapping_family_));
  return absl::OkStatus();
}

absl::Status CopyStreamInfo(
    const iamf_tools_cli_proto::FlacMetaBlockStreamInfo& user_stream_info,
    FlacMetaBlockStreamInfo& obu_stream_info) {
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint16_t>(
      "FlacMetaBlockStreamInfo.minimum_block_size",
      user_stream_info.minimum_block_size(),
      obu_stream_info.minimum_block_size));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint16_t>(
      "FlacMetaBlockStreamInfo.maximum_block_size",
      user_stream_info.maximum_block_size(),
      obu_stream_info.maximum_block_size));
  obu_stream_info.minimum_frame_size = user_stream_info.minimum_frame_size();
  obu_stream_info.maximum_frame_size = user_stream_info.maximum_frame_size();
  obu_stream_info.sample_rate = user_stream_info.sample_rate();

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "FlacMetaBlockStreamInfo.number_of_channels",
      user_stream_info.number_of_channels(),
      obu_stream_info.number_of_channels));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "FlacMetaBlockStreamInfo.bits_per_sample",
      user_stream_info.bits_per_sample(), obu_stream_info.bits_per_sample));
  obu_stream_info.total_samples_in_stream =
      user_stream_info.total_samples_in_stream();
  if (user_stream_info.md5_signature().size() !=
      obu_stream_info.md5_signature.size()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected a 16 byte MD5 signature. Actual size: ",
                     user_stream_info.md5_signature().size()));
  }
  std::transform(user_stream_info.md5_signature().begin(),
                 user_stream_info.md5_signature().end(),
                 obu_stream_info.md5_signature.begin(),
                 [](const char x) { return static_cast<uint8_t>(x); });
  return absl::OkStatus();
}

absl::Status GenerateFlacDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    FlacDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_flac()) {
    return absl::InvalidArgumentError("Missing FLAC decoder config.");
  }

  const auto& flac_metadata = user_codec_config.decoder_config_flac();

  obu_decoder_config.metadata_blocks_.reserve(
      flac_metadata.metadata_blocks().size());
  for (const auto& metadata_block : flac_metadata.metadata_blocks()) {
    FlacMetadataBlock obu_metadata_block;

    // Generate the header.
    if (metadata_block.header().has_last_metadata_block_flag()) {
      ABSL_LOG(WARNING)
          << "`last_metadata_block_flag` is deprecated will be ignored.";
    }
    if (metadata_block.header().has_metadata_data_block_length()) {
      ABSL_LOG(WARNING)
          << "`metadata_data_block_length` is deprecated will be ignored.";
    }

    RETURN_IF_NOT_OK(CopyFlacBlockType(metadata_block.header().block_type(),
                                       obu_metadata_block.header.block_type));
    // Generate the block specific fields.
    if (obu_metadata_block.header.block_type ==
        FlacMetaBlockHeader::kFlacStreamInfo) {
      // Stream info has semantic meaning for IAMF. Copy in all fields.
      if (!metadata_block.has_stream_info()) {
        return absl::InvalidArgumentError("Missing FLAC stream info.");
      }

      FlacMetaBlockStreamInfo obu_stream_info;
      RETURN_IF_NOT_OK(
          CopyStreamInfo(metadata_block.stream_info(), obu_stream_info));
      obu_metadata_block.payload = obu_stream_info;
    } else {
      // For most blocks just copy in the payload.
      if (!metadata_block.has_generic_block()) {
        return absl::InvalidArgumentError("Missing generic block.");
      }

      std::vector<uint8_t> payload(metadata_block.generic_block().size());
      std::transform(metadata_block.generic_block().begin(),
                     metadata_block.generic_block().end(), payload.begin(),
                     [](const char x) { return static_cast<uint8_t>(x); });
      obu_metadata_block.payload = payload;
    }

    obu_decoder_config.metadata_blocks_.push_back(obu_metadata_block);
  }

  return absl::OkStatus();
}

absl::Status GenerateAacDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    AacDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_aac()) {
    return absl::InvalidArgumentError("Missing AAC decoder config.");
  }
  const auto& aac_metadata = user_codec_config.decoder_config_aac();

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AacDecoderConfig.decoder_config_descriptor_tag",
      aac_metadata.decoder_config_descriptor_tag(),
      obu_decoder_config.decoder_config_descriptor_tag_));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AacDecoderConfig.object_type_indication",
      aac_metadata.object_type_indication(),
      obu_decoder_config.object_type_indication_));
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AacDecoderConfig.stream_type", aac_metadata.stream_type(),
      obu_decoder_config.stream_type_));
  obu_decoder_config.upstream_ = aac_metadata.upstream();
  obu_decoder_config.reserved_ = aac_metadata.reserved();
  obu_decoder_config.buffer_size_db_ = aac_metadata.buffer_size_db();
  obu_decoder_config.max_bitrate_ = aac_metadata.max_bitrate();
  obu_decoder_config.average_bit_rate_ = aac_metadata.average_bit_rate();

  if (!aac_metadata.has_decoder_specific_info()) {
    return absl::InvalidArgumentError("Missing AAC decoder specific info.");
  }
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AacDecoderConfig.decoder_specific_info_descriptor_tag",
      aac_metadata.decoder_specific_info()
          .decoder_specific_info_descriptor_tag(),
      obu_decoder_config.decoder_specific_info_.decoder_specific_info_tag));
  auto& audio_specific_config =
      obu_decoder_config.decoder_specific_info_.audio_specific_config;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AacDecoderConfig.audio_object_type",
      aac_metadata.decoder_specific_info().audio_object_type(),
      audio_specific_config.audio_object_type_));

  if (aac_metadata.decoder_specific_info().sample_frequency_index() ==
      iamf_tools_cli_proto::AAC_SAMPLE_FREQUENCY_INDEX_ESCAPE_VALUE) {
    ABSL_LOG(WARNING) << "`sampling_frequency` is deprecated and will be "
                         "automatically upgraded to "
                         "`sample_frequency_index`.";
    // The escape value is forbidden to be used in IAMF, but we can upgrade it
    // to the explicit sampling frequency index.
    static const auto kSampleFrequencyIndexToSampleFrequency =
        BuildStaticMapFromInvertedPairs(
            AacDecoderConfig::kSampleFrequencyIndexAndSampleFrequency);
    RETURN_IF_NOT_OK(
        CopyFromMap(*kSampleFrequencyIndexToSampleFrequency,
                    aac_metadata.decoder_specific_info().sampling_frequency(),
                    "Sample frequency index for `sampling_frequency`",
                    audio_specific_config.sample_frequency_index_));
  } else {
    RETURN_IF_NOT_OK(CopySampleFrequencyIndex(
        aac_metadata.decoder_specific_info().sample_frequency_index(),
        audio_specific_config.sample_frequency_index_));
  }

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "AacDecoderConfig.channel_configuration",
      aac_metadata.decoder_specific_info().channel_configuration(),
      audio_specific_config.channel_configuration_));

  audio_specific_config.ga_specific_config_.frame_length_flag =
      aac_metadata.ga_specific_config().frame_length_flag();
  audio_specific_config.ga_specific_config_.depends_on_core_coder =
      aac_metadata.ga_specific_config().depends_on_core_coder();
  audio_specific_config.ga_specific_config_.extension_flag =
      aac_metadata.ga_specific_config().extension_flag();

  return absl::OkStatus();
}

void LogCodecConfigObus(
    const absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  for (const auto& [codec_config_id, codec_config_obu] : codec_config_obus) {
    codec_config_obu.PrintObu();
  }
}

absl::Status OverrideCodecDelay(
    const iamf_tools_cli_proto::CodecConfig& codec_config_metadata,
    CodecConfigObu& codec_config_obu) {
  const auto required_codec_delay =
      AudioFrameGenerator::GetNumberOfSamplesToDelayAtStart(
          codec_config_metadata, codec_config_obu);
  if (!required_codec_delay.ok()) {
    return required_codec_delay.status();
  }

  return codec_config_obu.SetCodecDelay(*required_codec_delay);
}

}  // namespace

absl::Status CodecConfigGenerator::Generate(
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus) {
  // Codec Config-related parameters.
  for (auto const& codec_config_metadata : codec_config_metadata_) {
    // Common section for all codecs.
    // Most fields nested within the inner `codec_config`.
    const auto& input_codec_config = codec_config_metadata.codec_config();

    CodecConfig::CodecId obu_codec_id;
    RETURN_IF_NOT_OK(CopyCodecId(input_codec_config, obu_codec_id));

    CodecConfig obu_codec_config{
        .codec_id = obu_codec_id,
        .num_samples_per_frame = input_codec_config.num_samples_per_frame()};

    RETURN_IF_NOT_OK(StaticCastIfInRange<int32_t, int16_t>(
        "CodecConfigObu.audio_roll_distance",
        input_codec_config.audio_roll_distance(),
        obu_codec_config.audio_roll_distance));

    // Process the codec-specific `decoder_config` field.
    if (obu_codec_id == CodecConfig::kCodecIdLpcm) {
      LpcmDecoderConfig lpcm_decoder_config;
      RETURN_IF_NOT_OK(
          GenerateLpcmDecoderConfig(input_codec_config, lpcm_decoder_config));
      obu_codec_config.decoder_config = lpcm_decoder_config;
    } else if (obu_codec_id == CodecConfig::kCodecIdOpus) {
      OpusDecoderConfig opus_decoder_config;
      RETURN_IF_NOT_OK(
          GenerateOpusDecoderConfig(input_codec_config, opus_decoder_config));
      obu_codec_config.decoder_config = opus_decoder_config;
    } else if (obu_codec_id == CodecConfig::kCodecIdFlac) {
      FlacDecoderConfig flac_decoder_config;
      RETURN_IF_NOT_OK(
          GenerateFlacDecoderConfig(input_codec_config, flac_decoder_config));
      obu_codec_config.decoder_config = flac_decoder_config;
    } else if (obu_codec_id == CodecConfig::kCodecIdAacLc) {
      AacDecoderConfig aac_decoder_config;
      RETURN_IF_NOT_OK(
          GenerateAacDecoderConfig(input_codec_config, aac_decoder_config));
      obu_codec_config.decoder_config = aac_decoder_config;
    } else {
      // This should not be possible because `CopyCodecId` would have already
      // detected the error.
      return absl::InvalidArgumentError(
          absl::StrCat("Unsupported codec with codec_id= ", obu_codec_id));
    }

    auto obu = CodecConfigObu::Create(
        GetHeaderFromMetadata(codec_config_metadata.obu_header()),
        codec_config_metadata.codec_config_id(), obu_codec_config,
        input_codec_config.automatically_override_audio_roll_distance());
    if (!obu.ok()) {
      return obu.status();
    }
    if (input_codec_config.automatically_override_codec_delay()) {
      RETURN_IF_NOT_OK(OverrideCodecDelay(input_codec_config, *obu));
    }

    codec_config_obus.emplace(codec_config_metadata.codec_config_id(),
                              *std::move(obu));
  }

  LogCodecConfigObus(codec_config_obus);
  return absl::OkStatus();
}

}  // namespace iamf_tools
