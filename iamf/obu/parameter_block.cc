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
#include "iamf/obu/parameter_block.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/macros.h"
#include "iamf/common/obu_util.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"

namespace iamf_tools {
namespace {

void PrintMixGainParameterData(
    const MixGainParameterData& mix_gain_param_data) {
  LOG(INFO) << "     animation_type= "
            << absl::StrCat(mix_gain_param_data.animation_type);
  switch (mix_gain_param_data.animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep:
      std::get<AnimationStepInt16>(mix_gain_param_data.param_data).Print();
      break;
    case kAnimateLinear:
      std::get<AnimationLinearInt16>(mix_gain_param_data.param_data).Print();
      break;
    case kAnimateBezier:
      std::get<AnimationBezierInt16>(mix_gain_param_data.param_data).Print();
      break;
    default:
      LOG(ERROR) << "Unknown animation type: "
                 << absl::StrCat(mix_gain_param_data.animation_type);
  }
}

void PrintDemixingInfoParameterData(
    const DemixingInfoParameterData& demixing_param_data) {
  LOG(INFO) << "    dmixp_mode= "
            << absl::StrCat(demixing_param_data.dmixp_mode);
  LOG(INFO) << "    reserved= " << absl::StrCat(demixing_param_data.reserved);
}

void PrintReconGainInfoParameterData(
    const ReconGainInfoParameterData& recon_gain_info_param_data,
    const int num_layers) {
  for (int l = 0; l < num_layers; l++) {
    const auto& recon_gain_element =
        recon_gain_info_param_data.recon_gain_elements[l];
    LOG(INFO) << "    recon_gain_elements[" << l << "]:";
    LOG(INFO) << "      recon_gain_flag= "
              << recon_gain_element.recon_gain_flag;
    for (int b = 0; b < recon_gain_element.recon_gain.size(); b++) {
      LOG(INFO) << "      recon_gain[" << b
                << "]= " << absl::StrCat(recon_gain_element.recon_gain[b]);
    }
  }
}

// Writes a `MixGainParameterData` within a Parameter Block OBU subblock.
absl::Status WriteMixGainParamData(const MixGainParameterData& param,
                                   WriteBitBuffer& wb) {
  // Write the `animation_type` field.
  RETURN_IF_NOT_OK(
      wb.WriteUleb128(static_cast<DecodedUleb128>(param.animation_type)));

  // Write the fields dependent on the `animation_type` field.
  switch (param.animation_type) {
    using enum MixGainParameterData::AnimationType;
    case kAnimateStep:
      RETURN_IF_NOT_OK(
          std::get<AnimationStepInt16>(param.param_data).ValidateAndWrite(wb));
      break;
    case kAnimateLinear:
      RETURN_IF_NOT_OK(std::get<AnimationLinearInt16>(param.param_data)
                           .ValidateAndWrite(wb));
      break;
    case kAnimateBezier:
      RETURN_IF_NOT_OK(std::get<AnimationBezierInt16>(param.param_data)
                           .ValidateAndWrite(wb));
      break;
  }
  return absl::OkStatus();
}

// Writes a `ReconGainInfoParameterData` within a Parameter Block OBU subblock.
absl::Status WriteReconGainInfoParameterData(
    const std::vector<bool>& recon_gain_is_present_flags,
    const ReconGainInfoParameterData& param, WriteBitBuffer& wb) {
  for (int i = 0; i < recon_gain_is_present_flags.size(); i++) {
    // Each layer depends on the `recon_gain_is_present_flags` within the
    // associated Audio Element OBU.
    if (!recon_gain_is_present_flags[i]) continue;

    const ReconGainElement* element = &param.recon_gain_elements[i];

    RETURN_IF_NOT_OK(wb.WriteUleb128(element->recon_gain_flag));

    const DecodedUleb128 recon_gain_flag = element->recon_gain_flag;
    DecodedUleb128 mask = 1;

    // Apply bitmask to examine each bit in the flag. Only write elements with
    // the flag implying they should be written.
    for (const auto& recon_gain : element->recon_gain) {
      if (recon_gain_flag & mask) {
        RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(recon_gain, 8));
      }
      mask <<= 1;
    }
  }

