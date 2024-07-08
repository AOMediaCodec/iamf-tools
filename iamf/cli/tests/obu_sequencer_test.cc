/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/obu_sequencer.h"

#include <cstdint>
#include <list>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/write_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 1;
const uint32_t kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kSecondAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 1;
constexpr DecodedUleb128 kSecondSubstreamId = 2;
constexpr DecodedUleb128 kFirstMixPresentationId = 100;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
const uint32_t kCommonMixGainParameterRate = kSampleRate;

// TODO(b/302470464): Add test coverage `ObuSequencer::WriteTemporalUnit()` and
//                    `ObuSequencer::PickAndPlace()` configured with minimal and
//                    fixed-size leb generators.

void AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
    uint32_t audio_element_id, uint32_t substream_id, int32_t start_timestamp,
    int32_t end_timestamp,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames) {
  ASSERT_TRUE(audio_elements.contains(audio_element_id));

  audio_frames.emplace_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), substream_id, {}),
      .start_timestamp = start_timestamp,
      .end_timestamp = end_timestamp,
      .raw_samples = {},
      .down_mixing_params = {.in_bitstream = false},
      .audio_element_with_data = &audio_elements.at(audio_element_id)});
}

TEST(GenerateTemporalUnitMap, SubstreamsOrderedByAudioElementIdSubstreamId) {
  // Initialize two audio elements each with two substreams.
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus = {};
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements = {};
  const uint32_t kCodecConfigId = 0;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, 48000,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      /*audio_element_id=*/100, kCodecConfigId, {2000, 4000}, codec_config_obus,
      audio_elements);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      /*audio_element_id=*/200, kCodecConfigId, {3000, 5000}, codec_config_obus,
      audio_elements);

  // Add some audio frames in an arbitrary order.
  std::list<AudioFrameWithData> audio_frames;
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      200, 5000, 0, 16, audio_elements, audio_frames);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      100, 2000, 0, 16, audio_elements, audio_frames);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      200, 3000, 0, 16, audio_elements, audio_frames);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      100, 4000, 0, 16, audio_elements, audio_frames);

  // By default the results are expected to be sorted by audio element ID then
  // by substream ID.
  struct ExpectedAudioElementIdAndSubstreamId {
    uint32_t audio_element_id;
    uint32_t substream_id;
  };
  std::vector<ExpectedAudioElementIdAndSubstreamId> expected_results = {
      {100, 2000}, {100, 4000}, {200, 3000}, {200, 5000}};

  // Generate the temporal unit map.
  TemporalUnitMap temporal_unit_map;
  EXPECT_THAT(ObuSequencerBase::GenerateTemporalUnitMap(audio_frames, {},
                                                        temporal_unit_map),
              IsOk());

  // The test is hard-coded with one temporal unit and four frames.
  ASSERT_EQ(temporal_unit_map.size(), 1);
  ASSERT_TRUE(temporal_unit_map.contains(0));
  ASSERT_EQ(temporal_unit_map[0].audio_frames.size(), 4);

  // Validate the order of the output frames matches the expected order.
  auto expected_results_iter = expected_results.begin();
  for (const auto& audio_frame : temporal_unit_map[0].audio_frames) {
    EXPECT_EQ(audio_frame->audio_element_with_data->obu.GetAudioElementId(),
              expected_results_iter->audio_element_id);
    EXPECT_EQ(audio_frame->obu.GetSubstreamId(),
              expected_results_iter->substream_id);

    expected_results_iter++;
  }
}

class ObuSequencerTest : public ::testing::Test {
 public:
  void InitializeDescriptorObus() {
    ia_sequence_header_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                                    ProfileVersion::kIamfSimpleProfile,
                                    ProfileVersion::kIamfSimpleProfile);
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus_);
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
        codec_config_obus_, audio_elements_);
    AddMixPresentationObuWithAudioElementIds(
        kFirstMixPresentationId, {kFirstAudioElementId},
        kCommonMixGainParameterId, kCommonMixGainParameterRate,
        mix_presentation_obus_);

    ASSERT_TRUE(ia_sequence_header_obu_.has_value());
    ASSERT_TRUE(codec_config_obus_.contains(kCodecConfigId));
    ASSERT_TRUE(audio_elements_.contains(kFirstAudioElementId));
    ASSERT_FALSE(mix_presentation_obus_.empty());
  }

  void ValidateWriteDescriptorObuSequence(
      const std::list<const ObuBase*>& expected_sequence) {
    WriteBitBuffer expected_wb(128);
    for (const auto* expected_obu : expected_sequence) {
      ASSERT_NE(expected_obu, nullptr);
      EXPECT_THAT(expected_obu->ValidateAndWriteObu(expected_wb), IsOk());
    }

    WriteBitBuffer result_wb(128);
    EXPECT_THAT(ObuSequencerBase::WriteDescriptorObus(
                    ia_sequence_header_obu_.value(), codec_config_obus_,
                    audio_elements_, mix_presentation_obus_, arbitrary_obus_,
                    result_wb),
                IsOk());

    EXPECT_EQ(result_wb.bit_buffer(), expected_wb.bit_buffer());
  }

 protected:
  std::optional<IASequenceHeaderObu> ia_sequence_header_obu_;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements_;
  std::list<MixPresentationObu> mix_presentation_obus_;
  std::list<ArbitraryObu> arbitrary_obus_;
};

