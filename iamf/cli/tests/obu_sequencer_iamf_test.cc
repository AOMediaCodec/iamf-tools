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
#include "iamf/cli/obu_sequencer_iamf.h"

#include <cstdint>
#include <filesystem>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/audio_frame_with_data.h"
#include "iamf/cli/parameter_block_with_data.h"
#include "iamf/cli/temporal_unit_view.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/common/leb_generator.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/obu/arbitrary_obu.h"
#include "iamf/obu/audio_frame.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/demixing_info_parameter_data.h"
#include "iamf/obu/demixing_param_definition.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/parameter_block.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

constexpr DecodedUleb128 kCodecConfigId = 1;
constexpr uint32_t kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 1;
constexpr DecodedUleb128 kFirstSubstreamId = 1;
constexpr DecodedUleb128 kFirstMixPresentationId = 100;
constexpr DecodedUleb128 kFirstDemixingParameterId = 998;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr uint32_t kCommonMixGainParameterRate = kSampleRate;

constexpr absl::string_view kOmitOutputIamfFile = "";
constexpr bool kIncludeTemporalDelimiters = true;
constexpr bool kDoNotIncludeTemporalDelimiters = false;

constexpr std::nullopt_t kOriginalSamplesAreIrrelevant = std::nullopt;

constexpr int64_t kReadBitBufferCapacity = 1024;

// TODO(b/302470464): Add test coverage for `ObuSequencerIamf::PickAndPlace()`
//                    configured with minimal and fixed-size leb generators.

void AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
    uint32_t audio_element_id, uint32_t substream_id,
    InternalTimestamp start_timestamp, InternalTimestamp end_timestamp,
    const absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements,
    std::list<AudioFrameWithData>& audio_frames) {
  ASSERT_TRUE(audio_elements.contains(audio_element_id));

  audio_frames.emplace_back(AudioFrameWithData{
      .obu = AudioFrameObu(ObuHeader(), substream_id, {}),
      .start_timestamp = start_timestamp,
      .end_timestamp = end_timestamp,
      .encoded_samples = kOriginalSamplesAreIrrelevant,
      .down_mixing_params = {.in_bitstream = false},
      .audio_element_with_data = &audio_elements.at(audio_element_id)});
}

DemixingParamDefinition CreateDemixingParamDefinition(
    const DecodedUleb128 parameter_id) {
  DemixingParamDefinition demixing_param_definition;
  demixing_param_definition.parameter_id_ = parameter_id;
  demixing_param_definition.parameter_rate_ = 48000;
  demixing_param_definition.param_definition_mode_ = 0;
  demixing_param_definition.duration_ = 8;
  demixing_param_definition.constant_subblock_duration_ = 8;
  demixing_param_definition.reserved_ = 10;

  return demixing_param_definition;
}

void InitializeOneParameterBlockAndOneAudioFrame(
    DemixingParamDefinition& param_definition,
    std::list<ParameterBlockWithData>& parameter_blocks,
    std::list<AudioFrameWithData>& audio_frames,
    absl::flat_hash_map<uint32_t, CodecConfigObu>& codec_config_obus,
    absl::flat_hash_map<uint32_t, AudioElementWithData>& audio_elements) {
  constexpr InternalTimestamp kStartTimestamp = 0;
  constexpr InternalTimestamp kEndTimestamp = 16;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus, audio_elements);
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, kStartTimestamp, kEndTimestamp,
      audio_elements, audio_frames);
  auto data = std::make_unique<DemixingInfoParameterData>();
  data->dmixp_mode = DemixingInfoParameterData::kDMixPMode1;
  data->reserved = 0;
  auto parameter_block = std::make_unique<ParameterBlockObu>(
      ObuHeader(), param_definition.parameter_id_, param_definition);
  ASSERT_THAT(parameter_block->InitializeSubblocks(), IsOk());
  parameter_block->subblocks_[0].param_data = std::move(data);
  parameter_blocks.emplace_back(ParameterBlockWithData{
      .obu = std::move(parameter_block),
      .start_timestamp = 0,
      .end_timestamp = 16,
  });
}

class ObuSequencerIamfTest : public ::testing::Test {
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

