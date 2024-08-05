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
#include "iamf/obu/audio_element.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {

using absl::OkStatus;

namespace {

// Returns the number of elements in the demixing_matrix.
size_t GetNumDemixingMatrixElements(const AmbisonicsProjectionConfig& config) {
  const size_t c = static_cast<size_t>(config.output_channel_count);
  const size_t n = static_cast<size_t>(config.substream_count);
  const size_t m = static_cast<size_t>(config.coupled_substream_count);

  return (n + m) * c;
}

void LogChannelBased(const ScalableChannelLayoutConfig& channel_config) {
  LOG(INFO) << "  scalable_channel_layout_config:";
  LOG(INFO) << "    num_layers= " << absl::StrCat(channel_config.num_layers);
  LOG(INFO) << "    reserved= " << absl::StrCat(channel_config.reserved);
  for (int i = 0; i < channel_config.num_layers; ++i) {
    LOG(INFO) << "    channel_audio_layer_configs[" << i << "]:";
    const auto& channel_audio_layer_config =
        channel_config.channel_audio_layer_configs[i];
    LOG(INFO) << "      loudspeaker_layout= "
              << absl::StrCat(channel_audio_layer_config.loudspeaker_layout);
    LOG(INFO) << "      output_gain_is_present_flag= "
              << absl::StrCat(
                     channel_audio_layer_config.output_gain_is_present_flag);
    LOG(INFO) << "      recon_gain_is_present_flag= "
              << absl::StrCat(
                     channel_audio_layer_config.recon_gain_is_present_flag);
    LOG(INFO) << "      reserved= "
              << absl::StrCat(channel_audio_layer_config.reserved_a);
    LOG(INFO) << "      substream_count= "
              << absl::StrCat(channel_audio_layer_config.substream_count);
    LOG(INFO) << "      coupled_substream_count= "
              << absl::StrCat(
                     channel_audio_layer_config.coupled_substream_count);
    if (channel_audio_layer_config.output_gain_is_present_flag == 1) {
      LOG(INFO) << "      output_gain_flag= "
                << absl::StrCat(channel_audio_layer_config.output_gain_flag);
      LOG(INFO) << "      reserved= "
                << absl::StrCat(channel_audio_layer_config.reserved_b);
      LOG(INFO) << "      output_gain= "
                << channel_audio_layer_config.output_gain;
    }
  }
}

void LogAmbisonicsMonoConfig(const AmbisonicsMonoConfig& mono_config) {
  LOG(INFO) << "  ambisonics_mono_config:";
  LOG(INFO) << "    output_channel_count:"
            << absl::StrCat(mono_config.output_channel_count);
  LOG(INFO) << "    substream_count:"
            << absl::StrCat(mono_config.substream_count);
  std::stringstream channel_mapping_stream;
  for (int c = 0; c < mono_config.output_channel_count; c++) {
    channel_mapping_stream << absl::StrCat(mono_config.channel_mapping[c])
                           << ", ";
  }
  LOG(INFO) << "    channel_mapping: [ " << channel_mapping_stream.str() << "]";
}

void LogAmbisonicsProjectionConfig(
    const AmbisonicsProjectionConfig& projection_config) {
  LOG(INFO) << "  ambisonics_projection_config:";
  LOG(INFO) << "    output_channel_count:"
            << absl::StrCat(projection_config.output_channel_count);
  LOG(INFO) << "    substream_count:"
            << absl::StrCat(projection_config.substream_count);
  LOG(INFO) << "    coupled_substream_count:"
            << absl::StrCat(projection_config.coupled_substream_count);
  std::string demixing_matrix_string;
  for (int i = 0; i < (projection_config.substream_count +
                       projection_config.coupled_substream_count) *
                          projection_config.output_channel_count;
       i++) {
    absl::StrAppend(&demixing_matrix_string,
                    projection_config.demixing_matrix[i], ",");
  }
  LOG(INFO) << "    demixing_matrix: [ " << demixing_matrix_string << "]";
}

void LogSceneBased(const AmbisonicsConfig& ambisonics_config) {
  LOG(INFO) << "  ambisonics_config:";
  LOG(INFO) << "    ambisonics_mode= "
            << absl::StrCat(ambisonics_config.ambisonics_mode);
  if (ambisonics_config.ambisonics_mode ==
      AmbisonicsConfig::kAmbisonicsModeMono) {
    LogAmbisonicsMonoConfig(
        std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config));
  } else if (ambisonics_config.ambisonics_mode ==
             AmbisonicsConfig::kAmbisonicsModeProjection) {
    LogAmbisonicsProjectionConfig(std::get<AmbisonicsProjectionConfig>(
        ambisonics_config.ambisonics_config));
  }
}

