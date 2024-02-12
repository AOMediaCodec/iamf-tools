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

  /*!\brief Reads AND consumes the lower `num_bits` from buffer into `data`.
   *
   * \param data Data from buffer will be written here.
   * \param num_bits Number of lower bits of the data to read. Maximum value of
   *     64.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
   *     `num_bits > 64`. `absl::UnknownError()` if the `rb->bit_offset` is
   *     negative.
   */
  absl::Status ReadUnsignedLiteral(uint64_t* data, int num_bits);

  /*!\brief Gets the offset in bits of the buffer.
   *
   * \return Offset in bits of the read buffer.
   */
  int64_t bit_offset() const { return bit_buffer_offset_; }

  /*!\brief Checks whether the current data in the buffer is byte-aligned.
   *
   * \return `true` when the current data in the buffer is byte-aligned.
   */
  bool IsByteAligned() const { return bit_buffer_offset_ % 8 == 0; }

  /*!\brief Loads data from source into the read buffer.
   *
   * Loads exactly enough bytes from `source_` into `bit_buffer_` such that
   * it contains at least `required_num_bits` bits. Load bits updates the
   * `source_offset_` based on the number of bytes that were loaded into the
   * buffer.
   *
   * \return `absl::OkStatus()` on success.
   */
  absl::Status LoadBits(int32_t required_num_bits);

  /*!\brief Empties the buffer.*/
  absl::Status DiscardAllBits();

  LebGenerator leb_generator_;

 private:
  // Read buffer.
  std::vector<uint8_t> bit_buffer_;
  // Specifies the next bit to consume in the `bit_buffer_`.
  int64_t bit_buffer_offset_ = 0;
  // Pointer to the source data.
  std::vector<uint8_t>* source_;
  // Specifies the next byte to consume from the source data `source_`.
  int64_t source_offset_ = 0;
};

}  // namespace iamf_tools

#endif  // READ_BIT_BUFFER_H_
