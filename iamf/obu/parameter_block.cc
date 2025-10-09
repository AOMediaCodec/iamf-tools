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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/obu_util.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

absl::Status ParameterSubblock::ReadAndValidate(
    const ParamDefinition& param_definition, ReadBitBuffer& rb) {
  if (subblock_duration.has_value()) {
    RETURN_IF_NOT_OK(rb.ReadULeb128(*subblock_duration));
  }

  param_data = param_definition.CreateParameterData();
  RETURN_IF_NOT_OK(param_data->ReadAndValidate(rb));

  return absl::OkStatus();
}

absl::Status ParameterSubblock::Write(WriteBitBuffer& wb) const {
  if (subblock_duration.has_value()) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(*subblock_duration));
  }

  // Write the specific parameter data depending on the specific type.
  RETURN_IF_NOT_OK(param_data->Write(wb));

  return absl::OkStatus();
}

void ParameterSubblock::Print() const {
  if (subblock_duration.has_value()) {
    ABSL_LOG(INFO) << "    subblock_duration= " << *subblock_duration;
  }
  param_data->Print();
}

std::unique_ptr<ParameterBlockObu> ParameterBlockObu::CreateMode0(
    const ObuHeader& header, DecodedUleb128 parameter_id,
    const ParamDefinition& param_definition) {
  if (param_definition.param_definition_mode_ != 0) {
    ABSL_LOG(WARNING) << "CreateMode0() should only be called when "
                         "param_definition_mode == 0.";
    return nullptr;
  }

  auto parameter_block_obu = absl::WrapUnique(
      new ParameterBlockObu(header, parameter_id, param_definition));
  parameter_block_obu->subblocks_.resize(
      static_cast<size_t>(parameter_block_obu->GetNumSubblocks()));

  return parameter_block_obu;
}

std::unique_ptr<ParameterBlockObu> ParameterBlockObu::CreateMode1(
    const ObuHeader& header, DecodedUleb128 parameter_id,
    const ParamDefinition& param_definition, DecodedUleb128 duration,
    DecodedUleb128 constant_subblock_duration, DecodedUleb128 num_subblocks) {
  if (param_definition.param_definition_mode_ != 1) {
    ABSL_LOG(WARNING) << "CreateMode1() should only be called when "
                         "param_definition_mode == 1.";
    return nullptr;
  }
  auto parameter_block_obu = absl::WrapUnique(
      new ParameterBlockObu(header, parameter_id, param_definition));

  // Under param definition mode 1, several fields are explicitly in the OBU.
  parameter_block_obu->duration_ = duration;
  parameter_block_obu->constant_subblock_duration_ = constant_subblock_duration;
  if (constant_subblock_duration == 0) {
    // This field is explicitly in the OBU.
    parameter_block_obu->num_subblocks_ = num_subblocks;
  }
  parameter_block_obu->subblocks_.resize(
      static_cast<size_t>(parameter_block_obu->GetNumSubblocks()));

  return parameter_block_obu;
}

absl::StatusOr<std::unique_ptr<ParameterBlockObu>>
ParameterBlockObu::CreateFromBuffer(
    const ObuHeader& header, int64_t payload_size,
    const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
        param_definition_variants,
    ReadBitBuffer& rb) {
  DecodedUleb128 parameter_id;
  int8_t encoded_uleb128_size = 0;
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_id, encoded_uleb128_size));

  if (payload_size < encoded_uleb128_size) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Read beyond the end of the OBU for parameter_id=", parameter_id));
  }

  const auto parameter_definition_it =
      param_definition_variants.find(parameter_id);
  if (parameter_definition_it == param_definition_variants.end()) {
    return absl::InvalidArgumentError(
        "Found a stray parameter block OBU (no matching parameter "
        "definition).");
  }

  // TODO(b/359588455): Use `ReadBitBuffer::Seek` to go back to the start of the
  //                    OBU. Update `ReadAndValidatePayload` to expect to read
  //                    `parameter_id`.

  const auto cast_to_base_pointer = [](const auto& param_definition) {
    return static_cast<const ParamDefinition*>(&param_definition);
  };
  const int64_t remaining_payload_size = payload_size - encoded_uleb128_size;
  auto parameter_block_obu = absl::WrapUnique(new ParameterBlockObu(
      header, parameter_id,
      *std::visit(cast_to_base_pointer, parameter_definition_it->second)));

  // TODO(b/338474387): Test reading in extension parameters.
  RETURN_IF_NOT_OK(
      parameter_block_obu->ReadAndValidatePayload(remaining_payload_size, rb));
  return parameter_block_obu;
}

