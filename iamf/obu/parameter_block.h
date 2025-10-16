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
#ifndef OBU_PARAMETER_BLOCK_H_
#define OBU_PARAMETER_BLOCK_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/mix_gain_parameter_data.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definition_variant.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief An element of the Parameter Block OBU's `subblocks` vector. */
struct ParameterSubblock {
  /*!\brief Reads and validates the parameter subblock.
   *
   * \param param_definition Parameter definition.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()`. Or a specific error code on failure.
   */
  absl::Status ReadAndValidate(const ParamDefinition& param_definition,
                               ReadBitBuffer& rb);

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Prints the parameter subblock.
   */
  void Print() const;

  // `subblock_duration` is conditionally included based on
  // `param_definition_mode` and `constant_subblock_duration`.
  std::optional<DecodedUleb128> subblock_duration;

  // The active field depends on `param_definition_type` in the metadata.
  std::unique_ptr<ParameterData> param_data;
};

/*!\brief A Parameter Block OBU.
 *
 * The metadata specified in this OBU defines the parameter values for an
 * algorithm for an indicated duration, including any animation of the parameter
 * values over this duration.
 */
class ParameterBlockObu : public ObuBase {
 public:
  /*!\brief Creates a `ParameterBlockObu` with `param_definition_mode` of 0.
   *
   * \param header `ObuHeader` of the OBU.
   * \param param_definition Parameter definition to use.
   * \return Unique pointer to a `ParameterBlockObu` on success, or `nullptr`
   *         on failure.
   */
  static std::unique_ptr<ParameterBlockObu> CreateMode0(
      const ObuHeader& header, const ParamDefinition& param_definition);

  /*!\brief Creates a `ParameterBlockObu` with `param_definition_mode` of 1.
   *
   * \param header `ObuHeader` of the OBU.
   * \param param_definition Parameter definition to use.
   * \param duration Duration of the parameter block.
   * \param constant_subblock_duration Constant subblock duration of the
   *        parameter block.
   * \param num_subblocks Number of subblocks in the parameter block.
   * \return Unique pointer to a `ParameterBlockObu` on success, or `nullptr`
   *         on failure.
   */
  static std::unique_ptr<ParameterBlockObu> CreateMode1(
      const ObuHeader& header, const ParamDefinition& param_definition,
      DecodedUleb128 duration, DecodedUleb128 constant_subblock_duration,
      DecodedUleb128 num_subblocks);

  /*!\brief Creates a `ParameterBlockObu` from a `ReadBitBuffer`.
   *
   * This function is designed to be used from the perspective of the decoder.
   * It will call `ReadAndValidatePayload` in order to read from the buffer;
   * therefore it can fail.
   *
   * \param header `ObuHeader` of the OBU.
   * \param payload_size Size of the obu payload in bytes.
   * \param param_definition_variants Mapping from parameter IDs to param
   *        definitions.
   * \param rb `ReadBitBuffer` where the `ParameterBlockObu` data is stored.
   *        Data read from the buffer is consumed.
   * \return Unique pointer to a `ParameterBlockObu` on success. A specific
   *         status on failure.
   */
  static absl::StatusOr<std::unique_ptr<ParameterBlockObu>> CreateFromBuffer(
      const ObuHeader& header, int64_t payload_size,
      const absl::flat_hash_map<DecodedUleb128, ParamDefinitionVariant>&
          param_definition_variants,
      ReadBitBuffer& rb);

  /*!\brief Destructor. */
  ~ParameterBlockObu() override = default;

  /*!\brief Interpolate the value of a `MixGainParameterData`.
   *
   * \param mix_gain_parameter_data `MixGainParameterData` to interpolate.
   * \param start_time Start time of the `MixGainParameterData`.
   * \param end_time End time of the `MixGainParameterData`.
   * \param target_time Target time to get the interpolated value of.
   * \param target_mix_gain_db Output inteprolated mix gain value in dB.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status InterpolateMixGainParameterData(
      const MixGainParameterData* mix_gain_parameter_data,
      InternalTimestamp start_time, InternalTimestamp end_time,
      InternalTimestamp target_time, float& target_mix_gain_db);

  /*!\brief Gets the duration of the parameter block.
   *
   * \return Duration of the OBU.
   */
  DecodedUleb128 GetDuration() const;

  /*!\brief Gest the constant subblock interval of the OBU.
   *
   * \return Constant subblock duration of the OBU.
   */
  DecodedUleb128 GetConstantSubblockDuration() const;

  /*!\brief Gets the number of subblocks of the OBU.
   *
   * \return Number of subblocks of the OBU.
   */
  DecodedUleb128 GetNumSubblocks() const;

  /*!\brief Gets the duration of the subblock.
   *
   * \param subblock_index Index of the subblock to get the duration of.
   * \return Duration of the subblock or `absl::InvalidArgumentError()` on
   *         failure.
   */
  absl::StatusOr<DecodedUleb128> GetSubblockDuration(int subblock_index) const;

  /*!\brief Sets the `duration` of a subblock in the output OBU or metadata.
   *
   * May modify the metadata or the OBU as required by `param_definition_mode`.
   * The duration field within the subblock of a `ParameterBlockObu` only has
   * semantic meaning and is serialized with the OBU when
   * `param_definition_mode == 1 && constant_subblock_duration != 0` as per the
   * IAMF spec. This function zeroes out the duration field within the subblock
   * of a `ParameterBlockObu` when it has no semantic meaning.
   *
   * \param subblock_index Index of the subblock to set.
   * \param duration `duration` to set.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` on
   *        failure.
   */
  absl::Status SetSubblockDuration(int subblock_index, DecodedUleb128 duration);

  /*!\brief Outputs the linear mix gain at the target time.
   *
   * \param obu_relative_time Time relative to the start of the OBU to get the
   *        mix gain of.
   * \param linear_mix_gain Output linear mix gain converted from a dB value
   *        stored as Q7.8.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` on
   *         failure.
   */
  absl::Status GetLinearMixGain(InternalTimestamp obu_relative_time,
                                float& linear_mix_gain) const;

  /*!\brief Prints logging information about the OBU.*/
  void PrintObu() const override;

  // Mapped from an Audio Element or Mix Presentation OBU parameter ID.
  const DecodedUleb128 parameter_id_;

  // Length `num_subblocks_`.
  std::vector<ParameterSubblock> subblocks_;

 private:
  /*!\brief Constructor.
   *
   * \param header `ObuHeader` of the OBU.
   * \param param_definition Parameter definition.
   */
  ParameterBlockObu(const ObuHeader& header,
                    const ParamDefinition& param_definition);

  /*!\brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *         failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  /*!\brief Reads the OBU payload from the buffer.
   *
   * \param payload_size Size of the obu payload in bytes.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *        failure.
   */
  absl::Status ReadAndValidatePayloadDerived(int64_t payload_size,
                                             ReadBitBuffer& rb) override;

  // `duration` and `constant_subblock_duration` are conditionally included
  // based on `param_definition_mode`.
  DecodedUleb128 duration_;
  DecodedUleb128 constant_subblock_duration_;

  // `num_subblocks` is only included if `param_definition_mode == 0` and
  // `constant_subblock_duration_ == 0`.
  DecodedUleb128 num_subblocks_;

  // Parameter definition corresponding to this parameter block.
  const ParamDefinition& param_definition_;
};

}  // namespace iamf_tools

#endif  // OBU_PARAMETER_BLOCK_H_
