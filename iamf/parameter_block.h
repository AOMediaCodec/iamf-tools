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
#ifndef PARAMETER_BLOCK_H_
#define PARAMETER_BLOCK_H_

#include <array>
#include <cstdint>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/demixing_info_param_data.h"
#include "iamf/ia.h"
#include "iamf/obu_base.h"
#include "iamf/obu_header.h"
#include "iamf/param_definitions.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

/*!\brief The metadata to describe animation of type `kAnimateStep`. */
struct AnimationStepInt16 {
  friend bool operator==(const AnimationStepInt16& lhs,
                         const AnimationStepInt16& rhs) = default;

  void Print() const;

  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  int16_t start_point_value;
};

/*!\brief The metadata to describe animation of type `kAnimateLinear`. */
struct AnimationLinearInt16 {
  friend bool operator==(const AnimationLinearInt16& lhs,
                         const AnimationLinearInt16& rhs) = default;

  void Print() const;

  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  int16_t start_point_value;
  int16_t end_point_value;
};

/*!\brief The metadata to describe animation of type `kAnimateBezier`. */
struct AnimationBezierInt16 {
  friend bool operator==(const AnimationBezierInt16& lhs,
                         const AnimationBezierInt16& rhs) = default;

  void Print() const;

  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  int16_t start_point_value;
  int16_t end_point_value;
  int16_t control_point_value;
  uint8_t control_point_relative_time;  // Q0.8 format.
};

/*!\brief The metadata for a mix gain parameter. */
struct MixGainParameterData {
  /*!\brief A `DecodedUleb128` enum for the type of animation to apply. */
  enum AnimationType : DecodedUleb128 {
    kAnimateStep = 0,
    kAnimateLinear = 1,
    kAnimateBezier = 2,
  };

  friend bool operator==(const MixGainParameterData& lhs,
                         const MixGainParameterData& rhs) = default;

  AnimationType animation_type;  // Serialized to a ULEB128.

  // The active field depends on `animation_type`.
  std::variant<AnimationStepInt16, AnimationLinearInt16, AnimationBezierInt16>
      param_data;
};

/*!\brief An element of the `ReconGainInfoParameterData` vector.
 *
 * This is not present in the bitstream when recon_gain_is_present_flag(i) == 0
 * in the associated Audio Element OBU.
 */
struct ReconGainElement {
  /*!\brief A `DecodedUleb128` bitmask to determine channels with recon gain.
   *
   * Apply the bitmask to the `ReconGainElement::recon_gain_flag` to determine
   * if recon gain should be applied. Values are offset from the spec as they
   * will be applied to a `DecodedUleb128` instead of a serialized LEB128.
   */
  enum ReconGainFlagBitmask : DecodedUleb128 {
    kReconGainFlagL = 0x01,
    kReconGainFlagC = 0x02,
    kReconGainFlagR = 0x04,
    kReconGainFlagLss = 0x08,
    kReconGainFlagRss = 0x10,
    kReconGainFlagLtf = 0x20,
    kReconGainFlagRtf = 0x40,
    kReconGainFlagLrs = 0x80,
    kReconGainFlagRrs = 0x100,
    kReconGainFlagLtb = 0x200,
    kReconGainFlagRtb = 0x400,
    kReconGainFlagLfe = 0x800,
  };

  // Apply the `ReconGainFlagBitmaskDecodedUleb` bitmask to determine which
  // channels recon gain should be applied to.
  DecodedUleb128 recon_gain_flag;

  // Value is only present in the stream for channels with Recon Gain flag set.
  std::array<uint8_t, 12> recon_gain;
};

/*!\brief The metadata for a recon gain parameter. */
struct ReconGainInfoParameterData {
  // vector of length `num_layers` in the Audio associated Audio Element OBU.
  std::vector<ReconGainElement> recon_gain_elements;
};

struct ExtensionParameterData {
  DecodedUleb128 parameter_data_size;
  std::vector<uint8_t> parameter_data_bytes;
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

struct PerIdParameterMetadata {
  ParamDefinition::ParameterDefinitionType param_definition_type;

  // Common (base) part of the parameter definition.
  ParamDefinition param_definition;

  // Below are from the Audio Element. Only used when `param_definition_type` =
  // `kParameterDefinitionReconGain`.
  uint32_t audio_element_id;
  uint8_t num_layers;

  // Whether recon gain is present per layer.
  std::vector<bool> recon_gain_is_present_flags;

  // Channel numbers per layer.
  std::vector<ChannelNumbers> channel_numbers_for_layers;
};

/*!\brief An element of the Parameter Block OBU's `subblocks` vector. */
struct ParameterSubblock {
  // `subblock_duration` is conditionally included based on
  // `param_definition_mode` and `constant_subblock_duration`.
  DecodedUleb128 subblock_duration;

  // The active field depends on `param_definition_type` in the metadata.
  std::variant<MixGainParameterData, DemixingInfoParameterData,
               ReconGainInfoParameterData, ExtensionParameterData>
      param_data;
};

/*!\brief A Parameter Block OBU.
 *
 * The metadata specified in this OBU defines the parameter values for an
 * algorithm for an indicated duration, including any animation of the parameter
 * values over this duration.
 */
class ParameterBlockObu : public ObuBase {
 public:
  /*!\brief Constructor.
   *
   * After constructing `InitializeSubblocks()` MUST be called and return
   * successfully before using most functionality of the OBU.
   *
   * \param header `ObuHeader` of the OBU.
   * \param parameter_id Parameter ID.
   * \param metadata Per-ID parameter metadata.
   */
  ParameterBlockObu(const ObuHeader& header, DecodedUleb128 parameter_id,
                    PerIdParameterMetadata* metadata);