ParameterBlockObu::ParameterBlockObu(const ObuHeader& header,
                                     DecodedUleb128 parameter_id,
                                     const ParamDefinition& param_definition)
    : ObuBase(header, kObuIaParameterBlock),
      parameter_id_(parameter_id),
      param_definition_(param_definition) {}

absl::Status ParameterBlockObu::InterpolateMixGainParameterData(
    const MixGainParameterData* mix_gain_parameter_data,
    InternalTimestamp start_time, InternalTimestamp end_time,
    InternalTimestamp target_time, float& target_mix_gain_db) {
  return InterpolateMixGainValue(
      mix_gain_parameter_data->animation_type,
      MixGainParameterData::kAnimateStep, MixGainParameterData::kAnimateLinear,
      MixGainParameterData::kAnimateBezier,
      [&mix_gain_parameter_data]() {
        return std::get<AnimationStepInt16>(mix_gain_parameter_data->param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationLinearInt16>(
                   mix_gain_parameter_data->param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationLinearInt16>(
                   mix_gain_parameter_data->param_data)
            .end_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .start_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .end_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .control_point_value;
      },
      [&mix_gain_parameter_data]() {
        return std::get<AnimationBezierInt16>(
                   mix_gain_parameter_data->param_data)
            .control_point_relative_time;
      },
      start_time, end_time, target_time, target_mix_gain_db);
}

DecodedUleb128 ParameterBlockObu::GetDuration() const {
  if (param_definition_.param_definition_mode_ == 1) {
    return duration_;
  } else {
    return param_definition_.duration_;
  }
}

DecodedUleb128 ParameterBlockObu::GetConstantSubblockDuration() const {
  if (param_definition_.param_definition_mode_ == 1) {
    return constant_subblock_duration_;
  } else {
    return param_definition_.constant_subblock_duration_;
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

  // The subblocks is explicitly in the OBU or `param_definition_`.
  if (param_definition_.param_definition_mode_ == 1) {
    num_subblocks = num_subblocks_;
  } else {
    num_subblocks = param_definition_.GetNumSubblocks();
  }
  return num_subblocks;
}

absl::StatusOr<DecodedUleb128> ParameterBlockObu::GetSubblockDuration(
    int subblock_index) const {
  return GetParameterSubblockDuration<DecodedUleb128>(
      subblock_index, GetNumSubblocks(), GetConstantSubblockDuration(),
      GetDuration(), param_definition_.param_definition_mode_,
      [this](int i) { return *this->subblocks_[i].subblock_duration; },
      [this](int i) { return this->param_definition_.GetSubblockDuration(i); });
}

absl::Status ParameterBlockObu::SetSubblockDuration(int subblock_index,
                                                    DecodedUleb128 duration) {
  ABSL_CHECK_NE(param_definition_.param_definition_mode_, 0)
      << "Calling ParameterBlockObu::SetSubblockDuration() is disallowed when "
      << "`param_definition_mode_ == 0`";

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  if (subblock_index > num_subblocks) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Setting subblock duration for subblock_index = ", subblock_index,
        " but there are only num_subblocks = ", num_subblocks));
  }
  const DecodedUleb128 constant_subblock_duration =
      GetConstantSubblockDuration();

  // Resets the subblock duration to not holding any value.
  subblocks_[subblock_index].subblock_duration.reset();

  if (constant_subblock_duration == 0) {
    // Overwrite the value in the parameter block.
    subblocks_[subblock_index].subblock_duration = duration;
  }
  return absl::OkStatus();
}

