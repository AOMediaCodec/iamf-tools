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
#include "iamf/cli/global_timing_module.h"

#include <cstdint>
#include <memory>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "iamf/cli/audio_element_with_data.h"
#include "iamf/cli/proto/parameter_block.pb.h"
#include "iamf/cli/proto/user_metadata.pb.h"
#include "iamf/cli/tests/cli_test_utils.h"
#include "iamf/obu/codec_config.h"
#include "iamf/obu/param_definitions.h"
#include "iamf/obu/types.h"

namespace iamf_tools {
namespace {

using ::absl_testing::IsOk;

const DecodedUleb128 kCodecConfigId = 0;
const DecodedUleb128 kSampleRate = 48000;
const DecodedUleb128 kFirstAudioElementId = 0;
const DecodedUleb128 kFirstAudioFrameId = 1000;
const DecodedUleb128 kFirstParameterId = 0;

class GlobalTimingModuleTest : public ::testing::Test {
 protected:
  void InitializeForTestingValidateParameterBlockCoverage() {
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus_);
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kFirstAudioElementId, kCodecConfigId, {kFirstAudioFrameId},
        codec_config_obus_, audio_elements_);
    EXPECT_THAT(Initialize(), IsOk());

    TestGetNextAudioFrameStamps(kFirstAudioFrameId, 512, 0, 512);
    TestGetNextAudioFrameStamps(kFirstAudioFrameId, 512, 512, 1024);
  }

  // Constructs and initializes `global_timing_module_`.
  absl::Status Initialize() {
    global_timing_module_ =
        std::make_unique<GlobalTimingModule>(GlobalTimingModule());

    // Normally the `ParamDefinitions` are stored in the descriptor OBUs. For
    // simplicity they are stored in the class. Transform it to a map of
    // pointers to pass to `Initialize`.
    absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
        parameter_id_to_param_definition_pointer = {};
    for (const auto& [parameter_id, param_definition] : param_definitions_) {
      parameter_id_to_param_definition_pointer[parameter_id] =
          &param_definition;
    }

    return global_timing_module_->Initialize(
        audio_elements_, parameter_id_to_param_definition_pointer);
  }

  void TestGetNextAudioFrameStamps(
      DecodedUleb128 substream_id, uint32_t duration,
      InternalTimestamp expected_start_timestamp,
      InternalTimestamp expected_end_timestamp,
      absl::StatusCode expected_status_code = absl::StatusCode::kOk) {
    InternalTimestamp start_timestamp;
    InternalTimestamp end_timestamp;
    EXPECT_EQ(global_timing_module_
                  ->GetNextAudioFrameTimestamps(substream_id, duration,
                                                start_timestamp, end_timestamp)
                  .code(),
              expected_status_code);
    EXPECT_EQ(start_timestamp, expected_start_timestamp);
    EXPECT_EQ(end_timestamp, expected_end_timestamp);
  }

  void TestGetNextParameterBlockTimestamps(
      DecodedUleb128 parameter_id, InternalTimestamp input_start_timestamp,
      uint32_t duration, InternalTimestamp expected_start_timestamp,
      InternalTimestamp expected_end_timestamp) {
    InternalTimestamp start_timestamp;
    InternalTimestamp end_timestamp;
    EXPECT_THAT(global_timing_module_->GetNextParameterBlockTimestamps(
                    parameter_id, input_start_timestamp, duration,
                    start_timestamp, end_timestamp),
                IsOk());
    EXPECT_EQ(start_timestamp, expected_start_timestamp);
    EXPECT_EQ(end_timestamp, expected_end_timestamp);
  }

  std::unique_ptr<GlobalTimingModule> global_timing_module_ = nullptr;

 protected:
  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_ = {};
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_ =
      {};
  absl::flat_hash_map<DecodedUleb128, ParamDefinition> param_definitions_ = {};
};

TEST_F(GlobalTimingModuleTest, OneSubstream) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus_);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstAudioFrameId},
      codec_config_obus_, audio_elements_);
  EXPECT_THAT(Initialize(), IsOk());

  TestGetNextAudioFrameStamps(kFirstAudioFrameId, 128, 0, 128);
  TestGetNextAudioFrameStamps(kFirstAudioFrameId, 128, 128, 256);
  TestGetNextAudioFrameStamps(kFirstAudioFrameId, 128, 256, 384);
}

