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
#include "iamf/cli/iamf_lite_encoder_factory_utils.h"

#include <cstdint>
#include <variant>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/channel_label.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/ambisonics_config.h"
#include "iamf/obu/audio_element.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/decoder_config/aac_decoder_config.h"
#include "iamf/obu/decoder_config/opus_decoder_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::iamf_tools::api::AudioElementType;
using ::iamf_tools::api::PcmSampleRate;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr uint32_t kSampleRate48kHz = 48000;
constexpr DecodedUleb128 kCodecConfigId = 200;
constexpr DecodedUleb128 kAudioElementId = 300;
constexpr DecodedUleb128 kMixPresentationId = 42;
constexpr DecodedUleb128 kNumSamplesPerFrame = 960;

TEST(MakeIaSequenceHeaderObu, SelectsSimpleProfileForStandardLayouts) {
  const IASequenceHeaderObu obu =
      MakeIaSequenceHeaderObu(AudioElementType::kLayoutStereo);

  EXPECT_EQ(obu.GetPrimaryProfile(), ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu.GetAdditionalProfile(), ProfileVersion::kIamfSimpleProfile);
}

TEST(MakeIaSequenceHeaderObu,
     SelectsBaseEnhancedProfileForFourthOrderAmbisonicsAnd9_1_6) {
  const IASequenceHeaderObu soa4_obu =
      MakeIaSequenceHeaderObu(AudioElementType::kAmbisonicSphericalOrder4);
  const IASequenceHeaderObu hoa4_obu =
      MakeIaSequenceHeaderObu(AudioElementType::kAmbisonicHorizontalOrder4);
  const IASequenceHeaderObu layout_9_1_6_obu =
      MakeIaSequenceHeaderObu(AudioElementType::kLayout9_1_6);

  EXPECT_EQ(soa4_obu.GetPrimaryProfile(),
            ProfileVersion::kIamfBaseEnhancedProfile);
  EXPECT_EQ(hoa4_obu.GetPrimaryProfile(),
            ProfileVersion::kIamfBaseEnhancedProfile);
  EXPECT_EQ(layout_9_1_6_obu.GetPrimaryProfile(),
            ProfileVersion::kIamfBaseEnhancedProfile);
}

TEST(MakeIaSequenceHeaderObu, SelectsBaseAdvancedProfileForExpanded7_1_5_4) {
  const IASequenceHeaderObu obu =
      MakeIaSequenceHeaderObu(AudioElementType::kLayout7_1_5_4);

  EXPECT_EQ(obu.GetPrimaryProfile(), ProfileVersion::kIamfBaseAdvancedProfile);
  EXPECT_EQ(obu.GetAdditionalProfile(),
            ProfileVersion::kIamfBaseAdvancedProfile);
}

TEST(MakeIaSequenceHeaderObu, SelectsAdvanced2ProfileForExpanded10_2_9_3) {
  const IASequenceHeaderObu obu =
      MakeIaSequenceHeaderObu(AudioElementType::kLayout10_2_9_3);

  EXPECT_EQ(obu.GetPrimaryProfile(), ProfileVersion::kIamfAdvanced2Profile);
  EXPECT_EQ(obu.GetAdditionalProfile(), ProfileVersion::kIamfAdvanced2Profile);
}

