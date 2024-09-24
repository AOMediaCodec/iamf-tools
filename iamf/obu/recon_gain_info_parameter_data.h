/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_RECON_GAIN_INFO_PARAMETER_DATA_H_
#define OBU_RECON_GAIN_INFO_PARAMETER_DATA_H_

#include <array>
#include <cstdint>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/parameter_data.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief An element of the vector inside the `ReconGainInfoParameterData`.
 *
 * This is not present in the bitstream when
 * `recon_gain_is_present_flags(i) == 0` in the associated Audio Element OBU.
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

struct ReconGainInfoParameterData : public ParameterData {
  /*!\brief Constructor.
   *
   * \param input_recon_gain_elements Input vector of recon gain elements.
   */
  ReconGainInfoParameterData(
      const std::vector<ReconGainElement>& input_recon_gain_elements)
      : ParameterData(), recon_gain_elements(input_recon_gain_elements) {}
  ReconGainInfoParameterData() = default;

  /*!\brief Overridden destructor.*/
  ~ReconGainInfoParameterData() override = default;

  /*!\brief Reads and validates a `ReconGainInfoParameterData` from a buffer.
   *
   * \param per_id_metadata Per-ID parameter metadata.
   * \param rb Buffer to read from.
   * \return `absl::OkStatus()`. A specific error code on failure.
   */
  absl::Status ReadAndValidate(const PerIdParameterMetadata& per_id_metadata,
                               ReadBitBuffer& rb) override;

  /*!\brief Validates and writes to a buffer.
   *
   * \param per_id_metadata Per-ID parameter metadata.
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status Write(const PerIdParameterMetadata& per_id_metadata,
                     WriteBitBuffer& wb) const override;

  /*!\brief Prints the recon gain info parameter data.
   */
  void Print() const override;

  // Vector of length `num_layers` in the Audio associated Audio Element OBU.
  std::vector<ReconGainElement> recon_gain_elements;
};

}  // namespace iamf_tools

#endif  // OBU_RECON_GAIN_INFO_PARAMETER_DATA_H_
