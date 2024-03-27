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
#ifndef CODEC_CONFIG_H_
#define CODEC_CONFIG_H_

#include <cstdint>
#include <variant>

#include "absl/status/status.h"
#include "iamf/aac_decoder_config.h"
#include "iamf/flac_decoder_config.h"
#include "iamf/ia.h"
#include "iamf/lpcm_decoder_config.h"
#include "iamf/obu_base.h"
#include "iamf/obu_header.h"
#include "iamf/opus_decoder_config.h"
#include "iamf/read_bit_buffer.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

// TODO(b/305752871): Port this `std::variant` to a virtual class.
typedef std::variant<OpusDecoderConfig, AacDecoderConfig, FlacDecoderConfig,
                     LpcmDecoderConfig>
    DecoderConfig;

struct CodecConfig {
  enum CodecId : uint32_t {
    kCodecIdOpus = 0x4f707573,   // "Opus"
    kCodecIdFlac = 0x664c6143,   // "fLaC"
    kCodecIdLpcm = 0x6970636d,   // "ipcm"
    kCodecIdAacLc = 0x6d703461,  // "mp4a"
  };

  friend bool operator==(const CodecConfig& lhs,
                         const CodecConfig& rhs) = default;

  CodecId codec_id;
  DecodedUleb128 num_samples_per_frame;
  int16_t audio_roll_distance;

  // Active field depends on `codec_id`.
  DecoderConfig decoder_config;
};

class CodecConfigObu : public ObuBase {
 public:
  /*!\brief Constructor.
   *
   * After constructing `Initialize` MUST be called and return successfully
   * before using most functionality of the OBU.
   *
   * \param header `ObuHeader` of the OBU.
   * \param codec_config_id `codec_config_id` in the OBU.
   * \param codec_config `codec_config` in the OBU.
   */
  CodecConfigObu(const ObuHeader& header, const DecodedUleb128 codec_config_id,
                 const CodecConfig& codec_config);

  /*\!brief Move constructor.*/
  CodecConfigObu(CodecConfigObu&& other) = default;

  /*!\brief Destructor. */
  ~CodecConfigObu() override = default;

  friend bool operator==(const CodecConfigObu& lhs,
                         const CodecConfigObu& rhs) = default;

  /*\!brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  /*!\brief Initializes the OBU.
   *
   * `GetOutputSampleRate`, `GetInputSampleRate`, and
   * `GetBitDepthToMeasureLoudness` may return inaccurate values if this
   * function did not return `absl::OkStatus()`.
   *
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status Initialize();

  /*!\brief Validates and writes the `DecoderConfig` portion of the OBU.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ValidateAndWriteDecoderConfig(WriteBitBuffer& wb) const;

  /*!\brief Gets the output sample rate associated with the OBU.
   *
   * \return Output sample rate in Hz if the OBU was initialized successfully.
   */
  uint32_t GetOutputSampleRate() const { return output_sample_rate_; }

  /*!\brief Gets the input sample rate associated with the OBU.
   *
   * \return Input sample rate in Hz if the OBU was initialized successfully.
   */
  uint32_t GetInputSampleRate() const { return input_sample_rate_; }

  /*!\brief Gets the bit-depth of the PCM to be used to measure loudness.
   *
   * This typically is the highest bit-depth associated substreams should be
   * decoded to.
   *
   * \return Bit-depth of the PCM which will be used to measure loudness if the
   *     OBU was initialized successfully.
   */
  uint32_t GetBitDepthToMeasureLoudness() const {
    return bit_depth_to_measure_loudness_;
  }

  /*!\brief Gets the number of samples per frame of the OBU.
   *
   * \return Num samples per frame of the OBU.
   */
  uint32_t GetNumSamplesPerFrame() const {
    return codec_config_.num_samples_per_frame;
  }

  // Fields in the OBU as per the IAMF specification.
  const DecodedUleb128 codec_config_id_;
  const CodecConfig codec_config_;

  // Metadata fields.
  const bool is_lossless_;

 private:
  // Metadata fields.
  uint32_t input_sample_rate_ = 0;
  uint32_t output_sample_rate_ = 0;
  uint8_t bit_depth_to_measure_loudness_ = 0;

  // Tracks whether the OBU was initialized correctly.
  absl::Status init_status_ = absl::UnknownError("");

  /*\!brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*\!brief Reads the OBU payload from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndReadPayload(ReadBitBuffer& rb) override;
};

}  // namespace iamf_tools

#endif  // CODEC_CONFIG_H_