// Returns `absl::OkStatus()` if all parameters have a unique
// `param_definition_type` in the OBU. `absl::InvalidArgumentError()`
// otherwise.
absl::Status ValidateUniqueParamDefinitionType(
    const std::vector<AudioElementParam>& audio_element_params) {
  std::vector<ParamDefinition::ParameterDefinitionType>
      collected_param_definition_types;
  collected_param_definition_types.reserve(audio_element_params.size());
  for (const auto& param : audio_element_params) {
    collected_param_definition_types.push_back(param.param_definition_type);
  }

  return ValidateUnique(collected_param_definition_types.begin(),
                        collected_param_definition_types.end(),
                        "audio_element_params");
}

absl::Status ValidateOutputChannelCount(const uint8_t channel_count) {
  uint8_t next_valid_output_channel_count;
  RETURN_IF_NOT_OK(AmbisonicsConfig ::GetNextValidOutputChannelCount(
      channel_count, next_valid_output_channel_count));

  if (next_valid_output_channel_count == channel_count) {
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(absl::StrCat(
      "Invalid Ambisonics output channel_count = ", channel_count));
}

// Writes an element of the `audio_element_params` array of a scalable channel
// `AudioElementObu`.
absl::Status ValidateAndWriteAudioElementParam(const AudioElementParam& param,
                                               WriteBitBuffer& wb) {
  // Write the main portion of the `AudioElementParam`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(
      static_cast<DecodedUleb128>(param.param_definition_type)));

  if (param.param_definition_type ==
      ParamDefinition::kParameterDefinitionMixGain) {
    return absl::InvalidArgumentError(
        "Mix Gain parameter type is explicitly forbidden for "
        "Audio Element OBUs.");
  }
  RETURN_IF_NOT_OK(param.param_definition->ValidateAndWrite(wb));

  return absl::OkStatus();
}

// Writes the `ScalableChannelLayoutConfig` of an `AudioElementObu`.
absl::Status ValidateAndWriteScalableChannelLayout(
    const ScalableChannelLayoutConfig& layout,
    const DecodedUleb128 num_substreams, WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(layout.Validate(num_substreams));

  // Write the main portion of the `ScalableChannelLayoutConfig`.
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(layout.num_layers, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(layout.reserved, 5));

  // Loop to write the `channel_audio_layer_configs` array.
  for (const auto& layer_config : layout.channel_audio_layer_configs) {
    RETURN_IF_NOT_OK(layer_config.Write(wb));
  }

  return absl::OkStatus();
}

// Reads the `ScalableChannelLayoutConfig` of an `AudioElementObu`.
absl::Status ReadAndValidateScalableChannelLayout(
    ScalableChannelLayoutConfig& layout, const DecodedUleb128 num_substreams,
    ReadBitBuffer& rb) {
  // Read the main portion of the `ScalableChannelLayoutConfig`.
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, layout.num_layers));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, layout.reserved));

  for (int i = 0; i < layout.num_layers; ++i) {
    ChannelAudioLayerConfig layer_config;
    RETURN_IF_NOT_OK(layer_config.Read(rb));
    layout.channel_audio_layer_configs.push_back(layer_config);
  }

  RETURN_IF_NOT_OK(layout.Validate(num_substreams));

  return absl::OkStatus();
}

