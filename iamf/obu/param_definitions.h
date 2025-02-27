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
#include <memory>
#include <optional>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/* !\brief Forward declarartion of `ParameterData`. */
struct ParameterData;

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

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;
  /*!\brief Creates a parameter data.
   *
   * The created instance will be of type `MixGainParameterData`.
   *
   * \return Unique pointer to the created parameter data.
   */
  std::unique_ptr<ParameterData> CreateParameterData() const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  friend bool operator==(const MixGainParamDefinition& lhs,
                         const MixGainParamDefinition& rhs) = default;

  int16_t default_mix_gain_;
};

struct ChannelNumbers {
  friend bool operator==(const ChannelNumbers& lhs,
                         const ChannelNumbers& rhs) = default;
  // Number of surround channels.
  int surround;
  // Number of low-frequency effects channels.
  int lfe;
  // Number of height channels.
  int height;
};

/* !\brief Parameter definition for recon gain.
 */
class ReconGainParamDefinition : public ParamDefinition {
 public:
  /* Additional data useful for creating parameter (sub)blocks.
   *
   * Present only in some intermediate stages of encoder, decoder, and
   * transcoder and are will not be read from/written to bitstreams.
   */
  struct ReconGainAuxiliaryData {
    bool recon_gain_is_present_flag;
    ChannelNumbers channel_numbers_for_layer;
    friend bool operator==(const ReconGainAuxiliaryData& lhs,
                           const ReconGainAuxiliaryData& rhs) = default;
  };

  /*!\brief Constructor.
   *
   * \param audio_element_id ID of the Audio Element OBU that uses this
   *        recon gain parameter.
   */
  ReconGainParamDefinition(uint32_t audio_element_id)
      : ParamDefinition(kParameterDefinitionReconGain),
        audio_element_id_(audio_element_id) {}

  /*!\brief Default destructor.
   */
  ~ReconGainParamDefinition() override = default;

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Creates a parameter data.
   *
   * The created instance will be of type `ReconGainInfoParameterData`.
   *
   * \return Unique pointer to the created parameter data.
   */
  std::unique_ptr<ParameterData> CreateParameterData() const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  friend bool operator==(const ReconGainParamDefinition& lhs,
                         const ReconGainParamDefinition& rhs) = default;

  /*!\brief ID of the Audio Element OBU that uses this recon gain parameter.
   */
  const uint32_t audio_element_id_;

  // Vector of size equal to the number of layers in the corresponding
  // audio element.
  std::vector<ReconGainAuxiliaryData> aux_data_;
};

/* !\brief Parameter definition reserved for future use; should be ignored.
 */
class ExtendedParamDefinition : public ParamDefinition {
 public:
  /*!\brief Default constructor.
   */
  explicit ExtendedParamDefinition(
      ParamDefinition::ParameterDefinitionType type)
      : ParamDefinition(type) {}

  /*!\brief Default destructor.
   */
  ~ExtendedParamDefinition() override = default;

  /*!\brief Validates and writes a `ExtendedParamDefinition` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const override;

  /*!\brief Reads from a buffer and validates the resulting output.
   *
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ReadAndValidate(ReadBitBuffer& rb) override;

  /*!\brief Creates a parameter data.
   *
   * The created instance will be of type `ExtensionParameterData`.
   *
   * \return Unique pointer to the created parameter data.
   */
  std::unique_ptr<ParameterData> CreateParameterData() const override;

  /*!\brief Prints the parameter definition.
   */
  void Print() const override;

  friend bool operator==(const ExtendedParamDefinition& lhs,
                         const ExtendedParamDefinition& rhs) = default;

  // Size and vector of the bytes the OBU parser should ignore.
  DecodedUleb128 param_definition_size_ = 0;
  std::vector<uint8_t> param_definition_bytes_ = {};
};

}  // namespace iamf_tools

#endif  // OBU_PARAM_DEFINITIONS_H_
