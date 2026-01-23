/*
 * Copyright (c) 2025, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear
 * License and the Alliance for Open Media Patent License 1.0. If the BSD
 * 3-Clause Clear License was not distributed with this source code in the
 * LICENSE file, you can obtain it at
 * www.aomedia.org/license/software-license/bsd-3-c-c. If the Alliance for
 * Open Media Patent License 1.0 was not distributed with this source code
 * in the PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include "iamf/cli/obu_sequencer_streaming_iamf.h"

#include <array>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/leb_generator.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/obu_base.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

using ::testing::IsEmpty;

using absl::MakeConstSpan;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr uint32_t kEightSamplesPerFrame = 8;
constexpr uint8_t kBitDepth = 16;
constexpr uint32_t kSampleRate = 48000;
constexpr InternalTimestamp kStartTimestamp = 0;
constexpr InternalTimestamp kEndTimestamp = 8;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kFirstSubstreamId = 1;

constexpr bool kDoNotIncludeTemporalDelimiters = false;

constexpr std::nullopt_t kOriginalSamplesAreIrrelevant = std::nullopt;

constexpr std::array<uint8_t, 16> kEightSampleAudioFrame{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

constexpr absl::Span<ParameterBlockWithData> kNoParameterBlocks = {};
constexpr absl::Span<ArbitraryObu> kNoArbitraryObus = {};

void AddOneFrame(
    uint32_t audio_element_id, uint32_t substream_id,
    InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames) {
  ASSERT_TRUE(audio_elements.contains(audio_element_id));

  audio_frames.push_back(
      {.obu = AudioFrameObu(ObuHeader(), substream_id,
                            MakeConstSpan(kEightSampleAudioFrame)),
       .start_timestamp = start_timestamp,
       .end_timestamp = end_timestamp,
       .encoded_samples = kOriginalSamplesAreIrrelevant,
       .down_mixing_params = {.in_bitstream = false},
       .audio_element_with_data = &audio_elements.at(audio_element_id)});
}

TEST(GetSerializedDescriptorObus, IsEmptyBeforePushDescriptorObus) {
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());

  EXPECT_TRUE(sequencer.GetSerializedDescriptorObus().empty());
}

TEST(GetPreviousSerializedTemporalUnit, IsEmptyBeforeFirstPushTemporalUnit) {
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());

  EXPECT_TRUE(sequencer.GetPreviousSerializedTemporalUnit().empty());
}

TEST(GetPreviousSerializedTemporalUnit, IsEmptyAfterClose) {
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());
  EXPECT_THAT(sequencer.Close(), IsOk());

  EXPECT_TRUE(sequencer.GetPreviousSerializedTemporalUnit().empty());
}

TEST(GetSerializedDescriptorObus, ReturnsSerializedPushedDescriptorObus) {
  const IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());
  EXPECT_THAT(sequencer.PushDescriptorObus(
                  ia_sequence_header_obu, /*metadata_obus=*/{},
                  /*codec_config_obus=*/{},
                  /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                  /*arbitrary_obus=*/{}),
              IsOk());

  const std::vector<uint8_t> expected_serialized_descriptor_obus =
      SerializeObusExpectOk(
          std::list<const ObuBase*>({&ia_sequence_header_obu}));
  EXPECT_EQ(sequencer.GetSerializedDescriptorObus(),
            expected_serialized_descriptor_obus);
}

TEST(GetSerializedDescriptorObus, ReturnsSerializedUpdatedDescriptorObus) {
  const IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfBaseProfile);
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());
  EXPECT_THAT(sequencer.PushDescriptorObus(
                  ia_sequence_header_obu, /*metadata_obus=*/{},
                  /*codec_config_obus=*/{},
                  /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                  /*arbitrary_obus=*/{}),
              IsOk());

  // Push a new descriptor OBU.
  const IASequenceHeaderObu updated_ia_sequence_header_obu(
      ObuHeader(), ProfileVersion::kIamfBaseProfile,
      ProfileVersion::kIamfBaseProfile);
  const std::vector<uint8_t> expected_serialized_descriptor_obus =
      SerializeObusExpectOk(
          std::list<const ObuBase*>({&updated_ia_sequence_header_obu}));
  EXPECT_THAT(sequencer.UpdateDescriptorObusAndClose(
                  updated_ia_sequence_header_obu, /*metadata_obus=*/{},
                  /*codec_config_obus=*/{},
                  /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                  /*arbitrary_obus=*/{}),
              IsOk());

  EXPECT_EQ(sequencer.GetSerializedDescriptorObus(),
            expected_serialized_descriptor_obus);
}

