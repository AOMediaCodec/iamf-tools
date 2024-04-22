/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
 */
#include "iamf/cli/cli_util.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/obu_header.pb.h"
#include "iamf/cli/proto/parameter_data.pb.h"
#include "iamf/cli/proto/temporal_delimiter.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/demixing_info_param_data.h"
#include "iamf/obu/ia_sequence_header.h"
#include "iamf/obu/leb128.h"
#include "iamf/obu/mix_presentation.h"
#include "iamf/obu/obu_header.h"
#include "iamf/obu/param_definitions.h"
#include "src/google/protobuf/text_format.h"

namespace iamf_tools {
namespace {

struct IncludeTemporalDelimitersTestCase {
  ProfileVersion primary_profile;
  ProfileVersion additional_profile;
  bool enable_temporal_delimiters;
  absl::StatusCode status_code;
};

using IncludeTemporalDelimiters =
    ::testing::TestWithParam<IncludeTemporalDelimitersTestCase>;

TEST_P(IncludeTemporalDelimiters, CliUtils) {
  const IncludeTemporalDelimitersTestCase& test_case = GetParam();

  // Initialize the arguments for `get_include_temporal_delimiter_obus`.
  IASequenceHeaderObu ia_sequence_header_obu(
      ObuHeader(), IASequenceHeaderObu::kIaCode, test_case.primary_profile,
      test_case.additional_profile);

  iamf_tools_cli_proto::UserMetadata user_metadata;
  user_metadata.mutable_temporal_delimiter_metadata()
      ->set_enable_temporal_delimiters(test_case.enable_temporal_delimiters);

  // Call and validate results match expected.
  bool result;
  EXPECT_EQ(GetIncludeTemporalDelimiterObus(user_metadata,
                                            ia_sequence_header_obu, result)
                .code(),
            test_case.status_code);
  if (test_case.status_code == absl::StatusCode::kOk) {
    EXPECT_EQ(result, test_case.enable_temporal_delimiters);
  }
}

INSTANTIATE_TEST_SUITE_P(DisabledSimpleProfile, IncludeTemporalDelimiters,
                         testing::ValuesIn<IncludeTemporalDelimitersTestCase>(
                             {{ProfileVersion::kIamfSimpleProfile,
                               ProfileVersion::kIamfSimpleProfile, false,
                               absl::StatusCode::kOk}}));

INSTANTIATE_TEST_SUITE_P(EnabledSimpleProfile, IncludeTemporalDelimiters,
                         testing::ValuesIn<IncludeTemporalDelimitersTestCase>(
                             {{ProfileVersion::kIamfSimpleProfile,
                               ProfileVersion::kIamfSimpleProfile, true,
                               absl::StatusCode::kOk}}));

INSTANTIATE_TEST_SUITE_P(DisabledBaseProfile, IncludeTemporalDelimiters,
                         testing::ValuesIn<IncludeTemporalDelimitersTestCase>(
                             {{ProfileVersion::kIamfBaseProfile,
                               ProfileVersion::kIamfBaseProfile, false,
                               absl::StatusCode::kOk}}));

INSTANTIATE_TEST_SUITE_P(EnabledBaseProfile, IncludeTemporalDelimiters,
                         testing::ValuesIn<IncludeTemporalDelimitersTestCase>(
                             {{ProfileVersion::kIamfBaseProfile,
                               ProfileVersion::kIamfBaseProfile, true,
                               absl::StatusCode::kOk}}));

INSTANTIATE_TEST_SUITE_P(EnabledBaseAndSimpleProfile, IncludeTemporalDelimiters,
                         testing::ValuesIn<IncludeTemporalDelimitersTestCase>(
                             {{ProfileVersion::kIamfSimpleProfile,
                               ProfileVersion::kIamfBaseProfile, true,
                               absl::StatusCode::kOk}}));

TEST(WritePcmFrameToBuffer, ResizesOutputBuffer) {
  const size_t kExpectedSize = 12;  // 3 bytes per sample * 4 samples.
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f000000, 0x7e000000},
                                                      {0x7f000000, 0x7e000000}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_TRUE(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer)
                  .ok());

  EXPECT_EQ(output_buffer.size(), kExpectedSize);
}

