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

#include <array>
#include <cstdint>
#include <optional>

#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

const QFormatOrFloatingPoint kIntegratedLoudness =
    QFormatOrFloatingPoint::MakeFromQ7_8(18);
const QFormatOrFloatingPoint kDigitalPeak =
    QFormatOrFloatingPoint::MakeFromQ7_8(19);
const QFormatOrFloatingPoint kTruePeak =
    QFormatOrFloatingPoint::MakeFromQ7_8(20);
const QFormatOrFloatingPoint kAnchoredLoudness0 =
    QFormatOrFloatingPoint::MakeFromQ7_8(20);
const QFormatOrFloatingPoint kAnchoredLoudness1 =
    QFormatOrFloatingPoint::MakeFromQ7_8(21);

TEST(CreateFromBuffer, ReadsMinimalLoudnessInfo) {
  constexpr auto kPayload = std::to_array<uint8_t>({
      // `info_type`.
      0,
      // `integrated_loudness`.
      0,
      18,
      // `digital_peak`.
      0,
      19,
  });
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kPayload);

  const auto loudness_info = LoudnessInfo::CreateFromBuffer(*rb);

  ASSERT_THAT(loudness_info, IsOk());
  EXPECT_EQ(loudness_info->info_type, 0);
  EXPECT_EQ(loudness_info->integrated_loudness, kIntegratedLoudness);
  EXPECT_EQ(loudness_info->digital_peak, kDigitalPeak);
  EXPECT_EQ(loudness_info->true_peak, std::nullopt);
  EXPECT_EQ(loudness_info->anchored_loudness, std::nullopt);
  EXPECT_EQ(loudness_info->layout_extension, std::nullopt);
  EXPECT_FALSE(rb->IsDataAvailable());
}

TEST(CreateFromBuffer, ReadsAllOptionalFields) {
  constexpr auto kPayload = std::to_array<uint8_t>({
      // `info_type`.
      LoudnessInfo::kTruePeak | LoudnessInfo::kAnchoredLoudness |
          LoudnessInfo::kInfoTypeBitMask128,
      // `integrated_loudness`.
      0,
      18,
      // `digital_peak`.
      0,
      19,
      // `true_peak`.
      0,
      20,
      // `num_anchored_loudness`.
      2,
      // `anchor_elements[0]`.
      AnchoredLoudnessElement::kAnchorElementAlbum,
      0,
      20,
      // `anchor_elements[1]`.
      AnchoredLoudnessElement::kAnchorElementDialogue,
      0,
      21,
      // `info_type_size`.
      2,
      // `info_type_bytes`.
      'a',
      'b',
  });
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kPayload);

  const auto loudness_info = LoudnessInfo::CreateFromBuffer(*rb);

  ASSERT_THAT(loudness_info, IsOk());
  EXPECT_EQ(loudness_info->info_type, LoudnessInfo::kTruePeak |
                                          LoudnessInfo::kAnchoredLoudness |
                                          LoudnessInfo::kInfoTypeBitMask128);
  EXPECT_EQ(loudness_info->integrated_loudness, kIntegratedLoudness);
  EXPECT_EQ(loudness_info->digital_peak, kDigitalPeak);
  EXPECT_EQ(loudness_info->true_peak, kTruePeak);
  EXPECT_EQ(
      loudness_info->anchored_loudness,
      (AnchoredLoudness{
          .anchor_elements = {
              {.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
               .anchored_loudness = kAnchoredLoudness0},
              {.anchor_element =
                   AnchoredLoudnessElement::kAnchorElementDialogue,
               .anchored_loudness = kAnchoredLoudness1}}}));
  EXPECT_EQ(loudness_info->layout_extension,
            (LayoutExtension{.info_type_bytes = {'a', 'b'}}));
  EXPECT_FALSE(rb->IsDataAvailable());
}

TEST(CreateFromBuffer, ReturnsErrorWhenAnchorElementsAreNotUnique) {
  constexpr auto kPayload = std::to_array<uint8_t>({
      // `info_type`.
      LoudnessInfo::kAnchoredLoudness,
      // `integrated_loudness`.
      0,
      18,
      // `digital_peak`.
      0,
      19,
      // `num_anchored_loudness`.
      2,
      // `anchor_elements[0]`.
      AnchoredLoudnessElement::kAnchorElementAlbum,
      0,
      20,
      // `anchor_elements[1]`.
      AnchoredLoudnessElement::kAnchorElementAlbum,
      0,
      21,
  });
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kPayload);

  EXPECT_THAT(LoudnessInfo::CreateFromBuffer(*rb), Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorWhenInfoTypeSizeExceedsMaxObuSize) {
  constexpr auto kPayload = std::to_array<uint8_t>({
      // `info_type`.
      LoudnessInfo::kInfoTypeBitMask128,
      // `integrated_loudness`.
      0,
      18,
      // `digital_peak`.
      0,
      19,
      // `info_type_size` = 2 MB + 1.
      0x81,
      0x80,
      0x80,
      0x01,
  });
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kPayload);

  EXPECT_THAT(LoudnessInfo::CreateFromBuffer(*rb), Not(IsOk()));
}

TEST(CreateFromBuffer, ReturnsErrorWhenBufferIsExhausted) {
  constexpr auto kPayload = std::to_array<uint8_t>({
      // `info_type`.
      0,
      // `integrated_loudness` (missing `digital_peak`).
      0,
      18,
  });
  auto rb = MemoryBasedReadBitBuffer::CreateFromSpan(kPayload);

  EXPECT_THAT(LoudnessInfo::CreateFromBuffer(*rb), Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
