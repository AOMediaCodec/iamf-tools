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
#include "iamf/obu/ambisonics_config.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

size_t GetNumDemixingMatrixElements(const AmbisonicsProjectionConfig& config) {
  const size_t c = static_cast<size_t>(config.output_channel_count);
  const size_t n = static_cast<size_t>(config.substream_count);
  const size_t m = static_cast<size_t>(config.coupled_substream_count);

  return (n + m) * c;
}

void LogAmbisonicsMonoConfig(const AmbisonicsMonoConfig& mono_config) {
  ABSL_VLOG(1) << "  ambisonics_mono_config:";
  ABSL_VLOG(1) << "    output_channel_count:"
               << absl::StrCat(mono_config.output_channel_count);
  ABSL_VLOG(1) << "    substream_count:"
               << absl::StrCat(mono_config.substream_count);
  std::stringstream channel_mapping_stream;
  for (int c = 0; c < mono_config.output_channel_count; c++) {
    channel_mapping_stream << absl::StrCat(mono_config.channel_mapping[c])
                           << ", ";
  }
  ABSL_VLOG(1) << "    channel_mapping: [ " << channel_mapping_stream.str()
               << "]";
}

void LogAmbisonicsProjectionConfig(
    const AmbisonicsProjectionConfig& projection_config) {
  ABSL_VLOG(1) << "  ambisonics_projection_config:";
  ABSL_VLOG(1) << "    output_channel_count:"
               << absl::StrCat(projection_config.output_channel_count);
  ABSL_VLOG(1) << "    substream_count:"
               << absl::StrCat(projection_config.substream_count);
  ABSL_VLOG(1) << "    coupled_substream_count:"
               << absl::StrCat(projection_config.coupled_substream_count);
  std::string demixing_matrix_string;
  for (int i = 0; i < (projection_config.substream_count +
                       projection_config.coupled_substream_count) *
                          projection_config.output_channel_count;
       i++) {
    absl::StrAppend(&demixing_matrix_string,
                    projection_config.demixing_matrix[i], ",");
  }
  ABSL_VLOG(1) << "    demixing_matrix: [ " << demixing_matrix_string << "]";
}

absl::Status ValidateOutputChannelCount(const uint8_t channel_count) {
  uint8_t next_valid_output_channel_count;
  RETURN_IF_NOT_OK(AmbisonicsConfig::GetNextValidOutputChannelCount(
      channel_count, next_valid_output_channel_count));

  if (next_valid_output_channel_count == channel_count) {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(absl::StrCat(
      "Invalid Ambisonics output channel_count = ", channel_count));
}

// Writes the `AmbisonicsMonoConfig` of an ambisonics mono `AudioElementObu`.
absl::Status ValidateAndWriteAmbisonicsMono(
    const AmbisonicsMonoConfig& mono_config, WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(mono_config.Validate());

  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(mono_config.output_channel_count, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(mono_config.substream_count, 8));

  RETURN_IF_NOT_OK(
      wb.WriteUint8Span(absl::MakeConstSpan(mono_config.channel_mapping)));

  return absl::OkStatus();
}

// Writes the `AmbisonicsProjectionConfig` of an ambisonics projection
// `AudioElementObu`.
absl::Status ValidateAndWriteAmbisonicsProjection(
    const AmbisonicsProjectionConfig& projection_config, WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(projection_config.Validate());

  // Write the main portion of the `AmbisonicsProjectionConfig`.
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(projection_config.output_channel_count, 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(projection_config.substream_count, 8));
  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(projection_config.coupled_substream_count, 8));

  // Loop to write the `demixing_matrix`.
  for (size_t i = 0; i < projection_config.demixing_matrix.size(); i++) {
    RETURN_IF_NOT_OK(wb.WriteSigned16(projection_config.demixing_matrix[i]));
  }

  return absl::OkStatus();
}

absl::Status ReadAndValidateAmbisonicsProjection(
    AmbisonicsProjectionConfig& projection_config, ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(
      rb.ReadUnsignedLiteral(8, projection_config.output_channel_count));
  RETURN_IF_NOT_OK(
      rb.ReadUnsignedLiteral(8, projection_config.substream_count));
  RETURN_IF_NOT_OK(
      rb.ReadUnsignedLiteral(8, projection_config.coupled_substream_count));
  const size_t demixing_matrix_size =
      GetNumDemixingMatrixElements(projection_config);
  for (size_t i = 0; i < demixing_matrix_size; ++i) {
    int16_t demixing_matrix_value;
    RETURN_IF_NOT_OK(rb.ReadSigned16(demixing_matrix_value));
    projection_config.demixing_matrix.push_back(demixing_matrix_value);
  }
  RETURN_IF_NOT_OK(projection_config.Validate());
  return absl::OkStatus();
}

absl::Status ReadAndValidateAmbisonicsMonoConfig(
    AmbisonicsMonoConfig& mono_config, ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, mono_config.output_channel_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, mono_config.substream_count));
  const size_t channel_mapping_size = mono_config.output_channel_count;
  mono_config.channel_mapping.resize(channel_mapping_size);
  RETURN_IF_NOT_OK(
      rb.ReadUint8Span(absl::MakeSpan(mono_config.channel_mapping)));
  RETURN_IF_NOT_OK(mono_config.Validate());
  return absl::OkStatus();
}

}  // namespace

