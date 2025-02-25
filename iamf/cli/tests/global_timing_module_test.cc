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
#include <optional>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status_matchers.h"
#include "absl/types/span.h"
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
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;

constexpr DecodedUleb128 kSampleRate = 48000;
constexpr uint32_t kDuration = 960;
constexpr DecodedUleb128 kFirstAudioFrameId = 1000;
constexpr DecodedUleb128 kFirstParameterId = 0;

// Normally the `ParamDefinitions` are stored in the descriptor OBUs. Tests can
// hold the raw definitions, for simplicity tests can hold the raw
// definitions and use this function to adapt to a map of pointers for the API.
absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
GetParamDefinitionMap(
    const absl::flat_hash_map<DecodedUleb128, ParamDefinition>&
        param_definitions) {
  absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      parameter_id_to_param_definition_pointer;
  for (const auto& [parameter_id, param_definition] : param_definitions) {
    parameter_id_to_param_definition_pointer[parameter_id] = &param_definition;
  }
  return parameter_id_to_param_definition_pointer;
}

class GlobalTimingModuleTest : public ::testing::Test {
 protected:
  // Sets up a single audio element with the given substream IDs.
  void SetupObusForSubstreamIds(
      absl::Span<const DecodedUleb128> substream_ids) {
    constexpr DecodedUleb128 kCodecConfigId = 0;
    constexpr DecodedUleb128 kFirstAudioElementId = 0;
    AddLpcmCodecConfigWithIdAndSampleRate(kCodecConfigId, kSampleRate,
                                          codec_config_obus_);
    AddAmbisonicsMonoAudioElementWithSubstreamIds(
        kFirstAudioElementId, kCodecConfigId, substream_ids, codec_config_obus_,
        audio_elements_);
  }

  absl::flat_hash_map<uint32_t, CodecConfigObu> codec_config_obus_;
  absl::flat_hash_map<DecodedUleb128, AudioElementWithData> audio_elements_;
};

TEST(Create, SucceedsForEmptyAudioElementsAndParamDefinitions) {
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;

  // OK. To support "trivial IA Sequences" it is convenient to be able to
  // support a null case.
  EXPECT_THAT(
      GlobalTimingModule::Create(kEmptyAudioElements, kEmptyParamDefinitions),
      NotNull());
}

TEST_F(GlobalTimingModuleTest, CreateFailsForDuplicateSubstreamIds) {
  const DecodedUleb128 kDuplicateSubsteamId = kFirstAudioFrameId;
  SetupObusForSubstreamIds({kDuplicateSubsteamId, kDuplicateSubsteamId});
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;

  EXPECT_THAT(
      GlobalTimingModule::Create(audio_elements_, kEmptyParamDefinitions),
      IsNull());
}

TEST(Create, FailsForParameterIdWithZeroRate) {
  constexpr DecodedUleb128 kInvalidParameterRate = 0;
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  absl::flat_hash_map<DecodedUleb128, ParamDefinition> param_definitions;
  // The timing model does not care about the specific type of parameter. Use a
  // generic one.
  AddParamDefinitionWithMode0AndOneSubblock(
      kFirstParameterId, kInvalidParameterRate, 64, param_definitions);

  EXPECT_THAT(
      GlobalTimingModule::Create(kEmptyAudioElements,
                                 GetParamDefinitionMap(param_definitions)),
      IsNull());
}

TEST_F(GlobalTimingModuleTest, GetNextAudioFrameTimestampAdvancesTimestamps) {
  SetupObusForSubstreamIds({kFirstAudioFrameId});
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_, kEmptyParamDefinitions);
  ASSERT_THAT(global_timing_module, NotNull());

  constexpr uint32_t kDuration = 128;
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  EXPECT_THAT(
      global_timing_module->GetNextAudioFrameTimestamps(
          kFirstAudioFrameId, kDuration, start_timestamp, end_timestamp),
      IsOk());

  EXPECT_EQ(start_timestamp, 0);
  EXPECT_EQ(end_timestamp, kDuration);
}

