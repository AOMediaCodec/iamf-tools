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

#include <cstdint>
#include <string>
#include <vector>

#include "fuzztest/fuzztest.h"
#include "gtest/gtest.h"
// [internal] Placeholder for FLAC fuzzing include.
#include "iamf/api/decoder/iamf_decoder.h"
#include "iamf/api/iamf_tools_api_types.h"

namespace iamf_tools {
namespace {

using api::OutputLayout;
using ::fuzztest::Arbitrary;
using ::fuzztest::ElementOf;

constexpr OutputLayout kStereoLayout =
    OutputLayout::kItu2051_SoundSystemA_0_2_0;

void DoesNotDieWithBasicDecode(const std::string& data) {
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  ASSERT_TRUE(api::IamfDecoder::Create(kStereoLayout, iamf_decoder).ok());

  std::vector<uint8_t> bitstream(data.begin(), data.end());
  auto decode_status = iamf_decoder->Decode(bitstream.data(), bitstream.size());
}

FUZZ_TEST(IamfDecoderFuzzTest_ArbitraryBytes, DoesNotDieWithBasicDecode);

void DoesNotDieCreateFromDescriptors(const std::string& descriptor_data,
                                     const std::string& temporal_unit_data) {
  std::vector<uint8_t> descriptors(descriptor_data.begin(),
                                   descriptor_data.end());

  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  api::IamfStatus status = api::IamfDecoder::CreateFromDescriptors(
      kStereoLayout, descriptors.data(), descriptors.size(), iamf_decoder);

  if (status.ok()) {
    std::vector<uint8_t> temporal_unit(temporal_unit_data.begin(),
                                       temporal_unit_data.end());
    auto decoder_status =
        iamf_decoder->Decode(temporal_unit.data(), temporal_unit.size());
  }
}

FUZZ_TEST(IamfDecoderFuzzTest_ArbitraryBytesToDescriptors,
          DoesNotDieCreateFromDescriptors);

void DoesNotDieAllParams(api::OutputLayout output_layout,
                         api::OutputSampleType output_sample_type,
                         uint32_t mix_presentation_id, std::string data) {
  std::vector<uint8_t> bitstream(data.begin(), data.end());
  std::unique_ptr<api::IamfDecoder> iamf_decoder;
  ASSERT_TRUE(api::IamfDecoder::Create(output_layout, iamf_decoder).ok());

  auto unused_decode_status =
      iamf_decoder->Decode(bitstream.data(), bitstream.size());
  iamf_decoder->ConfigureOutputSampleType(output_sample_type);
  auto unused_configure_status =
      iamf_decoder->ConfigureMixPresentationId(mix_presentation_id);
}

// // TODO(b/378912426): Update this to support all output layouts.
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