TEST_F(GlobalTimingModuleTest, InvalidUnknownSubstreamId) {
  constexpr DecodedUleb128 kSubstreamId = 9999;
  constexpr DecodedUleb128 kUnknownSubstreamId = 10000;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus_);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kSubstreamId}, codec_config_obus_,
      audio_elements_);
  EXPECT_THAT(Initialize(), IsOk());

  TestGetNextAudioFrameStamps(kUnknownSubstreamId, 128, 0, 128,
                              absl::StatusCode::kInvalidArgument);
}

TEST_F(GlobalTimingModuleTest, InvalidDuplicateSubstreamIds) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus_);
  const DecodedUleb128 kDuplicateSubsteamId = kFirstAudioFrameId;
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId,
      {kDuplicateSubsteamId, kDuplicateSubsteamId}, codec_config_obus_,
      audio_elements_);
  EXPECT_EQ(Initialize().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(GlobalTimingModuleTest, TwoAudioElements) {
  constexpr DecodedUleb128 kFirstSubstreamId = kFirstAudioFrameId;
  constexpr DecodedUleb128 kSecondSubstreamId = 2000;
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus_);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kFirstAudioElementId, kCodecConfigId, {kFirstSubstreamId},
      codec_config_obus_, audio_elements_);
  const DecodedUleb128 kSecondAudioElementId = 1;
  ASSERT_NE(kFirstAudioElementId, kSecondAudioElementId);
  AddAmbisonicsMonoAudioElementWithSubstreamIds(
      kSecondAudioElementId, kCodecConfigId, {kSecondSubstreamId},
      codec_config_obus_, audio_elements_);
  EXPECT_THAT(Initialize(), IsOk());

  // All subtreams have separate time keeping functionality.
  TestGetNextAudioFrameStamps(kFirstSubstreamId, 128, 0, 128);
  TestGetNextAudioFrameStamps(kFirstSubstreamId, 128, 128, 256);
  TestGetNextAudioFrameStamps(kFirstSubstreamId, 128, 256, 384);

  TestGetNextAudioFrameStamps(2000, 256, 0, 256);
  TestGetNextAudioFrameStamps(2000, 256, 256, 512);
}

TEST_F(GlobalTimingModuleTest, OneParameterId) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus_);
  // The timing model does not care about the specific type of parameter. Use a
  // generic one.
  AddParamDefinitionWithMode0AndOneSubblock(kFirstParameterId,
                                            /*parameter_rate=*/kSampleRate, 64,
                                            param_definitions_);
  EXPECT_THAT(Initialize(), IsOk());

  TestGetNextParameterBlockTimestamps(kFirstParameterId, 0, 64, 0, 64);
  TestGetNextParameterBlockTimestamps(kFirstParameterId, 64, 64, 64, 128);
  TestGetNextParameterBlockTimestamps(kFirstParameterId, 128, 64, 128, 192);
}

TEST_F(GlobalTimingModuleTest,
       FailsWhenGettingTimestampForStrayParameterBlock) {
  AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                        codec_config_obus_);

  EXPECT_THAT(Initialize(), IsOk());

  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  const auto kStrayParameterBlockId = kFirstParameterId + 1;
  EXPECT_FALSE(global_timing_module_
                   ->GetNextParameterBlockTimestamps(kStrayParameterBlockId, 0,
                                                     64, start_timestamp,
                                                     end_timestamp)
                   .ok());
}

TEST_F(GlobalTimingModuleTest, InvalidWhenParameterRateIsZero) {
  // The timing model does not care about the specific type of parameter. Use a
  // generic one.
  AddParamDefinitionWithMode0AndOneSubblock(kFirstParameterId,
                                            /*parameter_rate=*/0, 64,
                                            param_definitions_);
  EXPECT_FALSE(Initialize().ok());
}

// TODO(b/291732058): Bring back parameter block coverage validation.

}  // namespace
}  // namespace iamf_tools