TEST_F(ObuSequencerTest, OrdersByAParticularObuType) {
  InitializeDescriptorObus();
  // The IAMF spec REQUIRES descriptor OBUs to be ordered by `obu_type` in a
  // particular order (i.e. IA Sequence Header, Codec Config Audio Element, Mix
  // Presentation).
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, ArbitraryObuAfterIaSequenceHeader) {
  InitializeDescriptorObus();

  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterIaSequenceHeader));

  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(),
      &arbitrary_obus_.back(),
      &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back(),
  };

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, CodecConfigAreAscendingOrderByDefault) {
  InitializeDescriptorObus();

  // Initialize a second Codec Config OBU.
  const DecodedUleb128 kSecondCodecConfigId = 101;
  AddLpcmCodecConfigWithIdAndSampleRate(kSecondCodecConfigId, kSampleRate,
                                        codec_config_obus_);

  // IAMF makes no recommendation for the ordering between multiple descriptor
  // OBUs of the same type. By default `WriteDescriptorObus` orders them in
  // ascending order.
  ASSERT_LT(kCodecConfigId, kSecondCodecConfigId);
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &codec_config_obus_.at(kSecondCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, AudioElementAreAscendingOrderByDefault) {
  InitializeDescriptorObus();

  // Initialize a second Audio Element OBU.
  const DecodedUleb128 kSecondAudioElementId = 101;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus_, audio_elements_);

  // IAMF makes no recommendation for the ordering between multiple descriptor
  // OBUs of the same type. By default `WriteDescriptorObus` orders them in
  // ascending order.
  ASSERT_LT(kFirstAudioElementId, kSecondAudioElementId);
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &audio_elements_.at(kSecondAudioElementId).obu,
      &mix_presentation_obus_.back()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

TEST_F(ObuSequencerTest, MixPresentationsAreAscendingOrderByDefault) {
  InitializeDescriptorObus();

  // Initialize a second Mix Presentation OBU.
  const DecodedUleb128 kSecondMixPresentationId = 99;
  AddMixPresentationObuWithAudioElementIds(
      kSecondMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus_);

  // IAMF makes no recommendation for the ordering between multiple descriptor
  // OBUs of the same type. By default `WriteDescriptorObus` orders them in
  // ascending order regardless of their order in the input list.
  ASSERT_LT(kSecondMixPresentationId, kFirstMixPresentationId);
  ASSERT_EQ(mix_presentation_obus_.back().GetMixPresentationId(),
            kSecondMixPresentationId);
  ASSERT_EQ(mix_presentation_obus_.front().GetMixPresentationId(),
            kFirstMixPresentationId);
  const std::list<const ObuBase*> expected_sequence = {
      &ia_sequence_header_obu_.value(), &codec_config_obus_.at(kCodecConfigId),
      &audio_elements_.at(kFirstAudioElementId).obu,
      &mix_presentation_obus_.back(), &mix_presentation_obus_.front()};

  ValidateWriteDescriptorObuSequence(expected_sequence);
}

void InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
    absl::flat_hash_map<DecodedUleb128, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<MixPresentationObu>& mix_presentation_obus) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus, audio_elements);
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId, kSecondAudioElementId},
      kCommonMixGainParameterId, kCommonMixGainParameterRate,
      mix_presentation_obus);
}

TEST(WriteDescriptorObus,
     InvalidWhenMixPresentationDoesNotComplyWithIaSequenceHeader) {
  IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);

  WriteBitBuffer unused_wb(0);
  EXPECT_FALSE(ObuSequencerBase::WriteDescriptorObus(
                   ia_sequence_header_obu, codec_config_obus, audio_elements,
                   mix_presentation_obus, /*arbitrary_obus=*/{}, unused_wb)
                   .ok());
}

TEST(WriteDescriptorObus,
     ValidWhenMixPresentationCompliesWithIaSequenceHeader) {
  IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentation_obus;
  InitializeDescriptorObusForTwoMonoAmbisonicsAudioElement(
      codec_config_obus, audio_elements, mix_presentation_obus);

  WriteBitBuffer unused_wb(0);
  EXPECT_THAT(ObuSequencerBase::WriteDescriptorObus(
                  ia_sequence_header_obu, codec_config_obus, audio_elements,
                  mix_presentation_obus, /*arbitrary_obus=*/{}, unused_wb),
              IsOk());
}

}  // namespace
}  // namespace iamf_tools
