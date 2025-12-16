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
#include "iamf/obu/param_definitions/param_definition_base.h"

#include <cstdint>
#include <vector>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/numeric_utils.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

namespace {

absl::Status ValidateSpecificParamDefinition(
    const ParamDefinition& param_definition) {
  using enum ParamDefinition::ParameterDefinitionType;
  const auto& type = param_definition.GetType();
  if (!type.has_value()) {
    return absl::OkStatus();
  }
  switch (*type) {
    case kParameterDefinitionDemixing:
    case kParameterDefinitionReconGain:
      RETURN_IF_NOT_OK(ValidateEqual(
          param_definition.param_definition_mode_, uint8_t{0},
          absl::StrCat("`param_definition_mode` for parameter_id= ",
                       param_definition.parameter_id_)));
      RETURN_IF_NOT_OK(
          ValidateNotEqual(param_definition.duration_, DecodedUleb128{0},
                           absl::StrCat("duration for parameter_id= ",
                                        param_definition.parameter_id_)));
      RETURN_IF_NOT_OK(ValidateEqual(
          param_definition.constant_subblock_duration_,
          param_definition.duration_,
          absl::StrCat("`constant_subblock_duration` for parameter_id= ",
                       param_definition.parameter_id_)));
      return absl::OkStatus();
    // Neither Mix gain nor Polar have any specific validation. For backwards
    // compatibility we must assume extension param definitions are valid as
    // well.
    case kParameterDefinitionMixGain:
    case kParameterDefinitionPolar:
    default:
      return absl::OkStatus();
  }
}

}  // namespace

DecodedUleb128 ParamDefinition::GetNumSubblocks() const {
  return num_subblocks_;
}

void ParamDefinition::InitializeSubblockDurations(
    const uint32_t num_subblocks) {
  // Check if the `subblock_durations` is included.
  if (!IncludeSubblockDurationArray()) {
    subblock_durations_.clear();
  } else {
    num_subblocks_ = num_subblocks;
    subblock_durations_.resize(num_subblocks);
  }
}

DecodedUleb128 ParamDefinition::GetSubblockDuration(int subblock_index) const {
  return subblock_durations_[subblock_index];
}

absl::Status ParamDefinition::SetSubblockDuration(int subblock_index,
                                                  DecodedUleb128 duration) {
  if (subblock_index >= subblock_durations_.size() || subblock_index < 0) {
    return absl::InvalidArgumentError(
        absl::StrCat("Subblock index ", subblock_index,
                     " is out of bounds. `subblock_durations_.size()`= ",
                     subblock_durations_.size()));
  }

  subblock_durations_[subblock_index] = duration;
  return absl::OkStatus();
}

absl::Status ParamDefinition::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(Validate());

  // Write the fields that are always present in `param_definition`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_id_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_rate_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(param_definition_mode_, 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_, 7));
  if (param_definition_mode_ != 0) {
    return absl::OkStatus();
  }

  // Write the fields dependent on `param_definition_mode == 0`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(duration_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(constant_subblock_duration_));
  if (constant_subblock_duration_ != 0) {
    return absl::OkStatus();
  }

  // Loop to write the `subblock_durations` array if it should be included.
  RETURN_IF_NOT_OK(wb.WriteUleb128(num_subblocks_));
  for (const auto& subblock_duration : subblock_durations_) {
    RETURN_IF_NOT_OK(wb.WriteUleb128(subblock_duration));
  }
  return absl::OkStatus();
}

absl::Status ParamDefinition::ReadAndValidate(ReadBitBuffer& rb) {
  // Read the fields that are always present in `param_definition`.
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_id_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(parameter_rate_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(1, param_definition_mode_));
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, reserved_));
  if (param_definition_mode_ != 0) {
    return absl::OkStatus();
  }

  // Read the fields dependent on `param_definition_mode == 0`.
  RETURN_IF_NOT_OK(rb.ReadULeb128(duration_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(constant_subblock_duration_));
  if (constant_subblock_duration_ != 0) {
    return absl::OkStatus();
  }

  // Loop to read the `subblock_durations` array if it should be included.
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_subblocks_));
  for (int i = 0; i < num_subblocks_; i++) {
    DecodedUleb128 subblock_duration;
    RETURN_IF_NOT_OK(rb.ReadULeb128(subblock_duration));
    subblock_durations_.push_back(subblock_duration);
  }

  RETURN_IF_NOT_OK(Validate());
  return absl::OkStatus();
}

