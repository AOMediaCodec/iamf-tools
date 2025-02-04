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
#ifndef OBU_OBU_HEADER_H_
#define OBU_OBU_HEADER_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief A 5-bit enum for the type of OBU. */
enum ObuType : uint8_t {
  kObuIaCodecConfig = 0,
  kObuIaAudioElement = 1,
  kObuIaMixPresentation = 2,
  kObuIaParameterBlock = 3,
  kObuIaTemporalDelimiter = 4,
  kObuIaAudioFrame = 5,
  kObuIaAudioFrameId0 = 6,
  kObuIaAudioFrameId1 = 7,
  kObuIaAudioFrameId2 = 8,
  kObuIaAudioFrameId3 = 9,
  kObuIaAudioFrameId4 = 10,
  kObuIaAudioFrameId5 = 11,
  kObuIaAudioFrameId6 = 12,
  kObuIaAudioFrameId7 = 13,
  kObuIaAudioFrameId8 = 14,
  kObuIaAudioFrameId9 = 15,
  kObuIaAudioFrameId10 = 16,
  kObuIaAudioFrameId11 = 17,
  kObuIaAudioFrameId12 = 18,
  kObuIaAudioFrameId13 = 19,
  kObuIaAudioFrameId14 = 20,
  kObuIaAudioFrameId15 = 21,
  kObuIaAudioFrameId16 = 22,
  kObuIaAudioFrameId17 = 23,
  kObuIaReserved24 = 24,
  kObuIaReserved25 = 25,
  kObuIaReserved26 = 26,
  kObuIaReserved27 = 27,
  kObuIaReserved28 = 28,
  kObuIaReserved29 = 29,
  kObuIaReserved30 = 30,
  kObuIaSequenceHeader = 31,
};

struct HeaderMetadata {
  ObuType obu_type;
  int64_t total_obu_size;
};

struct ObuHeader {
  friend bool operator==(const ObuHeader& lhs, const ObuHeader& rhs) = default;

  /*!\brief Validates and writes an `ObuHeader`.
   *
   * \param payload_serialized_size `payload_serialized_size` of the output OBU.
   *        The value MUST be able to be cast to `uint32_t` without losing data.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()`
   *         if the fields are invalid or inconsistent or if writing the
   *         `Leb128` representation of `obu_size` fails.
   *         `absl::InvalidArgumentError()` if fields are set inconsistent with
   *         the IAMF specification or if the calculated `obu_size_` larger
   *         than IAMF limitations. Or a specific status if the write fails.
   */
  absl::Status ValidateAndWrite(int64_t payload_serialized_size,
                                WriteBitBuffer& wb) const;

  /*!\brief Validates and reads an `ObuHeader`.
   *
   * \param rb Buffer to read from.

   * \param output_payload_serialized_size `output_payload_serialized_size` Size
   *        of the payload of the OBU.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
   *         the fields are invalid or set in a manner that is inconsistent with
   *         the IAMF specification.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb,
                               int64_t& output_payload_serialized_size);

  /*!\brief Prints logging information about an `ObuHeader`.
   *
   * \param leb_generator `LebGenerator` to use when calculating `obu_size_`.
   * \param payload_serialized_size `payload_serialized_size` of the output OBU.
   *        The value MUST be able to be cast to `uint32_t` without losing data.
   */
  void Print(const LebGenerator& leb_generator,
             int64_t payload_serialized_size) const;

  /*!\brief Peeks the type and total OBU size from the bitstream.
   *
   * This function does not consume any data from the bitstream.
   *
   * \param rb Buffer to read from.
   * \return `HeaderMetadata` containing the OBU type and total OBU size if
   *         successful. Returns an absl::ResourceExhaustedError if there is not
   *         enough data to read the obu_type and obu_size. Returns other errors
   *         if the bitstream is invalid.
   */
  static absl::StatusOr<HeaderMetadata> PeekObuTypeAndTotalObuSize(
      ReadBitBuffer& rb);

  static bool IsTemporalUnitObuType(ObuType obu_type);

  ObuType obu_type;
  // `obu_size` is inserted automatically.
  bool obu_redundant_copy = false;
  bool obu_trimming_status_flag = false;
  bool obu_extension_flag = false;
  DecodedUleb128 num_samples_to_trim_at_end = 0;
  DecodedUleb128 num_samples_to_trim_at_start = 0;
  DecodedUleb128 extension_header_size = 0;
  std::vector<uint8_t> extension_header_bytes = {};
};

}  // namespace iamf_tools

#endif  // OBU_OBU_HEADER_H_