TEST(WritePcmFrameToBuffer, WritesBigEndian) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = true;
  std::vector<uint8_t> output_buffer;
  EXPECT_TRUE(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer)
                  .ok());

  const std::vector<uint8_t> kExpectedBytes = {
      0x7f, 0x00, 0x12, 0x7e, 0x00, 0x34, 0x7f, 0x00, 0x56, 0x7e, 0x00, 0x78};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, WritesLittleEndian) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_TRUE(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer)
                  .ok());

  const std::vector<uint8_t> kExpectedBytes = {
      0x12, 0x00, 0x7f, 0x34, 0x00, 0x7e, 0x56, 0x00, 0x7f, 0x78, 0x00, 0x7e};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, TrimsSamples) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 24;
  const uint32_t kSamplesToTrimAtStart = 1;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;
  EXPECT_TRUE(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                    kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                    output_buffer)
                  .ok());

  const std::vector<uint8_t> kExpectedBytes = {0x56, 0x00, 0x7f,
                                               0x78, 0x00, 0x7e};
  EXPECT_EQ(output_buffer, kExpectedBytes);
}

TEST(WritePcmFrameToBuffer, RequiresBitDepthIsMultipleOfEight) {
  std::vector<std::vector<int32_t>> frame_to_write = {{0x7f001200, 0x7e003400},
                                                      {0x7f005600, 0x7e007800}};
  const uint8_t kBitDepth = 23;
  const uint32_t kSamplesToTrimAtStart = 0;
  const uint32_t kSamplesToTrimAtEnd = 0;
  const bool kBigEndian = false;
  std::vector<uint8_t> output_buffer;

  EXPECT_FALSE(WritePcmFrameToBuffer(frame_to_write, kSamplesToTrimAtStart,
                                     kSamplesToTrimAtEnd, kBitDepth, kBigEndian,
                                     output_buffer)
                   .ok());
}

class GetCommonSampleRateAndBitDepthTest : public ::testing::Test {
 public:
  GetCommonSampleRateAndBitDepthTest()
      : sample_rates_({48000}),
        bit_depths_({16}),
        expected_status_code_(absl::StatusCode::kOk),
        expected_sample_rate_(48000),
        expected_bit_depth_(16),
        expected_requires_resampling_(false) {}
  void Test() {
    uint32_t common_sample_rate;
    uint8_t common_bit_depth;
    bool requires_resampling;
    EXPECT_EQ(GetCommonSampleRateAndBitDepth(
                  sample_rates_, bit_depths_, common_sample_rate,
                  common_bit_depth, requires_resampling)
                  .code(),
              expected_status_code_);

    if (expected_status_code_ == absl::StatusCode::kOk) {
      EXPECT_EQ(common_sample_rate, expected_sample_rate_);
      EXPECT_EQ(common_bit_depth, expected_bit_depth_);
      EXPECT_EQ(requires_resampling, expected_requires_resampling_);
    }
  }

  absl::flat_hash_set<uint32_t> sample_rates_;
  absl::flat_hash_set<uint8_t> bit_depths_;
  absl::StatusCode expected_status_code_;
  uint32_t expected_sample_rate_;
  uint8_t expected_bit_depth_;
  bool expected_requires_resampling_;
};

TEST_F(GetCommonSampleRateAndBitDepthTest, DefaultUnique) { Test(); }

TEST_F(GetCommonSampleRateAndBitDepthTest, InvalidSampleRatesArg) {
  sample_rates_ = {};
  expected_status_code_ = absl::StatusCode::kInvalidArgument;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, InvalidBitDepthsArg) {
  bit_depths_ = {};
  expected_status_code_ = absl::StatusCode::kInvalidArgument;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest,
       DifferentSampleRatesResampleTo48Khz) {
  sample_rates_ = {16000, 96000};
  expected_sample_rate_ = 48000;
  expected_requires_resampling_ = true;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, DifferentBitDepthResampleTo16Bits) {
  bit_depths_ = {24, 32};
  expected_bit_depth_ = 16;
  expected_requires_resampling_ = true;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, SampleRatesAndBitDepthsVary) {
  bit_depths_ = {24, 32};
  expected_bit_depth_ = 16;

  sample_rates_ = {16000, 96000};
  expected_sample_rate_ = 48000;

  expected_requires_resampling_ = true;

  Test();
}

TEST_F(GetCommonSampleRateAndBitDepthTest, LargeCommonSampleRatesAndBitDepths) {
  sample_rates_ = {192000};
  expected_sample_rate_ = 192000;
  bit_depths_ = {32};
  expected_bit_depth_ = 32;

  Test();
}

TEST(CopyDemixingInfoParameterData, Basic) {
  iamf_tools_cli_proto::DemixingInfoParameterData
      demixing_info_parameter_data_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        dmixp_mode: DMIXP_MODE_3 reserved: 0
      )pb",
      &demixing_info_parameter_data_metadata));
  DemixingInfoParameterData demixing_info_parameter_data;
  EXPECT_TRUE(
      CopyDemixingInfoParameterData(demixing_info_parameter_data_metadata,
                                    demixing_info_parameter_data)
          .ok());

  EXPECT_EQ(demixing_info_parameter_data.dmixp_mode,
            DemixingInfoParameterData::kDMixPMode3);
  EXPECT_EQ(demixing_info_parameter_data.reserved, 0);
}