// Writes the `AmbisonicsMonoConfig` of an ambisonics mono `AudioElementObu`.
absl::Status ValidateAndWriteAmbisonicsMono(
    const AmbisonicsMonoConfig& mono_config, DecodedUleb128 num_substreams,
    WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(mono_config.Validate(num_substreams));

  RETURN_IF_NOT_OK(
      wb.WriteUnsignedLiteral(mono_config.output_channel_count, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(mono_config.substream_count, 8));

  RETURN_IF_NOT_OK(wb.WriteUint8Vector(mono_config.channel_mapping));

  return absl::OkStatus();
}

// Writes the `AmbisonicsProjectionConfig` of an ambisonics projection
// `AudioElementObu`.
absl::Status ValidateAndWriteAmbisonicsProjection(
    const AmbisonicsProjectionConfig& projection_config,
    DecodedUleb128 num_substreams, WriteBitBuffer& wb) {
  RETURN_IF_NOT_OK(projection_config.Validate(num_substreams));

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

// Writes the `AmbisonicsConfig` of an ambisonics `AudioElementObu`.
absl::Status ValidateAndWriteAmbisonicsConfig(const AmbisonicsConfig& config,
                                              DecodedUleb128 num_substreams,
                                              WriteBitBuffer& wb) {
  // Write the main portion of the `AmbisonicsConfig`.
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(config.ambisonics_mode)));

  // Write the specific config based on `ambisonics_mode`.
  switch (config.ambisonics_mode) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono:
      return ValidateAndWriteAmbisonicsMono(
          std::get<AmbisonicsMonoConfig>(config.ambisonics_config),
          num_substreams, wb);
    case kAmbisonicsModeProjection:
      return ValidateAndWriteAmbisonicsProjection(
          std::get<AmbisonicsProjectionConfig>(config.ambisonics_config),
          num_substreams, wb);
    default:
      return absl::OkStatus();
  }
}

absl::Status ReadAndValidateAmbisonicsProjection(
    AmbisonicsProjectionConfig& projection_config,
    DecodedUleb128 num_substreams, ReadBitBuffer& rb) {
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
  RETURN_IF_NOT_OK(projection_config.Validate(num_substreams));
  return OkStatus();
}

absl::Status ReadAndValidateAmbisonicsMonoConfig(
    AmbisonicsMonoConfig& mono_config, DecodedUleb128 num_substreams,
    ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, mono_config.output_channel_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, mono_config.substream_count));
  const size_t channel_mapping_size = mono_config.output_channel_count;
  RETURN_IF_NOT_OK(
      rb.ReadUint8Vector(channel_mapping_size, mono_config.channel_mapping));
  RETURN_IF_NOT_OK(mono_config.Validate(num_substreams));
  return OkStatus();
}

// Reads the `AmbisonicsConfig` of an ambisonics `AudioElementObu`.
absl::Status ReadAndValidateAmbisonicsConfig(AmbisonicsConfig& config,
                                             DecodedUleb128 num_substreams,
                                             ReadBitBuffer& rb) {
  DecodedUleb128 ambisonics_mode;
  RETURN_IF_NOT_OK(rb.ReadULeb128(ambisonics_mode));
  config.ambisonics_mode =
      static_cast<AmbisonicsConfig::AmbisonicsMode>(ambisonics_mode);
  switch (config.ambisonics_mode) {
    using enum AmbisonicsConfig::AmbisonicsMode;
    case kAmbisonicsModeMono: {
      config.ambisonics_config = AmbisonicsMonoConfig();
      return ReadAndValidateAmbisonicsMonoConfig(
          std::get<AmbisonicsMonoConfig>(config.ambisonics_config),
          num_substreams, rb);
    }
    case kAmbisonicsModeProjection: {
      config.ambisonics_config = AmbisonicsProjectionConfig();
      return ReadAndValidateAmbisonicsProjection(
          std::get<AmbisonicsProjectionConfig>(config.ambisonics_config),
          num_substreams, rb);
    }
    default:
      return OkStatus();
  }
}

}  // namespace

absl::Status AudioElementParam::ReadAndValidate(uint32_t audio_element_id,
                                                ReadBitBuffer& rb) {
  // Reads the main portion of the `AudioElementParam`.
  DecodedUleb128 param_definition_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(param_definition_type_uleb));
  param_definition_type = static_cast<ParamDefinition::ParameterDefinitionType>(
      param_definition_type_uleb);

  switch (param_definition_type) {
    case ParamDefinition::kParameterDefinitionMixGain: {
      return absl::InvalidArgumentError(
          "Mix Gain parameter type is explicitly forbidden for Audio Element "
          "OBUs.");
    }
    case ParamDefinition::kParameterDefinitionReconGain: {
      auto recon_gain_param_definition =
          std::make_unique<ReconGainParamDefinition>(audio_element_id);
      RETURN_IF_NOT_OK(recon_gain_param_definition->ReadAndValidate(rb));
      param_definition = std::move(recon_gain_param_definition);
      return absl::OkStatus();
    }
    case ParamDefinition::kParameterDefinitionDemixing: {
      auto demixing_param_definition =
          std::make_unique<DemixingParamDefinition>();
      RETURN_IF_NOT_OK(demixing_param_definition->ReadAndValidate(rb));
      param_definition = std::move(demixing_param_definition);
      return absl::OkStatus();
    }
    default:
      auto extended_param_definition =
          std::make_unique<ExtendedParamDefinition>(param_definition_type);
      RETURN_IF_NOT_OK(extended_param_definition->ReadAndValidate(rb));
      param_definition = std::move(extended_param_definition);
      return absl::OkStatus();
  }
}