  /*!\brief Destructor. */
  ~ParameterBlockObu() override = default;

  /*!\brief Interpolate the value of a `MixGainParameterData`.
   *
   * \param mix_gain_parameter_data `MixGainParameterData` to interpolate.
   * \param start_time Start time of the `MixGainParameterData`.
   * \param end_time End time of the `MixGainParameterData`.
   * \param target_time Target time to the get interpolated value of.
   * \param target_mix_gain Output argument for the inteprolated value.
   * \return `absl::OkStatus()` on success. A specific status on failure.
   */
  static absl::Status InterpolateMixGainParameterData(
      const MixGainParameterData& mix_gain_parameter_data, int32_t start_time,
      int32_t end_time, int32_t target_time, int16_t& target_mix_gain);

  /*\!brief Gets the duration of the parameter block.
   *
   * \return Duration of the OBU.
   */
  DecodedUleb128 GetDuration() const;

  /*\!brief Gest the constant subblock interval of the OBU.
   *
   * \return Constant subblock duration of the OBU.
   */
  DecodedUleb128 GetConstantSubblockDuration() const;

  /*\!brief Gets the number of subblocks of the OBU.
   *
   * \return Number of subblocks of the OBU.
   */
  DecodedUleb128 GetNumSubblocks() const;

  /*\!brief Gets the duration of the subblock.
   *
   * \param subblock_index Index of the subblock to get the duration of.
   * \return Duration of the subblock or `absl::InvalidArgumentError()` on
   *     failure.
   */
  absl::StatusOr<DecodedUleb128> GetSubblockDuration(int subblock_index) const;

  /*\!brief Sets the `duration` of a subblock in the output OBU or metadata.
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
   *     failure.
   */
  absl::Status SetSubblockDuration(int subblock_index, DecodedUleb128 duration);

  /*\!brief Writes the mix gain at the target time to the output argument.
   *
   * \param obu_relative_time Time relative to the start of the OBU to get the
   *     mix gain of.
   * \param mix_gain Output argument for the mix gain.
   * \return `absl::OkStatus()` on success. `absl::InvalidArgumentError()` on
   *     failure.
   */
  absl::Status GetMixGain(int32_t obu_relative_time, int16_t& mix_gain) const;

  /*\!brief Initialize the vector of subblocks.
   *
   * \param duration Duration of the parameter block.
   * \param constant_subblock_duration Constant subblock duration.
   * \param num_subblocks Number of subblocks.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status InitializeSubblocks(DecodedUleb128 duration,
                                   DecodedUleb128 constant_subblock_duration,
                                   DecodedUleb128 num_subblocks);

  /*\!brief Initialize the vector of subblocks using existing information.
   *
   * This should only be called if the `param_definition_mode == 0`,
   * and the `duration`, `constant_subblock_duration`, and `num_subblocks`
   * defined in the `metadata_.param_definition` are already correct.
   *
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status InitializeSubblocks();

  void PrintObu() const override;

  // Mapped from an Audio Element or Mix Presentation OBU parameter ID.
  const DecodedUleb128 parameter_id_;

  // Length `num_subblocks_`.
  std::vector<ParameterSubblock> subblocks_;

 private:
  /*\!brief Sets the `duration` of the output OBU or metadata.
   *
   * May modify the metadata or the OBU as required by `param_definition_mode`.
   *
   * \param duration `duration` to set.
   */
  void SetDuration(DecodedUleb128 duration);

  /*\!brief Sets the `constant_subblock_duration` of the output OBU or metadata.
   *
   * May modify the metadata or the OBU as required by `param_definition_mode`.
   *
   * \param constant_subblock_duration `constant_subblock_duration` to set.
   */
  void SetConstantSubblockDuration(DecodedUleb128 constant_subblock_duration);

  /*\!brief Sets the `num_subblocks` of the output OBU or metadata.
   *
   * May modify the metadata or the OBU as required by `param_definition_mode`.
   *
   * \param num_subblocks `num_subblocks` to set.
   */
  void SetNumSubblocks(DecodedUleb128 num_subblocks);

  /*\!brief Writes the OBU payload to the buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if the payload is valid. A specific status on
   *     failure.
   */
  absl::Status ValidateAndWritePayload(WriteBitBuffer& wb) const override;

  // `duration` and `constant_subblock_duration` are conditionally included
  // based on `param_definition_mode`.
  DecodedUleb128 duration_;
  DecodedUleb128 constant_subblock_duration_;

  // `num_subblocks` is only included if `param_definition_mode == 0` and
  // `constant_subblock_duration_ == 0`.
  DecodedUleb128 num_subblocks_;

  // Per-ID parameter metadata.
  PerIdParameterMetadata* metadata_;

  // Tracks whether the OBU was initialized correctly.
  absl::Status init_status_ = absl::UnknownError("");
};

}  // namespace iamf_tools

#endif  // PARAMETER_BLOCK_H_
