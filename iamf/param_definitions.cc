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
#include "iamf/param_definitions.h"

#include <cstdint>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/obu_util.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

namespace {

absl::Status ValidateDemixingOrReconGainParamDefinition(
    const ParamDefinition& param_definition) {
  // The IAMF spec requires several restrictions for demixing and recon gain
  // parameter definitions.
  if (param_definition.param_definition_mode_ == 0 &&
      param_definition.duration_ != 0 &&
      param_definition.duration_ ==
          param_definition.constant_subblock_duration_) {
    // `num_subblocks` is calculated implicitly as
    // `ceil(duration / constant_subblock_duration)`. Since the values being
    // divided are non-zero and equal it is implicitly the required value of 1.

    return absl::OkStatus();
  }

  return absl::InvalidArgumentError("");
}

}  // namespace

bool operator==(const ParamDefinition& lhs, const ParamDefinition& rhs) {
  // First check always-present fields.
  if (lhs.param_definition_mode_ != rhs.param_definition_mode_) {
    return false;
  }
  if (lhs.type_ != rhs.type_) {
    return false;
  }
  if (!lhs.EquivalentDerived(rhs)) {
    return false;
  }

  if (lhs.param_definition_mode_ == 1) {
    // Equivalent. We can filter out below irrelevant fields.
    return true;
  }

  if (lhs.duration_ != rhs.duration_ ||
      lhs.constant_subblock_duration_ != rhs.constant_subblock_duration_) {
    return false;
  }

  if (lhs.constant_subblock_duration_ != 0) {
    // Equivalent. We can filter out below irrelevant fields.
    return true;
  }

  if (lhs.num_subblocks_ != rhs.num_subblocks_ ||
      lhs.subblock_durations_ != rhs.subblock_durations_) {
    return false;
  }

  return true;
}

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
  if (subblock_index > subblock_durations_.size()) {
    LOG(ERROR) << "Subblock index " << subblock_index
               << " greater than `subblock_durations_.size()`= "
               << subblock_durations_.size();
    return absl::InvalidArgumentError("");
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

void ParamDefinition::Print() const {
  LOG(INFO) << "  parameter_id= " << parameter_id_;
  LOG(INFO) << "  parameter_rate= " << parameter_rate_;
  LOG(INFO) << "  param_definition_mode= "
            << static_cast<int>(param_definition_mode_);
  LOG(INFO) << "  reserved= " << static_cast<int>(reserved_);
  if (param_definition_mode_ == 0) {
    LOG(INFO) << "  duration= " << duration_;
    LOG(INFO) << "  constant_subblock_duration= "
              << constant_subblock_duration_;
    LOG(INFO) << "  num_subblocks=" << GetNumSubblocks();

    // Subblock durations.
    if (constant_subblock_duration_ == 0) {
      for (int k = 0; k < GetNumSubblocks(); k++) {
        LOG(INFO) << "  subblock_durations[" << k
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
    LOG(ERROR) << "Parameter rate should not be zero. Parameter ID= "
               << parameter_id;
    status = absl::InvalidArgumentError("");
  }

  // Fields below are conditional on `param_definition_mode == 1`. Otherwise
  // these are defined directly in the Parameter Block OBU.
  if (param_definition_mode_ == 0) {
    if (duration_ == 0) {
      LOG(ERROR) << "Duration should not be zero. Parameter ID= "
                 << parameter_id;
      status = absl::InvalidArgumentError("");
    }

    // Check if the `subblock_durations` is included.
    if (IncludeSubblockDurationArray()) {
      RETURN_IF_NOT_OK(ValidateVectorSizeEqual(
          "subblock_durations", subblock_durations_.size(), num_subblocks_));

      // Loop to add cumulative durations.
      uint32_t total_subblock_durations = 0;
      for (int i = 0; i < num_subblocks_; i++) {
        if (subblock_durations_[i] == 0) {
          LOG(ERROR) << "Illegal zero duration for subblock[" << i << "]. "
                     << "Parameter ID= " << parameter_id;
          status = absl::InvalidArgumentError("");
        }

        RETURN_IF_NOT_OK(AddUint32CheckOverflow(total_subblock_durations,
                                                subblock_durations_[i],
                                                total_subblock_durations));
      }

      // Check total duration matches expected duration.
      if (total_subblock_durations != duration_) {
        LOG(ERROR) << "Inconsistent total duration and the cumulative "
                   << "durations of subblocks. Parameter ID= " << parameter_id;
        status = absl::InvalidArgumentError("");
      }
    }
  }

  return status;
}

absl::Status MixGainParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(wb.WriteSigned16(default_mix_gain_));
  return absl::OkStatus();
}

void MixGainParamDefinition::Print() const {
  LOG(INFO) << "MixGainParamDefinition:";
  ParamDefinition::Print();
  LOG(INFO) << "  default_mix_gain= " << default_mix_gain_;
}

absl::Status DemixingParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // The sub-class specific part.
  RETURN_IF_NOT_OK(default_demixing_info_parameter_data_.Write(wb));

  // Validate restrictions, but obey the user if `NO_CHECK_ERROR` is defined.
  RETURN_IF_NOT_OK(ValidateDemixingOrReconGainParamDefinition(*this));

  return absl::OkStatus();
}

void DemixingParamDefinition::Print() const {
  LOG(INFO) << "DemixingParamDefinition:";
  ParamDefinition::Print();
  default_demixing_info_parameter_data_.Print();
}

absl::Status ReconGainParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // The common part.
  RETURN_IF_NOT_OK(ParamDefinition::ValidateAndWrite(wb));

  // No sub-class specific part for Recon Gain Parameter Definition.

  // Validate restrictions, but obey the user if `NO_CHECK_ERROR` is defined.
  RETURN_IF_NOT_OK(ValidateDemixingOrReconGainParamDefinition(*this));

  return absl::OkStatus();
}

void ReconGainParamDefinition::Print() const {
  LOG(INFO) << "ReconGainParamDefinition:";
  ParamDefinition::Print();
}

absl::Status ExtendedParamDefinition::ValidateAndWrite(
    WriteBitBuffer& wb) const {
  // This class does not write the base class's data, i.e. it doesn't call
  // `ParamDefinition::ValidateAndWrite(wb)`.
  RETURN_IF_NOT_OK(wb.WriteUleb128(param_definition_size_));
  RETURN_IF_NOT_OK(ValidateVectorSizeEqual("param_definition_bytes_",
                                           param_definition_bytes_.size(),
                                           param_definition_size_));
  RETURN_IF_NOT_OK(wb.WriteUint8Vector(param_definition_bytes_));

  return absl::OkStatus();
}

void ExtendedParamDefinition::Print() const {
  LOG(INFO) << "ExtendedParamDefinition:";
  ParamDefinition::Print();
  LOG(INFO) << "  param_definition_size= " << param_definition_size_;
  LOG(INFO) << "  // Skipped printing param_definition_bytes";
}

}  // namespace iamf_tools