absl::Status ChannelAudioLayerConfig::Write(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(loudspeaker_layout, 4));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(output_gain_is_present_flag, 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(recon_gain_is_present_flag, 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_a, 2));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(substream_count, 8));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(coupled_substream_count, 8));

  if (output_gain_is_present_flag == 1) {
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(output_gain_flag, 6));
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_b, 2));
    RETURN_IF_NOT_OK(wb.WriteSigned16(output_gain));
  }

  if (loudspeaker_layout == kLayoutExpanded) {
    RETURN_IF_NOT_OK(ValidateHasValue(expanded_loudspeaker_layout,
                                      "`expanded_loudspeaker_layout`"));
    RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(*expanded_loudspeaker_layout, 8));
  }

  return absl::OkStatus();
}

absl::Status ChannelAudioLayerConfig::Read(ReadBitBuffer& rb) {
  uint8_t loudspeaker_layout_uint8;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(4, loudspeaker_layout_uint8));
  loudspeaker_layout = static_cast<ChannelAudioLayerConfig::LoudspeakerLayout>(
      loudspeaker_layout_uint8);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(1, output_gain_is_present_flag));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(1, recon_gain_is_present_flag));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, reserved_a));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, substream_count));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, coupled_substream_count));

  if (output_gain_is_present_flag == 1) {
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(6, output_gain_flag));
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(2, reserved_b));
    RETURN_IF_NOT_OK(rb.ReadSigned16(output_gain));
  }

  if (loudspeaker_layout == kLayoutExpanded) {
    uint8_t expanded_loudspeaker_layout_uint8;
    RETURN_IF_NOT_OK(
        rb.ReadUnsignedLiteral(8, expanded_loudspeaker_layout_uint8));
    expanded_loudspeaker_layout =
        static_cast<ChannelAudioLayerConfig::ExpandedLoudspeakerLayout>(
            expanded_loudspeaker_layout_uint8);
  }

  return absl::OkStatus();
}

absl::Status ScalableChannelLayoutConfig::Validate(
    DecodedUleb128 num_substreams_in_audio_element) const {
  if (num_layers == 0 || num_layers > 6) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected `num_layers` in [1, 6]; got ", num_layers));
  }
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual("channel_audio_layer_configs",
                                           num_layers,
                                           channel_audio_layer_configs.size()));

  // Determine whether any binaural layouts are found and the total number of
  // substreams.
  DecodedUleb128 cumulative_substream_count = 0;
  bool has_binaural_layout = false;
  for (const auto& layer_config : channel_audio_layer_configs) {
    if (layer_config.loudspeaker_layout ==
        ChannelAudioLayerConfig::kLayoutBinaural) {
      has_binaural_layout = true;
    }

    cumulative_substream_count +=
        static_cast<DecodedUleb128>(layer_config.substream_count);
  }

  if (cumulative_substream_count != num_substreams_in_audio_element) {
    return absl::InvalidArgumentError(
        "Cumulative substream count from all layers is not equal to "
        "the `num_substreams` in the OBU.");
  }

  if (has_binaural_layout && num_layers != 1) {
    return absl::InvalidArgumentError(
        "There must be exactly 1 layer if there is a binaural layout.");
  }

  return absl::OkStatus();
}

absl::Status AmbisonicsMonoConfig::Validate(
    DecodedUleb128 num_substreams_in_audio_element) const {
  RETURN_IF_NOT_OK(ValidateOutputChannelCount(output_channel_count));
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "channel_mapping", channel_mapping.size(),
      static_cast<DecodedUleb128>(output_channel_count)));
  if (substream_count > output_channel_count) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected substream_count=", substream_count,
                     " to be less than or equal to `output_channel_count`=",
                     output_channel_count, "."));
  }
  if (num_substreams_in_audio_element != substream_count) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected substream_count=", substream_count,
                     " to be equal to num_substreams_in_audio_element=",
                     num_substreams_in_audio_element, "."));
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

