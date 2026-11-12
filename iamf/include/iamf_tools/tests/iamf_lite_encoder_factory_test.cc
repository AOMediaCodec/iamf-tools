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
#include "iamf/include/iamf_tools/iamf_lite_encoder_factory.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/obu_processor.h"
#include "iamf/common/read_bit_buffer.h"
#include "iamf/include/iamf_tools/iamf_encoder_interface.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/include/iamf_tools/iamf_tools_encoder_api_types.h"
#include "iamf/obu/ia_sequence_header.h"

namespace iamf_tools {
namespace {

TEST(CreateOneElementPcmIamfEncoder,
     ReturnsNullptrWhenSampleRateIsUnsupported) {
  const auto kUnsupportedSampleRate = static_cast<api::PcmSampleRate>(99);

  auto iamf_encoder =
      api::IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
          kUnsupportedSampleRate, api::AudioElementType::kLayoutStereo);

  EXPECT_EQ(iamf_encoder, nullptr);
}

TEST(CreateOneElementPcmIamfEncoder,
     ReturnsNullptrWhenAudioElementTypeIsUnsupported) {
  const auto kUnsupportedElementType = static_cast<api::AudioElementType>(99);

  auto iamf_encoder =
      api::IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
          api::PcmSampleRate::k48kHz, kUnsupportedElementType);

  EXPECT_EQ(iamf_encoder, nullptr);
}

TEST(CreateOneElementOpusIamfEncoder,
     ReturnsNullptrWhenAudioElementTypeIsUnsupported) {
  const auto kUnsupportedElementType = static_cast<api::AudioElementType>(99);

  auto iamf_encoder =
      api::IamfLiteEncoderFactory::CreateOneElementOpusIamfEncoder(
          kUnsupportedElementType);

  EXPECT_EQ(iamf_encoder, nullptr);
}

TEST(CreateOneElementPcmIamfEncoder,
     EncodeReturnsErrorWhenInputChannelCountMismatchesStereoLayout) {
  auto iamf_encoder =
      api::IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
          api::PcmSampleRate::k48kHz, api::AudioElementType::kLayoutStereo);
  ASSERT_NE(iamf_encoder, nullptr);
  const std::vector<double> kMonoSamples(1024, 0.0);
  const std::array<std::span<const double>, 1> kOneChannel = {kMonoSamples};

  const api::IamfStatus encode_status = iamf_encoder->Encode(kOneChannel);

  EXPECT_FALSE(encode_status.ok());
}

struct PcmSampleRateTestCase {
  std::string test_name;
  api::PcmSampleRate sample_rate;
};

using CreateOneElementPcmIamfEncoderSampleRateTest =
    ::testing::TestWithParam<PcmSampleRateTestCase>;

TEST_P(CreateOneElementPcmIamfEncoderSampleRateTest,
       EncodesStereoFrameAtSupportedPcmSampleRate) {
  const PcmSampleRateTestCase& test_case = GetParam();
  const std::vector<double> kOneChannelSamples(1024, 0.0);
  const std::array<std::span<const double>, 2> kStereoChannels = {
      kOneChannelSamples, kOneChannelSamples};

  auto iamf_encoder =
      api::IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
          test_case.sample_rate, api::AudioElementType::kLayoutStereo);
  ASSERT_NE(iamf_encoder, nullptr);
  std::vector<uint8_t> descriptor_obus;
  bool output_obus_are_finalized = false;
  std::vector<uint8_t> temporal_unit_obus;

  const api::IamfStatus descriptor_status = iamf_encoder->GetDescriptorObus(
      /*redundant_copy=*/false, descriptor_obus, output_obus_are_finalized);
  const api::IamfStatus encode_status = iamf_encoder->Encode(kStereoChannels);
  const api::IamfStatus finalize_status = iamf_encoder->FinalizeEncode();
  const api::IamfStatus output_status =
      iamf_encoder->OutputTemporalUnit(temporal_unit_obus);

  EXPECT_EQ(iamf_encoder->GetNumSamplesPerFrame(), 1024);
  EXPECT_TRUE(descriptor_status.ok());
  EXPECT_FALSE(descriptor_obus.empty());
  EXPECT_TRUE(encode_status.ok());
  EXPECT_TRUE(finalize_status.ok());
  EXPECT_TRUE(output_status.ok());
  EXPECT_FALSE(temporal_unit_obus.empty());
}