TEST_F(GlobalTimingModuleTest,
       GetNextAudioFrameTimestampAdvancesTimestampsEachSubsteamIndependently) {
  constexpr DecodedUleb128 kFirstSubstreamId = kFirstAudioFrameId;
  constexpr DecodedUleb128 kSecondSubstreamId = 2000;
  SetupObusForSubstreamIds({kFirstSubstreamId, kSecondSubstreamId});
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_, kEmptyParamDefinitions);
  ASSERT_THAT(global_timing_module, NotNull());

  constexpr uint32_t kDuration = 128;
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  EXPECT_THAT(
      global_timing_module->GetNextAudioFrameTimestamps(
          kFirstAudioFrameId, kDuration, start_timestamp, end_timestamp),
      IsOk());
  EXPECT_EQ(start_timestamp, 0);
  EXPECT_EQ(end_timestamp, kDuration);

  // It's OK for another substream to tick at a different rate. It will advance
  // independently.
  constexpr uint32_t kLongerDuration = 256;
  EXPECT_THAT(
      global_timing_module->GetNextAudioFrameTimestamps(
          kSecondSubstreamId, kLongerDuration, start_timestamp, end_timestamp),
      IsOk());
  EXPECT_EQ(start_timestamp, 0);
  EXPECT_EQ(end_timestamp, kLongerDuration);
}

TEST(GetNextAudioFrameTimestamp, FailsForUnknownSubstreamId) {
  constexpr DecodedUleb128 kUnknownSubstreamId = 1000;
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;
  auto global_timing_module =
      GlobalTimingModule::Create(kEmptyAudioElements, kEmptyParamDefinitions);
  ASSERT_THAT(global_timing_module, NotNull());

  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  EXPECT_THAT(
      global_timing_module->GetNextAudioFrameTimestamps(
          kUnknownSubstreamId, kDuration, start_timestamp, end_timestamp),
      Not(IsOk()));

  // Despite the error, the timestamps should be set to the duration, which
  // facilitates generating negative test vectors.
  EXPECT_EQ(start_timestamp, 0);
  EXPECT_EQ(end_timestamp, kDuration);
}

TEST(GetNextParameterBlockTimestamps, AdvancesTimestamps) {
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  absl::flat_hash_map<DecodedUleb128, ParamDefinition> param_definitions;
  // The timing model does not care about the specific type of parameter. Use a
  // generic one.
  AddParamDefinitionWithMode0AndOneSubblock(kFirstParameterId,
                                            /*parameter_rate=*/kSampleRate, 64,
                                            param_definitions);
  auto global_timing_module = GlobalTimingModule::Create(
      kEmptyAudioElements, GetParamDefinitionMap(param_definitions));
  ASSERT_THAT(global_timing_module, NotNull());

  constexpr uint32_t kDuration = 64;
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  EXPECT_THAT(
      global_timing_module->GetNextParameterBlockTimestamps(
          kFirstParameterId, 0, kDuration, start_timestamp, end_timestamp),
      IsOk());
  EXPECT_EQ(start_timestamp, 0);
  EXPECT_EQ(end_timestamp, kDuration);

  EXPECT_THAT(global_timing_module->GetNextParameterBlockTimestamps(
                  kFirstParameterId, end_timestamp, kDuration, start_timestamp,
                  end_timestamp),
              IsOk());
  EXPECT_EQ(start_timestamp, 64);
  EXPECT_EQ(end_timestamp, 128);
}

TEST(GetNextParameterBlockTimestamps, FailsWhenInputTimestampDoesNotAgree) {
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  absl::flat_hash_map<DecodedUleb128, ParamDefinition> param_definitions;
  // The timing model does not care about the specific type of parameter. Use a
  // generic one.
  AddParamDefinitionWithMode0AndOneSubblock(kFirstParameterId,
                                            /*parameter_rate=*/kSampleRate, 64,
                                            param_definitions);
  auto global_timing_module = GlobalTimingModule::Create(
      kEmptyAudioElements, GetParamDefinitionMap(param_definitions));
  ASSERT_THAT(global_timing_module, NotNull());

  constexpr uint32_t kDuration = 64;
  constexpr InternalTimestamp kMismatchedInputStartTimestamp = 1;
  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;
  EXPECT_THAT(global_timing_module->GetNextParameterBlockTimestamps(
                  kFirstParameterId, kMismatchedInputStartTimestamp, kDuration,
                  start_timestamp, end_timestamp),
              Not(IsOk()));
  // Despite, the error, the timestamps are set to the duration, which
  // facilitates generating negative test vectors.
  EXPECT_EQ(start_timestamp, 0);
  EXPECT_EQ(end_timestamp, kDuration);
}

TEST(GetNextParameterBlockTimestamps, FailsForUnknownParameterId) {
  constexpr DecodedUleb128 kStrayParameterBlockId = kFirstParameterId + 1;
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  absl::flat_hash_map<DecodedUleb128, ParamDefinition> param_definitions;
  // The timing model does not care about the specific type of parameter. Use a
  // generic one.
  AddParamDefinitionWithMode0AndOneSubblock(kFirstParameterId,
                                            /*parameter_rate=*/kSampleRate, 64,
                                            param_definitions);
  auto global_timing_module = GlobalTimingModule::Create(
      kEmptyAudioElements, GetParamDefinitionMap(param_definitions));
  ASSERT_THAT(global_timing_module, NotNull());

  InternalTimestamp start_timestamp;
  InternalTimestamp end_timestamp;

  EXPECT_THAT(
      global_timing_module->GetNextParameterBlockTimestamps(
          kStrayParameterBlockId, 0, 64, start_timestamp, end_timestamp),
      Not(IsOk()));
}