absl::Status ParameterBlockObu::GetLinearMixGain(
    InternalTimestamp obu_relative_time, float& linear_mix_gain) const {
  if (param_definition_.GetType() !=
      ParamDefinition::kParameterDefinitionMixGain) {
    return absl::InvalidArgumentError("Expected Mix Gain Parameter Definition");
  }

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  int target_subblock_index = -1;
  InternalTimestamp target_subblock_start_time = -1;
  InternalTimestamp subblock_relative_start_time = 0;
  InternalTimestamp subblock_relative_end_time = 0;
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

  float mix_gain_db = 0;
  RETURN_IF_NOT_OK(InterpolateMixGainParameterData(
      static_cast<const MixGainParameterData*>(
          subblocks_[target_subblock_index].param_data.get()),
      subblock_relative_start_time, subblock_relative_end_time,
      obu_relative_time, mix_gain_db));

  // Mix gain data is in dB and stored in Q7.8. Convert to the linear value.
  linear_mix_gain = std::pow(10.0f, mix_gain_db / 20.0f);
  return absl::OkStatus();
}

void ParameterBlockObu::PrintObu() const {
  ABSL_LOG(INFO) << "Parameter Block OBU:";
  ABSL_LOG(INFO) << "  // param_definition:";
  param_definition_.Print();

  ABSL_LOG(INFO) << "  parameter_id= " << parameter_id_;
  if (param_definition_.param_definition_mode_ == 1) {
    ABSL_LOG(INFO) << "  duration= " << duration_;
    ABSL_LOG(INFO) << "  constant_subblock_duration= "
                   << constant_subblock_duration_;
    if (constant_subblock_duration_ == 0) {
      ABSL_LOG(INFO) << "  num_subblocks= " << num_subblocks_;
    }
  }

  const DecodedUleb128 num_subblocks = GetNumSubblocks();
  for (int i = 0; i < num_subblocks; i++) {
    ABSL_LOG(INFO) << "  subblocks[" << i << "]";
    subblocks_[i].Print();
  }
}

absl::Status ParameterBlockObu::ValidateAndWritePayload(
    WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_id_));

  // Initialized from OBU or `param_definition_` depending on
  // `param_definition_mode_`.
  // Write fields that are conditional on `param_definition_mode_`.
  bool validate_total_subblock_durations = false;
  if (param_definition_.param_definition_mode_ != 0) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(duration_));
    RETURN_IF_NOT_OK(wb.WriteUleb128(constant_subblock_duration_));
    if (constant_subblock_duration_ == 0) {
      RETURN_IF_NOT_OK(wb.WriteUleb128(num_subblocks_));
      validate_total_subblock_durations = true;
    }
  }

  // Validate the associated `param_definition`.
  RETURN_IF_NOT_OK(param_definition_.Validate());

  // Loop through to write the `subblocks_` vector and validate the total
  // subblock duration if needed.
  int64_t total_subblock_durations = 0;
  for (const auto& subblock : subblocks_) {
    if (validate_total_subblock_durations) {
      total_subblock_durations += *subblock.subblock_duration;
    }
    RETURN_IF_NOT_OK(subblock.Write(wb));
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
  RETURN_IF_NOT_OK(param_definition_.Validate());

  if (param_definition_.param_definition_mode_) {
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
      param_definition_.param_definition_mode_ &&
      constant_subblock_duration_ == 0;
  int64_t total_subblock_durations = 0;
  for (auto& subblock : subblocks_) {
    if (include_subblock_duration) {
      // First make `subblock_duration` contain a value so it will be read in.
      subblock.subblock_duration = 0;
    }
    RETURN_IF_NOT_OK(subblock.ReadAndValidate(param_definition_, rb));
    if (include_subblock_duration) {
      total_subblock_durations += *subblock.subblock_duration;
    }
  }

  if (include_subblock_duration && total_subblock_durations != duration_) {
    return absl::InvalidArgumentError(
        "Subblock durations do not match the total duration.");
  }

  return absl::OkStatus();
}

}  // namespace iamf_tools
