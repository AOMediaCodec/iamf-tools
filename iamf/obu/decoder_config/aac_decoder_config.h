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
#ifndef OBU_DECODER_CONFIG_AAC_DECODER_CONFIG_H_
#define OBU_DECODER_CONFIG_AAC_DECODER_CONFIG_H_

#include <cstdint>

#include "absl/status/status.h"
#include "iamf/common/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief `AudioSpecificConfig` as defined in ISO 14496-3. */
class AudioSpecificConfig {
 public:
  /*!\brief A 4-bit enum to describe the sampling frequency.
   *
   * See `extensionSamplingFrequencyIndex` in ISO-14496.
   */
  enum SampleFrequencyIndex {
    kSampleFrequencyIndex96000 = 0,
    kSampleFrequencyIndex88200 = 1,
    kSampleFrequencyIndex64000 = 2,
    kSampleFrequencyIndex48000 = 3,
    kSampleFrequencyIndex44100 = 4,
    kSampleFrequencyIndex32000 = 5,
    kSampleFrequencyIndex23000 = 6,
    kSampleFrequencyIndex22050 = 7,
    kSampleFrequencyIndex16000 = 8,
    kSampleFrequencyIndex12000 = 9,
    kSampleFrequencyIndex11025 = 10,
    kSampleFrequencyIndex8000 = 11,
    kSampleFrequencyIndex7350 = 12,
    kSampleFrequencyIndexReservedA = 13,
    kSampleFrequencyIndexReservedB = 14,
    kSampleFrequencyIndexEscapeValue = 15
  };

  static constexpr uint8_t kAudioObjectType = 2;
  static constexpr uint8_t kChannelConfiguration = 2;

  friend bool operator==(const AudioSpecificConfig& lhs,
                         const AudioSpecificConfig& rhs) = default;

  /*!\brief Validates and writes the `AudioSpecificConfig` to a buffer.
   *
   * \return `absl::OkStatus()` if the `AudioSpecificConfig` is valid. A
   *     specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*\!brief Prints logging information about the audio specific config.
   */
  void Print() const;

  uint8_t audio_object_type_ = kAudioObjectType;  // 5 bits.
  SampleFrequencyIndex sample_frequency_index_;   // 4 bits.
  // if(sample_frequency_index == kSampleFrequencyIndexEscapeValue) {
  uint32_t sampling_frequency_ = 0;  // 24 bits.
  // }
  uint8_t channel_configuration_ = kChannelConfiguration;  // 4 bits.

  // The ISO spec allows several different types of configs to follow depending
  // on `audio_object_type`. Valid IAMF streams always use the general audio
  // specific config because of the fixed `audio_object_type == 2`.
  struct GaSpecificConfig {
    static constexpr bool kFrameLengthFlag = false;
    static constexpr bool kDependsOnCoreCoder = false;
    static constexpr bool kExtensionFlag = false;

    friend bool operator==(const GaSpecificConfig& lhs,
                           const GaSpecificConfig& rhs) = default;

    bool frame_length_flag = kFrameLengthFlag;
    bool depends_on_core_coder = kDependsOnCoreCoder;
    bool extension_flag = kExtensionFlag;
  } ga_specific_config_;
};

/*!\brief The `CodecConfig` `decoder_config` field for AAC. */
class AacDecoderConfig {
 public:
  static constexpr uint8_t kDecoderConfigDescriptorTag = 0x04;
  static constexpr uint8_t kObjectTypeIndication = 0x40;
  static constexpr uint8_t kStreamType = 0x05;
  static constexpr bool kUpstream = false;

  friend bool operator==(const AacDecoderConfig& lhs,
                         const AacDecoderConfig& rhs) = default;

  /*!\brief Validates and writes the `AacDecoderConfig` to a buffer.
   *
   * \param audio_roll_distance `audio_roll_distance` in the associated Codec
   *     Config OBU.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the decoder config is valid. A specific
   *     status on failure.
   */
  absl::Status ValidateAndWrite(int16_t audio_roll_distance,
                                WriteBitBuffer& wb) const;

  /*\!brief Gets the output sample rate of the `AacDecoderConfig`.
   *
   * \param output_sample_rate Output sample rate.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *     if the metadata is an unrecognized type.
   */
  absl::Status GetOutputSampleRate(uint32_t& output_sample_rate) const;

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth associated substreams should be
   * decoded to.
   *
   * \return Bit-depth of the PCM which will be used to measure loudness if the
   *     OBU was initialized successfully.
   */
  static uint8_t GetBitDepthToMeasureLoudness();

  /*\!brief Prints logging information about the decoder config.
   */
  void Print() const;

  uint8_t decoder_config_descriptor_tag_ = kDecoderConfigDescriptorTag;
  uint8_t object_type_indication_ = kObjectTypeIndication;
  uint8_t stream_type_ = kStreamType;  // 6 bits.
  bool upstream_ = kUpstream;
  bool reserved_;
  uint32_t buffer_size_db_;  // 24 bits.
  uint32_t max_bitrate_;
  uint32_t average_bit_rate_;

  struct DecoderSpecificInfo {
    static constexpr uint8_t kDecoderSpecificInfoTag = 0x05;

    friend bool operator==(const DecoderSpecificInfo& lhs,
                           const DecoderSpecificInfo& rhs) = default;
    uint8_t decoder_specific_info_tag = kDecoderSpecificInfoTag;
    AudioSpecificConfig audio_specific_config;
  } decoder_specific_info_;
  // ProfileLevelIndicationIndexDescriptor is an extension in the original
  // message, but is unused in IAMF.
};

}  // namespace iamf_tools

#endif  // OBU_DECODER_CONFIG_AAC_DECODER_CONFIG_H_
