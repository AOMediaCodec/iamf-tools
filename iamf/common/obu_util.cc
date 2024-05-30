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
#include "iamf/common/obu_util.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/obu/leb128.h"

namespace iamf_tools {

absl::Status AddUint32CheckOverflow(uint32_t x_1, uint32_t x_2,
                                    uint32_t& result) {
  // Add in the payload size.
  const uint64_t sum = static_cast<uint64_t>(x_1) + static_cast<uint64_t>(x_2);
  // Check if this would overflow as a `uint32_t`.
  if (sum > std::numeric_limits<uint32_t>::max()) {
    return absl::InvalidArgumentError(
        "Result of AddUint32CheckOverflow would overflow a uint32_t.");
  }
  result = static_cast<uint32_t>(sum);
  return absl::OkStatus();
}

absl::Status FloatToQ7_8(float value, int16_t& result) {
  // Q7.8 format can represent values in the range [-2^7, 2^7 - 2^-8].
  if (std::isnan(value) || value < -128 || (128.0 - 1.0 / 256.0) < value) {
    return absl::UnknownError(absl::StrCat(
        "Value, ", value, " cannot be represented in Q7.8 format."));
  }
  result = static_cast<int16_t>(value * (1 << 8));
  return absl::OkStatus();
}

float Q7_8ToFloat(int16_t value) {
  return static_cast<float>(value) * 1.0f / 256.0f;
}

absl::Status FloatToQ0_8(float value, uint8_t& result) {
  // Q0.8 format can represent values in the range [0, 1 - 2^-8].
  if (std::isnan(value) || value < 0 || 1 <= value) {
    return absl::UnknownError(absl::StrCat(
        "Value, ", value, " cannot be represented in Q0.8 format."));
  }

  result = static_cast<uint8_t>(value * (1 << 8));
  return absl::OkStatus();
}

float Q0_8ToFloat(uint8_t value) {
  return static_cast<float>(value) * 1.0f / 256.0f;
}

constexpr double kMaxInt32PlusOneAsDouble =
    static_cast<double>(std::numeric_limits<int32_t>::max()) + 1.0;
float Int32ToNormalizedFloat(int32_t value) {
  // Perform calculations in double. The final cast to `float` will result in
  // loss of precision. Note that casting `int32_t` to `double` is lossless;
  // every `int32_t` can be exactly represented.
  return static_cast<float>(static_cast<double>(value) /
                            kMaxInt32PlusOneAsDouble);
}

absl::Status NormalizedFloatToInt32(float value, int32_t& result) {
  if (std::isnan(value) || std::isinf(value)) {
    return absl::InvalidArgumentError("Input is NaN or infinity.");
  }

  const double clamped_input =
      std::clamp(static_cast<double>(value), -1.0, 1.0);
  // Clip the result to be safe. Although only values near
  // `std::numeric_limits<int32_t>::max() + 1` will be out of range.
  return ClipDoubleToInt32(clamped_input * kMaxInt32PlusOneAsDouble, result);
}

absl::Status Uint32ToUint16(uint32_t input, uint16_t& output) {
  if (std::numeric_limits<uint16_t>::max() < input) {
    return absl::InvalidArgumentError("Input is too large for uint16_t.");
  }
  output = static_cast<uint16_t>(input);
  return absl::OkStatus();
}

absl::Status Uint32ToUint8(uint32_t input, uint8_t& output) {
  if (std::numeric_limits<uint8_t>::max() < input) {
    return absl::InvalidArgumentError("Input is too large for uint8_t.");
  }
  output = static_cast<uint8_t>(input);
  return absl::OkStatus();
}

absl::Status Int32ToInt16(int32_t input, int16_t& output) {
  if (input < std::numeric_limits<int16_t>::min() ||
      std::numeric_limits<int16_t>::max() < input) {
    return absl::InvalidArgumentError(
        "Input is outside the range of an int16_t.");
  }
  output = static_cast<int16_t>(input);
  return absl::OkStatus();
}

absl::Status LittleEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                      int32_t& output) {
  // If we have bytes A, B, C, D, then we need to read them as:
  //   (D << 24) | (C << 16) | (B << 8) | A
  // If we have less than four bytes, e.g. two bytes, we would read them as:
  //   (B << 8) | A
  // with the upper bits filled with the sign bit.
  const size_t num_bytes = bytes.size();
  if (num_bytes > 4 || num_bytes < 1) {
    return absl::InvalidArgumentError("Need [1, 4] bytes to make an int32_t");
  }
  int32_t result = 0;
  for (int i = 0; i < bytes.size(); ++i) {
    const auto shift = 8 * ((4 - num_bytes) + i);
    result |= static_cast<int32_t>(bytes[i]) << shift;
  }
  output = result;
  return absl::OkStatus();
}