TEST(CreatePcmCodecConfigObu,
     CreatesLpcmCodecConfigWithExpectedSampleRateForAllSupportedRates) {
  const auto pcm_16khz = CreatePcmCodecConfigObu(
      PcmSampleRate::k16kHz, kCodecConfigId, kNumSamplesPerFrame);
  const auto pcm_32khz = CreatePcmCodecConfigObu(
      PcmSampleRate::k32kHz, kCodecConfigId, kNumSamplesPerFrame);
  const auto pcm_44_1khz = CreatePcmCodecConfigObu(
      PcmSampleRate::k44_1kHz, kCodecConfigId, kNumSamplesPerFrame);
  const auto pcm_48khz = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  const auto pcm_96khz = CreatePcmCodecConfigObu(
      PcmSampleRate::k96kHz, kCodecConfigId, kNumSamplesPerFrame);

  ASSERT_THAT(pcm_16khz, IsOk());
  EXPECT_EQ(pcm_16khz->GetOutputSampleRate(), 16000);
  ASSERT_THAT(pcm_32khz, IsOk());
  EXPECT_EQ(pcm_32khz->GetOutputSampleRate(), 32000);
  ASSERT_THAT(pcm_44_1khz, IsOk());
  EXPECT_EQ(pcm_44_1khz->GetOutputSampleRate(), 44100);
  ASSERT_THAT(pcm_48khz, IsOk());
  EXPECT_EQ(pcm_48khz->GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(pcm_48khz->GetNumSamplesPerFrame(), kNumSamplesPerFrame);
  EXPECT_EQ(pcm_48khz->GetInputSampleRate(), kSampleRate48kHz);
  EXPECT_EQ(pcm_48khz->GetOutputSampleRate(), kSampleRate48kHz);
  EXPECT_EQ(pcm_48khz->GetCodecConfig().GetCodecId(),
            CodecConfig::kCodecIdLpcm);
  ASSERT_THAT(pcm_96khz, IsOk());
  EXPECT_EQ(pcm_96khz->GetOutputSampleRate(), 96000);
}

TEST(CreatePcmCodecConfigObu,
     ReturnsInvalidArgumentForUnsupportedPcmSampleRate) {
  const auto unsupported_sample_rate = static_cast<PcmSampleRate>(99);

  const auto result = CreatePcmCodecConfigObu(
      unsupported_sample_rate, kCodecConfigId, kNumSamplesPerFrame);

  EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CreateOpusCodecConfigObu,
     Creates48kHzOpusCodecConfigWithNonZeroPreSkipDelay) {
  const auto codec_config_obu =
      CreateOpusCodecConfigObu(kCodecConfigId, kNumSamplesPerFrame);

  ASSERT_THAT(codec_config_obu, IsOk());
  EXPECT_EQ(codec_config_obu->GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(codec_config_obu->GetNumSamplesPerFrame(), kNumSamplesPerFrame);
  EXPECT_EQ(codec_config_obu->GetInputSampleRate(), kSampleRate48kHz);
  EXPECT_EQ(codec_config_obu->GetOutputSampleRate(), kSampleRate48kHz);
  EXPECT_EQ(codec_config_obu->GetCodecConfig().GetCodecId(),
            CodecConfig::kCodecIdOpus);
  ASSERT_TRUE(std::holds_alternative<OpusDecoderConfig>(
      codec_config_obu->GetCodecConfig().decoder_config));
  EXPECT_GT(std::get<OpusDecoderConfig>(
                codec_config_obu->GetCodecConfig().decoder_config)
                .pre_skip_,
            0);
}

TEST(CreateAudioElementWithData,
     CreatesFirstOrderSphericalAmbisonicsWithFourMonoSubstreams) {
  const auto codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());

  const auto audio_element =
      CreateAudioElementWithData(AudioElementType::kAmbisonicSphericalOrder1,
                                 kAudioElementId, *codec_config_obu);

  ASSERT_THAT(audio_element, IsOk());
  EXPECT_EQ(audio_element->obu.GetAudioElementId(), kAudioElementId);
  EXPECT_EQ(audio_element->obu.GetCodecConfigId(), kCodecConfigId);
  EXPECT_EQ(audio_element->obu.GetAudioElementType(),
            AudioElementObu::kAudioElementSceneBased);
  EXPECT_THAT(audio_element->substream_id_to_labels,
              UnorderedElementsAre(Pair(0, ElementsAre(ChannelLabel::kA0)),
                                   Pair(1, ElementsAre(ChannelLabel::kA1)),
                                   Pair(2, ElementsAre(ChannelLabel::kA2)),
                                   Pair(3, ElementsAre(ChannelLabel::kA3))));
}

TEST(CreateAudioElementWithData,
     CreatesFirstOrderHorizontalAmbisonicsWithInactiveAcn2) {
  const auto codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());

  const auto audio_element =
      CreateAudioElementWithData(AudioElementType::kAmbisonicHorizontalOrder1,
                                 kAudioElementId, *codec_config_obu);

  ASSERT_THAT(audio_element, IsOk());
  EXPECT_THAT(audio_element->substream_id_to_labels,
              UnorderedElementsAre(Pair(0, ElementsAre(ChannelLabel::kA0)),
                                   Pair(1, ElementsAre(ChannelLabel::kA1)),
                                   Pair(2, ElementsAre(ChannelLabel::kA3))));
  const auto& ambisonics_config =
      std::get<AmbisonicsConfig>(audio_element->obu.config_);
  const auto& mono_config =
      std::get<AmbisonicsMonoConfig>(ambisonics_config.ambisonics_config);
  EXPECT_THAT(
      mono_config.GetChannelMappingView(),
      ElementsAre(0, 1, AmbisonicsMonoConfig::kInactiveAmbisonicsChannelNumber,
                  2));
}

TEST(CreateAudioElementWithData,
     CreatesStereoChannelLayoutWithCoupledSubstream) {
  const auto codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());

  const auto audio_element = CreateAudioElementWithData(
      AudioElementType::kLayoutStereo, kAudioElementId, *codec_config_obu);

  ASSERT_THAT(audio_element, IsOk());
  EXPECT_EQ(audio_element->obu.GetAudioElementType(),
            AudioElementObu::kAudioElementChannelBased);
  EXPECT_THAT(audio_element->substream_id_to_labels,
              UnorderedElementsAre(
                  Pair(0, ElementsAre(ChannelLabel::kL2, ChannelLabel::kR2))));
}