  return absl::OkStatus();
}

}  // namespace

void AnimationStepInt16::Print() const {
  LOG(INFO) << "     // Step";
  LOG(INFO) << "     start_point_value= " << start_point_value;
}

absl::Status AnimationStepInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  return absl::OkStatus();
}

absl::Status AnimationStepInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  return absl::OkStatus();
}

void AnimationLinearInt16::Print() const {
  LOG(INFO) << "     // Linear";
  LOG(INFO) << "     start_point_value= " << start_point_value;
  LOG(INFO) << "     end_point_value= " << end_point_value;
}

absl::Status AnimationLinearInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(end_point_value));
  return absl::OkStatus();
}

absl::Status AnimationLinearInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(end_point_value));
  return absl::OkStatus();
}

void AnimationBezierInt16::Print() const {
  LOG(INFO) << "     // Bezier";
  LOG(INFO) << "     start_point_value= " << start_point_value;
  LOG(INFO) << "     end_point_value= " << end_point_value;
  LOG(INFO) << "     control_point_value= " << control_point_value;
  LOG(INFO) << "     control_point_relative_time= "
            << control_point_relative_time;
}

absl::Status AnimationBezierInt16::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteSigned16(start_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(end_point_value));
  RETURN_IF_NOT_OK(wb.WriteSigned16(control_point_value));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(control_point_relative_time, 8));
  return absl::OkStatus();
}

absl::Status AnimationBezierInt16::ReadAndValidate(ReadBitBuffer& rb) {
  RETURN_IF_NOT_OK(rb.ReadSigned16(start_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(end_point_value));
  RETURN_IF_NOT_OK(rb.ReadSigned16(control_point_value));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, control_point_relative_time));

  return absl::OkStatus();
}

absl::Status MixGainParameterData::ReadAndValidate(ReadBitBuffer& rb) {
  DecodedUleb128 animation_type_uleb;
  RETURN_IF_NOT_OK(rb.ReadULeb128(animation_type_uleb));
  animation_type = static_cast<AnimationType>(animation_type_uleb);
  switch (animation_type) {
    using enum AnimationType;
    case kAnimateStep:
      AnimationStepInt16 step_param_data;
      RETURN_IF_NOT_OK(step_param_data.ReadAndValidate(rb));
      param_data = step_param_data;
      return absl::OkStatus();
    case kAnimateLinear:
      AnimationLinearInt16 linear_param_data;
      RETURN_IF_NOT_OK(linear_param_data.ReadAndValidate(rb));
      param_data = linear_param_data;
      return absl::OkStatus();
    case kAnimateBezier:
      AnimationBezierInt16 bezier_param_data;
      RETURN_IF_NOT_OK(bezier_param_data.ReadAndValidate(rb));
      param_data = bezier_param_data;
      return absl::OkStatus();
    default:
      return absl::UnimplementedError(
          absl::StrCat("Unknown animation type= ", animation_type_uleb));
  }
}

absl::Status ReconGainInfoParameterData::ReadAndValidate(
    const std::vector<bool>& recon_gain_is_present_flags, ReadBitBuffer& rb) {
  for (int i = 0; i < recon_gain_is_present_flags.size(); i++) {
    // Each layer depends on the `recon_gain_is_present_flags` within the
    // associated Audio Element OBU. The size of `recon_gain_is_present_flags`
    // is equal to the number of layers.
    if (!recon_gain_is_present_flags[i]) continue;

    ReconGainElement element;
    RETURN_IF_NOT_OK(rb.ReadULeb128(element.recon_gain_flag));

    const DecodedUleb128 recon_gain_flag = element.recon_gain_flag;
    DecodedUleb128 mask = 1;

    // Apply bitmask to examine each bit in the flag. Only read elements with
    // the flag implying they should be read.
    for (int j = 0; j < element.recon_gain.size(); j++) {
      if (recon_gain_flag & mask) {
        RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, element.recon_gain[j]));
      } else {
        element.recon_gain[j] = 0;
      }
      mask <<= 1;
    }
    recon_gain_elements.push_back(element);
  }

  return absl::OkStatus();
}

