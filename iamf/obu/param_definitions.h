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
#ifndef OBU_PARAM_DEFINITIONS_H_
#define OBU_PARAM_DEFINITIONS_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/leb128.h"

namespace iamf_tools {

/* !\brief Common part of the parameter definitions.
 *
 * Extended by `MixGainParamDefinition`, `DemixingParamDefinition`, and
 * `ReconGainParamDefinition`.
 */
class ParamDefinition {
 public:
  /*!\brief A `DecodedUleb128` enum for the type of parameter. */
  enum ParameterDefinitionType : DecodedUleb128 {
    kParameterDefinitionMixGain = 0,
    kParameterDefinitionDemixing = 1,
    kParameterDefinitionReconGain = 2,
    // Values in the range of [3, (1 << 32) - 1] are reserved.
    kParameterDefinitionReservedStart = 3,
    kParameterDefinitionReservedEnd = std::numeric_limits<DecodedUleb128>::max()
  };

  /*!\brief Default constructor.
   *
   * After constructing `InitializeSubblockDurations()` MUST be called
   * before using most functionality.
   */
  ParamDefinition() : type_(std::nullopt) {}

  /*!\brief Default destructor.
   */
  virtual ~ParamDefinition() = default;

  friend bool operator==(const ParamDefinition& lhs,
                         const ParamDefinition& rhs);

  /*\!brief Gets the number of subblocks.
   *
   * \return Number of subblocks.
   */
  DecodedUleb128 GetNumSubblocks() const;

  /*!\brief Initializes the subblock durations.
   *
   * This must be called before calling `SetSubblockDuration()` and
   * `GetSubblockDuration()`.
   *
   * \param num_subblocks Number of subblocks.
   */
  void InitializeSubblockDurations(DecodedUleb128 num_subblocks);

  /*\!brief Gets the subblock duration.
   *
   * \param subblock_index Index of the subblock to get the duration.
   * \return Duration of the subblock.
   */
  DecodedUleb128 GetSubblockDuration(int subblock_index) const;

  /*\!brief Sets the subblock duration.
   *
   * \param subblock_index Index of the subblock to set the duration.
   * \param duration Duration to set.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status SetSubblockDuration(int subblock_index, DecodedUleb128 duration);

  /*!\brief Validates and writes the parameter definition.
   *
   * This function defines the validating and writing of the common parts,
   * and the sub-classes's overridden ones shall define their specific parts.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Gets the `ParameterDefinitionType`.
   *
   * \return Type of this parameter definition.
   */
  std::optional<ParameterDefinitionType> GetType() const { return type_; }

  /*!\brief Prints the parameter definition.
   */
  virtual void Print() const;

  DecodedUleb128 parameter_id_;
  DecodedUleb128 parameter_rate_;
  uint8_t param_definition_mode_;  // 1 bit.
  uint8_t reserved_ = 0;           // 7 bits.

  // All fields below are only included if `param_definition_mode_ == 0`.
  DecodedUleb128 duration_ = 0;
  DecodedUleb128 constant_subblock_duration_ = 0;

 protected:
  /*!\brief Constructor with a passed-in type used by sub-classes.
   *
   * \param type Type of the specific parameter definition.
   */
  ParamDefinition(ParameterDefinitionType type) : type_(type) {}

  /*\!brief Validates the specific `ParamDefinition`s are equivalent.
   *
   * \param other `ParamDefinition` to compare.
   * \return `true` if equivalent. `false` otherwise.
   */
  virtual bool EquivalentDerived(const ParamDefinition& /*other*/) const {
    return true;
  }

 private:
  /*!\brief Whether the subblock durations are included in this object.
   *
   * \return True if the subblock durations are included.
   */
  bool IncludeSubblockDurationArray() const;

  /*!\brief Validates the parameter definition called by `ValidateAndWrite()`.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate() const;

  // Type of this parameter definition.
  std::optional<ParameterDefinitionType> type_;

  // `num_subblocks` is only included if `param_definition_mode_ == 0` and
  // `constant_subblock_duration == 0`.
  DecodedUleb128 num_subblocks_ = 0;

  // Vector of length `num_subblocks`.
  std::vector<DecodedUleb128> subblock_durations_;
};

/* !\brief Parameter definition of mix gains to be applied to a signal.
 */
class MixGainParamDefinition : public ParamDefinition {
 public:
  /*!\brief Default constructor.
   */
  MixGainParamDefinition() : ParamDefinition(kParameterDefinitionMixGain) {}

  /*!\brief Default destructor.
   */
  ~MixGainParamDefinition() override = default;

