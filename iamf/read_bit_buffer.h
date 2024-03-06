/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef READ_BIT_BUFFER_H_
#define READ_BIT_BUFFER_H_

#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "iamf/cli/leb_generator.h"
#include "iamf/ia.h"

namespace iamf_tools {

/*!\brief Holds a buffer and tracks the next bit to be read from. */
class ReadBitBuffer {
 public:
  /*!\brief Constructor.
   *
   * \param capacity Capacity of the internal buffer in bytes.
   * \param source Pointer to the data source from which the read buffer will
   *     iteratively load data.
   * \param leb_generator `LebGenerator` to use.
   */
  ReadBitBuffer(int64_t capacity, std::vector<uint8_t>* source,
                const LebGenerator& leb_generator = *LebGenerator::Create());

  /*!\brief Destructor.*/
  ~ReadBitBuffer() = default;

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *     64.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *     `num_bits > 64`. `absl::ResourceExhaustedError()` if the buffer runs
   *     out of data and cannot get more from source before the desired
   *     `num_bits` are read.`absl::UnknownError()` if the `rb->bit_offset` is
   *     negative.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint64_t& output);

  /*!\brief Reads an unsigned leb128 from buffer into `uleb128`.
   *
   * \param uleb128 Decoded unsigned leb128 from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *     the consumed data from the buffer does not fit into the 32 bits of
   *     uleb128, or if the data in the buffer requires that we read more than
   *     `kMaxLeb128Size` bytes. `absl::ResourceExhaustedError()` if the buffer
   *     is exhausted before the uleb128 is fully read and source does not have
   *     the requisite data to complete the uleb128. `absl::UnknownError()` if
   *     the `rb->bit_offset` is negative.
   */
  absl::Status ReadULeb128(DecodedUleb128& uleb128);

  /*!\brief Reads an uint8 vector from buffer into `output`.
   *
   * \param count Number of uint8s to read from the buffer.
   * \param output uint8 vector from buffer is written here.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *     the buffer runs out of data and cannot get more from source before the
   *     desired `count` uint8s are read. `absl::UnknownError()` if the
   *     `rb->bit_offset` is negative.
   */
  absl::Status ReadUint8Vector(const int& count, std::vector<uint8_t>& output);

  /*!\brief Reads a boolean from buffer into `output`.
   *
   * \param output Boolean bit from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *     the buffer runs out of data and cannot get more from source before the
   *     desired boolean is read. `absl::UnknownError()` if the `rb->bit_offset`
   *     is negative.
   */
  absl::Status ReadBoolean(bool& output);

  /*!\brief Returns a `const` pointer to the underlying buffer.
   *
   * \return A `const` pointer to the underlying buffer.
   */
  const std::vector<uint8_t>& bit_buffer() const { return bit_buffer_; }

  /*!\brief Gets the offset in bits of the buffer.
   *
   * \return Offset in bits of the read buffer.
   */
  int64_t buffer_bit_offset() const { return buffer_bit_offset_; }

  /*!\brief Checks whether the current data in the buffer is byte-aligned.
   *
   * \return `true` when the current data in the buffer is byte-aligned.
   */
  bool IsByteAligned() const { return buffer_bit_offset_ % 8 == 0; }

  /*!\brief Gets the offset in bits of the source.
   *
   * \return Offset in bits of the source.
   */
  int64_t source_bit_offset() const { return source_bit_offset_; }

  /*!\brief Loads data from source into the read buffer.
   *
   * Loads exactly enough bytes from `source_` into `bit_buffer_` such that
   * it contains at least `required_num_bits` bits. Load bits updates the
   * `source_bit_offset` based on the number of bytes that were loaded into the
   * buffer.
   *
   * \return `absl::OkStatus()` on success.
   */
  absl::Status LoadBits(int32_t required_num_bits);

  /*!\brief Empties the buffer.*/
  void DiscardAllBits();

  LebGenerator leb_generator_;

 private:
  // Read buffer.
  std::vector<uint8_t> bit_buffer_;
  // Specifies the next bit to consume in the `bit_buffer_`.
  int64_t buffer_bit_offset_ = 0;
  // Pointer to the source data.
  std::vector<uint8_t>* source_;
  // Specifies the next bit to consume from the source data `source_`.
  int64_t source_bit_offset_ = 0;
};

}  // namespace iamf_tools

#endif  // READ_BIT_BUFFER_H_