absl::StatusOr<ParameterBlockObu> ParameterBlockObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size,
    absl::flat_hash_map<DecodedUleb128, PerIdParameterMetadata>&
        parameter_id_to_metadata,
    ReadBitBuffer& rb) {
  DecodedUleb128 parameter_id;
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_id));
  auto it = parameter_id_to_metadata.find(parameter_id);
  if (it == parameter_id_to_metadata.end()) {
    return absl::InvalidArgumentError(
        "Found a stray parameter block OBU (no matching parameter "
        "definition).");
  }

  ParameterBlockObu parameter_block_obu(header, parameter_id, it->second);
  RETURN_IF_NOT_OK(
      parameter_block_obu.ReadAndValidatePayload(payload_size, rb));
  return parameter_block_obu;
}

ParameterBlockObu::ParameterBlockObu(const ObuHeader& header,
                                     DecodedUleb128 parameter_id,
                                     PerIdParameterMetadata& metadata)
    : ObuBase(header, kObuIaParameterBlock),
      parameter_id_(parameter_id),
      metadata_(metadata) {}

absl::Status ParameterBlockObu::InterpolateMixGainParameterData(
    const MixGainParameterData& mix_gain_parameter_data, int32_t start_time,
    int32_t end_time, int32_t target_time, int16_t& target_mix_gain) {
  return InterpolateMixGainValue(
      mix_gain_parameter_data.animation_type,
      MixGainParameterData::kAnimateStep, MixGainParameterData::kAnimateLinear,
      MixGainParameterData::kAnimateBezier,
      [&mix_gain_parameter_data]() {
        return std::get<AnimationStepInt16>(mix_gain_parameter_data.param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationLinearInt16>(
                   mix_gain_parameter_data.param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationLinearInt16>(
                   mix_gain_parameter_data.param_data)
            .end_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data.param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data.param_data)
            .end_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data.param_data)
            .control_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data.param_data)
            .control_point_relative_time;
      },
      start_time, end_time, target_time, target_mix_gain);
}

DecodedUleb128 ParameterBlockObu::GetDuration() const {
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    return duration_;
  } else {
    return metadata_.param_definition.duration_;
  }
}

DecodedUleb128 ParameterBlockObu::GetConstantSubblockDuration() const {
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    return constant_subblock_duration_;
  } else {
    return metadata_.param_definition.constant_subblock_duration_;
  }
}

DecodedUleb128 ParameterBlockObu::GetNumSubblocks() const {
  const DecodedUleb128 duration = GetDuration();
  const DecodedUleb128 constant_subblock_duration =
      GetConstantSubblockDuration();

  DecodedUleb128 num_subblocks;
  if (constant_subblock_duration != 0) {
    // Get the implicit value of `num_subblocks` using `ceil(duration /
    // constant_subblock_duration)`. Integer division with ceil is equivalent.
    num_subblocks = duration / constant_subblock_duration;
    if (duration % constant_subblock_duration != 0) {
      num_subblocks += 1;
    }
    return num_subblocks;
  }

  // The subblocks is explicitly in the OBU or `metadata_`.
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    num_subblocks = num_subblocks_;
  } else {
    num_subblocks = metadata_.param_definition.GetNumSubblocks();
  }
  return num_subblocks;
}

absl::StatusOr<DecodedUleb128> ParameterBlockObu::GetSubblockDuration(
    int subblock_index) const {
  return GetParameterSubblockDuration<DecodedUleb128>(
      subblock_index, GetNumSubblocks(), GetConstantSubblockDuration(),
      GetDuration(), metadata_.param_definition.param_definition_mode_,
      [this](int i) { return this->subblocks_[i].subblock_duration; },
      [this](int i) {
        return this->metadata_.param_definition.GetSubblockDuration(i);
      });
}

