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
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/common/q_format_or_floating_point.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/common/utils/tests/test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::HasSubstr;
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

TEST(ValidateAndWrite, WritesMinimalLoudnessInfo) {
  const LoudnessInfo loudness_info = {
      .info_type = 0,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
      // `info_type`.
      0,
      // `integrated_loudness`.
      0,
      18,
      // `digital_peak`.
      0,
      19,
  });
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, WritesAllOptionalFields) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kTruePeak | LoudnessInfo::kAnchoredLoudness |
                   LoudnessInfo::kInfoTypeBitMask128,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .true_peak = kTruePeak,
      .anchored_loudness =
          AnchoredLoudness{
              .anchor_elements =
                  {{.anchor_element =
                        AnchoredLoudnessElement::kAnchorElementAlbum,
                    .anchored_loudness = kAnchoredLoudness0},
                   {.anchor_element =
                        AnchoredLoudnessElement::kAnchorElementDialogue,
                    .anchored_loudness = kAnchoredLoudness1}}},
      .layout_extension = LayoutExtension{.info_type_bytes = {'a', 'b'}}};
  constexpr auto kExpectedPayload = std::to_array<uint8_t>({
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
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), IsOk());

  ValidateWriteResults(wb, kExpectedPayload);
}

TEST(ValidateAndWrite, ReturnsErrorWhenTruePeakIsMissing) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kTruePeak,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .true_peak = std::nullopt};
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), Not(IsOk()));
}

TEST(ValidateAndWrite, ReturnsErrorWhenAnchoredLoudnessIsMissing) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kAnchoredLoudness,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .anchored_loudness = std::nullopt};
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), Not(IsOk()));
}

TEST(ValidateAndWrite, ReturnsErrorWhenLayoutExtensionIsMissing) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kLive,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .layout_extension = std::nullopt};
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), Not(IsOk()));
}

TEST(ValidateAndWrite, ReturnsErrorWhenAnchorElementsAreNotUnique) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kAnchoredLoudness,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .anchored_loudness = AnchoredLoudness{
          .anchor_elements = {
              {.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
               .anchored_loudness = kAnchoredLoudness0},
              {.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
               .anchored_loudness = kAnchoredLoudness1}}}};
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), Not(IsOk()));
}

TEST(ValidateAndWrite, ReturnsErrorWhenTooManyAnchorElements) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kAnchoredLoudness,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .anchored_loudness = AnchoredLoudness{
          .anchor_elements = std::vector<AnchoredLoudnessElement>(
              256,
              {.anchor_element = AnchoredLoudnessElement::kAnchorElementAlbum,
               .anchored_loudness = kAnchoredLoudness0})}};
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), Not(IsOk()));
}

TEST(ValidateAndWrite, ReturnsErrorWhenInfoTypeSizeExceedsMaxObuSize) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kInfoTypeBitMask128,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .layout_extension =
          LayoutExtension{.info_type_bytes = std::vector<uint8_t>(
                              kEntireObuSizeMaxTwoMegabytes + 1, 0)}};
  WriteBitBuffer wb(0);

  EXPECT_THAT(loudness_info.ValidateAndWrite(wb), Not(IsOk()));
}

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

TEST(ToString, FormatsFieldsAndSupportsAbslStringify) {
  const LoudnessInfo loudness_info = {
      .info_type = LoudnessInfo::kTruePeak | LoudnessInfo::kAnchoredLoudness |
                   LoudnessInfo::kInfoTypeBitMask128,
      .integrated_loudness = kIntegratedLoudness,
      .digital_peak = kDigitalPeak,
      .true_peak = kTruePeak,
      .anchored_loudness =
          AnchoredLoudness{
              .anchor_elements =
                  {{.anchor_element =
                        AnchoredLoudnessElement::kAnchorElementAlbum,
                    .anchored_loudness = kAnchoredLoudness1}}},
      .layout_extension = LayoutExtension{.info_type_bytes = {'a'}}};

  const std::string result = loudness_info.ToString();

  EXPECT_EQ(absl::StrCat(loudness_info), result);
  EXPECT_THAT(result, HasSubstr("info_type= 131"));
  EXPECT_THAT(result, HasSubstr("anchor_element[0]= 2"));
  EXPECT_THAT(result, HasSubstr("info_type_bytes[0]= 97"));
}

}  // namespace
}  // namespace iamf_tools