absl::Status AmbisonicsProjectionConfig::Validate(
    DecodedUleb128 num_substreams_in_audio_element) const {
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
  if (num_substreams_in_audio_element != substream_count) {
    return absl::InvalidArgumentError(
        absl::StrCat("Expected substream_count= ", substream_count,
                     " to be equal to num_substreams_in_audio_element= ",
                     num_substreams_in_audio_element, "."));
  }

  const size_t expected_num_elements = GetNumDemixingMatrixElements(*this);
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "demixing_matrix", demixing_matrix.size(), expected_num_elements));

  return absl::OkStatus();
}

absl::Status AmbisonicsConfig::GetNextValidOutputChannelCount(
    uint8_t requested_output_channel_count,
    uint8_t& next_valid_output_channel_count) {
  // Valid values are `(1+n)^2`, for integer `n` in the range [0, 14].
  static constexpr auto kValidAmbisonicChannelCounts = []() -> auto {
    std::array<uint8_t, 15> channel_count_i;
    for (int i = 0; i < channel_count_i.size(); ++i) {
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

AudioElementObu::AudioElementObu(const ObuHeader& header,
                                 DecodedUleb128 audio_element_id,
                                 AudioElementType audio_element_type,
                                 const uint8_t reserved,
                                 DecodedUleb128 codec_config_id)
    : ObuBase(header, kObuIaAudioElement),
      num_substreams_(0),
      num_parameters_(0),
      audio_element_id_(audio_element_id),
      audio_element_type_(audio_element_type),
      reserved_(reserved),
      codec_config_id_(codec_config_id) {}

absl::StatusOr<AudioElementObu> AudioElementObu::CreateFromBuffer(
    const ObuHeader& header, ReadBitBuffer& rb) {
  AudioElementObu audio_element_obu(header);
  RETURN_IF_NOT_OK(audio_element_obu.ReadAndValidatePayload(rb));
  return audio_element_obu;
}

AudioElementObu AudioElementObu::Clone(const AudioElementObu& other) {
  AudioElementObu new_obu(other.header_, other.audio_element_id_,
                          other.audio_element_type_, other.reserved_,
                          other.codec_config_id_);
  new_obu.InitializeAudioSubstreams(other.num_substreams_);
  new_obu.audio_substream_ids_ = other.audio_substream_ids_;
  new_obu.InitializeParams(other.num_parameters_);
  for (int i = 0; i < other.audio_element_params_.size(); ++i) {
    new_obu.audio_element_params_[i].param_definition_type =
        other.audio_element_params_[i].param_definition_type;
    // Clone the underlying specific parameter definition.
    new_obu.audio_element_params_[i].param_definition =
        other.audio_element_params_[i].param_definition->Clone();
  }
  new_obu.config_ = other.config_;

  return new_obu;
}

void AudioElementObu::InitializeAudioSubstreams(DecodedUleb128 num_substreams) {
  num_substreams_ = num_substreams;
  audio_substream_ids_.resize(static_cast<size_t>(num_substreams));
}

void AudioElementObu::InitializeParams(const DecodedUleb128 num_parameters) {
  num_parameters_ = num_parameters;
  audio_element_params_.resize(static_cast<size_t>(num_parameters));
}

// Initializes the scalable channel portion of an `AudioElementObu`.
absl::Status AudioElementObu::InitializeScalableChannelLayout(
    const uint32_t num_layers, const uint32_t reserved) {
  // Validate the audio element type is correct.
  if (audio_element_type_ != kAudioElementChannelBased) {
    return absl::InvalidArgumentError(absl::StrCat(
        "`InitializeScalableChannelLayout()` can only be called ",
        "when `audio_element_type_ == kAudioElementChannelBased`, ", "but got ",
        audio_element_type_));
  }

  ScalableChannelLayoutConfig config;
  RETURN_IF_NOT_OK(Uint32ToUint8(num_layers, config.num_layers));
  RETURN_IF_NOT_OK(Uint32ToUint8(reserved, config.reserved));
  config.channel_audio_layer_configs.resize(num_layers);
  config_ = config;
  return absl::OkStatus();
}

// Initializes the ambisonics mono portion of an `AudioElementObu`.
absl::Status AudioElementObu::InitializeAmbisonicsMono(
    const uint32_t output_channel_count, const uint32_t substream_count) {
  // Validate the audio element type and ambisonics mode are correct.
  if (audio_element_type_ != kAudioElementSceneBased) {
    return absl::InvalidArgumentError(
        absl::StrCat("`InitializeAmbisonicsMono()` can only be called ",
                     "when `audio_element_type_ == kAudioElementSceneBased`, ",
                     "but got ", audio_element_type_));
  }

  AmbisonicsConfig config;
  config.ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeMono;

  AmbisonicsMonoConfig mono_config;
  RETURN_IF_NOT_OK(
      Uint32ToUint8(output_channel_count, mono_config.output_channel_count));
  RETURN_IF_NOT_OK(Uint32ToUint8(substream_count, mono_config.substream_count));
  mono_config.channel_mapping.resize(output_channel_count);
  config.ambisonics_config = mono_config;
  config_ = config;

  return absl::OkStatus();
}

// Initializes the ambisonics projection portion of an `AudioElementObu`.
absl::Status AudioElementObu::InitializeAmbisonicsProjection(
    const uint32_t output_channel_count, const uint32_t substream_count,
    const uint32_t coupled_substream_count) {
  // Validate the audio element type and ambisonics mode are correct.
  if (audio_element_type_ != kAudioElementSceneBased) {
    return absl::InvalidArgumentError(
        absl::StrCat("`InitializeAmbisonicsProjection()` can only be called ",
                     "when `audio_element_type_ == kAudioElementSceneBased`, ",
                     "but got ", audio_element_type_));
  }

  AmbisonicsConfig config;
  config.ambisonics_mode = AmbisonicsConfig::kAmbisonicsModeProjection;

  AmbisonicsProjectionConfig projection_config;
  RETURN_IF_NOT_OK(Uint32ToUint8(output_channel_count,
                                 projection_config.output_channel_count));
  RETURN_IF_NOT_OK(
      Uint32ToUint8(substream_count, projection_config.substream_count));
  RETURN_IF_NOT_OK(Uint32ToUint8(coupled_substream_count,
                                 projection_config.coupled_substream_count));
  const size_t num_elements = GetNumDemixingMatrixElements(projection_config);
  projection_config.demixing_matrix.resize(num_elements);
  config.ambisonics_config = projection_config;
  config_ = config;

  return absl::OkStatus();
}

void AudioElementObu::InitializeExtensionConfig(
    const DecodedUleb128 audio_element_config_size) {
  config_ =
      ExtensionConfig{.audio_element_config_size = audio_element_config_size};
}

void AudioElementObu::PrintObu() const {
  LOG(INFO) << "Audio Element OBU:";
  LOG(INFO) << "  audio_element_id= " << audio_element_id_;
  LOG(INFO) << "  audio_element_type= " << absl::StrCat(audio_element_type_);
  LOG(INFO) << "  reserved= " << absl::StrCat(reserved_);
  LOG(INFO) << "  codec_config_id= " << codec_config_id_;
  LOG(INFO) << "  num_substreams= " << num_substreams_;
  for (int i = 0; i < num_substreams_; ++i) {
    const auto& substream_id = audio_substream_ids_[i];
    LOG(INFO) << "  audio_substream_ids[" << i << "]= " << substream_id;
  }
  LOG(INFO) << "  num_parameters= " << num_parameters_;
  for (int i = 0; i < num_parameters_; ++i) {
    LOG(INFO) << "  params[" << i << "]";
    LOG(INFO) << "    param_definition_type= "
              << absl::StrCat(audio_element_params_[i].param_definition_type);
    audio_element_params_[i].param_definition->Print();
  }
  if (audio_element_type_ == kAudioElementChannelBased) {
    LogChannelBased(std::get<ScalableChannelLayoutConfig>(config_));
  } else if (audio_element_type_ == kAudioElementSceneBased) {
    LogSceneBased(std::get<AmbisonicsConfig>(config_));
  }
}

absl::Status AudioElementObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(ValidateUniqueParamDefinitionType(audio_element_params_));

  RETURN_IF_NOT_OK(wb.WriteUleb128(audio_element_id_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(audio_element_type_, 3));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_, 5));
  RETURN_IF_NOT_OK(wb.WriteUleb128(codec_config_id_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(num_substreams_));

  // Loop to write the audio substream IDs portion of the obu.
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "audio_substream_ids", audio_substream_ids_.size(), num_substreams_));
  for (const auto& audio_substream_id : audio_substream_ids_) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(audio_substream_id));
  }

  RETURN_IF_NOT_OK(wb.WriteUleb128(num_parameters_));

  // Loop to write the parameter portion of the obu.
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "num_parameters", audio_element_params_.size(), num_parameters_));
  for (const auto& audio_element_param : audio_element_params_) {
    RETURN_IF_NOT_OK(
        ValidateAndWriteAudioElementParam(audio_element_param, wb));
  }

  // Write the specific `audio_element_type`'s config.
  switch (audio_element_type_) {
    case kAudioElementChannelBased:
      return ValidateAndWriteScalableChannelLayout(
          std::get<ScalableChannelLayoutConfig>(config_), num_substreams_, wb);
    case kAudioElementSceneBased:
      return ValidateAndWriteAmbisonicsConfig(
          std::get<AmbisonicsConfig>(config_), num_substreams_, wb);
    default: {
      const auto& extension_config = std::get<ExtensionConfig>(config_);
      RETURN_IF_NOT_OK(
          wb.WriteUleb128(extension_config.audio_element_config_size));
      RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
          "audio_element_config_bytes",
          extension_config.audio_element_config_bytes.size(),
          extension_config.audio_element_config_size));
      RETURN_IF_NOT_OK(
          wb.WriteUint8Vector(extension_config.audio_element_config_bytes));

      return absl::OkStatus();
    }
  }

  return absl::OkStatus();
}