void ParamDefinition::Print() const {
  ABSL_LOG(INFO) << "  parameter_type= "
                 << (type_.has_value() ? absl::StrCat(*type_) : "NONE");
  ABSL_LOG(INFO) << "  parameter_id= " << parameter_id_;
  ABSL_LOG(INFO) << "  parameter_rate= " << parameter_rate_;
  ABSL_LOG(INFO) << "  param_definition_mode= "
                 << absl::StrCat(param_definition_mode_);
  ABSL_LOG(INFO) << "  reserved= " << absl::StrCat(reserved_);
  if (param_definition_mode_ == 0) {
    ABSL_LOG(INFO) << "  duration= " << duration_;
    ABSL_LOG(INFO) << "  constant_subblock_duration= "
                   << constant_subblock_duration_;
    ABSL_LOG(INFO) << "  num_subblocks= " << GetNumSubblocks();

    // Subblock durations.
    if (constant_subblock_duration_ == 0) {
      for (int k = 0; k < GetNumSubblocks(); k++) {
        ABSL_LOG(INFO) << "  subblock_durations[" << k
                       << "]= " << GetSubblockDuration(k);
      }
    }
  }
}

bool ParamDefinition::IncludeSubblockDurationArray() const {
  return param_definition_mode_ == 0 && constant_subblock_duration_ == 0;
}

absl::Status ParamDefinition::Validate() const {
  // For logging purposes.
  const uint32_t parameter_id = parameter_id_;

  absl::Status status = absl::OkStatus();
  if (parameter_rate_ == 0) {
    status = absl::InvalidArgumentError(absl::StrCat(
        "Parameter rate should not be zero. Parameter ID= ", parameter_id));
  }

  // Fields below are conditional on `param_definition_mode == 1`. Otherwise
  // these are defined directly in the Parameter Block OBU.
  if (param_definition_mode_ == 0) {
    if (duration_ == 0) {
      status = absl::InvalidArgumentError(absl::StrCat(
          "Duration should not be zero. Parameter ID = ", parameter_id));
    }
    if (constant_subblock_duration_ > duration_) {
      status = absl::InvalidArgumentError(absl::StrCat(
          "Constant subblock duration should not be greater than duration. "
          "Parameter ID = ",
          parameter_id));
    }

    // Check if the `subblock_durations` is included.
    if (IncludeSubblockDurationArray()) {
      RETURN_IF_NOT_OK(ValidateContainerSizeEqual(
          "subblock_durations", subblock_durations_, num_subblocks_));

      // Loop to add cumulative durations.
      uint32_t total_subblock_durations = 0;
      for (int i = 0; i < num_subblocks_; i++) {
        if (subblock_durations_[i] == 0) {
          status = absl::InvalidArgumentError(
              absl::StrCat("Illegal zero duration for subblock[", i, "]. ",
                           "Parameter ID = ", parameter_id));
        }

        RETURN_IF_NOT_OK(AddUint32CheckOverflow(total_subblock_durations,
                                                subblock_durations_[i],
                                                total_subblock_durations));
      }

      // Check total duration matches expected duration.
      if (total_subblock_durations != duration_) {
        status = absl::InvalidArgumentError(absl::StrCat(
            "Inconsistent total duration and the cumulative durations of ",
            "subblocks. Parameter ID = ", parameter_id));
      }
    }
  }

  RETURN_IF_NOT_OK(ValidateSpecificParamDefinition(*this));

  return status;
}

}  // namespace iamf_tools
