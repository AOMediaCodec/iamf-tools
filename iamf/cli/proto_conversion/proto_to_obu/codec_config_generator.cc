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
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/strings/str_cat.h"
#include "iamf/cli/descriptor_obus.h"
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

absl::StatusOr<LpcmDecoderConfig> GenerateLpcmDecoderConfig(
    const iamf_tools_cli_proto::LpcmDecoderConfig& lpcm_metadata) {
  LpcmDecoderConfig obu_decoder_config;
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

  return obu_decoder_config;
}

absl::StatusOr<OpusDecoderConfig> GenerateOpusDecoderConfig(
    const iamf_tools_cli_proto::OpusDecoderConfig& opus_metadata) {
  OpusDecoderConfig obu_decoder_config;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "OpusDecoderConfig.version", opus_metadata.version(),
      obu_decoder_config.version_));
  obu_decoder_config.input_sample_rate_ = opus_metadata.input_sample_rate();
  return obu_decoder_config;
}

absl::Status CopyStreamInfo(
    uint32_t num_samples_per_frame,
    const iamf_tools_cli_proto::FlacMetaBlockStreamInfo& user_stream_info,
    FlacMetaBlockStreamInfo& obu_stream_info) {
  uint16_t min_and_max_block_size;
  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint16_t>(
      "CodecConfig.num_samples_per_frame", num_samples_per_frame,
      min_and_max_block_size));
  obu_stream_info.minimum_block_size = min_and_max_block_size;
  obu_stream_info.maximum_block_size = min_and_max_block_size;
  obu_stream_info.sample_rate = user_stream_info.sample_rate();

  RETURN_IF_NOT_OK(StaticCastIfInRange<uint32_t, uint8_t>(
      "FlacMetaBlockStreamInfo.bits_per_sample",
      user_stream_info.bits_per_sample(), obu_stream_info.bits_per_sample));
  obu_stream_info.total_samples_in_stream =
      user_stream_info.total_samples_in_stream();
  return absl::OkStatus();
}

absl::StatusOr<FlacDecoderConfig> GenerateFlacDecoderConfig(
    uint32_t num_samples_per_frame,
    const iamf_tools_cli_proto::FlacDecoderConfig& flac_metadata) {
  FlacDecoderConfig obu_decoder_config;

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
      RETURN_IF_NOT_OK(CopyStreamInfo(num_samples_per_frame,
                                      metadata_block.stream_info(),
                                      obu_stream_info));
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

  return obu_decoder_config;
}

absl::StatusOr<AacDecoderConfig> GenerateAacDecoderConfig(
    const iamf_tools_cli_proto::AacDecoderConfig& aac_metadata) {
  AacDecoderConfig obu_decoder_config;
  obu_decoder_config.buffer_size_db_ = aac_metadata.buffer_size_db();
  obu_decoder_config.max_bitrate_ = aac_metadata.max_bitrate();
  obu_decoder_config.average_bit_rate_ = aac_metadata.average_bit_rate();

  if (!aac_metadata.has_decoder_specific_info()) {
    return absl::InvalidArgumentError("Missing AAC decoder specific info.");
  }
  auto& audio_specific_config =
      obu_decoder_config.decoder_specific_info_.audio_specific_config;

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

  return obu_decoder_config;
}

void LogCodecConfigsById(
    const DescriptorObus::CodecConfigsById& codec_config_obus) {
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

  codec_config_obu.SetCodecDelay(*required_codec_delay);
  return absl::OkStatus();
}

}  // namespace

absl::Status CodecConfigGenerator::Generate(
    DescriptorObus::CodecConfigsById& codec_config_obus) {
  // Codec Config-related parameters.
  for (auto const& codec_config_metadata : codec_config_metadata_) {
    // Common section for all codecs.
    // Most fields nested within the inner `codec_config`.
    const auto& input_codec_config = codec_config_metadata.codec_config();

    CodecConfig obu_codec_config{
        .num_samples_per_frame = input_codec_config.num_samples_per_frame()};

    // Process the codec-specific `decoder_config` field.
    switch (input_codec_config.decoder_config_case()) {
      using enum iamf_tools_cli_proto::CodecConfig::DecoderConfigCase;
      case kDecoderConfigLpcm: {
        ABSL_ASSIGN_OR_RETURN(obu_codec_config.decoder_config,
                              GenerateLpcmDecoderConfig(
                                  input_codec_config.decoder_config_lpcm()));
        break;
      }
      case kDecoderConfigOpus: {
        ABSL_ASSIGN_OR_RETURN(obu_codec_config.decoder_config,
                              GenerateOpusDecoderConfig(
                                  input_codec_config.decoder_config_opus()));
        break;
      }
      case kDecoderConfigFlac: {
        ABSL_ASSIGN_OR_RETURN(obu_codec_config.decoder_config,
                              GenerateFlacDecoderConfig(
                                  input_codec_config.num_samples_per_frame(),
                                  input_codec_config.decoder_config_flac()));
        break;
      }
      case kDecoderConfigAac: {
        ABSL_ASSIGN_OR_RETURN(
            obu_codec_config.decoder_config,
            GenerateAacDecoderConfig(input_codec_config.decoder_config_aac()));
        break;
      }
      case DECODER_CONFIG_NOT_SET:
      default:
        return absl::InvalidArgumentError("Missing `decoder_config` field.");
    }

    auto obu = CodecConfigObu::Create(
        GetHeaderFromMetadata(codec_config_metadata.obu_header()),
        codec_config_metadata.codec_config_id(), obu_codec_config);
    if (!obu.ok()) {
      return obu.status();
    }
    RETURN_IF_NOT_OK(OverrideCodecDelay(input_codec_config, *obu));

    codec_config_obus.emplace(codec_config_metadata.codec_config_id(),
                              *std::move(obu));
  }

  LogCodecConfigsById(codec_config_obus);
  return absl::OkStatus();
}

}  // namespace iamf_tools