  void InitObusForOneFrameIaSequence() {
    ia_sequence_header_obu_.emplace(ObuHeader(), IASequenceHeaderObu::kIaCode,
                                    ProfileVersion::kIamfSimpleProfile,
                                    ProfileVersion::kIamfSimpleProfile);
    param_definition_ =
        CreateDemixingParamDefinition(kFirstDemixingParameterId);
    InitializeOneParameterBlockAndOneAudioFrame(
        param_definition_, parameter_blocks_, audio_frames_, codec_config_obus_,
        audio_elements_);
    AddMixPresentationObuWithAudioElementIds(
        kFirstMixPresentationId, {audio_elements_.begin()->first},
        kCommonMixGainParameterId, kCommonMixGainParameterRate,
        mix_presentation_obus_);
  }

 protected:
  std::optional<IASequenceHeaderObu> ia_sequence_header_obu_;
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<uint32_t, AudioElementWithData> audio_elements_;
  std::list<MixPresentationObu> mix_presentation_obus_;

  DemixingParamDefinition param_definition_;
  std::list<ParameterBlockWithData> parameter_blocks_;
  std::list<AudioFrameWithData> audio_frames_;

  std::list<ArbitraryObu> arbitrary_obus_;
};

TEST(ObuSequencerIamf, PickAndPlaceWritesFileWithOnlyIaSequenceHeader) {
  const std::string kOutputIamfFilename = GetAndCleanupOutputFileName(".iamf");
  {
    const IASequenceHeaderObu ia_sequence_header_obu(
        ObuHeader(), IASequenceHeaderObu::kIaCode,
        ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
    ObuSequencerIamf sequencer(kOutputIamfFilename,
                               kDoNotIncludeTemporalDelimiters,
                               *LebGenerator::Create());

    EXPECT_THAT(sequencer.PickAndPlace(
                    ia_sequence_header_obu, /*codec_config_obus=*/{},
                    /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                    /*audio_frames=*/{}, /*parameter_blocks=*/{},
                    /*arbitrary_obus=*/{}),
                IsOk());

    // `ObuSequencerIamf` goes out of scope and closes the file.
  }

  EXPECT_TRUE(std::filesystem::exists(kOutputIamfFilename));
}

struct ProfileVersionsAndEnableTemporalDelimiters {
  ProfileVersion primary_profile;
  ProfileVersion additional_profile;
  bool enable_temporal_delimiters;
};

using TestProfileVersionAndEnableTemporalDelimiters =
    ::testing::TestWithParam<ProfileVersionsAndEnableTemporalDelimiters>;

TEST_P(TestProfileVersionAndEnableTemporalDelimiters, PickAndPlace) {
  const IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode, GetParam().primary_profile,
      GetParam().additional_profile);
  ObuSequencerIamf sequencer(std::string(kOmitOutputIamfFile),
                             GetParam().enable_temporal_delimiters,
                             *LebGenerator::Create());

  EXPECT_THAT(sequencer.PickAndPlace(
                  ia_sequence_header_obu, /*codec_config_obus=*/{},
                  /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                  /*audio_frames=*/{}, /*parameter_blocks=*/{},
                  /*arbitrary_obus=*/{}),
              IsOk());
}

INSTANTIATE_TEST_SUITE_P(
    SimpleProfileWithTemporalDelimiters,
    TestProfileVersionAndEnableTemporalDelimiters,
    testing::Values<ProfileVersionsAndEnableTemporalDelimiters>(
        {ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile,
         kIncludeTemporalDelimiters}));

INSTANTIATE_TEST_SUITE_P(
    SimpleProfileWithoutTemporalDelimiters,
    TestProfileVersionAndEnableTemporalDelimiters,
    testing::Values<ProfileVersionsAndEnableTemporalDelimiters>(
        {ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfSimpleProfile,
         kDoNotIncludeTemporalDelimiters}));

INSTANTIATE_TEST_SUITE_P(
    BaseProfileWithoutTemporalDelimiters,
    TestProfileVersionAndEnableTemporalDelimiters,
    testing::Values<ProfileVersionsAndEnableTemporalDelimiters>(
        {ProfileVersion::kIamfBaseProfile, ProfileVersion::kIamfBaseProfile,
         kDoNotIncludeTemporalDelimiters}));

INSTANTIATE_TEST_SUITE_P(
    BaseEnhancedProfileWithoutTemporalDelimiters,
    TestProfileVersionAndEnableTemporalDelimiters,
    testing::Values<ProfileVersionsAndEnableTemporalDelimiters>(
        {ProfileVersion::kIamfBaseEnhancedProfile,
         ProfileVersion::kIamfBaseEnhancedProfile,
         kDoNotIncludeTemporalDelimiters}));

