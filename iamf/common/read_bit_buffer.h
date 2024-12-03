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
#ifndef COMMON_READ_BIT_BUFFER_H_
#define COMMON_READ_BIT_BUFFER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Holds a buffer and tracks the next bit to be read from. */
class ReadBitBuffer {
 public:
  /*!\brief Constructor.
   *
   * \param capacity Capacity of the internal buffer in bytes.
   * \param source Pointer to the data source from which the read buffer will
   *        load data. The entire content will be moved into the constructed
   *        instance.
   */
  ReadBitBuffer(int64_t capacity, std::vector<uint8_t>* source);

  /*!\brief Destructor.*/
  ~ReadBitBuffer() = default;

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        64.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 64` or the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if the buffer
   *         runs out of data and cannot get more from source before the desired
   *         `num_bits` are read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint64_t& output);

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        32.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 32` or the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if the buffer
   *         runs out of data and cannot get more from source before the desired
   *         `num_bits` are read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint32_t& output);

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        16.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 16` or the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if the buffer runs out of data
   *         and cannot get more from source before the desired `num_bits` are
   *         read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint16_t& output);

  /*!\brief Reads upper `num_bits` from buffer to lower `num_bits` of `output`.
   *
   * \param num_bits Number of upper bits to read from buffer. Maximum value of
   *        8.
   * \param output Unsigned literal from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         `num_bits > 8` or the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if the buffer runs
   *         out of data and cannot get more from source before the desired
   *         `num_bits` are read.
   */
  absl::Status ReadUnsignedLiteral(int num_bits, uint8_t& output);

  /*!\brief Reads the signed 16 bit integer from the read buffer.
   *
   * \param output Signed 16 bit integer will be written here.
   * \return `absl::OkStatus()` on success.  `absl::ResourceExhaustedError()` if
   *         the buffer is exhausted before the signed 16 is fully read and
   *         source does not have the requisite data to complete the signed 16.
   *         `absl::InvalidArgumentError()` if the `rb->bit_offset` is negative.
   */
  absl::Status ReadSigned16(int16_t& output);

  /*!\brief Reads a null terminated string from the read buffer.
   *
   * \param output String will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the string is not terminated within `kIamfMaxStringSize` bytes.
   *         `absl::Status::kResourceExhausted` if the buffer is exhausted
   *         before the string is terminated and source does not have the
   *         requisite data to complete the string. Other specific statuses on
   *         failure.
   */
  absl::Status ReadString(std::string& output);

  /*!\brief Reads an unsigned leb128 from buffer into `uleb128`.
   *
   * This version is useful when the caller does not care about the number of
   * bytes used to encode the data in the bitstream.
   *
   * \param uleb128 Decoded unsigned leb128 from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the consumed data from the buffer does not fit into the 32 bits of
   *         uleb128, or if the data in the buffer requires that we read more
   *         than `kMaxLeb128Size` bytes, or the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if the buffer is exhausted before
   *         the uleb128 is fully read and source does not have the requisite
   *         data to complete the uleb128.
   */
  absl::Status ReadULeb128(DecodedUleb128& uleb128);

  /*!\brief Reads an unsigned leb128 from buffer into `uleb128`.
   *
   * This version also records the number of bytes used to store the encoded
   * uleb128 in the bitstream.
   *
   * \param uleb128 Decoded unsigned leb128 from buffer will be written here.
   * \param encoded_uleb128_size Number of bytes used to store the encoded
   *        uleb128 in the bitstream.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the consumed data from the buffer does not fit into the 32 bits of
   *         uleb128, or if the data in the buffer requires that we read more
   *         than `kMaxLeb128Size` bytes, the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if
   *         the buffer is exhausted before the uleb128 is fully read and
   *         source does not have the requisite data to complete the uleb128.
   */
  absl::Status ReadULeb128(DecodedUleb128& uleb128,
                           int8_t& encoded_uleb128_size);

  /*!\brief Reads the expandable size according to ISO 14496-1.
   *
   * \param max_class_size Maximum class size in bits.
   * \param size_of_instance Size of instance according to the expandable size.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *         the consumed data from the buffer does not fit into the 32 bit
   *         output, or if the data encoded is larger than the `max_class_size`
   *         bits, the `rb->bit_offset` is negative.
   *         `absl::ResourceExhaustedError()` if the buffer is exhausted
   *         before the expanded field is fully read and source does not have
   *         the requisite data to complete the expanded field.
   */
  absl::Status ReadIso14496_1Expanded(uint32_t max_class_size,
                                      uint32_t& size_of_instance);

  /*!\brief Reads `uint8_t`s into the output span.
   *
   * \param output Span of `uint8_t`s to write to.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data and cannot get more from source before
   *         filling the span. `absl::InvalidArgumentError()` if the
   *         `rb->bit_offset` is negative.
   */
  absl::Status ReadUint8Span(absl::Span<uint8_t> output);

  /*!\brief Reads a boolean from buffer into `output`.
   *
   * \param output Boolean bit from buffer will be written here.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data and cannot get more from source before
   *         the desired boolean is read. `absl::InvalidArgumentError()` if the
   *         `rb->bit_offset` is negative.
   */
  absl::Status ReadBoolean(bool& output);

  /*!\brief Checks whether there is any data left in the buffer or source.
   *
   * \return `true` if there is some data left in the buffer or source that has
   *         not been consumed yet. `false` otherwise.
   */
  bool IsDataAvailable() const;

  /*!\brief Returns the next reading position of the source in bits.
   *
   * \return Next reading position of the source in bits.
   */
  int64_t Tell() const;

  /*!\brief Moves the next reading position in bits of the source.
   *
   * \param position Requested position in bits to move to.
   * \return `absl::OkStatus()` on success. `absl::ResourceExhaustedError()` if
   *         the buffer runs out of data. `absl::InvalidArgumentError()` if
   *         the requested position is negative.
   */
  absl::Status Seek(int64_t position);

 private:
  absl::Status ReadUnsignedLiteralInternal(const int num_bits,
                                           const int max_num_bits,
                                           uint64_t& output);

  // Read buffer.
  std::vector<uint8_t> bit_buffer_;

  // Specifies the next bit to consume in the `bit_buffer_`.
  int64_t buffer_bit_offset_ = 0;

  // Size of the valid data in the buffer in bits.
  int64_t buffer_size_ = 0;

  // Source data.
  std::vector<uint8_t> source_;

  // Specifies the next bit to consume from the source data `source_`.
  int64_t source_bit_offset_ = 0;
};

}  // namespace iamf_tools

#endif  // COMMON_READ_BIT_BUFFER_H_