TEST(CopyObuHeader, Default) {
  iamf_tools_cli_proto::ObuHeaderMetadata obu_header_metadata;
  ObuHeader header_ = GetHeaderFromMetadata(obu_header_metadata);
  // `ObuHeader` is initialized with reasonable default values for typical use
  // cases.
  EXPECT_EQ(header_.obu_redundant_copy, false);
  EXPECT_EQ(header_.obu_trimming_status_flag, false);
  EXPECT_EQ(header_.obu_extension_flag, false);
}

TEST(CopyObuHeader, MostValuesModified) {
  iamf_tools_cli_proto::ObuHeaderMetadata obu_header_metadata;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        obu_redundant_copy: true
        obu_trimming_status_flag: true
        obu_extension_flag: true
        num_samples_to_trim_at_end: 1
        num_samples_to_trim_at_start: 2
        extension_header_size: 5
        extension_header_bytes: "extra"
      )pb",
      &obu_header_metadata));
  ObuHeader header_ = GetHeaderFromMetadata(obu_header_metadata);

  EXPECT_EQ(header_.obu_redundant_copy, true);
  EXPECT_EQ(header_.obu_trimming_status_flag, true);
  EXPECT_EQ(header_.obu_extension_flag, true);
  EXPECT_EQ(header_.num_samples_to_trim_at_end, 1);
  EXPECT_EQ(header_.num_samples_to_trim_at_start, 2);
  EXPECT_EQ(header_.extension_header_size, 5);
  EXPECT_EQ(header_.extension_header_bytes,
            (std::vector<uint8_t>{'e', 'x', 't', 'r', 'a'}));
}

TEST(CollectAndValidateParamDefinitions, IdenticalMixGain) {
  // Initialize prerequisites.
  const DecodedUleb128 kAudioElemenetId = 100;
  const DecodedUleb128 kMixPresentationId = 100;
  const DecodedUleb128 kCommonParameterId = 99999;
  const DecodedUleb128 kCommonParameterRate = 48000;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements = {};

  // Create a mix presentation OBU. It will have a `element_mix_config` and
  // `output_mix_gain` which common settings.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, kAudioElemenetId, kCommonParameterId,
      kCommonParameterRate, mix_presentation_obus);
  // Assert that the new mix presentation OBU has identical param definitions.
  ASSERT_EQ(mix_presentation_obus.back()
                .sub_mixes_[0]
                .audio_elements[0]
                .element_mix_config.mix_gain,
            mix_presentation_obus.back()
                .sub_mixes_[0]
                .output_mix_config.output_mix_gain);

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> result;
  EXPECT_TRUE(CollectAndValidateParamDefinitions(audio_elements,
                                                 mix_presentation_obus, result)
                  .ok());
  // Validate there is one unique param definition.
  EXPECT_EQ(result.size(), 1);
}

TEST(CollectAndValidateParamDefinitions,
     InvalidParametersWithSameIdHaveDifferentDefaultValues) {
  // Initialize prerequisites.
  const DecodedUleb128 kAudioElemenetId = 100;
  const DecodedUleb128 kMixPresentationId = 100;
  const DecodedUleb128 kCommonParameterId = 99999;
  const DecodedUleb128 kCommonParameterRate = 48000;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements = {};

  // Create a mix presentation OBU. It will have a `element_mix_config` and
  // `output_mix_gain` which common settings.
  std::list<MixPresentationObu> mix_presentation_obus;
  AddMixPresentationObuWithAudioElementIds(
      kMixPresentationId, kAudioElemenetId, kCommonParameterId,
      kCommonParameterRate, mix_presentation_obus);
  auto& output_mix_gain = mix_presentation_obus.back()
                              .sub_mixes_[0]
                              .output_mix_config.output_mix_gain;
  output_mix_gain.default_mix_gain_ = 1;
  // Assert that the new mix presentation OBU has different param definitions.
  ASSERT_NE(mix_presentation_obus.back()
                .sub_mixes_[0]
                .audio_elements[0]
                .element_mix_config.mix_gain,
            output_mix_gain);

  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*> result;
  EXPECT_EQ(CollectAndValidateParamDefinitions(audio_elements,
                                               mix_presentation_obus, result)
                .code(),
            absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace iamf_tools
