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
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "libSYS/include/machine_type.h"

namespace iamf_tools {

namespace {

using SampleFrequencyIndex = AudioSpecificConfig::SampleFrequencyIndex;

// ISO 14496:1 limits the max size of `DecoderConfigDescriptor` and
// `DecoderSpecificInfo` to 2^28 - 1 bits.
constexpr int32_t kMaxClassSize = (1 << 28) - 1;

// We typically expect the classes in this file to be very small (except when
// extensions are present).
constexpr int kInternalBufferSize = 32;

absl::Status ValidateAudioRollDistance(int16_t audio_roll_distance) {
  return ValidateEqual(audio_roll_distance,
                       AacDecoderConfig::GetRequiredAudioRollDistance(),
                       "audio_roll_distance");
}

// Copies all data from `original_wb` to `output_wb` with the corresponding ISO
// 14496-1:2010 expandable size field prepended.
absl::Status PrependWithIso14496_1Expanded(const WriteBitBuffer& original_wb,
                                           WriteBitBuffer& output_wb) {
  CHECK(original_wb.IsByteAligned());
  if (original_wb.bit_buffer().size() > kMaxClassSize) {
    return absl::ResourceExhaustedError(
        absl::StrCat("Buffer size ", original_wb.bit_buffer().size(),
                     " exceeds the maximum expected size."));
  }
  RETURN_IF_NOT_OK(
      output_wb.WriteIso14496_1Expanded(original_wb.bit_buffer().size()));
  RETURN_IF_NOT_OK(output_wb.WriteUint8Vector(original_wb.bit_buffer()));
  return absl::OkStatus();
}

absl::Status WriteDecoderSpecificInfo(
    const AacDecoderConfig::DecoderSpecificInfo& decoder_specific_info,
    WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(
      decoder_specific_info.decoder_specific_info_tag, 8));
  // Determine the size by writing the remaining `DecoderSpecificInfo`, then
  // prepend the size and write it to the output buffer.
  {
    WriteBitBuffer wb_internal(kInternalBufferSize);
    // Write nested `audio_specific_config`.
    RETURN_IF_NOT_OK(
        decoder_specific_info.audio_specific_config.ValidateAndWrite(
            wb_internal));
    // Write the `DecoderSpecificInfo` extension.
    RETURN_IF_NOT_OK(wb_internal.WriteUint8Vector(
        decoder_specific_info.decoder_specific_info_extension));
    RETURN_IF_NOT_OK(PrependWithIso14496_1Expanded(wb_internal, wb));
  }
  return absl::OkStatus();
}

absl::Status GetExpectedPositionFromIso14496_1Expanded(
    ReadBitBuffer& rb, int64_t& expected_position) {
  uint32_t size;
  RETURN_IF_NOT_OK(rb.ReadIso14496_1Expanded(kMaxClassSize, size));
  expected_position = rb.Tell() + (static_cast<int64_t>(size) * 8);
  return absl::OkStatus();
}

// Advances the buffer to the position. Dumps all skipped bytes to `extension`.
// OK if the buffer is already at the position. Fails if the buffer would need
// to go backwards.
absl::Status AdvanceBufferToPosition(absl::string_view debugging_context,
                                     ReadBitBuffer& rb,
                                     const int64_t expected_position,
                                     std::vector<uint8_t>& extension) {
  const int64_t actual_position = rb.Tell();
  if (actual_position == expected_position) {
    // Ok no extension is present.
    return absl::OkStatus();
  } else if (actual_position < expected_position) {
    // Advance and consume the extension.
    extension.resize((expected_position - actual_position) / 8);
    return rb.ReadUint8Span(absl::MakeSpan(extension));
  } else {
    // The buffer is already past the position.
    return absl::OutOfRangeError(
        absl::StrCat("Not enough bytes to parse ", debugging_context, "."));
  }
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
  if (sample_frequency_index_ == SampleFrequencyIndex::kEscapeValue) {
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
  if (sample_frequency_index_ == SampleFrequencyIndex::kEscapeValue) {
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
  MAYBE_RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));
  RETURN_IF_NOT_OK(Validate());

  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(decoder_config_descriptor_tag_, 8));
  // Write the remaining `DecoderConfigDescriptor`, then once we know the size,
  // prepend it with the expandable size field.
  {
    WriteBitBuffer wb_internal(kInternalBufferSize);
    RETURN_IF_NOT_OK(
        wb_internal.WriteUnsignedLiteral(object_type_indication_, 8));
    RETURN_IF_NOT_OK(wb_internal.WriteUnsignedLiteral(stream_type_, 6));
    RETURN_IF_NOT_OK(wb_internal.WriteUnsignedLiteral(upstream_, 1));
    RETURN_IF_NOT_OK(wb_internal.WriteUnsignedLiteral(reserved_, 1));
    RETURN_IF_NOT_OK(wb_internal.WriteUnsignedLiteral(buffer_size_db_, 24));
    RETURN_IF_NOT_OK(wb_internal.WriteUnsignedLiteral(max_bitrate_, 32));
    RETURN_IF_NOT_OK(wb_internal.WriteUnsignedLiteral(average_bit_rate_, 32));

    // Write nested `decoder_specific_info`.
    RETURN_IF_NOT_OK(
        WriteDecoderSpecificInfo(decoder_specific_info_, wb_internal));

    RETURN_IF_NOT_OK(wb_internal.WriteUint8Vector(decoder_config_extension_));

    RETURN_IF_NOT_OK(PrependWithIso14496_1Expanded(wb_internal, wb));
  }

