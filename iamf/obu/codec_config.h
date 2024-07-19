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
#ifndef OBU_CODEC_CONFIG_H_
#define OBU_CODEC_CONFIG_H_

#include <cstdint>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/flac_decoder_config.h"
#include "iamf/obu/decoder_config/lpcm_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"

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

  /*!\brief Creates a `CodecConfigObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param rb `ReadBitBuffer` where the `CodecConfigObu` data is stored. Data
   *    read from the buffer is consumed.
   * \return a `CodecConfigObu` on success. A specific status on failure.
   */
  static absl::StatusOr<CodecConfigObu> CreateFromBuffer(
      const ObuHeader& header, ReadBitBuffer& rb);

  /*!\brief Move constructor.*/
  CodecConfigObu(CodecConfigObu&& other) = default;

  /*!\brief Destructor. */
  ~CodecConfigObu() override = default;

  friend bool operator==(const CodecConfigObu& lhs,
                         const CodecConfigObu& rhs) = default;

  /*!\brief Prints logging information about the OBU.*/
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

  /*!\brief Validates and reads the `DecoderConfig` portion of the OBU.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  absl::Status ReadAndValidateDecoderConfig(ReadBitBuffer& rb);

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

  /*!\brief Gets the codec config id of the OBU.
   *
   * \return codec config id of the OBU.
   */
  DecodedUleb128 GetCodecConfigId() const { return codec_config_id_; }

  /*!\brief Gets the codec config of the OBU.
   *
   * \return codec config of the OBU.
   */
  const CodecConfig& GetCodecConfig() const { return codec_config_; }

  bool IsLossless() const;

 private:
  // Used only by the factory create function.
  explicit CodecConfigObu(const ObuHeader& header)
      : ObuBase(header, kObuIaCodecConfig),
        codec_config_id_(DecodedUleb128()),
        codec_config_(CodecConfig()) {}

  // Fields in the OBU as per the IAMF specification.
  DecodedUleb128 codec_config_id_;
  CodecConfig codec_config_;

  // Metadata fields.
  uint32_t input_sample_rate_ = 0;
  uint32_t output_sample_rate_ = 0;
  uint8_t bit_depth_to_measure_loudness_ = 0;

  // Tracks whether the OBU was initialized correctly.
  absl::Status init_status_ =
      absl::UnknownError("Codec Config OBU was not initialized correctly.");

  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the OBU is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *     failure.
   */
  absl::Status ReadAndValidatePayload(ReadBitBuffer& rb) override;
};

}  // namespace iamf_tools

#endif  // OBU_CODEC_CONFIG_H_
