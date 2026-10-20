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
#include "iamf/obu/loudness_info.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_macros.h"
#include "absl/status/statusor.h"
#include "absl/types/span.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/macros.h"
#include "iamf/common/utils/validation_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

absl::Status ValidateUniqueAnchorElements(
    absl::Span<const AnchoredLoudnessElement> anchor_elements) {
  std::vector<uint8_t> anchor_elements_as_uint8;
  anchor_elements_as_uint8.reserve(anchor_elements.size());
  for (const auto& anchor_element : anchor_elements) {
    anchor_elements_as_uint8.push_back(
        static_cast<uint8_t>(anchor_element.anchor_element));
  }
  return ValidateUnique(anchor_elements_as_uint8.begin(),
                        anchor_elements_as_uint8.end(),
                        "Anchored loudness types");
}

absl::StatusOr<QFormatOrFloatingPoint> ReadQ7_8(ReadBitBuffer& rb) {
  int16_t q7_8;
  RETURN_IF_NOT_OK(rb.ReadSigned16(q7_8));
  return QFormatOrFloatingPoint::MakeFromQ7_8(q7_8);
}

}  // namespace

absl::StatusOr<LoudnessInfo> LoudnessInfo::CreateFromBuffer(ReadBitBuffer& rb) {
  LoudnessInfo loudness;
  RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, loudness.info_type));
  ABSL_ASSIGN_OR_RETURN(loudness.integrated_loudness, ReadQ7_8(rb));
  ABSL_ASSIGN_OR_RETURN(loudness.digital_peak, ReadQ7_8(rb));

  // Conditionally read `true_peak` based on `info_type`.
  if ((loudness.info_type & kTruePeak) != 0) {
    ABSL_ASSIGN_OR_RETURN(loudness.true_peak, ReadQ7_8(rb));
  }
  // Conditionally read `anchored_loudness` based on `info_type`.
  if ((loudness.info_type & kAnchoredLoudness) != 0) {
    uint8_t num_anchored_loudness;
    RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, num_anchored_loudness));

    AnchoredLoudness& anchored_loudness = loudness.anchored_loudness.emplace();
    anchored_loudness.anchor_elements.resize(num_anchored_loudness);
    for (auto& anchor_loudness_element : anchored_loudness.anchor_elements) {
      uint8_t anchor_element;
      RETURN_IF_NOT_OK(rb.ReadUnsignedLiteral(8, anchor_element));
      anchor_loudness_element.anchor_element =
          static_cast<AnchoredLoudnessElement::AnchorElement>(anchor_element);
      ABSL_ASSIGN_OR_RETURN(anchor_loudness_element.anchored_loudness,
                            ReadQ7_8(rb));
    }
    RETURN_IF_NOT_OK(
        ValidateUniqueAnchorElements(anchored_loudness.anchor_elements));
  }
  // Conditionally read `layout_extension` based on `info_type`.
  if ((loudness.info_type & kAnyLayoutExtension) != 0) {
    DecodedUleb128 info_type_size;
    RETURN_IF_NOT_OK(rb.ReadULeb128(info_type_size));
    RETURN_IF_NOT_OK(ValidateInRange(
        info_type_size, {DecodedUleb128{0}, kEntireObuSizeMaxTwoMegabytes},
        "info_type_size"));
    LayoutExtension& layout_extension = loudness.layout_extension.emplace();
    layout_extension.info_type_bytes.resize(info_type_size);
    RETURN_IF_NOT_OK(
        rb.ReadUint8Span(absl::MakeSpan(layout_extension.info_type_bytes)));
  }

  return loudness;
}

absl::Status LoudnessInfo::ValidateAndWrite(WriteBitBuffer& /*wb*/) const {
  return absl::UnimplementedError(
      "LoudnessInfo::ValidateAndWrite is not implemented.");
}

std::string LoudnessInfo::ToString() const { return ""; }

}  // namespace iamf_tools