TEST(GetPreviousSerializedTemporalUnit, GetsPreviousSerializedTemporalUnit) {
  IASequenceHeaderObu ia_sequence_header_obu(ObuHeader(),
                                             ProfileVersion::kIamfSimpleProfile,
                                             ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddLpcmCodecConfig(kCodecConfigId, kEightSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());
  EXPECT_THAT(
      sequencer.PushDescriptorObus(ia_sequence_header_obu, /*metadata_obus=*/{},
                                   codec_config_obus, audio_elements,
                                   /*mix_presentation_obus=*/{},
                                   /*arbitrary_obus=*/{}),
      IsOk());
  std::list<AudioFrameWithData> audio_frames;
  AddOneFrame(kFirstAudioElementId, kFirstSubstreamId, kStartTimestamp,
              kEndTimestamp, audio_elements, audio_frames);
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  EXPECT_THAT(sequencer.PushTemporalUnit(*temporal_unit), IsOk());
  const std::vector<uint8_t> expected_serialized_temporal_unit =
      SerializeObusExpectOk(
          std::list<const ObuBase*>({&audio_frames.front().obu}));

  EXPECT_EQ(sequencer.GetPreviousSerializedTemporalUnit(),
            expected_serialized_temporal_unit);
}

TEST(Close, ClearsSerializedTemporalUnitObus) {
  IASequenceHeaderObu ia_sequence_header_obu(ObuHeader(),
                                             ProfileVersion::kIamfSimpleProfile,
                                             ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddLpcmCodecConfig(kCodecConfigId, kEightSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());
  EXPECT_THAT(
      sequencer.PushDescriptorObus(ia_sequence_header_obu, /*metadata_obus=*/{},
                                   codec_config_obus, audio_elements,
                                   /*mix_presentation_obus=*/{},
                                   /*arbitrary_obus=*/{}),
      IsOk());
  std::list<AudioFrameWithData> audio_frames;
  AddOneFrame(kFirstAudioElementId, kFirstSubstreamId, kStartTimestamp,
              kEndTimestamp, audio_elements, audio_frames);
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  EXPECT_THAT(sequencer.PushTemporalUnit(*temporal_unit), IsOk());

  EXPECT_THAT(sequencer.Close(), IsOk());

  EXPECT_THAT(sequencer.GetPreviousSerializedTemporalUnit(), IsEmpty());
}

TEST(Abort, ClearsSerializedDescriptorAndTemporalUnitObus) {
  IASequenceHeaderObu ia_sequence_header_obu(ObuHeader(),
                                             ProfileVersion::kIamfSimpleProfile,
                                             ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddLpcmCodecConfig(kCodecConfigId, kEightSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  ObuSequencerStreamingIamf sequencer(kDoNotIncludeTemporalDelimiters,
                                      *LebGenerator::Create());
  EXPECT_THAT(sequencer.PushDescriptorObus(
                  ia_sequence_header_obu, /*metadata_obus=*/{},
                  /*codec_config_obus=*/{},
                  /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                  /*arbitrary_obus=*/{}),
              IsOk());
  std::list<AudioFrameWithData> audio_frames;
  AddOneFrame(kFirstAudioElementId, kFirstSubstreamId, kStartTimestamp,
              kEndTimestamp, audio_elements, audio_frames);
  const auto temporal_unit = TemporalUnitView::Create(
      kNoParameterBlocks, audio_frames, kNoArbitraryObus);
  ASSERT_THAT(temporal_unit, IsOk());
  EXPECT_THAT(sequencer.PushTemporalUnit(*temporal_unit), IsOk());
  sequencer.Abort();

  EXPECT_THAT(sequencer.GetSerializedDescriptorObus(), IsEmpty());
  EXPECT_THAT(sequencer.GetPreviousSerializedTemporalUnit(), IsEmpty());
}
}  // namespace
}  // namespace iamf_tools