absl::Status AmbisonicsMonoConfig::Validate() const {
  MAYBE_RETURN_IF_NOT_OK(ValidateOutputChannelCount(output_channel_count));
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "channel_mapping", channel_mapping, output_channel_count));
  if (substream_count > output_channel_count) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected substream_count=", substream_count,
                     " to be less than or equal to `output_channel_count`=",
                     output_channel_count, "."));
  }

  // Track the number of unique substream indices in the mapping.
  absl::flat_hash_set<uint8_t> unique_substream_indices;
  for (const auto& substream_index : channel_mapping) {
    if (substream_index == kInactiveAmbisonicsChannelNumber) {
      // OK. This implies the nth ambisonics channel number is dropped (i.e. the
      // user wants mixed-order ambisonics).
      continue;
    }
    if (substream_index >= substream_count) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Mapping out of bounds. When substream_count= ", substream_count,
          " there is no substream_index= ", substream_index, "."));
    }

    unique_substream_indices.insert(substream_index);
  }

  if (unique_substream_indices.size() != substream_count) {
    return absl::InvalidArgumentError(absl::StrCat(
        "A substream is in limbo; it has no associated ACN. ",
        "substream_count= ", substream_count,
        ", unique_substream_indices.size()= ", unique_substream_indices.size(),
        "."));
  }

  return absl::OkStatus();
}

absl::Status AmbisonicsProjectionConfig::Validate() const {
  RETURN_IF_NOT_OK(ValidateOutputChannelCount(output_channel_count));
  if (coupled_substream_count > substream_count) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected coupled_substream_count= ", coupled_substream_count,
        " to be less than or equal to substream_count= ", substream_count));
  }

  if ((static_cast<int>(substream_count) +
       static_cast<int>(coupled_substream_count)) > output_channel_count) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected coupled_substream_count= ", coupled_substream_count,
        " + substream_count= ", substream_count,
        " to be less than or equal to `output_channel_count`= ",
        output_channel_count, "."));
  }

  const size_t expected_num_elements = GetNumDemixingMatrixElements(*this);
  RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
      "demixing_matrix", demixing_matrix, expected_num_elements));

  return absl::OkStatus();
}

absl::Status AmbisonicsConfig::GetNextValidOutputChannelCount(
    uint8_t requested_output_channel_count,
    uint8_t& next_valid_output_channel_count) {
  // Valid values are `(1+n)^2`, for integer `n` in the range [0, 14].
  static constexpr auto kValidAmbisonicChannelCounts = []() -> auto {
    std::array<uint8_t, 15> channel_count_i;
    for (size_t i = 0; i < channel_count_i.size(); ++i) {
      channel_count_i[i] = (i + 1) * (i + 1);
    }
    return channel_count_i;
  }();

  // Lookup the next higher or equal valid channel count.
  auto valid_channel_count_iter = std::lower_bound(
      kValidAmbisonicChannelCounts.begin(), kValidAmbisonicChannelCounts.end(),
      requested_output_channel_count);
  if (valid_channel_count_iter != kValidAmbisonicChannelCounts.end()) {
    next_valid_output_channel_count = *valid_channel_count_iter;
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(absl::StrCat(
      "Output channel count is too large. requested_output_channel_count= ",
      requested_output_channel_count,
      ". Max=", kValidAmbisonicChannelCounts.back(), "."));
}

absl::Status AmbisonicsConfig::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(GetAmbisonicsMode())));

  switch (GetAmbisonicsMode()) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono:
      return ValidateAndWriteAmbisonicsMono(
          std::get<AmbisonicsMonoConfig>(ambisonics_config), wb);
    case kAmbisonicsModeProjection:
      return ValidateAndWriteAmbisonicsProjection(
          std::get<AmbisonicsProjectionConfig>(ambisonics_config), wb);
    default:
      return absl::OkStatus();
  }
}

absl::Status AmbisonicsConfig::ReadAndValidate(ReadBitBuffer& rb) {
  DecodedUleb128 ambisonics_mode_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(ambisonics_mode_uleb));
  switch (static_cast<AmbisonicsMode>(ambisonics_mode_uleb)) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono: {
      ambisonics_config = AmbisonicsMonoConfig();
      return ReadAndValidateAmbisonicsMonoConfig(
          std::get<AmbisonicsMonoConfig>(ambisonics_config), rb);
    }
    case kAmbisonicsModeProjection: {
      ambisonics_config = AmbisonicsProjectionConfig();
      return ReadAndValidateAmbisonicsProjection(
          std::get<AmbisonicsProjectionConfig>(ambisonics_config), rb);
    }
    default:
      return absl::OkStatus();
  }
}

void AmbisonicsConfig::Print() const {
  ABSL_VLOG(1) << "  ambisonics_config:";
  ABSL_VLOG(1) << "    ambisonics_mode= " << absl::StrCat(GetAmbisonicsMode());
  if (GetAmbisonicsMode() == AmbisonicsConfig::kAmbisonicsModeMono) {
    LogAmbisonicsMonoConfig(std::get<AmbisonicsMonoConfig>(ambisonics_config));
  } else if (GetAmbisonicsMode() ==
             AmbisonicsConfig::kAmbisonicsModeProjection) {
    LogAmbisonicsProjectionConfig(
        std::get<AmbisonicsProjectionConfig>(ambisonics_config));
  }
}

uint8_t AmbisonicsConfig::GetNumSubstreams() const {
  return std::visit([](const auto& config) { return config.substream_count; },
                    ambisonics_config);
}

AmbisonicsConfig::AmbisonicsMode AmbisonicsConfig::GetAmbisonicsMode() const {
  return std::visit(
      [](const auto& config) {
        using Type = std::decay_t<decltype(config)>;
        if constexpr (std::is_same_v<Type, AmbisonicsMonoConfig>) {
          return kAmbisonicsModeMono;
        } else if constexpr (std::is_same_v<Type, AmbisonicsProjectionConfig>) {
          return kAmbisonicsModeProjection;
        }
      },
      ambisonics_config);
}

}  // namespace iamf_tools
