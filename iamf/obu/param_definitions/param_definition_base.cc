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
#include "absl/types/span.h"
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
  switch (param_definition.GetType()) {
    case kParameterDefinitionDemixing:
    case kParameterDefinitionReconGain:
      RETURN_IF_NOT_OK(ValidateEqual(
          param_definition.GetParamDefinitionMode(),
          ParamDefinition::kModeScheduleInParamDefinition,
          absl::StrCat("`param_definition_mode` for parameter_id= ",
                       param_definition.GetParameterId())));
      RETURN_IF_NOT_OK(
          ValidateNotEqual(param_definition.GetDuration(), DecodedUleb128{0},
                           absl::StrCat("duration for parameter_id= ",
                                        param_definition.GetParameterId())));
      RETURN_IF_NOT_OK(ValidateEqual(
          param_definition.GetConstantSubblockDuration(),
          param_definition.GetDuration(),
          absl::StrCat("`constant_subblock_duration` for parameter_id= ",
                       param_definition.GetParameterId())));
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

DecodedUleb128 ParamDefinition::GetParameterId() const { return parameter_id_; }

DecodedUleb128 ParamDefinition::GetParameterRate() const {
  return parameter_rate_;
}

ParamDefinition::ParamDefinitionMode ParamDefinition::GetParamDefinitionMode()
    const {
  return param_definition_mode_;
}

uint8_t ParamDefinition::GetReserved() const { return reserved_; }

DecodedUleb128 ParamDefinition::GetDuration() const { return duration_; }

DecodedUleb128 ParamDefinition::GetConstantSubblockDuration() const {
  return constant_subblock_duration_;
}

DecodedUleb128 ParamDefinition::GetNumSubblocks() const {
  return num_subblocks_;
}

absl::Span<const DecodedUleb128> ParamDefinition::GetSubblockDurations() const {
  return absl::MakeConstSpan(subblock_durations_);
}

absl::Status ParamDefinition::ValidateAndWrite(WriteBitBuffer& wb) const {
  RETURN_IF_NOT_OK(Validate());

  // Write the fields that are always present in `param_definition`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_id_));
  RETURN_IF_NOT_OK(wb.WriteUleb128(parameter_rate_));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(param_definition_mode_, 1));
  RETURN_IF_NOT_OK(wb.WriteUnsignedLiteral(reserved_, 7));
  if (param_definition_mode_ != kModeScheduleInParamDefinition) {
    return absl::OkStatus();
  }

  // Write the fields dependent on `param_definition_mode ==
  // kModeScheduleInParamDefinition`.
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
  uint8_t param_definition_mode;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(1, param_definition_mode));
  param_definition_mode_ =
      static_cast<ParamDefinitionMode>(param_definition_mode);
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(7, reserved_));
  if (param_definition_mode_ != kModeScheduleInParamDefinition) {
    return absl::OkStatus();
  }

  // Read the fields dependent on `param_definition_mode ==
  // kModeScheduleInParamDefinition`.
  RETURN_IF_NOT_OK(rb.ReadULeb128(duration_));
  RETURN_IF_NOT_OK(rb.ReadULeb128(constant_subblock_duration_));
  if (constant_subblock_duration_ != 0) {
    return absl::OkStatus();
  }

  // Loop to read the `subblock_durations` array if it should be included.
  RETURN_IF_NOT_OK(rb.ReadULeb128(num_subblocks_));
  if (num_subblocks_ > ParamDefinition::kMaxNumSubblocks) {
    return absl::InvalidArgumentError(
        absl::StrCat("num_subblocks= ", num_subblocks_, " exceeds maximum."));
  }
  for (DecodedUleb128 i = 0; i < num_subblocks_; i++) {
    DecodedUleb128 subblock_duration;
    RETURN_IF_NOT_OK(rb.ReadULeb128(subblock_duration));
    subblock_durations_.push_back(subblock_duration);
  }

  RETURN_IF_NOT_OK(Validate());
  return absl::OkStatus();
}

void ParamDefinition::Print() const {
  ABSL_LOG(INFO) << "  parameter_type= " << absl::StrCat(type_);
  ABSL_LOG(INFO) << "  parameter_id= " << parameter_id_;
  ABSL_LOG(INFO) << "  parameter_rate= " << parameter_rate_;
  ABSL_LOG(INFO) << "  param_definition_mode= "
                 << absl::StrCat(param_definition_mode_);
  ABSL_LOG(INFO) << "  reserved= " << absl::StrCat(reserved_);
  if (param_definition_mode_ == kModeScheduleInParamDefinition) {
    ABSL_LOG(INFO) << "  duration= " << duration_;
    ABSL_LOG(INFO) << "  constant_subblock_duration= "
                   << constant_subblock_duration_;
    ABSL_LOG(INFO) << "  num_subblocks= " << GetNumSubblocks();

    // Subblock durations.
    if (constant_subblock_duration_ == 0) {
      auto subblock_durations = GetSubblockDurations();
      for (DecodedUleb128 i = 0; i < subblock_durations.size(); i++) {
        ABSL_LOG(INFO) << "  subblock_durations[" << i
                       << "]= " << subblock_durations[i];
      }
    }
  }
}

bool ParamDefinition::IncludeSubblockDurationArray() const {
  return param_definition_mode_ == kModeScheduleInParamDefinition &&
         constant_subblock_duration_ == 0;
}

absl::Status ParamDefinition::Validate() const {
  // For logging purposes.
  const uint32_t parameter_id = parameter_id_;

  absl::Status status = absl::OkStatus();
  if (parameter_rate_ == 0) {
    status = absl::InvalidArgumentError(absl::StrCat(
        "Parameter rate should not be zero. Parameter ID= ", parameter_id));
  }

  // Fields below are conditional on `param_definition_mode ==
  // kModeScheduleInParamDefinition`. Otherwise these are defined directly in
  // the Parameter Block OBU.
  if (param_definition_mode_ == kModeScheduleInParamDefinition) {
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
      for (DecodedUleb128 i = 0; i < num_subblocks_; i++) {
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
