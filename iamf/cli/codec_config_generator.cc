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
#include "iamf/cli/codec_config_generator.h"

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/aac_decoder_config.h"
#include "iamf/cli/cli_util.h"
#include "iamf/cli/proto/codec_config.pb.h"
#include "iamf/codec_config.h"
#include "iamf/flac_decoder_config.h"
#include "iamf/ia.h"
#include "iamf/lpcm_decoder_config.h"
#include "iamf/obu_util.h"
#include "iamf/opus_decoder_config.h"
#include "src/google/protobuf/repeated_ptr_field.h"

namespace iamf_tools {

namespace {

// Copies the `CodecId` based on the input data. Uses the deprecated field as a
// backup.
absl::Status CopyCodecId(
    const iamf_tools_cli_proto::CodecConfig& input_codec_config,
    CodecConfig::CodecId& output_codec_id) {
  if (input_codec_config.has_codec_id()) {
    using enum iamf_tools_cli_proto::CodecId;
    using enum CodecConfig::CodecId;
    static const auto* kInputCodecIdToOutputCodecId =
        new absl::flat_hash_map<iamf_tools_cli_proto::CodecId,
                                CodecConfig::CodecId>({
            {CODEC_ID_OPUS, kCodecIdOpus},
            {CODEC_ID_FLAC, kCodecIdFlac},
            {CODEC_ID_AAC_LC, kCodecIdAacLc},
            {CODEC_ID_LPCM, kCodecIdLpcm},
        });

    if (!LookupInMap(*kInputCodecIdToOutputCodecId,
                     input_codec_config.codec_id(), output_codec_id)
             .ok()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown codec with codec_id= ", input_codec_config.codec_id()));
    }
    return absl::OkStatus();
  } else if (input_codec_config.has_deprecated_codec_id()) {
    LOG(WARNING) << "Please upgrade the `deprecated_codec_id` field to the new "
                    "`codec_id` field.";
    output_codec_id = static_cast<CodecConfig::CodecId>(
        input_codec_config.deprecated_codec_id());
    return absl::OkStatus();
  } else {
    LOG(ERROR) << "Missing `codec_id` field.";
    return absl::InvalidArgumentError("");
  }
}

absl::Status CopyFlacBlockType(
    const iamf_tools_cli_proto::FlacBlockType input_flac_block_type,
    FlacMetaBlockHeader::FlacBlockType& output_flac_block_type) {
  using enum iamf_tools_cli_proto::FlacBlockType;
  using enum FlacMetaBlockHeader::FlacBlockType;
  static const auto* kInputFlacBlockTypeToOutputFlacBlockType =
      new absl::flat_hash_map<iamf_tools_cli_proto::FlacBlockType,
                              FlacMetaBlockHeader::FlacBlockType>(
          {{FLAC_BLOCK_TYPE_STREAMINFO, kFlacStreamInfo},
           {FLAC_BLOCK_TYPE_PADDING, kFlacPadding},
           {FLAC_BLOCK_TYPE_APPLICATION, kFlacApplication},
           {FLAC_BLOCK_TYPE_SEEKTABLE, kFlacSeektable},
           {FLAC_BLOCK_TYPE_VORBIS_COMMENT, kFlacVorbisComment},
           {FLAC_BLOCK_TYPE_CUESHEET, kFlacCuesheet},
           {FLAC_BLOCK_TYPE_PICTURE, kFlacPicture}});

  if (!LookupInMap(*kInputFlacBlockTypeToOutputFlacBlockType,
                   input_flac_block_type, output_flac_block_type)
           .ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown input_flac_block_type= ", input_flac_block_type));
  }
  return absl::OkStatus();
}

absl::Status CopySampleFrequencyIndex(
    const iamf_tools_cli_proto::SampleFrequencyIndex
        input_sample_frequency_index,
    AudioSpecificConfig::SampleFrequencyIndex& output_sample_frequency_index) {
  using enum iamf_tools_cli_proto::SampleFrequencyIndex;
  using enum AudioSpecificConfig::SampleFrequencyIndex;
  static const auto* kInputSampleFrequencyIndexToOutputSampleFrequencyIndex =
      new absl::flat_hash_map<iamf_tools_cli_proto::SampleFrequencyIndex,
                              AudioSpecificConfig::SampleFrequencyIndex>({
          {AAC_SAMPLE_FREQUENCY_INDEX_96000, kSampleFrequencyIndex96000},
          {AAC_SAMPLE_FREQUENCY_INDEX_88200, kSampleFrequencyIndex88200},
          {AAC_SAMPLE_FREQUENCY_INDEX_64000, kSampleFrequencyIndex64000},
          {AAC_SAMPLE_FREQUENCY_INDEX_48000, kSampleFrequencyIndex48000},
          {AAC_SAMPLE_FREQUENCY_INDEX_44100, kSampleFrequencyIndex44100},
          {AAC_SAMPLE_FREQUENCY_INDEX_32000, kSampleFrequencyIndex32000},
          {AAC_SAMPLE_FREQUENCY_INDEX_23000, kSampleFrequencyIndex23000},
          {AAC_SAMPLE_FREQUENCY_INDEX_22050, kSampleFrequencyIndex22050},
          {AAC_SAMPLE_FREQUENCY_INDEX_16000, kSampleFrequencyIndex16000},
          {AAC_SAMPLE_FREQUENCY_INDEX_12000, kSampleFrequencyIndex12000},
          {AAC_SAMPLE_FREQUENCY_INDEX_11025, kSampleFrequencyIndex11025},
          {AAC_SAMPLE_FREQUENCY_INDEX_8000, kSampleFrequencyIndex8000},
          {AAC_SAMPLE_FREQUENCY_INDEX_7350, kSampleFrequencyIndex7350},
          {AAC_SAMPLE_FREQUENCY_INDEX_RESERVED_A,
           kSampleFrequencyIndexReservedA},
          {AAC_SAMPLE_FREQUENCY_INDEX_RESERVED_B,
           kSampleFrequencyIndexReservedB},
          {AAC_SAMPLE_FREQUENCY_INDEX_ESCAPE_VALUE,
           kSampleFrequencyIndexEscapeValue},
      });

  if (!LookupInMap(*kInputSampleFrequencyIndexToOutputSampleFrequencyIndex,
                   input_sample_frequency_index, output_sample_frequency_index)
           .ok()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown input_sample_frequency_index= ",
                     input_sample_frequency_index));
  }
  return absl::OkStatus();
}

