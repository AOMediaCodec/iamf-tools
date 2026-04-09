/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/descriptor_obus.h"

#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::testing::IsEmpty;

constexpr DecodedUleb128 kCodecConfigId = 9999;
constexpr DecodedUleb128 kAudioElementId = 9999;

TEST(DescriptorObusConstructor, CodecConfigsByIdIsValidAndEmpty) {
  DescriptorObus descriptor_obus;

  EXPECT_THAT(descriptor_obus.codec_config_obus, IsEmpty());
}

TEST(DescriptorObusConstructor, AudioElementsIsValidAndEmpty) {
  DescriptorObus descriptor_obus;

  EXPECT_THAT(descriptor_obus.audio_elements, IsEmpty());
}

TEST(DescriptorObusConstructor, MixPresentationObusIsEmpty) {
  DescriptorObus descriptor_obus;

  EXPECT_TRUE(descriptor_obus.mix_presentation_obus.empty());
}

TEST(DescriptorObus, CodecConfigObuPointerStabilityAfterMove) {
  DescriptorObus descriptor_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);
  const CodecConfigObu* initial_address =
      &descriptor_obus.codec_config_obus.at(kCodecConfigId);

  DescriptorObus moved_descriptor_obus = std::move(descriptor_obus);

  EXPECT_EQ(initial_address,
            &moved_descriptor_obus.codec_config_obus.at(kCodecConfigId));
}

TEST(DescriptorObus, CodecConfigObuPointerStabilityAfterInsert) {
  DescriptorObus descriptor_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);
  const CodecConfigObu* initial_address =
      &descriptor_obus.codec_config_obus.at(kCodecConfigId);

  for (int i = 2; i < 100; ++i) {
    AddOpusCodecConfigWithId(i, descriptor_obus.codec_config_obus);
  }

  EXPECT_EQ(initial_address,
            &descriptor_obus.codec_config_obus.at(kCodecConfigId));
}

TEST(DescriptorObus, AudioElementsPointerStabilityAfterMove) {
  DescriptorObus descriptor_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, {0}, descriptor_obus.codec_config_obus,
      descriptor_obus.audio_elements);
  const AudioElementWithData* initial_address =
      &descriptor_obus.audio_elements.at(kAudioElementId);

  DescriptorObus moved_descriptor_obus = std::move(descriptor_obus);

  EXPECT_EQ(initial_address,
            &moved_descriptor_obus.audio_elements.at(kAudioElementId));
}

TEST(DescriptorObus, AudioElementsWithDataPointerStabilityAfterInsert) {
  DescriptorObus descriptor_obus;
  AddOpusCodecConfigWithId(kCodecConfigId, descriptor_obus.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kAudioElementId, kCodecConfigId, /*substream_ids=*/{9999},
      descriptor_obus.codec_config_obus, descriptor_obus.audio_elements);
  const AudioElementWithData* initial_address =
      &descriptor_obus.audio_elements.at(kAudioElementId);

  for (DecodedUleb128 i = 0; i < 100; ++i) {
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        i, kCodecConfigId, /*substream_ids=*/std::vector<DecodedUleb128>{i},
        descriptor_obus.codec_config_obus, descriptor_obus.audio_elements);
  }

  EXPECT_EQ(initial_address,
            &descriptor_obus.audio_elements.at(kAudioElementId));
}

}  // namespace
}  // namespace iamf_tools