absl::Status ParameterBlockObu::SetSubblockDuration(int subblock_index,
                                                    DecodedUleb128 duration) {
  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  if (subblock_index > num_subblocks) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Setting subblock duration for subblock_index = ", subblock_index,
        " but there are only num_subblocks = ", num_subblocks));
  }
  const DecodedUleb128 constant_subblock_duration =
      GetConstantSubblockDuration();

  // Default the value in the Parameter block to 0.
  subblocks_[subblock_index].subblock_duration = 0;

  if (constant_subblock_duration == 0) {
    if (metadata_.param_definition.param_definition_mode_ == 1) {
      // Overwrite the default value in the parameter block.
      subblocks_[subblock_index].subblock_duration = duration;

    } else {
      // Set the duration in the metadata_.
      RETURN_IF_NOT_OK(metadata_.param_definition.SetSubblockDuration(
          subblock_index, duration));
    }
  }
  return absl::OkStatus();
}

absl::Status ParameterBlockObu::GetMixGain(int32_t obu_relative_time,
                                           int16_t& mix_gain) const {
  if (metadata_.param_definition_type !=
      ParamDefinition::kParameterDefinitionMixGain) {
    return absl::InvalidArgumentError("Expected Mix Gain Parameter Definition");
  }

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  int target_subblock_index = -1;
  int32_t target_subblock_start_time = -1;
  int32_t subblock_relative_start_time = 0;
  int32_t subblock_relative_end_time = 0;
  for (int i = 0; i < num_subblocks; i++) {
    const auto subblock_duration = GetSubblockDuration(i);
    if (!subblock_duration.ok()) {
      return subblock_duration.status();
    }

    if (subblock_relative_start_time <= obu_relative_time &&
        obu_relative_time <
            (subblock_relative_start_time + subblock_duration.value())) {
      target_subblock_index = i;
      target_subblock_start_time = subblock_relative_start_time;
      subblock_relative_end_time =
          subblock_relative_start_time + subblock_duration.value();
      break;
    }
    subblock_relative_start_time += subblock_duration.value();
  }

  if (target_subblock_index == -1) {
    return absl::UnknownError(
        absl::StrCat("Trying to get mix gain for target_subblock_index = -1, "
                     "with num_subblocks = ",
                     num_subblocks));
  }

  RETURN_IF_NOT_OK(InterpolateMixGainParameterData(
      std::get<MixGainParameterData>(
          subblocks_[target_subblock_index].param_data),
      subblock_relative_start_time, subblock_relative_end_time,
      obu_relative_time, mix_gain));

  return absl::OkStatus();
}

absl::Status ParameterBlockObu::InitializeSubblocks(
    DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration,
    DecodedUleb128 num_subblocks) {
  SetDuration(duration);
  SetConstantSubblockDuration(constant_subblock_duration);
  SetNumSubblocks(num_subblocks);

  subblocks_.resize(static_cast<size_t>(GetNumSubblocks()));

  init_status_ = absl::OkStatus();
  return init_status_;
}

absl::Status ParameterBlockObu::InitializeSubblocks() {
  if (metadata_.param_definition.param_definition_mode_ != 0) {
    init_status_ = absl::InvalidArgumentError(
        "InitializeSubblocks() without input arguments should only "
        "be called when `param_definition_mode_ == 0`");
    return init_status_;
  }
  subblocks_.resize(static_cast<size_t>(GetNumSubblocks()));
  init_status_ = absl::OkStatus();
  return absl::OkStatus();
}

