/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_UTIL_H_
#define OBU_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "iamf/ia.h"

namespace iamf_tools {

/*!\brief Sums the input values and checks for overflow.
 *
 * \param x_1 First summand.
 * \param x_2 Second summand.
 * \param result Sum of the inputs on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` when
 *     the sum would cause an overflow in a `uint32_t`.
 */
absl::Status AddUint32CheckOverflow(uint32_t x_1, uint32_t x_2,
                                    uint32_t& result);

/*!\brief Converts float input to Q7.8 format.
 *
 * \param value Value to convert.
 * \param result Converted value if successful. The result is floored to the
 *     nearest Q7.8 value.
 * \return `absl::OkStatus()` if successful. `absl::UnknownError()` if the input
 *     is not valid in Q7.8 format.
 */
absl::Status FloatToQ7_8(float value, int16_t& result);

/*!\brief Converts Q7.8 input to float output.
 *
 * \param value Value to convert.
 * \return Converted value.
 */
float Q7_8ToFloat(int16_t value);

// TODO(b/283281856): Consider removing `FloatToQ0_8()` if it is still an unused
//                    function after the encoder supports resampling parameter
//                    blocks.
/*!\brief Converts float input to Q0.8 format.
 *
 * \param value Value to convert.
 * \param result Converted value if successful. The result is floored to the
 *     nearest Q0.8 value.
 * \return `absl::OkStatus()` if successful. `absl::UnknownError()` if the input
 *     is not valid in Q0.8 format.
 */
absl::Status FloatToQ0_8(float value, uint8_t& result);

/*!\brief Converts Q0.8 input to float output.
 *
 * \param value Value to convert.
 * \return Converted value.
 */
float Q0_8ToFloat(uint8_t value);

/*\!brief Normalizes the input value to a `float` in the range [-1, +1].
 *
 * Normalizes the input from [std::numeric_limits<int32_t>::min(),
 * std::numeric_limits<int32_t>::max() + 1] to [-1, +1].
 *
 * \param value Value to normalize.
 * \return Normalized value.
 */
float Int32ToNormalizedFloat(int32_t value);

/*\!brief Converts normalized `float` input to an `int32_t`.
 *
 * Transforms the input from the range of [-1, +1] to the range of
 * [std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() +
 * 1].
 *
 * Input is clamped to [-1, +1] before processing. Output is clamped to the
 * full range of an `int32_t`.
 *
 * \param value Normalized float to convert.
 * \param result Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *     the input is any type of NaN or infinity.
 */
absl::Status NormalizedFloatToInt32(float value, int32_t& result);

/*!\brief Typecasts the input value and writes to the output argument if valid.
 *
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *     the input cannot be cast to a `uint8_t`.
 */
absl::Status Uint32ToUint8(uint32_t input, uint8_t& output);

/*!\brief Typecasts the input value and writes to the output argument if valid.
 *
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *     the input cannot be cast to a `uint16_t`.
 */
absl::Status Uint32ToUint16(uint32_t input, uint16_t& output);

/*!\brief Typecasts the input value and writes to the output argument if valid.
 *
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *     the input cannot be cast to a `int16_t`.
 */
absl::Status Int32ToInt16(int32_t input, int16_t& output);

/*!\brief Clips and typecasts the input value and writes to the output argument.
 *
 * \param input Value to convert.
 * \param output Converted value if successful.
 * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
 *     the input is NaN.
 */
absl::Status ClipDoubleToInt32(double input, int32_t& output);

/*\!brief Writes the input PCM sample to a buffer.
 *
 * Writes the most significant `sample_size` bits of `sample` starting at
 * `buffer[write_position]`. It is up to the user to ensure the buffer is valid.
 *
 * \param sample Sample to write the upper `sample_size` bits of.
 * \param sample_size Sample size in bits. MUST be one of {8, 16, 24, 32}.
 * \param big_endian `true` to write the sample as big endian. `false` to write
 *     it as little endian.
 * \param buffer Start of the buffer to write to.
 * \param write_position Offset of the buffer to write to. Incremented to one
 *     after the last byte written on success.
 * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` if
 *     `sample_size` is invalid.
 */
absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* buffer,
                            int& write_position);

/*\!brief Gets the native byte order of the runtime system.
 *
 * \return `true` if the runtime system natively uses big endian, `false`
 *     otherwise.
 */
bool IsNativeBigEndian();

/*\!brief Returns an error if the size arguments are not equivalent.
 *
 * Intended to be used in OBUs to ensure the reported and actual size of vectors
 * are equivalent.
 *
 * \param field_name Value to insert into the error message.
 * \param vector_size Size of the vector.
 * \param obu_reported_size Size reported in the OBU.
 * \return `absl::OkStatus()` if the size arguments are equivalent.
 *     `absl::InvalidArgumentError()` otherwise.
 */
absl::Status ValidateVectorSizeEqual(const std::string& field_name,
                                     size_t vector_size,
                                     DecodedUleb128 obu_reported_size);

template <typename T, typename U>
absl::Status LookupInMap(const absl::flat_hash_map<T, U>& map, T key,
                         U& value) {
  auto iter = map.find(key);
  if (iter == map.end()) {
    return absl::InvalidArgumentError("");
  }
  value = iter->second;
  return absl::OkStatus();
}

}  // namespace iamf_tools

#endif  // OBU_UTIL_H_