  return absl::OkStatus();
}

absl::Status AacDecoderConfig::ReadAndValidate(int16_t audio_roll_distance,
                                               ReadBitBuffer& rb) {
  // Read top-level fields.
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, decoder_config_descriptor_tag_));
  int64_t end_of_decoder_config_position;
  RETURN_IF_NOT_OK(GetExpectedPositionFromIso14496_1Expanded(
      rb, end_of_decoder_config_position));

  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, object_type_indication_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, stream_type_));
  RETURN_IF_NOT_OK(rb.ReadBoolean(upstream_));
  RETURN_IF_NOT_OK(rb.ReadBoolean(reserved_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(24, buffer_size_db_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, max_bitrate_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(32, average_bit_rate_));

  // Read nested `decoder_specific_info` the advance past its nested extension.
  {
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(
        8, decoder_specific_info_.decoder_specific_info_tag));
    int64_t end_of_decoder_specific_info_position;
    RETURN_IF_NOT_OK(GetExpectedPositionFromIso14496_1Expanded(
        rb, end_of_decoder_specific_info_position));
    // Read nested `audio_specific_config`.
    RETURN_IF_NOT_OK(decoder_specific_info_.audio_specific_config.Read(rb));
    RETURN_IF_NOT_OK(AdvanceBufferToPosition(
        "decoder_specific_info", rb, end_of_decoder_specific_info_position,
        decoder_specific_info_.decoder_specific_info_extension));
  }
  // Advance past the top-level extension.
  RETURN_IF_NOT_OK(AdvanceBufferToPosition("decoder_config_descriptor", rb,
                                           end_of_decoder_config_position,
                                           decoder_config_extension_));

  RETURN_IF_NOT_OK(ValidateAudioRollDistance(audio_roll_distance));
  RETURN_IF_NOT_OK(Validate());
  return absl::OkStatus();
}

absl::Status AacDecoderConfig::GetOutputSampleRate(
    uint32_t& output_sample_rate) const {
  using enum AudioSpecificConfig::SampleFrequencyIndex;
  static const absl::NoDestructor<
      absl::flat_hash_map<AudioSpecificConfig::SampleFrequencyIndex, uint32_t>>
      kSampleFrequencyIndexToSampleFrequency({{k96000, 96000},
                                              {k88200, 88200},
                                              {k64000, 64000},
                                              {k48000, 48000},
                                              {k44100, 44100},
                                              {k32000, 32000},
                                              {k24000, 24000},
                                              {k22050, 22050},
                                              {k16000, 16000},
                                              {k12000, 12000},
                                              {k11025, 11025},
                                              {k8000, 8000},
                                              {k7350, 7350}});

  const auto sample_frequency_index =
      decoder_specific_info_.audio_specific_config.sample_frequency_index_;

  if (sample_frequency_index == SampleFrequencyIndex::kEscapeValue) {
    // Accept the value directly from the bitstream.
    output_sample_rate =
        decoder_specific_info_.audio_specific_config.sampling_frequency_;
    return absl::OkStatus();
  }

  if (sample_frequency_index == SampleFrequencyIndex::kReservedA ||
      sample_frequency_index == SampleFrequencyIndex::kReservedB) {
    // Reject values reserved by the AAC spec.
    return absl::UnimplementedError(absl::StrCat(
        "Reserved sample_frequency_index= ", sample_frequency_index));
  }

  return CopyFromMap(
      *kSampleFrequencyIndexToSampleFrequency, sample_frequency_index,
      "Sample rate for AAC Sampling Frequency Index", output_sample_rate);
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
  if (sample_frequency_index_ == SampleFrequencyIndex::kEscapeValue) {
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
  LOG(INFO) << "      // decoder_specific_info_extension omitted.";
  LOG(INFO) << "      // decoder_config_extension omitted.";
}

}  // namespace iamf_tools
