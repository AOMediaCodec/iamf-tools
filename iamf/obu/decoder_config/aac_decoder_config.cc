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
#include "iamf/obu/decoder_config/aac_decoder_config.h"

#include <cstdint>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

namespace {

absl::Status ValidateAudioRollDistance(int16_t audio_roll_distance) {
  if (audio_roll_distance != -1) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid audio_roll_distance= ", audio_roll_distance));
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status AacDecoderConfig::Validate() const {
  RETURN_IF_NOT_OK(ValidateEqual(decoder_config_descriptor_tag_,
                                 AacDecoderConfig::kDecoderConfigDescriptorTag,
                                 "decoder_config_descriptor_tag"));
  // IAMF restricts several fields.
  RETURN_IF_NOT_OK(ValidateEqual(object_type_indication_,
                                 AacDecoderConfig::kObjectTypeIndication,
                                 "object_type_indication"));
  RETURN_IF_NOT_OK(ValidateEqual(stream_type_, AacDecoderConfig::kStreamType,
                                 "stream_type"));
  RETURN_IF_NOT_OK(
      ValidateEqual(upstream_, AacDecoderConfig::kUpstream, "upstream"));
  RETURN_IF_NOT_OK(
      ValidateEqual(reserved_, AacDecoderConfig::kReserved, "reserved"));
  RETURN_IF_NOT_OK(ValidateEqual(
      decoder_specific_info_.decoder_specific_info_tag,
      AacDecoderConfig::DecoderSpecificInfo::kDecoderSpecificInfoTag,
      "decoder_specific_info_tag"));

  const AudioSpecificConfig& audio_specific_config =
      decoder_specific_info_.audio_specific_config;

  RETURN_IF_NOT_OK(ValidateEqual(audio_specific_config.audio_object_type_,
                                 AudioSpecificConfig::kAudioObjectType,
                                 "audio_object_type"));
  RETURN_IF_NOT_OK(ValidateEqual(audio_specific_config.channel_configuration_,
                                 AudioSpecificConfig::kChannelConfiguration,
                                 "channel_configuration"));
  RETURN_IF_NOT_OK(
      ValidateEqual(audio_specific_config.ga_specific_config_.frame_length_flag,
                    AudioSpecificConfig::GaSpecificConfig::kFrameLengthFlag,
                    "frame_length_flag"));
  RETURN_IF_NOT_OK(ValidateEqual(
      audio_specific_config.ga_specific_config_.depends_on_core_coder,
      AudioSpecificConfig::GaSpecificConfig::kDependsOnCoreCoder,
      "depends_on_core_coder"));
  RETURN_IF_NOT_OK(ValidateEqual(
      audio_specific_config.ga_specific_config_.extension_flag,
      AudioSpecificConfig::GaSpecificConfig::kExtensionFlag, "extension_flag"));
  return absl::OkStatus();
}

absl::Status AudioSpecificConfig::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(audio_object_type_, 5));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      static_cast<uint32_t>(sample_frequency_index_), 4));
  if (sample_frequency_index_ == kSampleFrequencyIndexEscapeValue) {
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(sampling_frequency_, 24));
  }
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(channel_configuration_, 4));

  // Write nested `ga_specific_config`.
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(ga_specific_config_.frame_length_flag, 1));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(ga_specific_config_.depends_on_core_coder, 1));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(ga_specific_config_.extension_flag, 1));

  return absl::OkStatus();
}

absl::Status AudioSpecificConfig::Read(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, audio_object_type_));
  uint8_t sample_frequency_index_uint8;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, sample_frequency_index_uint8));
  sample_frequency_index_ =
      static_cast<SampleFrequencyIndex>(sample_frequency_index_uint8);
  if (sample_frequency_index_ == kSampleFrequencyIndexEscapeValue) {
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(24, sampling_frequency_));
  }
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, channel_configuration_));

  // Write nested `ga_specific_config`.
  RETURN_IF_NOT_OK(rb.ReadBoolean(ga_specific_config_.frame_length_flag));
  RETURN_IF_NOT_OK(rb.ReadBoolean(ga_specific_config_.depends_on_core_coder));
  RETURN_IF_NOT_OK(rb.ReadBoolean(ga_specific_config_.extension_flag));

  return absl::OkStatus();
}

absl::Status AacDecoderConfig::ValidateAndWrite(int16_t audio_roll_distance,
                                                WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));
  RETURN_IF_NOT_OK(Validate());

  // Write top-level fields.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(decoder_config_descriptor_tag_, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(object_type_indication_, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(stream_type_, 6));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(upstream_, 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_, 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(buffer_size_db_, 24));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(max_bitrate_, 32));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(average_bit_rate_, 32));

  // Write nested `decoder_specific_info`.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      decoder_specific_info_.decoder_specific_info_tag, 8));

  // Write nested `audio_specific_config`.
  RETURN_IF_NOT_OK(
      decoder_specific_info_.audio_specific_config.ValidateAndWrite(wb));

  return absl::OkStatus();
}