TEST(CreateAudioElementWithData,
     CreatesExpanded9_1_6ChannelLayoutWithNineSubstreams) {
  const auto codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());

  const auto audio_element = CreateAudioElementWithData(
      AudioElementType::kLayout9_1_6, kAudioElementId, *codec_config_obu);

  ASSERT_THAT(audio_element, IsOk());
  EXPECT_EQ(audio_element->obu.GetAudioElementType(),
            AudioElementObu::kAudioElementChannelBased);
  EXPECT_EQ(audio_element->substream_id_to_labels.size(), 9);
}

TEST(CreateAudioElementWithData,
     ReturnsInvalidArgumentForUnsupportedAudioElementType) {
  const auto codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());
  const auto unsupported_element_type = static_cast<AudioElementType>(99);

  const auto audio_element = CreateAudioElementWithData(
      unsupported_element_type, kAudioElementId, *codec_config_obu);

  EXPECT_THAT(audio_element, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CreateSubstreamEncoders,
     CreatesInitializedEncodersForAllSubstreamsIn5_1Element) {
  const auto codec_config_obu =
      CreateOpusCodecConfigObu(kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());
  const auto audio_element = CreateAudioElementWithData(
      AudioElementType::kLayout5_1, kAudioElementId, *codec_config_obu);
  ASSERT_THAT(audio_element, IsOk());

  const auto encoders =
      CreateSubstreamEncoders(*audio_element, *codec_config_obu);

  ASSERT_THAT(encoders, IsOk());
  EXPECT_EQ(encoders->size(), 4);
  ASSERT_TRUE(encoders->contains(0));
  ASSERT_NE(encoders->at(0), nullptr);
  EXPECT_GT(encoders->at(0)->GetNumberOfSamplesToDelayAtStart(), 0);
  ASSERT_TRUE(encoders->contains(1));
  ASSERT_NE(encoders->at(1), nullptr);
  EXPECT_GT(encoders->at(1)->GetNumberOfSamplesToDelayAtStart(), 0);
  ASSERT_TRUE(encoders->contains(2));
  ASSERT_NE(encoders->at(2), nullptr);
  EXPECT_GT(encoders->at(2)->GetNumberOfSamplesToDelayAtStart(), 0);
  ASSERT_TRUE(encoders->contains(3));
  ASSERT_NE(encoders->at(3), nullptr);
  EXPECT_GT(encoders->at(3)->GetNumberOfSamplesToDelayAtStart(), 0);
}

TEST(CreateSubstreamEncoders, ReturnsInvalidArgumentForUnsupportedCodecId) {
  const auto lpcm_codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(lpcm_codec_config_obu, IsOk());
  const auto audio_element = CreateAudioElementWithData(
      AudioElementType::kLayoutMono, kAudioElementId, *lpcm_codec_config_obu);
  ASSERT_THAT(audio_element, IsOk());
  const auto aac_codec_config_obu =
      CodecConfigObu::Create(ObuHeader(), kCodecConfigId,
                             CodecConfig{
                                 .num_samples_per_frame = kNumSamplesPerFrame,
                                 .decoder_config = AacDecoderConfig{},
                             });
  ASSERT_THAT(aac_codec_config_obu, IsOk());

  const auto encoders =
      CreateSubstreamEncoders(*audio_element, *aac_codec_config_obu);

  EXPECT_THAT(encoders, StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CreateMixPresentationObu,
     CreatesStereoMixPresentationReferencingAudioElement) {
  const auto codec_config_obu = CreatePcmCodecConfigObu(
      PcmSampleRate::k48kHz, kCodecConfigId, kNumSamplesPerFrame);
  ASSERT_THAT(codec_config_obu, IsOk());

  const auto mix_presentation_obu = CreateMixPresentationObu(
      kMixPresentationId, kAudioElementId, *codec_config_obu);

  ASSERT_THAT(mix_presentation_obu, IsOk());
  EXPECT_EQ(mix_presentation_obu->GetMixPresentationId(), kMixPresentationId);
  ASSERT_THAT(mix_presentation_obu->sub_mixes_, Not(IsEmpty()));
  ASSERT_THAT(mix_presentation_obu->sub_mixes_.front().audio_elements,
              Not(IsEmpty()));
  EXPECT_EQ(mix_presentation_obu->sub_mixes_.front()
                .audio_elements.front()
                .audio_element_id,
            kAudioElementId);
}

}  // namespace
}  // namespace iamf_tools
