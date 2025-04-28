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
#include "iamf/cli/proto_conversion/output_audio_format_utils.h"

#include <cstddef>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/rendering_mix_presentation_finalizer.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::testing::_;

typedef RenderingMixPresentationFinalizer::SampleProcessorFactory
    SampleProcessorFactory;
using enum iamf_tools_cli_proto::OutputAudioFormat;

constexpr DecodedUleb128 kMixPresentationId = 42;
constexpr int kSubMixIndex = 1;
constexpr int kLayoutIndex = 3;
const Layout kStereoLayout = {
    .layout_type = Layout::kLayoutTypeLoudspeakersSsConvention,
    .specific_layout = LoudspeakersSsConventionLayout{
        .sound_system = LoudspeakersSsConventionLayout::kSoundSystemA_0_2_0}};
constexpr int kNumChannels = 2;
constexpr int kSampleRate = 48000;
constexpr int kBitDepth16 = 16;
constexpr int kBitDepth24 = 24;
constexpr int kBitDepth32 = 32;
constexpr size_t kMaxInputSamplesPerFrame = 0;

TEST(GetWavSampleProcessorFactoryFromOutputAudioFormat,
     ForwardsArgumentsWhenBitDepthIsPreserved) {
  MockSampleProcessorFactory mock_factory;
  // Configure the factory to preserve the bit-depth.
  const auto kPreserveBitDepth = OUTPUT_FORMAT_WAV_BIT_DEPTH_AUTOMATIC;
  // All arguments should be preserved.
  EXPECT_CALL(mock_factory, Call(kMixPresentationId, kSubMixIndex, kLayoutIndex,
                                 kStereoLayout, kNumChannels, kSampleRate,
                                 kBitDepth16, kMaxInputSamplesPerFrame));
  SampleProcessorFactory sample_processor_factory =
      mock_factory.AsStdFunction();

  ApplyOutputAudioFormatToSampleProcessorFactory(kPreserveBitDepth,
                                                 sample_processor_factory);

  sample_processor_factory(kMixPresentationId, kSubMixIndex, kLayoutIndex,
                           kStereoLayout, kNumChannels, kSampleRate,
                           kBitDepth16, kMaxInputSamplesPerFrame);
}

TEST(GetWavSampleProcessorFactoryFromOutputAudioFormat,
     ForwardsmostArgumentsWhenBitDepthIsNotPreserved) {
  MockSampleProcessorFactory mock_factory;
  // Configure the factory to override the bit-depth.
  const auto kOverrideBitDepth = OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR;
  // All arguments, except the bit-depth, should be preserved.
  EXPECT_CALL(mock_factory, Call(kMixPresentationId, kSubMixIndex, kLayoutIndex,
                                 kStereoLayout, kNumChannels, kSampleRate, _,
                                 kMaxInputSamplesPerFrame));
  SampleProcessorFactory sample_processor_factory =
      mock_factory.AsStdFunction();

  ApplyOutputAudioFormatToSampleProcessorFactory(kOverrideBitDepth,
                                                 sample_processor_factory);

  sample_processor_factory(kMixPresentationId, kSubMixIndex, kLayoutIndex,
                           kStereoLayout, kNumChannels, kSampleRate,
                           kBitDepth16, kMaxInputSamplesPerFrame);
}

TEST(GetWavSampleProcessorFactoryFromOutputAudioFormat,
     DoesNotUseFactoryWhenOutputIsDisabled) {
  MockSampleProcessorFactory mock_factory;
  // Omit output wav files.
  const auto kPreserveBitDepth = OUTPUT_FORMAT_NONE;
  // The mock factory is thrown away, and observes no calls when the output
  // factory is used.
  EXPECT_CALL(mock_factory, Call(_, _, _, _, _, _, _, _)).Times(0);
  SampleProcessorFactory sample_processor_factory =
      mock_factory.AsStdFunction();

  ApplyOutputAudioFormatToSampleProcessorFactory(kPreserveBitDepth,
                                                 sample_processor_factory);

  sample_processor_factory(kMixPresentationId, kSubMixIndex, kLayoutIndex,
                           kStereoLayout, kNumChannels, kSampleRate,
                           kBitDepth16, kMaxInputSamplesPerFrame);
}

struct BitDepthOverrideTestParam {
  int initial_bit_depth;
  iamf_tools_cli_proto::OutputAudioFormat output_audio_format;
  int expected_bit_depth;
};

using BitDepthOverrideTest =
    ::testing::TestWithParam<BitDepthOverrideTestParam>;

TEST_P(BitDepthOverrideTest, ValidateBitDepthOverride) {
  MockSampleProcessorFactory mock_factory;
  EXPECT_CALL(mock_factory,
              Call(_, _, _, _, _, _, GetParam().expected_bit_depth, _));

  SampleProcessorFactory sample_processor_factory =
      mock_factory.AsStdFunction();

  ApplyOutputAudioFormatToSampleProcessorFactory(GetParam().output_audio_format,
                                                 sample_processor_factory);

  sample_processor_factory(kMixPresentationId, kSubMixIndex, kLayoutIndex,
                           kStereoLayout, kNumChannels, kSampleRate,
                           GetParam().initial_bit_depth,
                           kMaxInputSamplesPerFrame);
}

INSTANTIATE_TEST_SUITE_P(
    OverridesTo16Bit, BitDepthOverrideTest,
    testing::ValuesIn<BitDepthOverrideTestParam>(
        {{kBitDepth16, OUTPUT_FORMAT_WAV_BIT_DEPTH_SIXTEEN, kBitDepth16},
         {kBitDepth24, OUTPUT_FORMAT_WAV_BIT_DEPTH_SIXTEEN, kBitDepth16},
         {kBitDepth32, OUTPUT_FORMAT_WAV_BIT_DEPTH_SIXTEEN, kBitDepth16}}));

INSTANTIATE_TEST_SUITE_P(
    OverridesTo24Bit, BitDepthOverrideTest,
    testing::ValuesIn<BitDepthOverrideTestParam>(
        {{kBitDepth16, OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR, kBitDepth24},
         {kBitDepth24, OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR, kBitDepth24},
         {kBitDepth32, OUTPUT_FORMAT_WAV_BIT_DEPTH_TWENTY_FOUR, kBitDepth24}}));

INSTANTIATE_TEST_SUITE_P(
    OverridesTo32Bit, BitDepthOverrideTest,
    testing::ValuesIn<BitDepthOverrideTestParam>(
        {{kBitDepth16, OUTPUT_FORMAT_WAV_BIT_DEPTH_THIRTY_TWO, kBitDepth32},
         {kBitDepth24, OUTPUT_FORMAT_WAV_BIT_DEPTH_THIRTY_TWO, kBitDepth32},
         {kBitDepth32, OUTPUT_FORMAT_WAV_BIT_DEPTH_THIRTY_TWO, kBitDepth32}}));

}  // namespace
}  // namespace iamf_tools