TEST(GetGlobalAudioFrameTimestamp, ReturnsErrorWhenNoAudioFrames) {
  const absl::flat_hash_map<DecodedUleb128, AudioElementWithData>
      kEmptyAudioElements;
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;
  auto global_timing_module =
      GlobalTimingModule::Create(kEmptyAudioElements, kEmptyParamDefinitions);
  ASSERT_THAT(global_timing_module, NotNull());

  std::optional<InternalTimestamp> global_timestamp;
  EXPECT_THAT(
      global_timing_module->GetGlobalAudioFrameTimestamp(global_timestamp),
      Not(IsOk()));

  EXPECT_EQ(global_timestamp, std::nullopt);
}

TEST_F(
    GlobalTimingModuleTest,
    GetGlobalAudioFrameTimestampReturnsCommonTimestampWhenAudioFramesAreInSync) {
  constexpr DecodedUleb128 kFirstSubstreamId = kFirstAudioFrameId;
  constexpr DecodedUleb128 kSecondSubstreamId = 2000;
  SetupObusForSubstreamIds({kFirstSubstreamId, kSecondSubstreamId});
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_, kEmptyParamDefinitions);
  ASSERT_THAT(global_timing_module, NotNull());

  // Simulate a full temporal unit; two substreams are in sync.
  constexpr uint32_t kDuration = 128;
  InternalTimestamp ignored_start_timestamp;
  InternalTimestamp ignored_end_timestamp;
  EXPECT_THAT(global_timing_module->GetNextAudioFrameTimestamps(
                  kFirstSubstreamId, kDuration, ignored_start_timestamp,
                  ignored_end_timestamp),
              IsOk());
  EXPECT_THAT(global_timing_module->GetNextAudioFrameTimestamps(
                  kSecondSubstreamId, kDuration, ignored_start_timestamp,
                  ignored_end_timestamp),
              IsOk());

  std::optional<InternalTimestamp> global_timestamp;
  EXPECT_THAT(
      global_timing_module->GetGlobalAudioFrameTimestamp(global_timestamp),
      IsOk());

  EXPECT_EQ(global_timestamp, kDuration);
}

TEST_F(
    GlobalTimingModuleTest,
    GetGlobalAudioFrameTimestampReturnsOkButSetsNulloptWhenAudioFramesAreOutOfSync) {
  constexpr DecodedUleb128 kFirstSubstreamId = kFirstAudioFrameId;
  constexpr DecodedUleb128 kSecondSubstreamId = 2000;
  SetupObusForSubstreamIds({kFirstSubstreamId, kSecondSubstreamId});
  const absl::flat_hash_map<DecodedUleb128, const ParamDefinition*>
      kEmptyParamDefinitions;
  auto global_timing_module =
      GlobalTimingModule::Create(audio_elements_, kEmptyParamDefinitions);
  ASSERT_THAT(global_timing_module, NotNull());
  // Simulate substreams which are desynchronized.
  constexpr uint32_t kDuration = 128;
  constexpr uint32_t kLongerDuration = 129;
  InternalTimestamp ignored_start_timestamp;
  InternalTimestamp ignored_end_timestamp;
  EXPECT_THAT(global_timing_module->GetNextAudioFrameTimestamps(
                  kFirstSubstreamId, kDuration, ignored_start_timestamp,
                  ignored_end_timestamp),
              IsOk());
  EXPECT_THAT(global_timing_module->GetNextAudioFrameTimestamps(
                  kSecondSubstreamId, kLongerDuration, ignored_start_timestamp,
                  ignored_end_timestamp),
              IsOk());

  // It is OK for them to be out of sync; it's possible that the caller is
  // partially through a temporal unit. But that implies there is not currently
  // a "global timestamp".
  std::optional<InternalTimestamp> global_timestamp;
  EXPECT_THAT(
      global_timing_module->GetGlobalAudioFrameTimestamp(global_timestamp),
      IsOk());

  EXPECT_EQ(global_timestamp, std::nullopt);
}

// TODO(b/291732058): Bring back parameter block coverage validation.

}  // namespace
}  // namespace iamf_tools
