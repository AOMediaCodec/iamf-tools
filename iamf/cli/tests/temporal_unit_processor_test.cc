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

#include "iamf/cli/temporal_unit_processor.h"

#include <cstdint>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/descriptor_obus.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::testing::Not;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kSecondAudioElementId = 3;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

DescriptorObus InitDescriptorsForZerothOrderAmbisonics() {
  DescriptorObus descriptors;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, descriptors.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate,
      descriptors.mix_presentation_obus);
  return descriptors;
}

TEST(Create, SucceedsWithValidDescriptors) {
  const auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));

  EXPECT_THAT(TemporalUnitProcessor::Create(descriptors.audio_elements,
                                            descriptors.mix_presentation_obus,
                                            read_bit_buffer.get()),
              IsOk());
}

TEST(Create, FailsWithDuplicateSubstreamIds) {
  auto descriptors = InitDescriptorsForZerothOrderAmbisonics();
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));

  EXPECT_THAT(TemporalUnitProcessor::Create(descriptors.audio_elements,
                                            descriptors.mix_presentation_obus,
                                            read_bit_buffer.get()),
              Not(IsOk()));
}

TEST(Create, FailsWithZeroParameterRate) {
  DescriptorObus descriptors;
  AddOpusCodecConfigWithId(kFirstCodecConfigId, descriptors.codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      descriptors.codec_config_obus, descriptors.audio_elements);
  constexpr DecodedUleb128 kInvalidZeroParameterRate = 0;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kInvalidZeroParameterRate,
      descriptors.mix_presentation_obus);
  const std::vector<uint8_t> empty_bitstream;
  auto read_bit_buffer = MemoryBasedReadBitBuffer::CreateFromSpan(
      absl::MakeConstSpan(empty_bitstream));

  EXPECT_THAT(TemporalUnitProcessor::Create(descriptors.audio_elements,
                                            descriptors.mix_presentation_obus,
                                            read_bit_buffer.get()),
              Not(IsOk()));
}

}  // namespace
}  // namespace iamf_tools