absl::Status GenerateLpcmDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    LpcmDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_lpcm()) {
    LOG(ERROR) << "Missing LPCM decoder config";
    return absl::InvalidArgumentError("");
  }
  const auto& lpcm_metadata = user_codec_config.decoder_config_lpcm();
  switch (lpcm_metadata.sample_format_flags()) {
    using enum iamf_tools_cli_proto::LpcmFormatFlags;
    using enum LpcmDecoderConfig::LpcmFormatFlags;
    case LPCM_BIG_ENDIAN:
      obu_decoder_config.sample_format_flags_ = kLpcmBigEndian;
      break;
    case LPCM_LITTLE_ENDIAN:
      obu_decoder_config.sample_format_flags_ = kLpcmLittleEndian;
      break;
    default:
      return absl::InvalidArgumentError("");
  }

  obu_decoder_config.sample_rate_ = lpcm_metadata.sample_rate();
  RETURN_IF_NOT_OK(Uint32ToUint8(lpcm_metadata.sample_size(),
                                 obu_decoder_config.sample_size_));

  return absl::OkStatus();
}

absl::Status GenerateOpusDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    OpusDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_opus()) {
    LOG(ERROR) << "Missing Opus decoder config";
    return absl::InvalidArgumentError("");
  }
  const auto& opus_metadata = user_codec_config.decoder_config_opus();

  RETURN_IF_NOT_OK(
      Uint32ToUint8(opus_metadata.version(), obu_decoder_config.version_));
  RETURN_IF_NOT_OK(Uint32ToUint8(opus_metadata.output_channel_count(),
                                 obu_decoder_config.output_channel_count_));
  RETURN_IF_NOT_OK(
      Uint32ToUint16(opus_metadata.pre_skip(), obu_decoder_config.pre_skip_));
  obu_decoder_config.input_sample_rate_ = opus_metadata.input_sample_rate();
  RETURN_IF_NOT_OK(Int32ToInt16(opus_metadata.output_gain(),
                                obu_decoder_config.output_gain_));
  RETURN_IF_NOT_OK(Uint32ToUint8(opus_metadata.mapping_family(),
                                 obu_decoder_config.mapping_family_));
  return absl::OkStatus();
}

