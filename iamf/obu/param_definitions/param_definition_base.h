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
#include "absl/types/span.h"
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
  /*!\brief Static limit on num_subblocks prevents OOMs from implausible values.
   *
   * The maximum sample rate is 192000 Hz and maximum duration is 1 second.
   * Therefore the theoretical maximum number of subblocks is 192000.
   */
  static constexpr DecodedUleb128 kMaxNumSubblocks = 192000;

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

  /*!\brief Descriptive names for `parameter_definition_mode_`.
   *
   * The spec just calls these "mode 0" and "mode 1", but this results in poor
   * readability, and it can be easy to confuse the two modes.
   */
  enum ParamDefinitionMode : uint8_t {
    kModeScheduleInParamDefinition = 0,
    kModeScheduleInParameterBlock = 1,
  };

  /*!\brief Arguments for the `ParamDefinitionBase` constructor. */
  struct BaseArgs {
    DecodedUleb128 parameter_id = 0;
    DecodedUleb128 parameter_rate = 0;
    ParamDefinitionMode param_definition_mode = kModeScheduleInParamDefinition;
    uint8_t reserved = 0;
    DecodedUleb128 duration = 0;
    DecodedUleb128 constant_subblock_duration = 0;
    DecodedUleb128 num_subblocks = 0;
    std::vector<DecodedUleb128> subblock_durations = {};
  };

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

  /*!\brief Gets the parameter ID.
   *
   * \return Parameter ID.
   */
  DecodedUleb128 GetParameterId() const;

  /*!\brief Gets the parameter rate.
   *
   * \return Parameter rate.
   */
  DecodedUleb128 GetParameterRate() const;

  /*!\brief Gets the parameter definition mode.
   *
   * \return Parameter definition mode.
   */
  ParamDefinitionMode GetParamDefinitionMode() const;

  /*!\brief Gets the reserved field.
   *
   * \return Reserved field.
   */
  uint8_t GetReserved() const;

  /*!\brief Gets the duration.
   *
   * \return Duration.
   */
  DecodedUleb128 GetDuration() const;

  /*!\brief Gets the constant subblock duration.
   *
   * \return Constant subblock duration.
   */
  DecodedUleb128 GetConstantSubblockDuration() const;

  /*!\brief Gets the subblock durations.
   *
   * \return Subblock durations.
   */
  absl::Span<const DecodedUleb128> GetSubblockDurations() const;

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
  ParamDefinitionMode param_definition_mode_ = kModeScheduleInParamDefinition;
  uint8_t reserved_ = 0;  // 7 bits.

  // All fields below are only included if `param_definition_mode_ ==
  // kModeScheduleInParamDefinition`.
  DecodedUleb128 duration_ = 0;
  DecodedUleb128 constant_subblock_duration_ = 0;

 protected:
  /*!\brief Constructor with a passed-in type used by sub-classes.
   *
   * \param type Type of the specific parameter definition.
   * \param base_args Arguments for `ParamDefinitionBase`.
   */
  ParamDefinition(ParameterDefinitionType type, const BaseArgs& base_args)
      : type_(type),
        parameter_id_(base_args.parameter_id),
        parameter_rate_(base_args.parameter_rate),
        param_definition_mode_(base_args.param_definition_mode),
        reserved_(base_args.reserved),
        duration_(base_args.duration),
        constant_subblock_duration_(base_args.constant_subblock_duration),
        num_subblocks_(base_args.num_subblocks),
        subblock_durations_(base_args.subblock_durations) {}

 private:
  /*!\brief Whether the subblock durations are included in this object.
   *
   * \return True if the subblock durations are included.
   */
  bool IncludeSubblockDurationArray() const;

  // Type of this parameter definition.
  std::optional<ParameterDefinitionType> type_ = std::nullopt;

  // `num_subblocks` is only included if `param_definition_mode_ ==
  // ParamDefinition::kModeScheduleInParamDefinition` and
  // `constant_subblock_duration == 0`.
  DecodedUleb128 num_subblocks_ = 0;

  // Vector of length `num_subblocks`.
  std::vector<DecodedUleb128> subblock_durations_ = {};
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_H_
