/*
 * Copyright (c) 2026, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#ifndef OBU_MOMENTARY_LOUDNESS_H_
#define OBU_MOMENTARY_LOUDNESS_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/param_definitions/momentary_loudness_param_definition.h"
#include "iamf/obu/types.h"

namespace iamf_tools {

/*!\brief Momentary loudness information in `LoudnessInfo`.
 */
class MomentaryLoudness {
 public:
  friend bool operator==(const MomentaryLoudness& lhs,
                         const MomentaryLoudness& rhs) = default;

  /*!\brief Creates `MomentaryLoudness` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return Deserialized `MomentaryLoudness` or specific status on failure.
   */
  static absl::StatusOr<MomentaryLoudness> CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Creates `MomentaryLoudness` from values.
   *
   * \param momentary_loudness_param_def Parameter definition for momentary
   *        loudness information.
   * \param bin_width_minus_one 6-bit value specifying range covered by each
   *        histogram bin as `bin_width_minus_one + 1` LUs.
   * \param first_bin_center 6-bit magnitude of momentary loudness at center of
   *        first histogram bin in LKFS (actual center loudness value is
   *        `-1 * first_bin_center`).
   * \param reserved_momentary_loudness 1-bit reserved field.
   * \param count Histogram bin counts, where `count.size()` must be an even
   *        number in `[2, 16]` corresponding to
   *        `(num_bin_pairs_minus_one + 1) * 2`.
   * \return `MomentaryLoudness` or error if fields are out of bounds.
   */
  static absl::StatusOr<MomentaryLoudness> Create(
      const MomentaryLoudnessParamDefinition& momentary_loudness_param_def,
      uint8_t bin_width_minus_one, uint8_t first_bin_center,
      uint8_t reserved_momentary_loudness,
      absl::Span<const DecodedUleb128> count);

  /*!\brief Writes `MomentaryLoudness` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. Specific status on failure.
   */
  absl::Status Write(WriteBitBuffer& wb) const;

  /*!\brief Returns string representation of `MomentaryLoudness`.
   *
   * \return String representation of `MomentaryLoudness`.
   */
  std::string ToString() const;

  /*!\brief Supports Abseil stringification. */
  template <typename Sink>
  friend void AbslStringify(Sink& sink,
                            const MomentaryLoudness& momentary_loudness) {
    sink.Append(momentary_loudness.ToString());
  }

  const MomentaryLoudnessParamDefinition& momentary_loudness_param_def() const {
    return momentary_loudness_param_def_;
  }
  uint8_t num_bin_pairs_minus_one() const {
    return static_cast<uint8_t>(count_.size() / 2 - 1);
  }
  uint8_t bin_width_minus_one() const { return bin_width_minus_one_; }
  uint8_t first_bin_center() const { return first_bin_center_; }
  uint8_t reserved_momentary_loudness() const {
    return reserved_momentary_loudness_;
  }
  absl::Span<const DecodedUleb128> count() const { return count_; }

 private:
  MomentaryLoudness(
      MomentaryLoudnessParamDefinition momentary_loudness_param_def,
      uint8_t bin_width_minus_one, uint8_t first_bin_center,
      uint8_t reserved_momentary_loudness, std::vector<DecodedUleb128> count)
      : momentary_loudness_param_def_(std::move(momentary_loudness_param_def)),
        bin_width_minus_one_(bin_width_minus_one),
        first_bin_center_(first_bin_center),
        reserved_momentary_loudness_(reserved_momentary_loudness),
        count_(std::move(count)) {}

  MomentaryLoudnessParamDefinition momentary_loudness_param_def_;
  uint8_t bin_width_minus_one_ = 0;          // 6 bits.
  uint8_t first_bin_center_ = 0;             // 6 bits.
  uint8_t reserved_momentary_loudness_ = 0;  // 1 bit.
  // `num_bin_pairs_minus_one` (3 bits) is implicit based on the size of
  // `count_`, where `count_.size() == (num_bin_pairs_minus_one + 1) * 2`.
  std::vector<DecodedUleb128> count_;
};

}  // namespace iamf_tools

#endif  // OBU_MOMENTARY_LOUDNESS_H_