absl::Status CopyStreamInfo(
    const iamf_tools_cli_proto::FlacMetaBlockStreamInfo& user_stream_info,
    FlacMetaBlockStreamInfo& obu_stream_info) {
  RETURN_IF_NOT_OK(Uint32ToUint16(user_stream_info.minimum_block_size(),
                                  obu_stream_info.minimum_block_size));
  RETURN_IF_NOT_OK(Uint32ToUint16(user_stream_info.maximum_block_size(),
                                  obu_stream_info.maximum_block_size));
  obu_stream_info.minimum_frame_size = user_stream_info.minimum_frame_size();
  obu_stream_info.maximum_frame_size = user_stream_info.maximum_frame_size(),
  obu_stream_info.sample_rate = user_stream_info.sample_rate();

  RETURN_IF_NOT_OK(Uint32ToUint8(user_stream_info.number_of_channels(),
                                 obu_stream_info.number_of_channels));
  RETURN_IF_NOT_OK(Uint32ToUint8(user_stream_info.bits_per_sample(),
                                 obu_stream_info.bits_per_sample));
  obu_stream_info.total_samples_in_stream =
      user_stream_info.total_samples_in_stream();
  if (user_stream_info.md5_signature().size() != 16) {
    LOG(ERROR) << "Expected a 16 byte MD5 signature. Actual size: "
               << user_stream_info.md5_signature().size();
    return absl::InvalidArgumentError("");
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
    LOG(ERROR) << "Missing FLAC decoder config";
    return absl::InvalidArgumentError("");
  }

  const auto& flac_metadata = user_codec_config.decoder_config_flac();

  obu_decoder_config.metadata_blocks_.reserve(
      flac_metadata.metadata_blocks().size());
  for (const auto& metadata_block : flac_metadata.metadata_blocks()) {
    FlacMetadataBlock obu_metadata_block;

    // Generate the header.
    obu_metadata_block.header.last_metadata_block_flag =
        metadata_block.header().last_metadata_block_flag();

    RETURN_IF_NOT_OK(CopyFlacBlockType(metadata_block.header().block_type(),
                                       obu_metadata_block.header.block_type));

    obu_metadata_block.header.metadata_data_block_length =
        metadata_block.header().metadata_data_block_length();

    // Generate the block specific fields.
    if (obu_metadata_block.header.block_type ==
        FlacMetaBlockHeader::kFlacStreamInfo) {
      // Stream info has semantic meaning for IAMF. Copy in all fields.
      if (!metadata_block.has_stream_info()) {
        LOG(ERROR) << "Missing FLAC stream info";
        return absl::InvalidArgumentError("");
      }

      FlacMetaBlockStreamInfo obu_stream_info;
      RETURN_IF_NOT_OK(
          CopyStreamInfo(metadata_block.stream_info(), obu_stream_info));
      obu_metadata_block.payload = obu_stream_info;
    } else {
      // For most blocks just copy in the payload.
      if (!metadata_block.has_generic_block()) {
        LOG(ERROR) << "Missing generic block";
        return absl::InvalidArgumentError("");
      }

      obu_metadata_block.payload = std::vector<uint8_t>(
          obu_metadata_block.header.metadata_data_block_length);

      std::transform(
          metadata_block.generic_block().begin(),
          metadata_block.generic_block().end(),
          std::get<std::vector<uint8_t>>(obu_metadata_block.payload).begin(),
          [](const char x) { return static_cast<uint8_t>(x); });
    }

    obu_decoder_config.metadata_blocks_.push_back(obu_metadata_block);
  }

  return absl::OkStatus();
}