absl::Status BigEndianBytesToInt32(absl::Span<const uint8_t> bytes,
                                   int32_t& output) {
  // If we have bytes A, B, C, D, then we need to read them as:
  //   (A << 24) | (B << 16) | (C << 8) | D
  // If we have less than four bytes, e.g. two bytes, we would read them as:
  //   (A << 8) | B
  // with the upper bits filled with the sign bit.
  auto reversed_bytes = std::vector<uint8_t>(bytes.rbegin(), bytes.rend());
  return LittleEndianBytesToInt32(reversed_bytes, output);
}

absl::Status ClipDoubleToInt32(double input, int32_t& output) {
  if (std::isnan(input)) {
    return absl::InvalidArgumentError("Input is NaN.");
  }

  if (input > std::numeric_limits<int32_t>::max()) {
    output = std::numeric_limits<int32_t>::max();
  } else if (input < std::numeric_limits<int32_t>::min()) {
    output = std::numeric_limits<int32_t>::min();
  } else {
    output = static_cast<int32_t>(input);
  }

  return absl::OkStatus();
}

absl::Status WritePcmSample(uint32_t sample, uint8_t sample_size,
                            bool big_endian, uint8_t* const buffer,
                            int& write_position) {
  // Validate assumptions of the logic in the `for` loop below.
  if (sample_size % 8 != 0 || sample_size > 32) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid sample size: ", sample_size));
  }

  for (int shift = 32 - sample_size; shift < 32; shift += 8) {
    uint8_t byte = 0;
    if (big_endian) {
      byte = (sample >> ((32 - sample_size) + (32 - (shift + 8)))) & 0xff;
    } else {
      byte = (sample >> shift) & 0xff;
    }
    buffer[write_position++] = byte;
  }

  return absl::OkStatus();
}

absl::Status ReadFileToBytes(const std::filesystem::path& file_path,
                             std::vector<uint8_t>& buffer) {
  if (!std::filesystem::exists(file_path)) {
    return absl::NotFoundError("File not found.");
  }
  std::ifstream ifs(file_path);

  // Increase the size of the buffer. Write to the original end (before
  // resizing).
  const auto file_size = std::filesystem::file_size(file_path);
  const auto original_buffer_size = buffer.size();
  buffer.resize(original_buffer_size + file_size);
  ifs.read(reinterpret_cast<char*>(buffer.data() + original_buffer_size),
           file_size);
  return absl::OkStatus();
}

bool IsNativeBigEndian() {
  if (std::endian::native == std::endian::big) {
    return true;
  } else if (std::endian::native == std::endian::little) {
    return false;
  } else {
    CHECK(false) << "Mixed-endian systems are not supported.";
  }
}

absl::Status ValidateVectorSizeEqual(const std::string& field_name,
                                     size_t vector_size,
                                     DecodedUleb128 obu_reported_size) {
  if (vector_size != obu_reported_size) {
    auto error_message = absl::StrCat(
        "Found inconsistency with `", field_name, ".size()`= ", vector_size,
        ". Expected a value of ", obu_reported_size, ".");
    LOG(ERROR) << error_message;
    return absl::InvalidArgumentError(error_message);
  }
  return absl::OkStatus();
}

}  // namespace iamf_tools