INSTANTIATE_TEST_SUITE_P(
    AllPcmSampleRates, CreateOneElementPcmIamfEncoderSampleRateTest,
    ::testing::Values(
        PcmSampleRateTestCase{.test_name = "16kHz",
                              .sample_rate = api::PcmSampleRate::k16kHz},
        PcmSampleRateTestCase{.test_name = "32kHz",
                              .sample_rate = api::PcmSampleRate::k32kHz},
        PcmSampleRateTestCase{.test_name = "44_1kHz",
                              .sample_rate = api::PcmSampleRate::k44_1kHz},
        PcmSampleRateTestCase{.test_name = "48kHz",
                              .sample_rate = api::PcmSampleRate::k48kHz},
        PcmSampleRateTestCase{.test_name = "96kHz",
                              .sample_rate = api::PcmSampleRate::k96kHz}),
    [](const ::testing::TestParamInfo<PcmSampleRateTestCase>& info) {
      return info.param.test_name;
    });

struct OneElementEncoderLayoutTestCase {
  std::string test_name;
  bool use_opus;
  api::AudioElementType audio_element_type;
  size_t num_channels;
};

using CreateOneElementIamfEncoderLayoutTest =
    ::testing::TestWithParam<OneElementEncoderLayoutTestCase>;

TEST_P(CreateOneElementIamfEncoderLayoutTest,
       EncodesOneFrameAndOutputsNonEmptyDescriptorsAndTemporalUnit) {
  const OneElementEncoderLayoutTestCase& test_case = GetParam();
  const uint32_t expected_samples_per_frame = test_case.use_opus ? 960 : 1024;
  const std::vector<double> kOneChannelSamples(expected_samples_per_frame, 0.0);
  const std::vector<std::span<const double>> channel_spans(
      test_case.num_channels, kOneChannelSamples);

  auto iamf_encoder =
      test_case.use_opus
          ? api::IamfLiteEncoderFactory::CreateOneElementOpusIamfEncoder(
                test_case.audio_element_type)
          : api::IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
                api::PcmSampleRate::k48kHz, test_case.audio_element_type);
  ASSERT_NE(iamf_encoder, nullptr);
  std::vector<uint8_t> descriptor_obus;
  bool output_obus_are_finalized = false;
  std::vector<uint8_t> temporal_unit_obus;

  const api::IamfStatus descriptor_status = iamf_encoder->GetDescriptorObus(
      /*redundant_copy=*/false, descriptor_obus, output_obus_are_finalized);
  const api::IamfStatus encode_status = iamf_encoder->Encode(channel_spans);
  const api::IamfStatus finalize_status = iamf_encoder->FinalizeEncode();
  const api::IamfStatus output_status =
      iamf_encoder->OutputTemporalUnit(temporal_unit_obus);

  EXPECT_EQ(iamf_encoder->GetNumSamplesPerFrame(), expected_samples_per_frame);
  EXPECT_TRUE(descriptor_status.ok());
  EXPECT_FALSE(descriptor_obus.empty());
  EXPECT_TRUE(encode_status.ok());
  EXPECT_TRUE(finalize_status.ok());
  EXPECT_TRUE(output_status.ok());
  EXPECT_FALSE(temporal_unit_obus.empty());
}