absl::Status GenerateAacDecoderConfig(
    const iamf_tools_cli_proto::CodecConfig& user_codec_config,
    AacDecoderConfig& obu_decoder_config) {
  if (!user_codec_config.has_decoder_config_aac()) {
    LOG(ERROR) << "Missing AAC decoder config";
    return absl::InvalidArgumentError("");
  }
  const auto& aac_metadata = user_codec_config.decoder_config_aac();

  RETURN_IF_NOT_OK(
      Uint32ToUint8(aac_metadata.decoder_config_descriptor_tag(),
                    obu_decoder_config.decoder_config_descriptor_tag_));
  RETURN_IF_NOT_OK(Uint32ToUint8(aac_metadata.object_type_indication(),
                                 obu_decoder_config.object_type_indication_));
  RETURN_IF_NOT_OK(Uint32ToUint8(aac_metadata.stream_type(),
                                 obu_decoder_config.stream_type_));
  obu_decoder_config.upstream_ = aac_metadata.upstream();
  obu_decoder_config.reserved_ = aac_metadata.reserved();
  obu_decoder_config.buffer_size_db_ = aac_metadata.buffer_size_db();
  obu_decoder_config.max_bitrate_ = aac_metadata.max_bitrate();
  obu_decoder_config.average_bit_rate_ = aac_metadata.average_bit_rate();
  RETURN_IF_NOT_OK(Uint32ToUint8(
      aac_metadata.decoder_specific_info()
          .decoder_specific_info_descriptor_tag(),
      obu_decoder_config.decoder_specific_info_.decoder_specific_info_tag));
  auto& audio_specific_config =
      obu_decoder_config.decoder_specific_info_.audio_specific_config;
  RETURN_IF_NOT_OK(
      Uint32ToUint8(aac_metadata.decoder_specific_info().audio_object_type(),
                    audio_specific_config.audio_object_type_));
  RETURN_IF_NOT_OK(CopySampleFrequencyIndex(
      aac_metadata.decoder_specific_info().sample_frequency_index(),
      audio_specific_config.sample_frequency_index_));
  if (audio_specific_config.sample_frequency_index_ ==
      AudioSpecificConfig::kSampleFrequencyIndexEscapeValue) {
    // The `sampling_frequency` is directly included in the stream.
    audio_specific_config.sampling_frequency_ =
        aac_metadata.decoder_specific_info().sampling_frequency();
  }

  RETURN_IF_NOT_OK(Uint32ToUint8(
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

    RETURN_IF_NOT_OK(Int32ToInt16(input_codec_config.audio_roll_distance(),
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
      LOG(ERROR) << "Unsupported codec with codec_id=" << obu_codec_id;
      return absl::InvalidArgumentError("");
    }

    CodecConfigObu obu(
        GetHeaderFromMetadata(codec_config_metadata.obu_header()),
        codec_config_metadata.codec_config_id(), obu_codec_config);
    RETURN_IF_NOT_OK(obu.Initialize());

    codec_config_obus.emplace(codec_config_metadata.codec_config_id(),
                              std::move(obu));
  }

  LogCodecConfigObus(codec_config_obus);
  return absl::OkStatus();
}

}  // namespace iamf_tools