TEST(ObuSequencerIamf, PickAndPlaceSucceedsWithEmptyOutputFile) {
  const IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);

  ObuSequencerIamf sequencer(std::string(kOmitOutputIamfFile),
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());

  EXPECT_THAT(sequencer.PickAndPlace(
                  ia_sequence_header_obu, /*codec_config_obus=*/{},
                  /*audio_elements=*/{}, /*mix_presentation_obus=*/{},
                  /*audio_frames=*/{}, /*parameter_blocks=*/{},
                  /*arbitrary_obus=*/{}),
              IsOk());
}

TEST_F(ObuSequencerIamfTest, PickAndPlaceCreatesFileWithOneFrameIaSequence) {
  const std::string kOutputIamfFilename = GetAndCleanupOutputFileName(".iamf");
  InitObusForOneFrameIaSequence();
  ObuSequencerIamf sequencer(kOutputIamfFilename,
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());

  ASSERT_THAT(
      sequencer.PickAndPlace(*ia_sequence_header_obu_, codec_config_obus_,
                             audio_elements_, mix_presentation_obus_,
                             audio_frames_, parameter_blocks_, arbitrary_obus_),
      IsOk());

  EXPECT_TRUE(std::filesystem::exists(kOutputIamfFilename));
}

TEST_F(ObuSequencerIamfTest, PickAndPlaceFileCanBeReadBacks) {
  const std::string kOutputIamfFilename = GetAndCleanupOutputFileName(".iamf");
  InitializeDescriptorObus();
  AddEmptyAudioFrameWithAudioElementIdSubstreamIdAndTimestamps(
      kFirstAudioElementId, kFirstSubstreamId, 0, 16, audio_elements_,
      audio_frames_);

  ObuSequencerIamf sequencer(kOutputIamfFilename,
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());

  ASSERT_THAT(
      sequencer.PickAndPlace(*ia_sequence_header_obu_, codec_config_obus_,
                             audio_elements_, mix_presentation_obus_,
                             audio_frames_, parameter_blocks_, arbitrary_obus_),
      IsOk());

  // Read back the file, we expect all sequenced OBUs to be present.
  auto read_bit_buffer = FileBasedReadBitBuffer::CreateFromFilePath(
      kReadBitBufferCapacity, kOutputIamfFilename);
  ASSERT_NE(read_bit_buffer, nullptr);
  IASequenceHeaderObu ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  std::list<MixPresentationObu> mix_presentations;
  std::list<AudioFrameWithData> audio_frames;
  std::list<ParameterBlockWithData> parameter_blocks;
  EXPECT_THAT(CollectObusFromIaSequence(*read_bit_buffer, ia_sequence_header,
                                        codec_config_obus, audio_elements,
                                        mix_presentations, audio_frames,
                                        parameter_blocks),
              IsOk());
  EXPECT_EQ(ia_sequence_header, ia_sequence_header_obu_);
  EXPECT_EQ(codec_config_obus.size(), 1);
  EXPECT_EQ(codec_config_obus.size(), 1);
  EXPECT_EQ(audio_elements.size(), 1);
  EXPECT_EQ(mix_presentations.size(), 1);
  EXPECT_EQ(audio_frames.size(), 1);
  EXPECT_TRUE(parameter_blocks.empty());
}

TEST_F(ObuSequencerIamfTest,
       PickAndPlaceLeavesNoFileWhenDescriptorsAreInvalid) {
  constexpr uint32_t kInvalidIaCode = IASequenceHeaderObu::kIaCode + 1;
  const std::string kOutputIamfFilename = GetAndCleanupOutputFileName(".iamf");
  InitObusForOneFrameIaSequence();
  // Overwrite the IA Sequence Header with an invalid one.
  ia_sequence_header_obu_ = IASequenceHeaderObu(
      ObuHeader(), kInvalidIaCode, ProfileVersion::kIamfSimpleProfile,
      ProfileVersion::kIamfSimpleProfile);
  ObuSequencerIamf sequencer(kOutputIamfFilename,
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());

  ASSERT_FALSE(sequencer
                   .PickAndPlace(*ia_sequence_header_obu_, codec_config_obus_,
                                 audio_elements_, mix_presentation_obus_,
                                 audio_frames_, parameter_blocks_,
                                 arbitrary_obus_)
                   .ok());

  EXPECT_FALSE(std::filesystem::exists(kOutputIamfFilename));
}

