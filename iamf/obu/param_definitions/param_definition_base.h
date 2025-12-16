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
#ifndef OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_BASE_H_
#define OBU_PARAM_DEFINITIONS_PARAM_DEFINITION_BASE_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Common part of the parameter definitions.
 *
 * Extended by `MixGainParamDefinition`, `DemixingParamDefinition`, and
 * `ReconGainParamDefinition`, and various position-based parameter
 * definitions.
 */
class ParamDefinition {
 public:
  /*!\brief A `DecodedUleb128` enum for the type of parameter. */
  enum ParameterDefinitionType : DecodedUleb128 {
    kParameterDefinitionMixGain = 0,
    kParameterDefinitionDemixing = 1,
    kParameterDefinitionReconGain = 2,
    kParameterDefinitionPolar = 3,
    kParameterDefinitionCart8 = 4,
    kParameterDefinitionCart16 = 5,
    kParameterDefinitionDualPolar = 6,
    kParameterDefinitionDualCart8 = 7,
    kParameterDefinitionDualCart16 = 8,
    // Values in the range of [9, (1 << 32) - 1] are reserved.
    kParameterDefinitionReservedStart = 9,
    kParameterDefinitionReservedEnd = std::numeric_limits<DecodedUleb128>::max()
  };

  /*!\brief Default constructor.
   *
   * After constructing `InitializeSubblockDurations()` MUST be called
   * before using most functionality.
   */
  ParamDefinition() = default;

  /*!\brief Default destructor.
   */
  virtual ~ParamDefinition() = default;

  /*!\brief Gets the number of subblocks.
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

  /*!\brief Gets the subblock duration.
   *
   * \param subblock_index Index of the subblock to get the duration.
   * \return Duration of the subblock.
   */
  DecodedUleb128 GetSubblockDuration(int subblock_index) const;

  /*!\brief Sets the subblock duration.
   *
   * \param subblock_index Index of the subblock to set the duration.
   * \param duration Duration to set.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status SetSubblockDuration(int subblock_index, DecodedUleb128 duration);

  /*!\brief Validates the parameter definition called by `ValidateAndWrite()`.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Validate() const;

  /*!\brief Validates and writes the parameter definition.
   *
   * This function defines the validating and writing of the common parts,
   * and the sub-classes's overridden ones shall define their specific parts.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Reads and validates the parameter definition.
   *
   * This function defines the validating and reading of the common parts,
   * and the sub-classes's overridden ones shall define their specific parts.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status ReadAndValidate(ReadBitBuffer& rb);

  /*!\brief Gets the `ParameterDefinitionType`.
   *
   * \return Type of this parameter definition.
   */
  std::optional<ParameterDefinitionType> GetType() const { return type_; }

  /*!\brief Creates a parameter data.
   *
   * The created instance will one of the subclassees of `ParameterData`,
   * depending on the specific subclass implementing this function.
   *
   * \return Unique pointer to the created parameter data.
   */
  virtual std::unique_ptr<ParameterData> CreateParameterData() const = 0;

  /*!\brief Prints the parameter definition.
   */
  virtual void Print() const;

  friend bool operator==(const ParamDefinition& lhs,
                         const ParamDefinition& rhs) = default;

  DecodedUleb128 parameter_id_ = 0;
  DecodedUleb128 parameter_rate_ = 0;
  uint8_t param_definition_mode_ = 0;  // 1 bit.
  uint8_t reserved_ = 0;               // 7 bits.

  // All fields below are only included if `param_definition_mode_ == 0`.
  DecodedUleb128 duration_ = 0;
  DecodedUleb128 constant_subblock_duration_ = 0;

 protected:
  /*!\brief Constructor with a passed-in type used by sub-classes.
   *
   * \param type Type of the specific parameter definition.
   */
  ParamDefinition(ParameterDefinitionType type) : type_(type) {}

 private:
  /*!\brief Whether the subblock durations are included in this object.
   *
   * \return True if the subblock durations are included.
   */
  bool IncludeSubblockDurationArray() const;

  // Type of this parameter definition.
  std::optional<ParameterDefinitionType> type_ = std::nullopt;

  // `num_subblocks` is only included if `param_definition_mode_ == 0` and
  // `constant_subblock_duration == 0`.
  DecodedUleb128 num_subblocks_ = 0;

  // Vector of length `num_subblocks`.
  std::vector<DecodedUleb128> subblock_durations_ = {};
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_H_