void ParameterBlockObu::PrintObu() const {
  if (!init_status_.ok()) {
    LOG(ERROR) << "This OBU failed to initialize with error= " << init_status_;
  }

  LOG(INFO) << "Parameter Block OBU:";
  LOG(INFO) << "  parameter_id= " << parameter_id_;
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    LOG(INFO) << "  duration= " << duration_;
    LOG(INFO) << "  constant_subblock_duration= "
              << constant_subblock_duration_;
    if (constant_subblock_duration_ == 0) {
      LOG(INFO) << "  num_subblocks= " << num_subblocks_;
    }
  }

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  const bool include_subblock_duration =
      metadata_.param_definition.param_definition_mode_ == 1 &&
      constant_subblock_duration_ == 0;
  for (int i = 0; i < num_subblocks; i++) {
    LOG(INFO) << "  subblocks[" << i << "]";
    const auto& subblock = subblocks_[i];
    if (include_subblock_duration) {
      LOG(INFO) << "    subblock_duration= " << subblock.subblock_duration;
    }

    LOG(INFO) << "    // param_definition_type= "
              << metadata_.param_definition_type;
    LOG(INFO) << "    // param_definition:";
    metadata_.param_definition.Print();
    switch (metadata_.param_definition_type) {
      using enum ParamDefinition::ParameterDefinitionType;
      case kParameterDefinitionMixGain:
        PrintMixGainParameterData(
            std::get<MixGainParameterData>(subblock.param_data));
        break;
      case kParameterDefinitionDemixing:
        PrintDemixingInfoParameterData(
            std::get<DemixingInfoParameterData>(subblock.param_data));
        break;
      case kParameterDefinitionReconGain:
        PrintReconGainInfoParameterData(
            std::get<ReconGainInfoParameterData>(subblock.param_data),
            static_cast<int>(metadata_.num_layers));
        break;
      default:
        LOG(ERROR) << "Unknown parameter definition type: "
                   << absl::StrCat(metadata_.param_definition_type) << ".";
    }
  }
}

void ParameterBlockObu::SetDuration(DecodedUleb128 duration) {
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    duration_ = duration;
  } else {
    metadata_.param_definition.duration_ = duration;
  }
}

void ParameterBlockObu::SetConstantSubblockDuration(
    DecodedUleb128 constant_subblock_duration) {
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    constant_subblock_duration_ = constant_subblock_duration;
  } else {
    metadata_.param_definition.constant_subblock_duration_ =
        constant_subblock_duration;
  }
}

void ParameterBlockObu::SetNumSubblocks(DecodedUleb128 num_subblocks) {
  const DecodedUleb128 constant_subblock_duration =
      GetConstantSubblockDuration();
  if (constant_subblock_duration != 0) {
    // Nothing to do. The field is implicit.
    return;
  }

  // Set `num_subblocks_` explicitly in the OBU or metadata_.
  if (metadata_.param_definition.param_definition_mode_ == 1) {
    num_subblocks_ = num_subblocks;
  } else {
    metadata_.param_definition.InitializeSubblockDurations(num_subblocks);
  }
}

absl::Status ParameterBlockObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  if (!init_status_.ok()) {
    LOG(ERROR) << "Cannot write a Parameter Block OBU whose initialization "
               << "did not run successfully. init_status_= " << init_status_;
    return init_status_;
  }

  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_id_));

  // Initialized from OBU or `metadata_` depending on
  // `param_definition_mode_`.
  // Write fields that are conditional on `param_definition_mode_`.
  if (metadata_.param_definition.param_definition_mode_) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(duration_));
    RETURN_IF_NOT_OK(wb.WriteUleb128(constant_subblock_duration_));
    if (constant_subblock_duration_ == 0) {
      RETURN_IF_NOT_OK(wb.WriteUleb128(num_subblocks_));
    }
  }

  // Validate the associated `param_definition`.
  RETURN_IF_NOT_OK(metadata_.param_definition.Validate());

  int64_t total_subblock_durations = 0;
  bool validate_total_subblock_durations = false;

  // Loop through to write the `subblocks_` vector.
  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  for (int i = 0; i < num_subblocks; i++) {
    // `subblock_duration` is conditionally included based on
    // `param_definition_mode_` and `constant_subblock_duration_`.
    if (metadata_.param_definition.param_definition_mode_ &&
        constant_subblock_duration_ == 0) {
      RETURN_IF_NOT_OK(wb.WriteUleb128(subblocks_[i].subblock_duration));
      validate_total_subblock_durations = true;
      total_subblock_durations += subblocks_[i].subblock_duration;
    }

    // Write the specific parameter data depending on `param_definition_type`.
    const auto& param_data = subblocks_[i].param_data;
    switch (metadata_.param_definition_type) {
      using enum ParamDefinition::ParameterDefinitionType;
      case kParameterDefinitionMixGain:
        RETURN_IF_NOT_OK(WriteMixGainParamData(
            std::get<MixGainParameterData>(param_data), wb));
        break;
      case kParameterDefinitionDemixing:
        RETURN_IF_NOT_OK(
            std::get<DemixingInfoParameterData>(param_data).Write(wb));
        break;
      case kParameterDefinitionReconGain:
        RETURN_IF_NOT_OK(WriteReconGainInfoParameterData(
            metadata_.recon_gain_is_present_flags,
            std::get<ReconGainInfoParameterData>(param_data), wb));
        break;
      default: {
        // Write the `extension_parameter_data`.
        const auto& extension_parameter_data =
            std::get<ExtensionParameterData>(param_data);
        RETURN_IF_NOT_OK(
            wb.WriteUleb128(extension_parameter_data.parameter_data_size));
        RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
            "parameter_data_bytes",
            extension_parameter_data.parameter_data_bytes.size(),
            extension_parameter_data.parameter_data_size));
        RETURN_IF_NOT_OK(
            wb.WriteUint8Vector(extension_parameter_data.parameter_data_bytes));
        break;
      }
    }
  }

  // Check total duration matches expected duration.
  if (validate_total_subblock_durations &&
      total_subblock_durations != duration_) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Expected total_subblock_durations = ", total_subblock_durations,
        " to match the expected duration_ = ", duration_));
  }

  return absl::OkStatus();
}