  friend bool operator==(const MixGainParamDefinition& lhs,
                         const MixGainParamDefinition& rhs) {
    return static_cast<const ParamDefinition&>(lhs) ==
           static_cast<const ParamDefinition&>(rhs);
  }

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  int16_t default_mix_gain_;

 private:
  /*\!brief Validates the specific `ParamDefinition`s are equivalent.
   *
   * \param other `ParamDefinition` to compare.
   * \return `true` if equivalent. `false` otherwise.
   */
  bool EquivalentDerived(const ParamDefinition& other) const override {
    const auto& other_mix_gain =
        dynamic_cast<const MixGainParamDefinition&>(other);
    return default_mix_gain_ == other_mix_gain.default_mix_gain_;
  }
};

/* !\brief Parameter definition for demixing info.
 */
class DemixingParamDefinition : public ParamDefinition {
 public:
  /*!\brief Default constructor.
   */
  DemixingParamDefinition() : ParamDefinition(kParameterDefinitionDemixing) {}

  /*!\brief Default destructor.
   */
  ~DemixingParamDefinition() override = default;

  friend bool operator==(const DemixingParamDefinition& lhs,
                         const DemixingParamDefinition& rhs) {
    return static_cast<const ParamDefinition&>(lhs) ==
           static_cast<const ParamDefinition&>(rhs);
  }

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  DefaultDemixingInfoParameterData default_demixing_info_parameter_data_;

 private:
  /*\!brief Validates the specific `ParamDefinition`s are equivalent.
   *
   * \param other `ParamDefinition` to compare.
   * \return `true` if equivalent. `false` otherwise.
   */
  bool EquivalentDerived(const ParamDefinition& other) const override {
    const auto& other_demixing =
        dynamic_cast<const DemixingParamDefinition&>(other);
    return default_demixing_info_parameter_data_ ==
           other_demixing.default_demixing_info_parameter_data_;
  }
};

/* !\brief Parameter definition for recon gain.
 */
class ReconGainParamDefinition : public ParamDefinition {
 public:
  /*!\brief Constructor.
   *
   * \param audio_element_id ID of the Audio Element OBU that uses this
   *     recon gain parameter.
   */
  ReconGainParamDefinition(uint32_t audio_element_id)
      : ParamDefinition(kParameterDefinitionReconGain),
        audio_element_id_(audio_element_id) {}

  /*!\brief Default destructor.
   */
  ~ReconGainParamDefinition() override = default;

  friend bool operator==(const ReconGainParamDefinition& lhs,
                         const ReconGainParamDefinition& rhs) {
    return static_cast<const ParamDefinition&>(lhs) ==
           static_cast<const ParamDefinition&>(rhs);
  }

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  /*!\brief ID of the Audio Element OBU that uses this recon gain parameter.
   */
  const uint32_t audio_element_id_;

 private:
  /*\!brief Validates the specific `ParamDefinition`s are equivalent.
   *
   * \param other `ParamDefinition` to compare.
   * \return `true` if equivalent. `false` otherwise.
   */
  bool EquivalentDerived(const ParamDefinition& other) const override {
    const auto& other_recon_gain =
        dynamic_cast<const ReconGainParamDefinition&>(other);
    return audio_element_id_ == other_recon_gain.audio_element_id_;
  }
};

/* !\brief Parameter definition reserved for future use; should be ignored.
 */
class ExtendedParamDefinition : public ParamDefinition {
 public:
  /*!\brief Default constructor.
   */
  ExtendedParamDefinition()
      : ParamDefinition(kParameterDefinitionReservedStart) {}

  /*!\brief Default destructor.
   */
  ~ExtendedParamDefinition() override = default;

  friend bool operator==(const ExtendedParamDefinition& lhs,
                         const ExtendedParamDefinition& rhs) {
    return static_cast<const ParamDefinition&>(lhs) ==
           static_cast<const ParamDefinition&>(rhs);
  }

  /*!\brief Validates and writes a `ExtendedParamDefinition` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  // Size and vector of the bytes the OBU parser should ignore.
  DecodedUleb128 param_definition_size_ = 0;
  std::vector<uint8_t> param_definition_bytes_ = {};

 private:
  /*\!brief Validates the specific `ParamDefinition`s are equivalent.
   *
   * \param other `ParamDefinition` to compare.
   * \return `true` if equivalent. `false` otherwise.
   */
  bool EquivalentDerived(const ParamDefinition& other) const override {
    const auto& other_extended =
        dynamic_cast<const ExtendedParamDefinition&>(other);

    if (param_definition_size_ != other_extended.param_definition_size_) {
      return false;
    }
    return param_definition_bytes_ == other_extended.param_definition_bytes_;
  }
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_H_