INSTANTIATE_TEST_SUITE_P(
    AllAudioElementTypesAndEncodings, CreateOneElementIamfEncoderLayoutTest,
    ::testing::Values(
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicSphericalOrder1_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicSphericalOrder1,
            .num_channels = 4,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicSphericalOrder2_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicSphericalOrder2,
            .num_channels = 9,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicSphericalOrder3_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicSphericalOrder3,
            .num_channels = 16,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicSphericalOrder4_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicSphericalOrder4,
            .num_channels = 25,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicHorizontalOrder1_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicHorizontalOrder1,
            .num_channels = 3,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicHorizontalOrder2_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicHorizontalOrder2,
            .num_channels = 5,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicHorizontalOrder3_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicHorizontalOrder3,
            .num_channels = 7,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicHorizontalOrder4_Pcm",
            .use_opus = false,
            .audio_element_type =
                api::AudioElementType::kAmbisonicHorizontalOrder4,
            .num_channels = 9,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "LayoutMono_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayoutMono,
            .num_channels = 1,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "LayoutStereo_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayoutStereo,
            .num_channels = 2,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout5_1_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout5_1,
            .num_channels = 6,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout5_1_2_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout5_1_2,
            .num_channels = 8,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout5_1_4_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout5_1_4,
            .num_channels = 10,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout7_1_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout7_1,
            .num_channels = 8,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout7_1_2_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout7_1_2,
            .num_channels = 10,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout7_1_4_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout7_1_4,
            .num_channels = 12,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout3_1_2_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout3_1_2,
            .num_channels = 6,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "LayoutBinaural_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayoutBinaural,
            .num_channels = 2,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "ExpandedLayout9_1_6_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout9_1_6,
            .num_channels = 16,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "ExpandedLayout10_2_9_3_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout10_2_9_3,
            .num_channels = 24,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "ExpandedLayout7_1_5_4_Pcm",
            .use_opus = false,
            .audio_element_type = api::AudioElementType::kLayout7_1_5_4,
            .num_channels = 17,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicSphericalOrder1_Opus",
            .use_opus = true,
            .audio_element_type =
                api::AudioElementType::kAmbisonicSphericalOrder1,
            .num_channels = 4,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "AmbisonicHorizontalOrder2_Opus",
            .use_opus = true,
            .audio_element_type =
                api::AudioElementType::kAmbisonicHorizontalOrder2,
            .num_channels = 5,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "LayoutStereo_Opus",
            .use_opus = true,
            .audio_element_type = api::AudioElementType::kLayoutStereo,
            .num_channels = 2,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "Layout5_1_4_Opus",
            .use_opus = true,
            .audio_element_type = api::AudioElementType::kLayout5_1_4,
            .num_channels = 10,
        },
        OneElementEncoderLayoutTestCase{
            .test_name = "ExpandedLayout9_1_6_Opus",
            .use_opus = true,
            .audio_element_type = api::AudioElementType::kLayout9_1_6,
            .num_channels = 16,
        }),
    [](const ::testing::TestParamInfo<OneElementEncoderLayoutTestCase>& info) {
      return info.param.test_name;
    });

TEST(CreateOneElementPcmIamfEncoder,
     ProducesParseableIaSequenceWithExpectedStereoObus) {
  const std::vector<double> kLeftSamples(1024, 0.25);
  const std::vector<double> kRightSamples(1024, -0.25);
  const std::array<std::span<const double>, 2> kStereoChannels = {
      kLeftSamples, kRightSamples};
  auto iamf_encoder =
      api::IamfLiteEncoderFactory::CreateOneElementInt16LePcmIamfEncoder(
          api::PcmSampleRate::k48kHz, api::AudioElementType::kLayoutStereo);
  ASSERT_NE(iamf_encoder, nullptr);
  std::vector<uint8_t> descriptor_obus;
  bool output_obus_are_finalized = false;
  std::vector<uint8_t> temporal_unit_obus;
  auto rb = StreamBasedReadBitBuffer::Create(4096);
  ASSERT_NE(rb, nullptr);

  ASSERT_TRUE(iamf_encoder
                  ->GetDescriptorObus(/*redundant_copy=*/false, descriptor_obus,
                                      output_obus_are_finalized)
                  .ok());
  ASSERT_TRUE(iamf_encoder->Encode(kStereoChannels).ok());
  ASSERT_TRUE(iamf_encoder->FinalizeEncode().ok());
  ASSERT_TRUE(iamf_encoder->OutputTemporalUnit(temporal_unit_obus).ok());
  ASSERT_THAT(rb->PushBytes(absl::MakeConstSpan(descriptor_obus)),
              absl_testing::IsOk());
  ASSERT_THAT(rb->PushBytes(absl::MakeConstSpan(temporal_unit_obus)),
              absl_testing::IsOk());
  bool insufficient_data = false;
  auto obu_processor = ObuProcessor::Create(
      /*is_exhaustive_and_exact=*/false, rb.get(), insufficient_data);
  ASSERT_NE(obu_processor, nullptr);
  std::optional<ObuProcessor::OutputTemporalUnit> output_temporal_unit;
  bool continue_processing = false;
  ASSERT_THAT(obu_processor->ProcessTemporalUnit(
                  /*eos_is_end_of_sequence=*/true, output_temporal_unit,
                  continue_processing),
              absl_testing::IsOk());

  EXPECT_EQ(obu_processor->GetIaSequenceHeaderView().GetPrimaryProfile(),
            ProfileVersion::kIamfSimpleProfile);
  EXPECT_EQ(obu_processor->GetCodecConfigsByIdView().size(), 1);
  EXPECT_EQ(obu_processor->GetAudioElementsView().size(), 1);
  EXPECT_EQ(obu_processor->GetMixPresentationObusView().size(), 1);
  ASSERT_TRUE(output_temporal_unit.has_value());
  EXPECT_EQ(output_temporal_unit->output_audio_frames.size(), 1);
}

}  // namespace
}  // namespace iamf_tools