absl::Status AudioElementObu::ReadAndValidatePayload(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadULeb128(audio_element_id_));
  uint8_t audio_element_type;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(3, audio_element_type));
  audio_element_type_ = static_cast<AudioElementType>(audio_element_type);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(5, reserved_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(codec_config_id_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_substreams_));

  // Loop to read the audio substream IDs portion of the obu.
  for (int i = 0; i < num_substreams_; ++i) {
    DecodedUleb128 audio_substream_id;
    RETURN_IF_NOT_OK(rb.ReadULeb128(audio_substream_id));
    audio_substream_ids_.push_back(audio_substream_id);
  }
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "audio_substream_ids", audio_substream_ids_.size(), num_substreams_));

  RETURN_IF_NOT_OK(rb.ReadULeb128(num_parameters_));

  // Loop to read the parameter portion of the obu.
  for (int i = 0; i < num_parameters_; ++i) {
    AudioElementParam audio_element_param;
    RETURN_IF_NOT_OK(
        audio_element_param.ReadAndValidate(audio_element_id_, rb));
    audio_element_params_.push_back(std::move(audio_element_param));
  }
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
      "num_parameters", audio_element_params_.size(), num_parameters_));

  // Write the specific `audio_element_type`'s config.
  switch (audio_element_type_) {
    case kAudioElementChannelBased:
      config_ = ScalableChannelLayoutConfig();
      return ReadAndValidateScalableChannelLayout(
          std::get<ScalableChannelLayoutConfig>(config_), num_substreams_, rb);
    case kAudioElementSceneBased:
      config_ = AmbisonicsConfig();
      return ReadAndValidateAmbisonicsConfig(
          std::get<AmbisonicsConfig>(config_), num_substreams_, rb);
    default: {
      ExtensionConfig extension_config;
      RETURN_IF_NOT_OK(
          rb.ReadULeb128(extension_config.audio_element_config_size));
      for (int i = 0; i < extension_config.audio_element_config_size; ++i) {
        uint8_t config_bytes;
        RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, config_bytes));
        extension_config.audio_element_config_bytes.push_back(config_bytes);
      }

      RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
          "audio_element_config_bytes",
          extension_config.audio_element_config_bytes.size(),
          extension_config.audio_element_config_size));

      return absl::OkStatus();
    }
  }
  RETURN_IF_NOT_OK(ValidateUniqueParamDefinitionType(audio_element_params_));
  return absl::OkStatus();
}

}  // namespace iamf_tools
