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
#ifndef DEMIXING_INFO_PARAM_DATA_H_
#define DEMIXING_INFO_PARAM_DATA_H_

#include <cstdint>

#include "absl/status/status.h"
#include "iamf/write_bit_buffer.h"

namespace iamf_tools {

struct DownMixingParams {
  double alpha;
  double beta;
  double gamma;
  double delta;
  int w_idx_offset;
  int w_idx_used;
  double w;
  bool in_bitstream;
};

/*!\brief A 3-bit enum for the demixing info parameter. */
struct DemixingInfoParameterData {
  enum DMixPMode : uint8_t {
    kDMixPMode1 = 0,
    kDMixPMode2 = 1,
    kDMixPMode3 = 2,
    kDMixPModeReserved1 = 3,
    kDMixPMode1_n = 4,
    kDMixPMode2_n = 5,
    kDMixPMode3_n = 6,
    kDMixPModeReserved2 = 7,
  };

  enum WIdxUpdateRule {
    kNormal = 0,
    kFirstFrame = 1,
    kDefault = 2,
  };

  /*!\brief Default destructor.
   */
  virtual ~DemixingInfoParameterData() = default;

  bool friend operator==(const DemixingInfoParameterData& a,
                         const DemixingInfoParameterData& b) = default;

  /*!\brief Fill in the input `DownMixingParams` based on the `DMixPMode`.
   *
   * \param dmixp_mode Input demixing mode.
   * \param previous_w_idx Used to determine the value of `w`. Must be in the
   *     range [0, 10]. Pass in `default_w` when `w_idx_update_rule ==
   *     kDefault`.
   * \param w_idx_update_rule Rule to update `w_idx`. According to the Spec,
   *     there are two special rules: when the frame index == 0 and when
   *     the `default_w` should be used.
   * \param down_mixing_params Output demixing parameters.
   * \return `absl::OkStatus()` if successful. `absl::InvalidArgumentError()` if
   *     the `dmixp_mode` is unknown or `w_idx` is out of range.
   */
  static absl::Status DMixPModeToDownMixingParams(
      DMixPMode dmixp_mode, int previous_w_idx,
      WIdxUpdateRule w_idx_update_rule, DownMixingParams& down_mixing_params);

  DMixPMode dmixp_mode;  // 3 bits
  uint8_t reserved;      // 5 bits

  /*!\brief Validates and writes a `DemixingInfoParameterData` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  virtual absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Prints the demixing info parameter data.
   */
  virtual void Print() const;
};

struct DefaultDemixingInfoParameterData : public DemixingInfoParameterData {
  /*!\brief Overridden destructor.
   */
  ~DefaultDemixingInfoParameterData() override {}

  bool friend operator==(const DefaultDemixingInfoParameterData& a,
                         const DefaultDemixingInfoParameterData& b) = default;

  uint8_t default_w;         // 4 bits.
  uint8_t reserved_default;  // 4 bits.

  /*!\brief Validates and writes to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const override;

  /*!\brief Prints the default demixing info parameter data.
   */
  void Print() const override;
};

}  // namespace iamf_tools

#endif  // DEMIXING_INFO_PARAM_DATA_H_
