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
#ifndef OBU_LOUDNESS_INFO_H_
#define OBU_LOUDNESS_INFO_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/momentary_loudness.h"

namespace iamf_tools {

struct AnchoredLoudnessElement {
  /*!\brief A 8-bit enum for the associated loudness measurement.
   *
   * As defined in ISO-CICP.
   */
  enum AnchorElement : uint8_t {
    kAnchorElementUnknown = 0,
    kAnchorElementDialogue = 1,
    kAnchorElementAlbum = 2,
  };

  friend bool operator==(const AnchoredLoudnessElement& lhs,
                         const AnchoredLoudnessElement& rhs) = default;

  AnchorElement anchor_element;  // 8 bits.
  QFormatOrFloatingPoint anchored_loudness =
      QFormatOrFloatingPoint::MakeFromQ7_8(0);
};

struct AnchoredLoudness {
  friend bool operator==(const AnchoredLoudness& lhs,
                         const AnchoredLoudness& rhs) = default;

  // `num_anchored_loudness` is implicit based on the size of
  // `anchor_elements`.
  std::vector<AnchoredLoudnessElement> anchor_elements = {};
};

struct LayoutExtension {
  friend bool operator==(const LayoutExtension& lhs,
                         const LayoutExtension& rhs) = default;

  // Stores the bits of `info_type` corresponding to `kAnyLayoutExtension`. This
  // is needed because multiple bits in `kAnyLayoutExtension` can trigger the
  // presence of `layout_extension`, so `layout_extension.has_value()` alone is
  // not sufficient to reconstruct those bits in `LoudnessInfo::GetInfoType()`.
  uint8_t info_type = 0;

  // `info_type_size` is implicit based on the size of `info_type_bytes`
  std::vector<uint8_t> info_type_bytes;
};

/*!\brief The loudness information for a given audio signal. */
class LoudnessInfo {
 public:
  /*!\brief A 8-bit bitmask to determine the included optional loudness types.
   */
  enum InfoTypeBitmask : uint8_t {
    kTruePeak = 0x01,
    kAnchoredLoudness = 0x02,
    kLive = 0x04,
    kMomentaryLoudness = 0x08,
    kInfoTypeBitMask8 = 0x08,
    kInfoTypeBitMask16 = 0x10,
    kInfoTypeBitMask32 = 0x20,
    kInfoTypeBitMask64 = 0x40,
    kInfoTypeBitMask128 = 0x80,
    // For backwards compatibility several info types signal the need
    // for a `layout_extension`.
    kAnyLayoutExtension = 0xfc,
  };

  friend bool operator==(const LoudnessInfo& lhs,
                         const LoudnessInfo& rhs) = default;

  /*!\brief Creates a `LoudnessInfo` from a buffer.
   *
   * \param rb Buffer to read from.
   * \return `LoudnessInfo` if successful. A specific status on failure.
   */
  static absl::StatusOr<LoudnessInfo> CreateFromBuffer(ReadBitBuffer& rb);

  /*!\brief Validates and writes the `LoudnessInfo` to a buffer.
   *
   * \param wb Buffer to write to.
   * \return `absl::OkStatus()` if successful. A specific status on failure.
   */
  absl::Status ValidateAndWrite(WriteBitBuffer& wb) const;

  /*!\brief Gets the `info_type` bitfield computed from the present fields.
   *
   * \return `info_type` bitfield.
   */
  uint8_t GetInfoType() const;

  /*!\brief Returns string representation of `LoudnessInfo`.
   *
   * \return String representation of `LoudnessInfo`.
   */
  std::string ToString() const;

  /*!\brief Supports Abseil stringification. */
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const LoudnessInfo& loudness_info) {
    sink.Append(loudness_info.ToString());
  }

  QFormatOrFloatingPoint integrated_loudness =
      QFormatOrFloatingPoint::MakeFromQ7_8(0);
  QFormatOrFloatingPoint digital_peak = QFormatOrFloatingPoint::MakeFromQ7_8(0);

  // Present if `(info_type & kTruePeak) != 0`.
  std::optional<QFormatOrFloatingPoint> true_peak;

  // Present if `(info_type & kAnchoredLoudness) != 0`.
  std::optional<AnchoredLoudness> anchored_loudness;

  // Present if `(info_type & kMomentaryLoudness) != 0`.
  std::optional<MomentaryLoudness> momentary_loudness;

  // Present if `(info_type & kAnyLayoutExtension) != 0`.
  std::optional<LayoutExtension> layout_extension;
};

}  // namespace iamf_tools

#endif  // OBU_LOUDNESS_INFO_H_
