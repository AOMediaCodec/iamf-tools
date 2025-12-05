/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"
// [internal] Placeholder for FLAC fuzzing include.
#include "iamf/api/decoder/iamf_decoder.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace iamf_tools {
namespace {

using api::OutputLayout;
using ::fuzztest::Arbitrary;
using ::fuzztest::ElementOf;

constexpr absl::string_view kIamfFileTestPath = "iamf/cli/testdata/iamf/";

constexpr OutputLayout kStereoLayout =
    OutputLayout::kItu2051_SoundSystemA_0_2_0;

int GetBytesPerSample(api::OutputSampleType output_sample_type) {
  switch (output_sample_type) {
    case api::OutputSampleType::kInt16LittleEndian:
      return 2;
    case api::OutputSampleType::kInt32LittleEndian:
      return 4;
    default:
      return 0;
  }
}

void OutputAllTemporalUnits(api::IamfDecoder& iamf_decoder) {
  if (!iamf_decoder.IsDescriptorProcessingComplete()) {
    // Under fuzz testing, some streams are too corrupt to meaningfully decode
    // further.
    return;
  }

  // Compute the maximum size of the output audio buffer.
  const int bytes_per_sample =
      GetBytesPerSample(iamf_decoder.GetOutputSampleType());
  uint32_t frame_size;
  ASSERT_TRUE(iamf_decoder.GetFrameSize(frame_size).ok());
  int num_output_channels;
  ASSERT_TRUE(iamf_decoder.GetNumberOfOutputChannels(num_output_channels).ok());
  // Extract and throw away all temporal units.
  std::vector<uint8_t> output_buffer(bytes_per_sample * frame_size *
                                     num_output_channels);
  size_t unused_bytes_written;
  while (iamf_decoder.IsTemporalUnitAvailable()) {
    auto unused_get_status = iamf_decoder.GetOutputTemporalUnit(
        output_buffer.data(), output_buffer.size(), unused_bytes_written);
  }
}

void DoesNotDieWithBasicDecode(const std::string& data) {
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  const api::IamfDecoder::Settings kStereoLayoutSettings = {
      .requested_mix = {.output_layout = kStereoLayout}};
  ASSERT_TRUE(
      api::IamfDecoder::Create(kStereoLayoutSettings, iamf_decoder).ok());

  std::vector<uint8_t> bitstream(data.begin(), data.end());
  auto decode_status = iamf_decoder->Decode(bitstream.data(), bitstream.size());

  OutputAllTemporalUnits(*iamf_decoder);
}

FUZZ_TEST(IamfDecoderFuzzTest_ArbitraryBytes, DoesNotDieWithBasicDecode);

FUZZ_TEST(IamfDecoderFuzzTest_Seeded, DoesNotDieWithBasicDecode)
    .WithSeeds(
        fuzztest::ReadFilesFromDirectory(GetRunfilesPath(kIamfFileTestPath)));

void DoesNotDieCreateFromDescriptors(const std::string& descriptor_data,
                                     const std::string& temporal_unit_data) {
  std::vector<uint8_t> descriptors(descriptor_data.begin(),
                                   descriptor_data.end());

  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  // Intentionally check that defaulted settings are safe to use.
  const api::IamfDecoder::Settings kDefaultSettings;
  api::IamfStatus status = api::IamfDecoder::CreateFromDescriptors(
      kDefaultSettings, descriptors.data(), descriptors.size(), iamf_decoder);
  if (!status.ok()) {
    return;
  }

  std::vector<uint8_t> temporal_unit(temporal_unit_data.begin(),
                                     temporal_unit_data.end());
  auto unused_decode_status =
      iamf_decoder->Decode(temporal_unit.data(), temporal_unit.size());
  OutputAllTemporalUnits(*iamf_decoder);
}

FUZZ_TEST(IamfDecoderFuzzTest_ArbitraryBytesToDescriptors,
          DoesNotDieCreateFromDescriptors);

void DoesNotDieAllParams(api::OutputLayout output_layout,
                         api::OutputSampleType output_sample_type,
                         uint32_t mix_presentation_id, std::string data) {
  std::vector<uint8_t> bitstream(data.begin(), data.end());
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  const api::IamfDecoder::Settings kSettings = {
      .requested_mix = {.output_layout = output_layout},
      .requested_output_sample_type = output_sample_type,
  };
  ASSERT_TRUE(api::IamfDecoder::Create(kSettings, iamf_decoder).ok());

  auto unused_decode_status =
      iamf_decoder->Decode(bitstream.data(), bitstream.size());
  OutputAllTemporalUnits(*iamf_decoder);
}

auto AnyOutputLayout() {
  return ElementOf<api::OutputLayout>({
      api::OutputLayout::kItu2051_SoundSystemA_0_2_0,
      api::OutputLayout::kItu2051_SoundSystemB_0_5_0,
      api::OutputLayout::kItu2051_SoundSystemC_2_5_0,
      api::OutputLayout::kItu2051_SoundSystemD_4_5_0,
      api::OutputLayout::kItu2051_SoundSystemE_4_5_1,
      api::OutputLayout::kItu2051_SoundSystemF_3_7_0,
      api::OutputLayout::kItu2051_SoundSystemG_4_9_0,
      api::OutputLayout::kItu2051_SoundSystemH_9_10_3,
      api::OutputLayout::kItu2051_SoundSystemI_0_7_0,
      api::OutputLayout::kItu2051_SoundSystemJ_4_7_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_2_7_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_2_3_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_0_1_0,
      api::OutputLayout::kIAMF_SoundSystemExtension_6_9_0,
      api::OutputLayout::kIAMF_Binaural,
  });
}

auto AnyOutputSampleType() {
  return ElementOf<api::OutputSampleType>({
      api::OutputSampleType::kInt16LittleEndian,
      api::OutputSampleType::kInt32LittleEndian,
  });
}

FUZZ_TEST(IamfDecoderFuzzTest_AllArbitraryParams, DoesNotDieAllParams)
    .WithDomains(AnyOutputLayout(),          // output_layout,
                 AnyOutputSampleType(),      // output_sample_type,
                 Arbitrary<uint32_t>(),      // mix_presentation_id,
                 Arbitrary<std::string>());  // data

}  // namespace
}  // namespace iamf_tools
