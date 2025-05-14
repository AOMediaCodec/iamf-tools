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

#include "iamf/api/internal_utils/internal_utils.h"

#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/cli/wav_writer.h"
#include "iamf/include/iamf_tools/iamf_decoder.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using api::IamfDecoder;
using api::OutputLayout;

constexpr DecodedUleb128 kFirstCodecConfigId = 1;
constexpr uint32_t kNumSamplesPerFrame = 8;
constexpr uint32_t kBitDepth = 16;
constexpr DecodedUleb128 kSampleRate = 48000;
constexpr DecodedUleb128 kFirstAudioElementId = 2;
constexpr DecodedUleb128 kFirstSubstreamId = 18;
constexpr DecodedUleb128 kFirstMixPresentationId = 3;
constexpr DecodedUleb128 kCommonMixGainParameterId = 999;
constexpr DecodedUleb128 kCommonParameterRate = kSampleRate;

std::vector<uint8_t> GenerateBasicDescriptorObus() {
  const IASequenceHeaderObu ia_sequence_header(
      ObuHeader(), IASequenceHeaderObu::kIaCode,
      ProfileVersion::kIamfSimpleProfile, ProfileVersion::kIamfBaseProfile);
  absl::flat_hash_map<DecodedUleb128, CodecConfigObu> codec_configs;
  AddLpcmCodecConfig(kFirstCodecConfigId, kNumSamplesPerFrame, kBitDepth,
                     kSampleRate, codec_configs);
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kFirstCodecConfigId, {kFirstSubstreamId},
      codec_configs, audio_elements);
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kFirstMixPresentationId, {kFirstAudioElementId},
      kCommonMixGainParameterId, kCommonParameterRate, mix_presentation_obus);
  return SerializeObusExpectOk({&ia_sequence_header,
                                &codec_configs.at(kFirstCodecConfigId),
                                &audio_elements.at(kFirstAudioElementId).obu,
                                &mix_presentation_obus.front()});
}

TEST(SetUpAfterDescriptors, SetsWavWriterAndSampleBuffer) {
  auto bitstream = GenerateBasicDescriptorObus();
  std::unique_ptr<IamfDecoder> decoder;
  iamf_tools::api::IamfStatus status = IamfDecoder::CreateFromDescriptors(
      IamfDecoder::Settings{.requested_layout =
                                OutputLayout::kItu2051_SoundSystemA_0_2_0},
      bitstream.data(), bitstream.size(), decoder);
  OutputLayout output_layout;
  ASSERT_TRUE(decoder->GetOutputLayout(output_layout).ok());
  ASSERT_THAT(output_layout, OutputLayout::kItu2051_SoundSystemA_0_2_0);
  decoder->ConfigureOutputSampleType(
      iamf_tools::api::OutputSampleType::kInt16LittleEndian);

  std::unique_ptr<WavWriter> wav_writer;
  std::vector<uint8_t> reusable_sample_buffer;
  auto iamf_status =
      SetupAfterDescriptors(*decoder, GetAndCleanupOutputFileName("test.wav"),
                            wav_writer, reusable_sample_buffer);

  EXPECT_TRUE(iamf_status.ok());
  constexpr int kSampleSizeBytesFor16Bit = 2;
  constexpr int kNumChannels = 2;
  EXPECT_EQ(wav_writer->bit_depth(), kSampleSizeBytesFor16Bit * 8);
  EXPECT_EQ(reusable_sample_buffer.size(),
            kNumSamplesPerFrame * kNumChannels * kSampleSizeBytesFor16Bit);
}

TEST(SetUpAfterDescriptors, FailsWithInvalidWavWriter) {
  auto bitstream = GenerateBasicDescriptorObus();
  std::unique_ptr<IamfDecoder> decoder;
  iamf_tools::api::IamfStatus status = IamfDecoder::CreateFromDescriptors(
      IamfDecoder::Settings{.requested_layout =
                                OutputLayout::kItu2051_SoundSystemA_0_2_0},
      bitstream.data(), bitstream.size(), decoder);
  std::unique_ptr<WavWriter> wav_writer;
  std::vector<uint8_t> reusable_sample_buffer;

  auto iamf_status = SetupAfterDescriptors(*decoder, "non_existent_path",
                                           wav_writer, reusable_sample_buffer);
  EXPECT_FALSE(iamf_status.ok());
}

}  // namespace
}  // namespace iamf_tools