absl::Status ParameterBlockObu::ReadAndValidatePayloadDerived(
    int64_t /*payload_size*/, ReadBitBuffer& rb) {
  // Validate the associated `param_definition`.
  RETURN_IF_NOT_OK(metadata_.param_definition.Validate());

  if (metadata_.param_definition.param_definition_mode_) {
    RETURN_IF_NOT_OK(rb.ReadULeb128(duration_));
    RETURN_IF_NOT_OK(rb.ReadULeb128(constant_subblock_duration_));
    if (constant_subblock_duration_ == 0) {
      RETURN_IF_NOT_OK(rb.ReadULeb128(num_subblocks_));
    }
  }

  const auto num_subblocks = GetNumSubblocks();
  subblocks_.resize(num_subblocks);

  // `subblock_duration` is conditionally included based on
  // `param_definition_mode_` and `constant_subblock_duration_`.
  const bool include_subblock_duration =
      metadata_.param_definition.param_definition_mode_ &&
      constant_subblock_duration_ == 0;

  int64_t total_subblock_durations = 0;
  const bool validate_total_subblock_durations = include_subblock_duration;

  for (int i = 0; i < num_subblocks; i++) {
    if (include_subblock_duration) {
      RETURN_IF_NOT_OK(rb.ReadULeb128(subblocks_[i].subblock_duration));
      total_subblock_durations += subblocks_[i].subblock_duration;
    }

    auto param_definition_type = metadata_.param_definition.GetType();
    if (!param_definition_type.has_value()) {
      return absl::InvalidArgumentError("Unknown parameter definition type.");
    } else if (*param_definition_type ==
               ParamDefinition::kParameterDefinitionMixGain) {
      MixGainParameterData mix_gain_param_data;
      RETURN_IF_NOT_OK(mix_gain_param_data.ReadAndValidate(rb));
      subblocks_[i].param_data = mix_gain_param_data;
    } else if (*param_definition_type ==
               ParamDefinition::kParameterDefinitionReconGain) {
      ReconGainInfoParameterData recon_gain_info_param_data;
      RETURN_IF_NOT_OK(recon_gain_info_param_data.ReadAndValidate(
          metadata_.recon_gain_is_present_flags, rb));
      subblocks_[i].param_data = recon_gain_info_param_data;
    } else if (*param_definition_type ==
               ParamDefinition::kParameterDefinitionDemixing) {
      DemixingInfoParameterData demixing_info_param_data;
      RETURN_IF_NOT_OK(demixing_info_param_data.Read(rb));
      subblocks_[i].param_data = demixing_info_param_data;
    } else {
      return absl::UnimplementedError("Unsupported parameter definition type.");
    }
  }

  if (validate_total_subblock_durations &&
      total_subblock_durations != duration_) {
    return absl::InvalidArgumentError(
        "Subblock durations do not match the total duration.");
  }

  init_status_ = absl::OkStatus();
  return absl::OkStatus();
}

}  // namespace iamf_tools