TEST_F(ObuSequencerIamfTest,
       PickAndPlaceLeavesNoFileWhenTemporalUnitsAreInvalid) {
  constexpr bool kInvalidateTemporalUnit = true;
  const std::string kOutputIamfFilename = GetAndCleanupOutputFileName(".iamf");
  InitObusForOneFrameIaSequence();
  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterAudioFramesAtTick, 0,
                   kInvalidateTemporalUnit));
  ObuSequencerIamf sequencer(kOutputIamfFilename,
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());

  ASSERT_FALSE(sequencer
                   .PickAndPlace(*ia_sequence_header_obu_, codec_config_obus_,
                                 audio_elements_, mix_presentation_obus_,
                                 audio_frames_, parameter_blocks_,
                                 arbitrary_obus_)
                   .ok());

  EXPECT_FALSE(std::filesystem::exists(kOutputIamfFilename));
}

TEST_F(ObuSequencerIamfTest,
       PickAndPlaceOnInvalidTemporalUnitFailsWhenOutputFileIsOmitted) {
  constexpr bool kInvalidateTemporalUnit = true;
  InitObusForOneFrameIaSequence();
  arbitrary_obus_.emplace_back(
      ArbitraryObu(kObuIaReserved25, ObuHeader(), {},
                   ArbitraryObu::kInsertionHookAfterAudioFramesAtTick, 0,
                   kInvalidateTemporalUnit));
  ObuSequencerIamf sequencer(std::string(kOmitOutputIamfFile),
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());

  ASSERT_FALSE(sequencer
                   .PickAndPlace(*ia_sequence_header_obu_, codec_config_obus_,
                                 audio_elements_, mix_presentation_obus_,
                                 audio_frames_, parameter_blocks_,
                                 arbitrary_obus_)
                   .ok());
}

TEST_F(ObuSequencerIamfTest,
       FileContainsUpdatedDescriptorObusAfterUpdateDescriptorObusAndClose) {
  const ProfileVersion kOriginalProfile = ProfileVersion::kIamfBaseProfile;
  const ProfileVersion kUpdatedProfile =
      ProfileVersion::kIamfBaseEnhancedProfile;
  InitObusForOneFrameIaSequence();
  parameter_blocks_.clear();
  ia_sequence_header_obu_ =
      IASequenceHeaderObu(ObuHeader(), IASequenceHeaderObu::kIaCode,
                          kOriginalProfile, kOriginalProfile);
  const std::string kOutputIamfFilename = GetAndCleanupOutputFileName(".iamf");
  ObuSequencerIamf sequencer(kOutputIamfFilename,
                             kDoNotIncludeTemporalDelimiters,
                             *LebGenerator::Create());
  EXPECT_THAT(sequencer.PushDescriptorObus(
                  *ia_sequence_header_obu_, codec_config_obus_, audio_elements_,
                  mix_presentation_obus_, arbitrary_obus_),
              IsOk());
  const auto temporal_unit = TemporalUnitView::Create(
      parameter_blocks_, audio_frames_, arbitrary_obus_);
  ASSERT_THAT(temporal_unit, IsOk());
  EXPECT_THAT(sequencer.PushTemporalUnit(*temporal_unit), IsOk());
  // As a toy example, we will update the IA Sequence Header.
  ia_sequence_header_obu_ =
      IASequenceHeaderObu(ObuHeader(), IASequenceHeaderObu::kIaCode,
                          kUpdatedProfile, kUpdatedProfile);

  // Finalize the descriptor OBUs with a new IA Sequence Header.
  EXPECT_THAT(sequencer.UpdateDescriptorObusAndClose(
                  *ia_sequence_header_obu_, codec_config_obus_, audio_elements_,
                  mix_presentation_obus_, arbitrary_obus_),
              IsOk());

  auto read_bit_buffer = FileBasedReadBitBuffer::CreateFromFilePath(
      kReadBitBufferCapacity, kOutputIamfFilename);
  ASSERT_NE(read_bit_buffer, nullptr);
  IASequenceHeaderObu read_ia_sequence_header;
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> read_codec_config_obus;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> read_audio_elements;
  std::list<MixPresentationObu> read_mix_presentation_obus;
  std::list<AudioFrameWithData> read_audio_frames;
  std::list<ParameterBlockWithData> read_parameter_blocks;
  EXPECT_THAT(
      CollectObusFromIaSequence(*read_bit_buffer, read_ia_sequence_header,
                                read_codec_config_obus, read_audio_elements,
                                read_mix_presentation_obus, read_audio_frames,
                                read_parameter_blocks),
      IsOk());
  // Finally we expect to see evidence of the modified IA Sequence Header.
  EXPECT_EQ(read_ia_sequence_header.GetPrimaryProfile(), kUpdatedProfile);
}

}  // namespace
}  // namespace iamf_tools
