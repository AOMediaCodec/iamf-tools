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
#ifndef OBU_AMBISONICS_CONFIG_H_
#define OBU_AMBISONICS_CONFIG_H_

#include <cstdint>
#include <limits>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Configuration for mono-coded Ambisonics. */
struct AmbisonicsMonoConfig {
  // RFC 8486 reserves 255 to signal an inactive ACN (ambisonics channel
  // number).
  static constexpr uint8_t kInactiveAmbisonicsChannelNumber = 255;

  friend bool operator==(const AmbisonicsMonoConfig& lhs,
                         const AmbisonicsMonoConfig& rhs) = default;

  /*!\brief Validates the configuration.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate(DecodedUleb128 num_substreams_in_audio_element) const;

  uint8_t output_channel_count;  // (C).
  uint8_t substream_count;       // (N).

  // Vector of length (C).
  std::vector<uint8_t> channel_mapping;
};

/*!\brief Configuration for projection-coded Ambisonics. */
struct AmbisonicsProjectionConfig {
  friend bool operator==(const AmbisonicsProjectionConfig& lhs,
                         const AmbisonicsProjectionConfig& rhs) = default;

  /*!\brief Validates the configuration.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate(DecodedUleb128 num_substreams_in_audio_element) const;

  uint8_t output_channel_count;     // (C).
  uint8_t substream_count;          // (N).
  uint8_t coupled_substream_count;  // (M).

  // Vector of length (N + M) * C.
  std::vector<int16_t> demixing_matrix;
};

/*!\brief Config to reconstruct an Audio Element OBU using Ambisonics layout.
 *
 * The metadata required for combining the substreams identified here in order
 * to reconstruct an Ambisonics layout.
 */
struct AmbisonicsConfig {
  /*!\brief A `DecodedUleb128` enum for the method of coding Ambisonics. */
  enum AmbisonicsMode : DecodedUleb128 {
    kAmbisonicsModeMono = 0,
    kAmbisonicsModeProjection = 1,
    kAmbisonicsModeReservedStart = 2,
    kAmbisonicsModeReservedEnd = std::numeric_limits<DecodedUleb128>::max(),
  };
  friend bool operator==(const AmbisonicsConfig& lhs,
                         const AmbisonicsConfig& rhs) = default;

  /*!\brief Gets the next valid number of output channels.
   *
   * \param requested_output_channel_count Requested number of channels.
   * \param next_valid_output_channel_count Minimum valid `output_channel_count`
   *        that has at least the required number of channels.
   * \return `absl::OkStatus()` if successful. `kIamfInvalid` argument if
   *         the input is too large.
   */
  static absl::Status GetNextValidOutputChannelCount(
      uint8_t requested_output_channel_count,
      uint8_t& next_valid_output_channel_count);

  /*!\brief Validates and writes the configuration.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful.
   */
  absl::Status ValidateAndWrite(DecodedUleb128 num_substreams_in_audio_element,
                                WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the configuration from a buffer.
   *
   * \param num_substreams_in_audio_element Number of substreams in the
   *        corresponding OBU.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful.
   */
  absl::Status ReadAndValidate(DecodedUleb128 num_substreams_in_audio_element,
                               ReadBitBuffer& rb);

  /*!\brief Prints logging information about the configuration. */
  void Print() const;

  AmbisonicsMode ambisonics_mode;  // Serialized to a ULEB128.

  // The active field depends on `ambisonics_mode`.
  std::variant<AmbisonicsMonoConfig, AmbisonicsProjectionConfig>
      ambisonics_config;
};

}  // namespace iamf_tools

#endif  // OBU_AMBISONICS_CONFIG_H_