absl::Status AacDecoderConfig::ReadAndValidate(int16_t audio_roll_distance,
                                               ReadBitBuffer& rb) {
  // Write top-level fields.
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, decoder_config_descriptor_tag_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, object_type_indication_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, stream_type_));
  RETURN_IF_NOT_OK(rb.ReadBoolean(upstream_));
  RETURN_IF_NOT_OK(rb.ReadBoolean(reserved_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(24, buffer_size_db_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, max_bitrate_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, average_bit_rate_));

  // Write nested `decoder_specific_info`.
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(
      8, decoder_specific_info_.decoder_specific_info_tag));

  // Write nested `audio_specific_config`.
  RETURN_IF_NOT_OK(decoder_specific_info_.audio_specific_config.Read(rb));
  RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));
  RETURN_IF_NOT_OK(Validate());
  return absl::OkStatus();
}

absl::Status AacDecoderConfig::GetOutputSampleRate(
    uint32_t& output_sample_rate) const {
  using enum AudioSpecificConfig::SampleFrequencyIndex;
  static const absl::NoDestructor<
      absl::flat_hash_map<AudioSpecificConfig::SampleFrequencyIndex, uint32_t>>
      kSampleFrequencyIndexToSampleFrequency(
          {{kSampleFrequencyIndex96000, 96000},
           {kSampleFrequencyIndex88200, 88200},
           {kSampleFrequencyIndex64000, 64000},
           {kSampleFrequencyIndex48000, 48000},
           {kSampleFrequencyIndex44100, 44100},
           {kSampleFrequencyIndex32000, 32000},
           {kSampleFrequencyIndex23000, 23000},
           {kSampleFrequencyIndex22050, 22050},
           {kSampleFrequencyIndex16000, 16000},
           {kSampleFrequencyIndex12000, 12000},
           {kSampleFrequencyIndex11025, 11025},
           {kSampleFrequencyIndex8000, 8000},
           {kSampleFrequencyIndex7350, 7350}});

  const auto sample_frequency_index =
      decoder_specific_info_.audio_specific_config.sample_frequency_index_;

  if (sample_frequency_index == kSampleFrequencyIndexEscapeValue) {
    // Accept the value directly from the bitstream.
    output_sample_rate =
        decoder_specific_info_.audio_specific_config.sampling_frequency_;
    return absl::OkStatus();
  }

  if (sample_frequency_index == kSampleFrequencyIndexReservedA ||
      sample_frequency_index == kSampleFrequencyIndexReservedB) {
    // Reject values reserved by the AAC spec.
    return absl::UnimplementedError(absl::StrCat(
        "Reserved sample_frequency_index= ", sample_frequency_index));
  }

  auto sample_frequency_index_iter =
      kSampleFrequencyIndexToSampleFrequency->find(sample_frequency_index);
  if (sample_frequency_index_iter ==
      kSampleFrequencyIndexToSampleFrequency->end()) {
    // Reject anything else not in the map.
    return absl::InvalidArgumentError(absl::StrCat(
        "Unknown `sample_frequency_index`: ", sample_frequency_index));
  }

  // Accept the value from the map.
  output_sample_rate = sample_frequency_index_iter->second;
  return absl::OkStatus();
}

uint8_t AacDecoderConfig::GetBitDepthToMeasureLoudness() {
  // The input/output bit-depth depends on how `fdk_aac` was compiled. Measure
  // loudness based on that.
  return sizeof(INT_PCM) * 8;
}

void AudioSpecificConfig::Print() const {
  LOG(INFO) << "        audio_object_type= "
            << absl::StrCat(audio_object_type_);
  LOG(INFO) << "        sample_frequency_index= "
            << absl::StrCat(sample_frequency_index_);
  if (sample_frequency_index_ == kSampleFrequencyIndexEscapeValue) {
    LOG(INFO) << "        sampling_frequency= " << sampling_frequency_;
  }
  LOG(INFO) << "        channel_configuration= "
            << absl::StrCat(channel_configuration_);
  LOG(INFO) << "      ga_specific_info(aac):";
  LOG(INFO) << "        frame_length_flag= "
            << ga_specific_config_.frame_length_flag;
  LOG(INFO) << "        depends_on_core_coder= "
            << ga_specific_config_.depends_on_core_coder;
  LOG(INFO) << "        extension_flag= " << ga_specific_config_.extension_flag;
}

void AacDecoderConfig::Print() const {
  LOG(INFO) << "    decoder_config(aac):";
  LOG(INFO) << "      object_type_indication= "
            << absl::StrCat(object_type_indication_);
  LOG(INFO) << "      stream_type= " << absl::StrCat(stream_type_);
  LOG(INFO) << "      upstream= " << upstream_;
  LOG(INFO) << "      reserved= " << reserved_;
  LOG(INFO) << "      buffer_size_db= " << buffer_size_db_;
  LOG(INFO) << "      max_bitrate= " << max_bitrate_;
  LOG(INFO) << "      average_bit_rate= " << average_bit_rate_;
  LOG(INFO) << "      decoder_specific_info(aac):";

  decoder_specific_info_.audio_specific_config.Print();
}

}  // namespace iamf_tools
